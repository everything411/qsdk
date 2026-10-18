/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/debugfs.h>
#include <linux/hex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/ieee80211.h>
#include "../ieee80211_i.h"
#include "../key.h"
#include "debugfs_gtk.h"

/*
 * GTK Key Index Range:
 * Per IEEE 802.11-2020 Table 12-2, GTK occupies indices 1-3.
 * Index 0 is reserved for STAs as the default unicast (PTK) slot.
 */
#define GTK_IDX_MIN  1
#define GTK_IDX_MAX  (NUM_DEFAULT_KEYS - 1)

/* ------------------------------------------------------------------ */
/* GET: read GTK key material from mac80211's internal key store       */
/* ------------------------------------------------------------------ */

/*
 * GTK Debugfs Interface
 *
 * Multi-Link Operation (MLO):
 *   In MLO setups, each link maintains independent group keys.
 *   This function displays keys for all valid links.
 *   For non-MLO interfaces, only link 0 is shown.
 *
 * VLAN Interfaces:
 *   Per-VLAN GTK fetch is not supported in this implementation;
 *   only the keys installed on the parent AP interface are shown.
 */
static ssize_t gtk_get_read(struct file *file, char __user *user_buf,
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
	printk(KERN_DEBUG "mac80211 gtk_get: %s\n", sdata->name);

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

		for (i = GTK_IDX_MIN; i <= GTK_IDX_MAX; i++) {
			key = wiphy_dereference(sdata->local->hw.wiphy,
						link->gtk[i]);
			if (!key) {
				printk(KERN_DEBUG "mac80211 gtk_get: Link %d GTK[%d] not installed\n",
				       link_id, i);
				len += scnprintf(buf + len, buf_size - len,
						 "  GTK[%d]: not installed\n", i);
				continue;
			}

			printk(KERN_DEBUG "mac80211 gtk_get: Link %d GTK[%d] cipher=0x%08x keylen=%d\n",
			       link_id, i, key->conf.cipher, key->conf.keylen);

			len += scnprintf(buf + len, buf_size - len,
					 "  GTK[%d]: cipher=0x%08x keylen=%d key=",
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
/* SET: install a GTK key via mac80211's key management path           */
/* ------------------------------------------------------------------ */

/*
 * GTK SET Interface
 *
 * Format: <link_id> <keyidx> <cipher> <key_hex>
 *
 * Supported ciphers: ccmp, ccmp-256, gcmp, gcmp-256
 *
 * Multi-Link Operation:
 *   - For MLO interfaces, link_id specifies which link to install the key on
 *   - For non-MLO interfaces, link_id must be 0
 *
 * Example:
 *   echo "0 1 ccmp 0102030405060708090a0b0c0d0e0f10" > gtk_set
 *   (Install GTK index 1 with CCMP-128 on link 0)
 */
static ssize_t gtk_set_write(struct file *file, const char __user *user_buf,
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
		pr_err("mac80211 gtk_set: interface %s is not an AP or AP_VLAN\n",
		       sdata->name);
		return -EOPNOTSUPP;
	}

	if (count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;

	printk("mac80211 gtk_set: %s count=%zu\n", sdata->name, count);

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
		pr_err("mac80211 gtk_set: link_id %d out of range [0,%d]\n",
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

	if (keyidx < GTK_IDX_MIN || keyidx > GTK_IDX_MAX) {
		pr_err("mac80211 gtk_set: keyidx %d out of GTK range [%d,%d]\n",
		       keyidx, GTK_IDX_MIN, GTK_IDX_MAX);
		return -EINVAL;
	}

	/* Parse cipher string */
	key_hex = strchr(cipher_str, ' ');
	if (!key_hex)
		return -EINVAL;
	*key_hex++ = '\0';
	cipher_str = strim(cipher_str);

	if (!strcasecmp(cipher_str, "ccmp"))
		cipher = WLAN_CIPHER_SUITE_CCMP;
	else if (!strcasecmp(cipher_str, "ccmp-256"))
		cipher = WLAN_CIPHER_SUITE_CCMP_256;
	else if (!strcasecmp(cipher_str, "gcmp"))
		cipher = WLAN_CIPHER_SUITE_GCMP;
	else if (!strcasecmp(cipher_str, "gcmp-256"))
		cipher = WLAN_CIPHER_SUITE_GCMP_256;
	else {
		pr_err("mac80211 gtk_set: unknown cipher \"%s\" (use ccmp/ccmp-256/gcmp/gcmp-256)\n",
		       cipher_str);
		return -EINVAL;
	}

	/* Get the link data */
	if (!ieee80211_vif_is_mld(&sdata->vif)) {
		if (link_id != 0) {
			pr_err("mac80211 gtk_set: non-MLO interface, link_id must be 0\n");
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
			pr_err("mac80211 gtk_set: link %d not found\n", link_id);
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

	printk("mac80211 gtk_set: link=%d keyidx=%d cipher=0x%08x keylen=%d link_id=%d vif_is_mld=%d\n",
	       link_id, keyidx, cipher, keylen,
	       link->link_id, ieee80211_vif_is_mld(&sdata->vif));
	key = ieee80211_key_alloc(cipher, keyidx, keylen, key_data, 0, NULL);
	if (IS_ERR(key)) {
		printk(KERN_ERR "mac80211 gtk_set: key alloc failed for link=%d keyidx=%d cipher=0x%08x: err=%ld\n",
		       link_id, keyidx, cipher, PTR_ERR(key));
		return PTR_ERR(key);
	}

	/*
	 * GTK is per-link in MLO; not link-specific (-1) in non-MLO.
	 * Must match the existing key's link_id or ieee80211_iter_keys_rcu()
	 * will WARN "old link ID N doesn't match new link ID -1".
	 */
	key->conf.link_id = ieee80211_vif_is_mld(&sdata->vif) ? link->link_id : -1;

	/*
	 * ieee80211_key_link() must be called with the wiphy mutex held.
	 * Pass NULL for sta since GTK is a group key.
	 */
	wiphy_lock(sdata->local->hw.wiphy);
	ret = ieee80211_key_link(key, link, NULL);
	wiphy_unlock(sdata->local->hw.wiphy);

	printk("mac80211 gtk_set: ieee80211_key_link() ret=%d\n", ret);

	if (ret == -EALREADY)
		ret = 0;

	if (ret)
		printk(KERN_ERR "mac80211 gtk_set: key install failed link=%d keyidx=%d ret=%d\n",
		       link_id, keyidx, ret);
	else
		printk("mac80211 gtk_set: key installed ok link=%d keyidx=%d\n",
		       link_id, keyidx);

	return ret ? ret : (ssize_t)count;
}

/* ------------------------------------------------------------------ */
/* file_operations                                                      */
/* ------------------------------------------------------------------ */

static const struct file_operations gtk_get_ops = {
	.read   = gtk_get_read,
	.open   = simple_open,
	.llseek = default_llseek,
};

static const struct file_operations gtk_set_ops = {
	.write  = gtk_set_write,
	.open   = simple_open,
	.llseek = default_llseek,
};

/* ------------------------------------------------------------------ */
/* Registration                                                         */
/* ------------------------------------------------------------------ */

/**
 * ieee80211_debugfs_gtk_add - create GTK debugfs entries for an interface
 * @sdata: the virtual interface
 *
 * Creates two files under the interface's debugfs directory:
 *   gtk_get  (0400) - read current GTK key material from mac80211
 *   gtk_set  (0200) - install a new GTK key (triggers WMI install)
 *
 * Format for gtk_set: <link_id> <keyidx> <cipher> <key_hex>
 *   - link_id: 0 for non-MLO, 0-14 for MLO
 *   - keyidx: 1-3 (GTK_IDX_MIN to GTK_IDX_MAX)
 *   - cipher: ccmp | ccmp-256 | gcmp | gcmp-256
 *   - key_hex: 32 hex digits (16 bytes) or 64 hex digits (32 bytes)
 */
void ieee80211_debugfs_gtk_add(struct ieee80211_sub_if_data *sdata)
{
	struct dentry *dir = sdata->vif.debugfs_dir;

	if (!dir)
		return;

	debugfs_create_file("gtk_get", 0400, dir, sdata, &gtk_get_ops);
	debugfs_create_file("gtk_set", 0200, dir, sdata, &gtk_set_ops);
}
