/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/debugfs.h>
#include <linux/etherdevice.h>
#include <linux/hex.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/ieee80211.h>
#include "../ieee80211_i.h"
#include "../key.h"
#include "../sta_info.h"
#include "debugfs_ptk.h"

/* PTK uses key indices 0-3 (same range as NUM_DEFAULT_KEYS) */
#define PTK_IDX_MIN  0
#define PTK_IDX_MAX  (NUM_DEFAULT_KEYS - 1)

static ssize_t ptk_get_read(struct file *file, char __user *user_buf,
			    size_t count, loff_t *ppos)
{
	struct ieee80211_sub_if_data *sdata = file->private_data;
	struct sta_info *sta;
	struct ieee80211_key *key;
	char *buf;
	int len = 0;
	const int buf_size = 4096;
	ssize_t ret;
	int j;

	buf = kzalloc(buf_size, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	wiphy_lock(sdata->local->hw.wiphy);
	printk(KERN_DEBUG "mac80211 ptk_get: %s\n", sdata->name);

	list_for_each_entry(sta, &sdata->local->sta_list, list) {
		if (sta->sdata != sdata) {
			/*
			 * Also show stations on AP_VLAN sub-interfaces that
			 * belong to this AP's BSS — mirrors sta_info_get_bss()
			 * behaviour used in ptk_set_write.
			 */
			if (sta->sdata->vif.type != NL80211_IFTYPE_AP_VLAN ||
			    sta->sdata->bss != &sdata->u.ap)
				continue;
		}

		key = wiphy_dereference(sdata->local->hw.wiphy,
					sta->ptk[sta->ptk_idx]);
		if (!key) {
			printk(KERN_DEBUG "mac80211 ptk_get: PTK[%pM] not installed\n",
			       sta->sta.addr);
			len += scnprintf(buf + len, buf_size - len,
					 "PTK[%pM]: not installed\n",
					 sta->sta.addr);
			continue;
		}

		printk(KERN_DEBUG "mac80211 ptk_get: PTK[%pM] cipher=0x%08x keylen=%d\n",
		       sta->sta.addr, key->conf.cipher, key->conf.keylen);
		len += scnprintf(buf + len, buf_size - len,
				 "PTK[%pM]: cipher=0x%08x keylen=%d key=",
				 sta->sta.addr, key->conf.cipher,
				 key->conf.keylen);

		for (j = 0; j < key->conf.keylen; j++)
			len += scnprintf(buf + len, buf_size - len,
					 "%02x", key->conf.key[j]);

		len += scnprintf(buf + len, buf_size - len, "\n");
	}

	if (len == 0)
		len += scnprintf(buf, buf_size, "no stations associated\n");

	wiphy_unlock(sdata->local->hw.wiphy);

	ret = simple_read_from_buffer(user_buf, count, ppos, buf, len);
	kfree(buf);
	return ret;
}

/*
 * PTK SET Interface
 *
 * Format: <sta_mac> <link_id> <keyidx> <cipher> <key_hex>
 *
 * Supported ciphers: ccmp, ccmp-256, gcmp, gcmp-256
 */
static ssize_t ptk_set_write(struct file *file, const char __user *user_buf,
			     size_t count, loff_t *ppos)
{
	struct ieee80211_sub_if_data *sdata = file->private_data;
	char buf[256] = {};
	u8 key_data[WLAN_MAX_KEY_LEN];
	u8 addr[ETH_ALEN];
	u32 cipher;
	int link_id, keyidx, keylen;
	int ret;
	char *p, *link_id_str, *keyidx_str, *cipher_str, *key_hex;
	struct ieee80211_key *key;
	struct ieee80211_link_data *link;
	struct sta_info *sta;

	/* Validate interface type */
	if (sdata->vif.type != NL80211_IFTYPE_AP &&
	    sdata->vif.type != NL80211_IFTYPE_AP_VLAN) {
		pr_err("mac80211 ptk_set: interface %s is not an AP or AP_VLAN\n",
		       sdata->name);
		return -EOPNOTSUPP;
	}

	if (count >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, user_buf, count))
		return -EFAULT;
	printk(KERN_DEBUG "mac80211 ptk_set: %s count=%zu\n", sdata->name, count);

	/* Strip trailing newline */
	if (count && buf[count - 1] == '\n')
		buf[count - 1] = '\0';
	else
		buf[count] = '\0';

	p = buf;

	link_id_str = strchr(p, ' ');
	if (!link_id_str)
		return -EINVAL;
	*link_id_str++ = '\0';

	keyidx_str = strchr(link_id_str, ' ');
	if (!keyidx_str)
		return -EINVAL;
	*keyidx_str++ = '\0';

	cipher_str = strchr(keyidx_str, ' ');
	if (!cipher_str)
		return -EINVAL;
	*cipher_str++ = '\0';

	key_hex = strchr(cipher_str, ' ');
	if (!key_hex)
		return -EINVAL;
	*key_hex++ = '\0';

	/* Parse MAC address */
	if (!mac_pton(p, addr)) {
		pr_err("mac80211 ptk_set: invalid MAC address \"%s\"\n", p);
		return -EINVAL;
	}

	/* Parse link_id */
	if (kstrtoint(link_id_str, 10, &link_id))
		return -EINVAL;

	if (link_id < 0 || link_id >= IEEE80211_MLD_MAX_NUM_LINKS) {
		pr_err("mac80211 ptk_set: link_id %d out of range [0,%d]\n",
		       link_id, IEEE80211_MLD_MAX_NUM_LINKS - 1);
		return -EINVAL;
	}

	/* Parse keyidx */
	if (kstrtoint(keyidx_str, 10, &keyidx))
		return -EINVAL;

	if (keyidx < PTK_IDX_MIN || keyidx > PTK_IDX_MAX) {
		pr_err("mac80211 ptk_set: keyidx %d out of PTK range [%d,%d]\n",
		       keyidx, PTK_IDX_MIN, PTK_IDX_MAX);
		return -EINVAL;
	}

	/* Parse cipher string */
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
		pr_err("mac80211 ptk_set: unknown cipher \"%s\" (use ccmp/ccmp-256/gcmp/gcmp-256)\n",
		       cipher_str);
		return -EINVAL;
	}

	/* Get the link data */
	if (!ieee80211_vif_is_mld(&sdata->vif)) {
		if (link_id != 0) {
			pr_err("mac80211 ptk_set: non-MLO interface, link_id must be 0\n");
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
			pr_err("mac80211 ptk_set: link %d not found\n", link_id);
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

	key = ieee80211_key_alloc(cipher, keyidx, keylen, key_data, 0, NULL);
	if (IS_ERR(key)) {
		printk(KERN_ERR "mac80211 ptk_set: key alloc failed sta=%pM link=%d keyidx=%d cipher=0x%08x: err=%ld\n",
		       addr, link_id, keyidx, cipher, PTR_ERR(key));
		return PTR_ERR(key);
	}

	/*
	 * PTK is MLD-wide (one key per MLD association, not per-link).
	 * Hostapd always installs PTK with link_id=-1 even on MLO interfaces.
	 * Using link->link_id here causes "old link ID -1 doesn't match new link ID N"
	 * in ieee80211_iter_keys_rcu() when replacing the existing key.
	 * AP_VLAN also reports vif_is_mld=true but its keys are link_id=-1.
	 */
	key->conf.link_id = -1;
	/*
	 * PTK is always a pairwise key. Without this flag, ieee80211_key_enable_hw_accel()
	 * on an AP_VLAN sdata remaps to the parent AP but still passes sta != NULL,
	 * causing ath12k to WARN when iterating MLO links (ahvif->link[*] is NULL for
	 * AP_VLAN). With the flag set, the AP_VLAN pairwise path returns 0 immediately
	 * (no drv_set_key), and when writing via the parent AP the driver is called
	 * correctly with a valid AP vif.
	 */
	key->conf.flags |= IEEE80211_KEY_FLAG_PAIRWISE;

	wiphy_lock(sdata->local->hw.wiphy);

	sta = sta_info_get_bss(sdata, addr);
	if (!sta) {
		wiphy_unlock(sdata->local->hw.wiphy);
		printk(KERN_ERR "mac80211 ptk_set: station %pM not found\n", addr);
		ieee80211_key_free_unused(key);
		return -ENOENT;
	}
	printk(KERN_DEBUG "mac80211 ptk_set: found sta %pM on sdata=%s (lookup from %s)\n",
	       addr, sta->sdata->name, sdata->name);

	ret = ieee80211_key_link(key, link, sta);
	wiphy_unlock(sdata->local->hw.wiphy);

	printk(KERN_DEBUG "mac80211 ptk_set: ieee80211_key_link() ret=%d\n", ret);

	if (ret == -EALREADY)
		ret = 0;

	if (ret)
		printk(KERN_ERR "mac80211 ptk_set: key install failed sta=%pM link=%d keyidx=%d ret=%d\n",
		       addr, link_id, keyidx, ret);
	else
		printk(KERN_DEBUG "mac80211 ptk_set: key installed ok sta=%pM link=%d keyidx=%d\n",
		       addr, link_id, keyidx);

	return ret ? ret : (ssize_t)count;
}

static const struct file_operations ptk_get_ops = {
	.read   = ptk_get_read,
	.open   = simple_open,
	.llseek = default_llseek,
};

static const struct file_operations ptk_set_ops = {
	.write  = ptk_set_write,
	.open   = simple_open,
	.llseek = default_llseek,
};

/**
 * ieee80211_debugfs_ptk_add - create PTK debugfs entries for an interface
 * @sdata: the virtual interface
 *
 * Format for ptk_set: <sta_mac> <link_id> <keyidx> <cipher> <key_hex>
 *   - sta_mac: Station MAC address (xx:xx:xx:xx:xx:xx)
 *   - link_id: 0 for non-MLO, 0-14 for MLO
 *   - keyidx: 0-3 (PTK key index)
 *   - cipher: ccmp | ccmp-256 | gcmp | gcmp-256
 *   - key_hex: 32 hex digits (16 bytes) or 64 hex digits (32 bytes)
 */
void ieee80211_debugfs_ptk_add(struct ieee80211_sub_if_data *sdata)
{
	struct dentry *dir = sdata->vif.debugfs_dir;

	if (!dir)
		return;

	debugfs_create_file("ptk_get", 0400, dir, sdata, &ptk_get_ops);
	debugfs_create_file("ptk_set", 0200, dir, sdata, &ptk_set_ops);
}
