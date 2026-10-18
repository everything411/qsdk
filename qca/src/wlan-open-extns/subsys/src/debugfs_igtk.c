/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/debugfs.h>
#include <linux/hex.h>
#include <linux/printk.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/ieee80211.h>
#include "../ieee80211_i.h"
#include "../key.h"
#include "debugfs_igtk.h"

#define IGTK_IDX_MIN  NUM_DEFAULT_KEYS
#define IGTK_IDX_MAX  (NUM_DEFAULT_KEYS + NUM_DEFAULT_MGMT_KEYS - 1)

/* ------------------------------------------------------------------ */
/* GET: read IGTK key material from mac80211's internal key store      */
/* ------------------------------------------------------------------ */

/*
 * IGTK Debugfs Interface
 *
 * Multi-Link Operation (MLO):
 *   In MLO setups, each link maintains independent group keys.
 *   This function displays keys for all valid links.
 *   For non-MLO interfaces, only link 0 is shown.
 *
 * VLAN Interfaces:
 *   Per-VLAN IGTK fetch is not supported in this implementation;
 *   only the keys installed on the parent AP interface are shown.
 */
static ssize_t igtk_get_read(struct file *file, char __user *user_buf,
			     size_t count, loff_t *ppos)
{
	struct ieee80211_sub_if_data *sdata = file->private_data;
	struct ieee80211_key *key;
	char *buf;
	int len = 0;
	const int buf_size = 2048;
	ssize_t ret;
	int link_id, i, j;

	buf = kzalloc(buf_size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	/*
	 * Hold the wiphy mutex while dereferencing the RCU-protected
	 * gtk[] pointers so that the key cannot be freed under us.
	 */
	wiphy_lock(sdata->local->hw.wiphy);
	printk(KERN_DEBUG "mac80211 igtk_get: %s\n", sdata->name);

	for (link_id = 0; link_id < IEEE80211_MLD_MAX_NUM_LINKS; link_id++) {
		struct ieee80211_link_data *link;

		if (!ieee80211_vif_is_mld(&sdata->vif)) {
			link = &sdata->deflink;
		} else {
			link = sdata_dereference(sdata->link[link_id], sdata);
			if (!link)
				continue;
		}

		len += scnprintf(buf + len, buf_size - len,
				 "Link %d:\n", link_id);

		for (i = IGTK_IDX_MIN; i <= IGTK_IDX_MAX; i++) {
			key = wiphy_dereference(sdata->local->hw.wiphy,
						link->gtk[i]);
			if (!key) {
				printk(KERN_DEBUG "mac80211 igtk_get: Link %d IGTK[%d] not installed\n",
				       link_id, i);
				len += scnprintf(buf + len, buf_size - len,
						 "  IGTK[%d]: not installed\n", i);
				continue;
			}

			printk(KERN_DEBUG "mac80211 igtk_get: Link %d IGTK[%d] cipher=0x%08x keylen=%d\n",
			       link_id, i, key->conf.cipher, key->conf.keylen);
			len += scnprintf(buf + len, buf_size - len,
					 "  IGTK[%d]: cipher=0x%08x keylen=%d key=",
					 i, key->conf.cipher, key->conf.keylen);

			for (j = 0; j < key->conf.keylen; j++)
				len += scnprintf(buf + len, buf_size - len,
						 "%02x", key->conf.key[j]);

			len += scnprintf(buf + len, buf_size - len, "\n");
		}

		/* Non-MLO: only link 0 exists */
		if (!ieee80211_vif_is_mld(&sdata->vif))
			break;
	}

	wiphy_unlock(sdata->local->hw.wiphy);

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret;
}

/* ------------------------------------------------------------------ */
/* SET: install an IGTK key via mac80211's key management path         */
/* ------------------------------------------------------------------ */

/*
 * IGTK SET Interface
 *
 * Format: <link_id> <keyidx> <cipher> <key_hex>
 *
 * Supported ciphers: bip-cmac-128, bip-cmac-256, bip-gmac-128, bip-gmac-256
 *
 * Multi-Link Operation:
 *   - For MLO interfaces, link_id specifies which link to install the key on
 *   - For non-MLO interfaces, link_id must be 0
 *
 * Example:
 *   echo "0 4 bip-cmac-128 0102030405060708090a0b0c0d0e0f10" > igtk_set
 *   (Install IGTK index 4 with BIP-CMAC-128 on link 0)
 */
static ssize_t igtk_set_write(struct file *file, const char __user *user_buf,
			      size_t count, loff_t *ppos)
{
	struct ieee80211_sub_if_data *sdata = file->private_data;
	char buf[256] = {};
	u8 key_data[WLAN_MAX_KEY_LEN];
	u32 cipher;
	int link_id, keyidx, keylen;
	int ret;
	char *p, *keyidx_str, *cipher_str, *key_hex;
	struct ieee80211_key *key;
	struct ieee80211_link_data *link;

	/* Validate interface type */
	if (sdata->vif.type != NL80211_IFTYPE_AP &&
	    sdata->vif.type != NL80211_IFTYPE_AP_VLAN) {
		pr_err("mac80211 igtk_set: interface %s is not an AP or AP_VLAN\n",
		       sdata->name);
		return -EOPNOTSUPP;
	}

	if (count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	printk("mac80211 igtk_set: %s count=%zu\n", sdata->name, count);

	/* Strip trailing newline */
	if (count && buf[count - 1] == '\n')
		buf[count - 1] = '\0';
	else
		buf[count] = '\0';

	/*
	 * Parse: "<link_id> <keyidx> <cipher> <key_hex>"
	 */
	p = buf;

	/* Parse link_id */
	keyidx_str = strchr(p, ' ');
	if (!keyidx_str)
		return -EINVAL;
	*keyidx_str++ = '\0';

	if (kstrtoint(p, 10, &link_id))
		return -EINVAL;

	if (link_id < 0 || link_id >= IEEE80211_MLD_MAX_NUM_LINKS) {
		pr_err("mac80211 igtk_set: link_id %d out of range [0,%d]\n",
		       link_id, IEEE80211_MLD_MAX_NUM_LINKS - 1);
		return -EINVAL;
	}

	/* Parse keyidx */
	cipher_str = strchr(keyidx_str, ' ');
	if (!cipher_str)
		return -EINVAL;
	*cipher_str++ = '\0';

	if (kstrtoint(keyidx_str, 10, &keyidx))
		return -EINVAL;

	if (keyidx < IGTK_IDX_MIN || keyidx > IGTK_IDX_MAX) {
		pr_err("mac80211 igtk_set: keyidx %d out of IGTK range [%d,%d]\n",
		       keyidx, IGTK_IDX_MIN, IGTK_IDX_MAX);
		return -EINVAL;
	}

	/* Parse cipher string */
	key_hex = strchr(cipher_str, ' ');
	if (!key_hex)
		return -EINVAL;
	*key_hex++ = '\0';
	cipher_str = strim(cipher_str);

	if (!strcasecmp(cipher_str, "bip-cmac-128"))
		cipher = WLAN_CIPHER_SUITE_AES_CMAC;
	else if (!strcasecmp(cipher_str, "bip-cmac-256"))
		cipher = WLAN_CIPHER_SUITE_BIP_CMAC_256;
	else if (!strcasecmp(cipher_str, "bip-gmac-128"))
		cipher = WLAN_CIPHER_SUITE_BIP_GMAC_128;
	else if (!strcasecmp(cipher_str, "bip-gmac-256"))
		cipher = WLAN_CIPHER_SUITE_BIP_GMAC_256;
	else {
		pr_err("mac80211 igtk_set: unknown cipher \"%s\" (use bip-cmac-128/bip-cmac-256/bip-gmac-128/bip-gmac-256)\n",
		       cipher_str);
		return -EINVAL;
	}

	/* Get the link data */
	if (!ieee80211_vif_is_mld(&sdata->vif)) {
		if (link_id != 0) {
			pr_err("mac80211 igtk_set: non-MLO interface, link_id must be 0\n");
			return -EINVAL;
		}
		link = &sdata->deflink;
	} else {
		int l;

		link = NULL;
		for (l = 0; l < IEEE80211_MLD_MAX_NUM_LINKS; l++) {
			struct ieee80211_link_data *tmp =
				sdata_dereference(sdata->link[l], sdata);
			if (tmp && tmp->link_id == link_id) {
				link = tmp;
				break;
			}
		}
		if (!link) {
			pr_err("mac80211 igtk_set: link %d not found\n", link_id);
			return -ENOENT;
		}
	}

	/* Decode hex key */
	key_hex = strim(key_hex);
	keylen = strlen(key_hex) / 2;
	if (keylen == 0 || keylen > WLAN_MAX_KEY_LEN)
		return -EINVAL;

	if (hex2bin(key_data, key_hex, keylen))
		return -EINVAL;

	printk("mac80211 igtk_set: link=%d keyidx=%d cipher=0x%08x keylen=%d link_id=%d vif_is_mld=%d\n",
	       link_id, keyidx, cipher, keylen,
	       link->link_id, ieee80211_vif_is_mld(&sdata->vif));
	key = ieee80211_key_alloc(cipher, keyidx, keylen, key_data, 0, NULL);
	if (IS_ERR(key)) {
		printk(KERN_ERR "mac80211 igtk_set: key alloc failed for link=%d keyidx=%d cipher=0x%08x: err=%ld\n",
		       link_id, keyidx, cipher, PTR_ERR(key));
		return PTR_ERR(key);
	}

	/*
	 * IGTK is per-link in MLO; not link-specific (-1) in non-MLO.
	 * Must match the existing key's link_id or ieee80211_iter_keys_rcu()
	 * will WARN "old link ID N doesn't match new link ID -1".
	 */
	key->conf.link_id = ieee80211_vif_is_mld(&sdata->vif) ? link->link_id : -1;

	/*
	 * ieee80211_key_link() must be called with the wiphy mutex held.
	 * Pass NULL for sta since IGTK is a group key.
	 */
	wiphy_lock(sdata->local->hw.wiphy);
	ret = ieee80211_key_link(key, link, NULL);
	wiphy_unlock(sdata->local->hw.wiphy);

	printk("mac80211 igtk_set: ieee80211_key_link() ret=%d\n", ret);

	if (ret == -EALREADY)
		ret = 0;

	if (ret)
		printk(KERN_ERR "mac80211 igtk_set: key install failed link=%d keyidx=%d ret=%d\n",
		       link_id, keyidx, ret);
	else
		printk("mac80211 igtk_set: key installed ok link=%d keyidx=%d\n",
		       link_id, keyidx);

	return ret ? ret : (ssize_t)count;
}

/* ------------------------------------------------------------------ */
/* file_operations                                                      */
/* ------------------------------------------------------------------ */

static const struct file_operations igtk_get_ops = {
	.read   = igtk_get_read,
	.open   = simple_open,
	.llseek = default_llseek,
};

static const struct file_operations igtk_set_ops = {
	.write  = igtk_set_write,
	.open   = simple_open,
	.llseek = default_llseek,
};

/* ------------------------------------------------------------------ */
/* Registration                                                         */
/* ------------------------------------------------------------------ */

/**
 * ieee80211_debugfs_igtk_add - create IGTK debugfs entries for an interface
 * @sdata: the virtual interface
 *
 * Creates two files under the interface's debugfs directory:
 *   igtk_get  (0400) - read current IGTK key material from mac80211
 *   igtk_set  (0200) - install a new IGTK key (triggers WMI install)
 *
 * Format for igtk_set: <link_id> <keyidx> <cipher> <key_hex>
 *   - link_id: 0 for non-MLO, 0-14 for MLO
 *   - keyidx: 4-5 (IGTK_IDX_MIN to IGTK_IDX_MAX)
 *   - cipher: bip-cmac-128 | bip-cmac-256 | bip-gmac-128 | bip-gmac-256
 *   - key_hex: 32 hex digits (16 bytes) or 64 hex digits (32 bytes)
 */
void ieee80211_debugfs_igtk_add(struct ieee80211_sub_if_data *sdata)
{
	struct dentry *dir = sdata->vif.debugfs_dir;

	if (!dir)
		return;

	debugfs_create_file("igtk_get", 0400, dir, sdata, &igtk_get_ops);
	debugfs_create_file("igtk_set", 0200, dir, sdata, &igtk_set_ops);
}
