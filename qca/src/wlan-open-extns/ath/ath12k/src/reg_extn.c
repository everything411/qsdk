/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/bitfield.h>
#include <linux/fs.h>
#include <linux/skbuff.h>
#include <net/cfg80211.h>
#include <net/mac80211.h>
#include <net/netlink.h>
#include "../core.h"
#include "../debug.h"
#include "../debugfs.h"
#include "../reg.h"
#include "../vendor.h"
#include "../wmi.h"
#include "../mac.h"
#include "../net/mac80211/ieee80211_i.h"
#include "reg_extn.h"
#include "vendor_extn.h"

int ath12k_reg_chan_list_cc_ext_parse_extn(struct ath12k_base *ab,
					   const void *next_tlv,
					   struct ath12k_reg_info_extn *reg_info_extn)
{
	const struct wmi_tlv *prio_tlv;
	const struct ath12k_wmi_reg_chan_priority *prio;
	u16 wrapper_len;
	u16 prio_tag;

	if (!reg_info_extn)
		return -EINVAL;

	reg_info_extn->reg_6ghz_thresh_priority_freq = 0;

	if (!next_tlv)
		return -EINVAL;

	prio_tlv = (const struct wmi_tlv *)next_tlv;
	if (le32_get_bits(prio_tlv->header, WMI_TLV_TAG) != WMI_TAG_ARRAY_STRUCT ||
	    !le32_get_bits(prio_tlv->header, WMI_TLV_LEN))
		return -EPROTO;

	wrapper_len = le32_get_bits(prio_tlv->header, WMI_TLV_LEN);
	if (wrapper_len < sizeof(*prio))
		return -EINVAL;

	prio = (const struct ath12k_wmi_reg_chan_priority *)(prio_tlv + 1);
	prio_tag = le32_get_bits(prio->tlv_header, WMI_TLV_TAG);
	if (prio_tag != WMI_TAG_REG_CHAN_PRIORITY)
		return -EINVAL;

	reg_info_extn->reg_6ghz_thresh_priority_freq =
		le32_to_cpu(prio->freq_info) & 0xFFFF;

	ath12k_dbg(ab, ATH12K_DBG_REG,
		   "6 GHz VLP priority threshold freq %u",
		   reg_info_extn->reg_6ghz_thresh_priority_freq);

	return 0;
}

void ath12k_reg_handle_chan_list_extn(struct ath12k_base *ab,
				      int pdev_idx,
				      const struct ath12k_reg_info_extn *reg_info_extn)
{
	if (!ab || !reg_info_extn)
		return;

	if (pdev_idx < 0 || pdev_idx >= ab->num_radios)
		return;

	ab->ath12k_base_extn.reg_6ghz_thresh_priority_freq[pdev_idx] =
		reg_info_extn->reg_6ghz_thresh_priority_freq;

	ath12k_dbg(ab, ATH12K_DBG_REG,
		   "pdev %d 6 GHz priority threshold set to %u\n",
		   pdev_idx,
		   ab->ath12k_base_extn.reg_6ghz_thresh_priority_freq[pdev_idx]);
}

void ath12k_wmi_set_hw_blocklist_host_service_flag_extn(
		struct ath12k_base *ab,
		struct ath12k_wmi_resource_config_params *wmi_cfg,
		struct ath12k_wmi_resource_config_arg *tg_cfg)
{
	if (!ath12k_cfg_get(ab, ATH12K_CFG_REG_6GHZ_LOW_POWER_HW_BLOCKLIST_ENABLE))
		return;

	wmi_cfg->host_service_flags |=
		cpu_to_le32(tg_cfg->hw_blocklist_chans_support <<
			    WMI_RSRC_CFG_HOST_SVC_FLAG_SUPP_HW_BLACKLIST_CHANS);
}

void ath12k_wmi_set_hw_blocklist_service_support_extn(
		struct ath12k_base *ab,
		struct ath12k_wmi_resource_config_arg *res_cfg)
{
	if (test_bit(WMI_SERVICE_HW_BLACKLIST_CHAN_SUPPORT, ab->wmi_ab.svc_map))
		res_cfg->hw_blocklist_chans_support = true;
}

static void
ath12k_reg_hw_blocklist_free_chans_extn(struct ath12k_hw_blocklist_chans *chans)
{
	kfree(chans->fb_chan);
	kfree(chans->pc_chan);
	chans->fb_chan = NULL;
	chans->pc_chan = NULL;
	chans->nfbchans = 0;
	chans->npcchans = 0;
}

static void ath12k_reg_info_free_extn(struct ath12k_reg_info *reg_info)
{
	if (!reg_info)
		return;

	kfree(reg_info->reg_rules_2g_ptr);
	kfree(reg_info->reg_rules_5g_ptr);
	if (reg_info->is_ext_reg_event) {
		int i;

		for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++) {
			int j;

			kfree(reg_info->reg_rules_6g_ap_ptr[i]);
			for (j = 0; j < WMI_REG_MAX_CLIENT_TYPE; j++)
				kfree(reg_info->reg_rules_6g_client_ptr[i][j]);
		}
	}

	ath12k_wmi_free_reg_hw_blocklist_extn(&reg_info->reg_info_extn);
	kfree(reg_info);
}

static struct ath12k_hw_blocklist_state *
ath12k_reg_hw_blocklist_get_state_extn(struct ath12k_base *ab, int pdev_idx)
{
	if (!ab || pdev_idx < 0 || pdev_idx >= ab->num_radios)
		return NULL;

	return ab->ath12k_base_extn.hw_blocklist[pdev_idx];
}

/**
 * ath12k_reg_hw_blocklist_ensure_state_extn - Return or allocate per-pdev blocklist state
 * @ab: Pointer to the ath12k base structure
 * @pdev_idx: pdev identifier
 *
 * If a state struct already exists for @pdev_idx return it.
 * Otherwise allocate and initialise a new one and store it.
 * The "ensure" name signals that callers get a valid pointer or NULL
 * without needing to check whether the struct was already present.
 *
 * Return: pointer to the state struct, or NULL on allocation failure
 */
static struct ath12k_hw_blocklist_state *
ath12k_reg_hw_blocklist_ensure_state_extn(struct ath12k_base *ab, int pdev_idx)
{
	struct ath12k_hw_blocklist_state *st;

	st = ath12k_reg_hw_blocklist_get_state_extn(ab, pdev_idx);
	if (st)
		return st;

	st = kzalloc(sizeof(*st), GFP_ATOMIC);
	if (!st)
		return NULL;

	st->last_cmd = ATH12K_HWBL_CMD_INVALID;
	ab->ath12k_base_extn.hw_blocklist[pdev_idx] = st;
	return st;
}

/* Lockless variant of 'ath12k_reg_hw_blocklist_free_for_mode_extn' */
static void
__ath12k_reg_hw_blocklist_free_for_mode_extn(struct ath12k_base *ab,
					     int pdev_idx,
					     enum wmi_reg_6g_ap_type pwr_type)
{
	struct ath12k_hw_blocklist_state *st;

	st = ath12k_reg_hw_blocklist_get_state_extn(ab, pdev_idx);
	if (!st || pwr_type >= WMI_REG_CURRENT_MAX_AP_TYPE)
		return;

	ath12k_reg_hw_blocklist_free_chans_extn(&st->pwr[pwr_type]);
}

void ath12k_reg_hw_blocklist_free_for_mode_extn(struct ath12k_base *ab,
						int pdev_idx,
						u32 pwr_type)
{
	spin_lock_bh(&ab->base_lock);
	__ath12k_reg_hw_blocklist_free_for_mode_extn(ab, pdev_idx, pwr_type);
	spin_unlock_bh(&ab->base_lock);
}

static void
ath12k_reg_free_hw_blocklist_afc_and_reg_info(struct ath12k_base *ab, int pdev_idx)
{
	struct ath12k_reg_info **reg_info = ab->ath12k_base_extn.hw_bl_reg_info;
	struct ath12k_afc_info **afc_info = ab->ath12k_base_extn.hw_bl_afc_info;

	if (reg_info[pdev_idx]) {
		ath12k_reg_info_free_extn(reg_info[pdev_idx]);
		reg_info[pdev_idx] = NULL;
	}

	if (afc_info[pdev_idx]) {
		ath12k_free_afc_power_event_info(afc_info[pdev_idx]);
		kfree(afc_info[pdev_idx]);
		afc_info[pdev_idx] = NULL;
	}
}

/* Lockless variant — caller must hold ab->base_lock */
static void
__ath12k_reg_hw_blocklist_free_for_pdev_extn(struct ath12k_base *ab, int pdev_idx)
{
	int pwr;
	struct ath12k_hw_blocklist_state *st;

	if (!ab || pdev_idx < 0 || pdev_idx >= ab->num_radios)
		return;

	st = ath12k_reg_hw_blocklist_get_state_extn(ab, pdev_idx);
	if (st) {
		for (pwr = 0; pwr < WMI_REG_CURRENT_MAX_AP_TYPE; pwr++)
			ath12k_reg_hw_blocklist_free_chans_extn(&st->pwr[pwr]);

		st->valid = false;
		st->complete = false;
		st->last_cmd = ATH12K_HWBL_CMD_INVALID;
		st->last_seq = 0;
		st->expected_seq = 0;
	}

	ath12k_reg_free_hw_blocklist_afc_and_reg_info(ab, pdev_idx);
}

static void
ath12k_reg_hw_blocklist_free_for_pdev_extn(struct ath12k_base *ab, int pdev_idx)
{
	spin_lock_bh(&ab->base_lock);
	__ath12k_reg_hw_blocklist_free_for_pdev_extn(ab, pdev_idx);
	spin_unlock_bh(&ab->base_lock);
}

void ath12k_reg_hw_blocklist_free_extn(struct ath12k_base *ab)
{
	int pdev_idx;

	spin_lock_bh(&ab->base_lock);
	for (pdev_idx = 0; pdev_idx < ab->num_radios; pdev_idx++) {
		struct ath12k_hw_blocklist_state **hw_blocklist = ab->ath12k_base_extn.hw_blocklist;

		__ath12k_reg_hw_blocklist_free_for_pdev_extn(ab, pdev_idx);
		kfree(hw_blocklist[pdev_idx]);
		hw_blocklist[pdev_idx] = NULL;
	}
	spin_unlock_bh(&ab->base_lock);
}

static bool ath12k_hw_blocklist_cmd_matches_pwr_mode_extn(u8 cmd, u8 pwr)
{
	switch (cmd) {
	case ATH12K_HWBL_CMD_SP_UPDATE:
	case ATH12K_HWBL_CMD_SP_CLEAR:
		return pwr == WMI_REG_STD_POWER_AP;
	case ATH12K_HWBL_CMD_VLP_UPDATE:
	case ATH12K_HWBL_CMD_VLP_CLEAR:
		return pwr == WMI_REG_VLP_AP;
	case ATH12K_HWBL_CMD_UPDATE_ALL:
	case ATH12K_HWBL_CMD_CLEAR_ALL:
		return true;
	default:
		return false;
	}
}

static void
ath12k_reg_hw_blocklist_clear_cmd_extn(struct ath12k_hw_blocklist_state *st, u8 cmd)
{
	int i;

	for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++) {
		if (!ath12k_hw_blocklist_cmd_matches_pwr_mode_extn(cmd, i))
			continue;

		ath12k_reg_hw_blocklist_free_chans_extn(&st->pwr[i]);
	}
}

static void
ath12k_reg_hw_blocklist_update_punc_chan_extn(struct ath12k_hw_blocklist_chans *dst,
					      const struct ath12k_hw_blocklist_chan_info *chan)
{
	struct ath12k_hw_blocklist_pc_chan *new_pc;
	u32 pc_idx = dst->npcchans;

	new_pc = krealloc(dst->pc_chan, (pc_idx + 1) * sizeof(*dst->pc_chan), GFP_ATOMIC);
	if (!new_pc)
		return;

	new_pc[pc_idx].center_freq = chan->center_freq;
	new_pc[pc_idx].max_bw = chan->max_bw;
	new_pc[pc_idx].punc_pat_bmap = chan->punc_pat_bmap;
	dst->pc_chan = new_pc;
	dst->npcchans++;
}

static void
ath12k_reg_hw_blocklist_update_fb_chan_extn(struct ath12k_hw_blocklist_chans *dst,
					    const struct ath12k_hw_blocklist_chan_info *chan)
{
	struct ath12k_hw_blocklist_fb_chan *new_fb;
	u32 fb_idx = dst->nfbchans;

	new_fb = krealloc(dst->fb_chan, (fb_idx + 1) * sizeof(*dst->fb_chan), GFP_ATOMIC);
	if (!new_fb)
		return;

	new_fb[fb_idx].pri20_bitmap = chan->pri20_bitmap;
	new_fb[fb_idx].center_freq = chan->center_freq;
	new_fb[fb_idx].max_bw = chan->max_bw;
	dst->fb_chan = new_fb;
	dst->nfbchans++;
}

static void
ath12k_reg_hw_blocklist_update_channels_extn(struct ath12k_hw_blocklist_state *st,
					     const struct ath12k_hw_blocklist_update *upd)
{
	u32 i;

	if (upd->is_first)
		ath12k_reg_hw_blocklist_clear_cmd_extn(st, upd->cmd);

	for (i = 0; i < upd->num_chans; i++) {
		const struct ath12k_hw_blocklist_chan_info *chan = &upd->chans[i];
		struct ath12k_hw_blocklist_chans *dst;

		if (chan->power_mode >= WMI_REG_CURRENT_MAX_AP_TYPE)
			continue;

		if (!ath12k_hw_blocklist_cmd_matches_pwr_mode_extn(upd->cmd, chan->power_mode))
			continue;

		dst = &st->pwr[chan->power_mode];
		if (chan->punc_pat_bmap)
			ath12k_reg_hw_blocklist_update_punc_chan_extn(dst, chan);
		else
			ath12k_reg_hw_blocklist_update_fb_chan_extn(dst, chan);
	}
}

static void
ath12k_reg_clear_hw_blocklist_ext(struct ath12k_hw_blocklist_state *st, u8 cmd)
{
	st->valid = true;
	st->last_cmd = cmd;
	ath12k_reg_hw_blocklist_clear_cmd_extn(st, cmd);
	st->complete = true;
	st->expected_seq = 0;
}

static int
ath12k_reg_update_hw_blocklist_ext(struct ath12k_base *ab, u8 phy_id,
				   struct ath12k_hw_blocklist_state *st,
				   const struct ath12k_hw_blocklist_update *upd)
{
	st->valid = true;
	st->last_cmd = upd->cmd;
	st->last_seq = upd->seq_num;

	if (upd->is_first) {
		st->complete = false;
		st->expected_seq = 1;
	}

	if (!st->expected_seq || upd->seq_num != st->expected_seq) {
		ath12k_warn(ab,
			    "hw blocklist: phy %u cmd %u seq mismatch got %u exp %u first %d more %d total %u\n",
			    phy_id, upd->cmd, upd->seq_num,
			    st->expected_seq, upd->is_first, upd->more,
			    upd->total_num);
		st->valid = false;
		st->complete = false;
		st->expected_seq = 0;
		return -EILSEQ;
	}

	ath12k_reg_hw_blocklist_update_channels_extn(st, upd);
	if (upd->more) {
		st->expected_seq++;
		return 0;
	}

	st->complete = !upd->total_num || upd->seq_num == upd->total_num;
	st->expected_seq = 0;
	if (!st->complete)
		ath12k_warn(ab,
			    "hw blocklist: phy %u cmd %u total mismatch seq %u total %u\n",
			    phy_id, upd->cmd, upd->seq_num, upd->total_num);
	return 0;
}

static void
ath12k_reg_handle_default_hw_blocklist_cmd_ext(struct ath12k_hw_blocklist_state *st)
{
	st->valid = false;
	st->complete = false;
	st->expected_seq = 0;
}

/**
 * ath12k_reg_update_hw_blocklist_extn - Update per-pdev HW blocklist state
 * @ab: Pointer to the ath12k base structure
 * @phy_id: pdev identifier (0-based radio index)
 * @upd: Parsed HW blocklist update from the WMI event
 *
 * Applies a HW blocklist update fragment to the per-pdev state machine.
 * Handles clear commands (immediate completion), update commands (may span
 * multiple fragments keyed by seq_num), and validates sequence ordering.
 *
 * Must be called with ab->base_lock held by the caller (or it acquires it
 * internally). Currently acquires base_lock itself.
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_reg_update_hw_blocklist_extn(struct ath12k_base *ab, u8 phy_id,
					const struct ath12k_hw_blocklist_update *upd)
{
	struct ath12k_hw_blocklist_state *st;

	if (!upd->valid)
		return 0;

	if (phy_id >= ab->num_radios)
		return -EINVAL;

	spin_lock_bh(&ab->base_lock);
	st = ath12k_reg_hw_blocklist_ensure_state_extn(ab, phy_id);
	if (!st) {
		spin_unlock_bh(&ab->base_lock);
		return -ENOMEM;
	}

	switch (upd->cmd) {
	case ATH12K_HWBL_CMD_SP_CLEAR:
	case ATH12K_HWBL_CMD_VLP_CLEAR:
	case ATH12K_HWBL_CMD_CLEAR_ALL:
		ath12k_reg_clear_hw_blocklist_ext(st, upd->cmd);
		break;
	case ATH12K_HWBL_CMD_SP_UPDATE:
	case ATH12K_HWBL_CMD_VLP_UPDATE:
	case ATH12K_HWBL_CMD_UPDATE_ALL: {
		int ret = ath12k_reg_update_hw_blocklist_ext(ab, phy_id, st, upd);

		if (ret) {
			spin_unlock_bh(&ab->base_lock);
			return ret;
		}
		break;
	}
	default:
		ath12k_reg_handle_default_hw_blocklist_cmd_ext(st);
		break;
	}

	ath12k_dbg(ab, ATH12K_DBG_REG,
		   "hw blocklist: phy %u cmd %u seq %u first %d more %d elems %u complete %d exp %u\n",
		   phy_id, upd->cmd, upd->seq_num, upd->is_first, upd->more,
		   upd->num_chans, st->complete, st->expected_seq);
	spin_unlock_bh(&ab->base_lock);

	return 0;
}

/* HW blocklist WMI message field macros */
#define ATH12K_HWBL_MSG_INFO_SEQ_NUM		GENMASK(7, 0)
#define ATH12K_HWBL_MSG_INFO_TOTAL_NUM		GENMASK(15, 8)
#define ATH12K_HWBL_MSG_INFO_RESP_CODE		GENMASK(19, 16)
#define ATH12K_HWBL_MSG_INFO_EVT_FLAG		BIT(20)

#define ATH12K_HWBL_NUM_TOTAL_CHAN		GENMASK(15, 0)
#define ATH12K_HWBL_NUM_CUR_CHAN		GENMASK(31, 16)

#define ATH12K_HWBL_FREQ_INFO_PRI20_BITMAP	GENMASK(15, 0)
#define ATH12K_HWBL_FREQ_INFO_CENTER_MHZ	GENMASK(31, 16)

#define ATH12K_HWBL_META_PWR_MODE		GENMASK(3, 0)
#define ATH12K_HWBL_META_MAX_BW			GENMASK(11, 4)

#define ATH12K_HWBL_PUNC_BITMAP			GENMASK(23, 0)

static void
ath12k_wmi_parse_hw_blocklist_chan_fixed_param_extn(struct ath12k_base *ab,
						    const void *ptr, u16 len,
						    struct ath12k_hw_blocklist_update *upd)
{
	const struct wmi_hw_blacklist_chan_fixed_param *fp = ptr;
	u32 msg_info, num_info;

	if (len < sizeof(*fp)) {
		ath12k_warn(ab, "HBL Fixed length Absent");
		return;
	}

	msg_info = le32_to_cpu(fp->blacklist_msg_info);
	num_info = le32_to_cpu(fp->num_hw_blocklist_channels);

	upd->total_num_chan = FIELD_GET(ATH12K_HWBL_NUM_TOTAL_CHAN, num_info);
	upd->current_num_chan = FIELD_GET(ATH12K_HWBL_NUM_CUR_CHAN, num_info);
	upd->valid = true;
	upd->cmd = FIELD_GET(ATH12K_HWBL_MSG_INFO_RESP_CODE, msg_info);
	upd->seq_num = FIELD_GET(ATH12K_HWBL_MSG_INFO_SEQ_NUM, msg_info);
	upd->total_num = FIELD_GET(ATH12K_HWBL_MSG_INFO_TOTAL_NUM, msg_info);
	upd->more = !!FIELD_GET(ATH12K_HWBL_MSG_INFO_EVT_FLAG, msg_info);
	upd->is_first = upd->seq_num == 1;
	upd->phy_id = le32_to_cpu(fp->phy_id);
}

static int
ath12k_wmi_parse_hw_blocklist_chan_data_extn(struct ath12k_base *ab,
					     const void *ptr, u16 len,
					     struct ath12k_hw_blocklist_update *upd)
{
	const struct wmi_hw_blacklist_chan_data *hw = ptr;
	struct ath12k_hw_blocklist_chan_info *new;
	u32 freq_info, meta, punc_info, punc;
	u16 pri20, center;
	u8 pwr, bw;
	u32 idx;

	if (len < sizeof(*hw)) {
		ath12k_warn(ab, "HBL Chan Data Length Absent");
		return 0;
	}

	if (!upd->valid) {
		ath12k_warn(ab, "HBL Not Valid Ignore Chan Data");
		return 0;
	}

	if (!upd->total_num_chan || !upd->current_num_chan) {
		ath12k_warn(ab, "HBL channel data not present total %u current %u",
			    upd->total_num_chan, upd->current_num_chan);
		return 0;
	}

	freq_info = le32_to_cpu(hw->freq_info);
	pri20  = FIELD_GET(ATH12K_HWBL_FREQ_INFO_PRI20_BITMAP, freq_info);
	center = FIELD_GET(ATH12K_HWBL_FREQ_INFO_CENTER_MHZ, freq_info);

	meta = le32_to_cpu(hw->chan_list_meta_data);
	pwr = FIELD_GET(ATH12K_HWBL_META_PWR_MODE, meta);
	bw  = FIELD_GET(ATH12K_HWBL_META_MAX_BW, meta);

	punc_info = le32_to_cpu(hw->puncture_pattern_bitmap_info);
	punc = FIELD_GET(ATH12K_HWBL_PUNC_BITMAP, punc_info);

	if (upd->num_chans >= upd->total_num_chan)
		return -EINVAL;

	idx = upd->num_chans;
	new = krealloc_array(upd->chans, idx + 1, sizeof(*upd->chans), GFP_ATOMIC);
	if (!new) {
		kfree(upd->chans);
		upd->chans = NULL;
		return -ENOMEM;
	}

	new[idx].pri20_bitmap = pri20;
	new[idx].center_freq = center;
	new[idx].power_mode = pwr;
	new[idx].max_bw = bw;
	new[idx].punc_pat_bmap = punc;
	upd->chans = new;
	upd->num_chans++;

	return 0;
}

static int ath12k_wmi_hw_blocklist_chan_data_iter_extn(struct ath12k_base *ab,
						      u16 tag, u16 len,
						      const void *ptr, void *data)
{
	struct ath12k_hw_blocklist_update *upd = data;

	if (tag == WMI_TAG_HW_BLACKLIST_CHAN_DATA)
		return ath12k_wmi_parse_hw_blocklist_chan_data_extn(ab, ptr, len, upd);

	return 0;
}

static int ath12k_wmi_hw_blocklist_elem_parse_extn(struct ath12k_base *ab, u16 tag,
						   u16 len, const void *ptr,
						   void *data)
{
	struct ath12k_hw_blocklist_update *upd = data;

	switch (tag) {
	case WMI_TAG_HW_BLACKLIST_CHAN_FIXED_PARAM:
		ath12k_wmi_parse_hw_blocklist_chan_fixed_param_extn(ab, ptr, len, upd);
		break;
	case WMI_TAG_HW_BLACKLIST_CHAN_DATA:
		return ath12k_wmi_parse_hw_blocklist_chan_data_extn(ab, ptr, len, upd);
	case WMI_TAG_ARRAY_STRUCT:
		if (len == 0)
			return 0;
		return ath12k_wmi_tlv_iter(ab, ptr, len,
					   ath12k_wmi_hw_blocklist_chan_data_iter_extn,
					   data);
	default:
		break;
	}

	return 0;
}

static int ath12k_wmi_hw_blocklist_tlv_parse_extn(struct ath12k_base *ab, u16 tag,
						   u16 len, const void *ptr,
						   void *data)
{
	const struct wmi_tlv *tlv;
	u16 inner_tag;

	if (tag != WMI_TAG_ARRAY_STRUCT)
		return 0;

	if (len < sizeof(*tlv))
		return 0;

	tlv = ptr;
	inner_tag = le32_get_bits(tlv->header, WMI_TLV_TAG);
	if (inner_tag != WMI_TAG_HW_BLACKLIST_CHAN_FIXED_PARAM &&
	    inner_tag != WMI_TAG_HW_BLACKLIST_CHAN_DATA)
		return 0;

	return ath12k_wmi_tlv_iter(ab, ptr, len,
				   ath12k_wmi_hw_blocklist_elem_parse_extn,
				   data);
}

void ath12k_wmi_pull_hw_blocklist_update_extn(struct ath12k_base *ab,
					      struct sk_buff *skb,
					      struct ath12k_hw_blocklist_update *upd)
{
	memset(upd, 0, sizeof(*upd));
	ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
			    ath12k_wmi_hw_blocklist_tlv_parse_extn, upd);
}

int ath12k_wmi_pull_reg_hw_blocklist_extn(
		struct ath12k_base *ab, struct sk_buff *skb,
		struct ath12k_reg_info_extn *reg_info_extn)
{
	if (!reg_info_extn)
		return -EINVAL;

	reg_info_extn->hw_blocklist = kzalloc(sizeof(*reg_info_extn->hw_blocklist),
					      GFP_ATOMIC);
	if (!reg_info_extn->hw_blocklist)
		return -ENOMEM;

	ath12k_wmi_pull_hw_blocklist_update_extn(ab, skb,
						 reg_info_extn->hw_blocklist);
	return 0;
}

bool ath12k_wmi_is_reg_hw_blocklist_fragment_only_extn(
		struct ath12k_base *ab,
		struct ath12k_reg_info *reg_info)
{
	struct ath12k_hw_blocklist_update *hw_bl = reg_info->reg_info_extn.hw_blocklist;

	if (hw_bl && hw_bl->valid && !hw_bl->is_first) {
		ath12k_warn(ab,
			    "hw blocklist fragment-only update (phy_id %u cmd %u seq %u elems %u)\n",
			    reg_info->phy_id,
			    hw_bl->cmd,
			    hw_bl->seq_num,
			    hw_bl->num_chans);
		return true;
	}

	return false;
}

void ath12k_wmi_free_reg_hw_blocklist_extn(
		struct ath12k_reg_info_extn *reg_info_extn)
{
	if (!reg_info_extn || !reg_info_extn->hw_blocklist)
		return;

	kfree(reg_info_extn->hw_blocklist->chans);
	kfree(reg_info_extn->hw_blocklist);
	reg_info_extn->hw_blocklist = NULL;
}

static bool
ath12k_wmi_handle_afc_hwbl_no_data_extn(struct ath12k_base *ab,
					struct ath12k_afc_info *afc_info,
					struct ath12k_hw_blocklist_update *upd)
{
	u8 phy_id = afc_info->phy_id;

	if (phy_id < ab->num_radios) {
		struct ath12k_afc_info **hw_bl_afc_info = ab->ath12k_base_extn.hw_bl_afc_info;

		spin_lock_bh(&ab->base_lock);
		__ath12k_reg_hw_blocklist_free_for_mode_extn(ab, phy_id, WMI_REG_STD_POWER_AP);
		if (hw_bl_afc_info[phy_id]) {
			ath12k_free_afc_power_event_info(hw_bl_afc_info[phy_id]);
			kfree(hw_bl_afc_info[phy_id]);
			hw_bl_afc_info[phy_id] = NULL;
		}
		spin_unlock_bh(&ab->base_lock);
	}

	kfree(upd->chans);
	return false;
}

static bool
ath12k_wmi_handle_afc_hwbl_more_frags_extn(struct ath12k_base *ab, u8 phy_id,
					   bool is_first,
					   struct ath12k_afc_info *afc_info)
{
	struct ath12k_afc_info *cached;

	if (!is_first)
		goto free_afc;


	spin_lock_bh(&ab->base_lock);
	cached = ab->ath12k_base_extn.hw_bl_afc_info[phy_id];
	if (!cached) {
		/* GFP_ATOMIC is safe under spinlock — no need to release */
		cached = kzalloc(sizeof(*cached), GFP_ATOMIC);
		if (!cached)
			goto unlock_and_free;
		ab->ath12k_base_extn.hw_bl_afc_info[phy_id] = cached;
	}
	ath12k_free_afc_power_event_info(cached);
	*cached = *afc_info;
	spin_unlock_bh(&ab->base_lock);
	return true;

unlock_and_free:
	spin_unlock_bh(&ab->base_lock);
free_afc:
	ath12k_free_afc_power_event_info(afc_info);
	return true;
}

static bool
ath12k_wmi_handle_afc_hwbl_final_frag_extn(struct ath12k *ar, u8 phy_id,
					   struct ath12k_afc_info *afc_info)
{
	struct ath12k_base *ab = ar->ab;
	struct ath12k_afc_info *process_info;
	int ret;

	spin_lock_bh(&ab->base_lock);
	process_info = ab->ath12k_base_extn.hw_bl_afc_info[phy_id];
	ab->ath12k_base_extn.hw_bl_afc_info[phy_id] = NULL;
	spin_unlock_bh(&ab->base_lock);
	ath12k_free_afc_power_event_info(afc_info);

	if (!process_info || !process_info->afc_reg_info) {
		ath12k_warn(ab,
			    "hw blocklist: no cached AFC info for final fragment phy_id %u\n",
			    phy_id);
		kfree(process_info);
		return true;
	}

	ret = ath12k_reg_process_afc_power_event(ar, process_info);
	if (ret)
		ath12k_warn(ab, "AFC reg rule update failed ret: %d\n", ret);

	ath12k_vendor_send_power_update_complete(ar, process_info);
	if (process_info->afc_reg_info->fw_status_code != REG_FW_AFC_POWER_EVENT_SUCCESS)
		ath12k_free_afc_power_event_info(process_info);

	kfree(process_info);
	return true;
}

/**
 * ath12k_wmi_handle_afc_power_info_hwbl_extn - Intercept AFC power event for HW blocklist
 * @ab: Pointer to the ath12k base structure
 * @skb: Socket buffer carrying the AFC WMI event
 * @afc_info: Parsed AFC info from the current event
 *
 * Extracts any HWBL update fragment embedded in the AFC event.
 *
 * - If no valid HWBL data: clears SP-mode blocklist for this pdev and returns
 *   false (caller should proceed with normal AFC processing).
 * - If more=true (multi-fragment): caches the first-fragment afc_info and
 *   returns true (caller should suppress normal processing).
 * - On the final fragment: swaps in the cached first-fragment afc_info and
 *   calls ath12k_reg_process_afc_power_event(), then returns true.
 *
 * Return: false if normal AFC source processing should continue;
 *         true if the event was consumed by this function.
 */
bool ath12k_wmi_handle_afc_power_info_hwbl_extn(
		struct ath12k_base *ab, struct sk_buff *skb,
		struct ath12k_afc_info *afc_info)
{
	struct ath12k_hw_blocklist_update upd = {};
	struct ath12k *ar;
	u8 phy_id;
	int ret;

	ath12k_wmi_pull_hw_blocklist_update_extn(ab, skb, &upd);
	if (!upd.valid)
		return ath12k_wmi_handle_afc_hwbl_no_data_extn(ab, afc_info, &upd);

	phy_id = upd.is_first ? afc_info->phy_id : upd.phy_id;
	if (phy_id >= ab->num_radios) {
		ath12k_warn(ab, "hw blocklist: invalid phy_id %u in AFC event\n", phy_id);
		goto free_and_return;
	}

	ar = ab->pdevs[phy_id].ar;
	if (!ar) {
		ath12k_warn(ab, "hw blocklist: no ar for phy_id %u in AFC event\n", phy_id);
		goto free_and_return;
	}

	if (upd.is_first)
		ath12k_reg_hw_blocklist_free_for_mode_extn(ab, phy_id, WMI_REG_STD_POWER_AP);

	ret = ath12k_reg_update_hw_blocklist_extn(ab, phy_id, &upd);
	kfree(upd.chans);
	if (ret) {
		ath12k_warn(ab, "hw blocklist: AFC update failed phy_id %u ret %d\n",
			    phy_id, ret);
		ath12k_free_afc_power_event_info(afc_info);
		return true;
	}

	if (upd.more)
		return ath12k_wmi_handle_afc_hwbl_more_frags_extn(ab, phy_id,
								  upd.is_first,
								  afc_info);

	if (upd.is_first) {
		struct ath12k_afc_info **hw_bl_afc_info = ab->ath12k_base_extn.hw_bl_afc_info;

		spin_lock_bh(&ab->base_lock);
		if (hw_bl_afc_info[phy_id]) {
			ath12k_free_afc_power_event_info(hw_bl_afc_info[phy_id]);
			kfree(hw_bl_afc_info[phy_id]);
			hw_bl_afc_info[phy_id] = NULL;
		}
		spin_unlock_bh(&ab->base_lock);
		return false;
	}

	return ath12k_wmi_handle_afc_hwbl_final_frag_extn(ar, phy_id, afc_info);

free_and_return:
	ath12k_free_afc_power_event_info(afc_info);
	kfree(upd.chans);
	return true;
}

bool ath12k_wmi_handle_afc_continuation_hwbl_extn(struct ath12k_base *ab,
						  struct sk_buff *skb,
						  struct ath12k_afc_info *afc_info)
{
	struct ath12k_hw_blocklist_update upd = {};
	bool mid_sequence;
	u8 phy_id;

	/* AFC continuation frames may leave pdev_id unset; get phy_id from
	 * HWBL TLVs. No valid HWBL data means this isn't a continuation.
	 */
	ath12k_wmi_pull_hw_blocklist_update_extn(ab, skb, &upd);
	if (!upd.valid)
		return false;

	phy_id = upd.phy_id;
	kfree(upd.chans);

	if (phy_id >= ab->num_radios)
		return false;

	spin_lock_bh(&ab->base_lock);
	mid_sequence = ab->ath12k_base_extn.hw_bl_afc_info[phy_id] != NULL;
	spin_unlock_bh(&ab->base_lock);

	if (!mid_sequence)
		return false;

	return ath12k_wmi_handle_afc_power_info_hwbl_extn(ab, skb, afc_info);
}

static bool
ath12k_wmi_handle_reg_chan_list_hwbl_more_extn(struct ath12k_base *ab, u8 phy_id,
					       bool is_first,
					       struct ath12k_reg_info **reg_info_ptr)
{
	if (!is_first)
		return true;

	spin_lock_bh(&ab->base_lock);
	if (ab->ath12k_base_extn.hw_bl_reg_info[phy_id])
		ath12k_reg_info_free_extn(ab->ath12k_base_extn.hw_bl_reg_info[phy_id]);

	ab->ath12k_base_extn.hw_bl_reg_info[phy_id] = *reg_info_ptr;
	*reg_info_ptr = NULL;
	spin_unlock_bh(&ab->base_lock);
	return true;
}

static bool
ath12k_wmi_handle_reg_chan_list_hwbl_final_extn(struct ath12k_base *ab, u8 phy_id,
						struct ath12k_reg_info **reg_info_ptr)
{
	struct ath12k_reg_info *cached;

	spin_lock_bh(&ab->base_lock);
	cached = ab->ath12k_base_extn.hw_bl_reg_info[phy_id];
	ab->ath12k_base_extn.hw_bl_reg_info[phy_id] = NULL;
	spin_unlock_bh(&ab->base_lock);
	if (!cached) {
		ath12k_warn(ab,
			    "hw blocklist: no cached reg_info for final fragment phy_id %u\n",
			    phy_id);
		return true;
	}

	ath12k_reg_info_free_extn(*reg_info_ptr);
	*reg_info_ptr = cached;
	return false;
}

/**
 * ath12k_wmi_handle_reg_chan_list_hwbl_extn - Handle HW blocklist in reg chan list event
 * @ab: Pointer to the ath12k base structure
 * @reg_info_ptr: Double pointer to the reg_info struct for this event.
 *
 * Three code paths:
 * (1) No valid HWBL data: clear per-pdev blocklist state and return false
 *     (caller should call ath12k_reg_handle_chan_list normally).
 * (2) First fragment with more=true: steal *reg_info_ptr (set to NULL),
 *     cache it, return true (caller should NOT call ath12k_reg_handle_chan_list).
 * (3) Final fragment: swap *reg_info_ptr to the cached first-fragment reg_info
 *     (fragment-only event's reg_info is freed), return false (caller should
 *     call ath12k_reg_handle_chan_list with the swapped pointer).
 *
 * Return: false if normal source processing should continue;
 *         true if the event was fully consumed or deferred.
 */
bool ath12k_wmi_handle_reg_chan_list_hwbl_extn(struct ath12k_base *ab,
					       struct ath12k_reg_info **reg_info_ptr)
{
	struct ath12k_reg_info *info = *reg_info_ptr;
	struct ath12k_hw_blocklist_update *upd;
	u8 phy_id;
	int ret;

	upd = info->reg_info_extn.hw_blocklist;
	if (!upd || !upd->valid) {
		phy_id = info->phy_id;
		if (phy_id < ab->num_radios)
			ath12k_reg_hw_blocklist_free_for_pdev_extn(ab, phy_id);
		return false;
	}

	phy_id = upd->is_first ? info->phy_id : upd->phy_id;
	if (phy_id >= ab->num_radios) {
		ath12k_warn(ab, "hw blocklist: invalid phy_id %u in reg event\n", phy_id);
		return true;
	}

	if (upd->is_first)
		ath12k_reg_hw_blocklist_free_for_pdev_extn(ab, phy_id);

	ret = ath12k_reg_update_hw_blocklist_extn(ab, phy_id, upd);
	if (ret) {
		ath12k_warn(ab, "hw blocklist: update failed phy_id %u ret %d\n",
			    phy_id, ret);
		return true;
	}

	if (upd->more)
		return ath12k_wmi_handle_reg_chan_list_hwbl_more_extn(ab, phy_id,
								      upd->is_first,
								      reg_info_ptr);

	if (upd->is_first)
		return false;

	return ath12k_wmi_handle_reg_chan_list_hwbl_final_extn(ab, phy_id, reg_info_ptr);
}

int ath12k_wmi_hw_blocklist_chan_event_extn(struct ath12k_base *ab,
					    struct sk_buff *skb)
{
	struct ath12k_hw_blocklist_update hw_blocklist = {};
	int ret;
	u8 phy_id;

	ret = ath12k_wmi_tlv_iter(ab, skb->data, skb->len,
				  ath12k_wmi_hw_blocklist_elem_parse_extn,
				  &hw_blocklist);
	if (ret) {
		ath12k_warn(ab, "Failed to parse HW blocklist event\n");
		return ret;
	}

	if (!hw_blocklist.valid) {
		ath12k_warn(ab, "Invalid HW blocklist");
		return -EINVAL;
	}

	phy_id = hw_blocklist.phy_id;
	if (phy_id >= ab->num_radios) {
		ath12k_warn(ab, "Received AFC Event for Invalid phy id %d\n", phy_id);
		kfree(hw_blocklist.chans);
		return -EINVAL;
	}

	ret = ath12k_reg_update_hw_blocklist_extn(ab, phy_id, &hw_blocklist);
	if (ret) {
		ath12k_warn(ab, "Failed to update HW blocklist\n");
		kfree(hw_blocklist.chans);
		return ret;
	}

	kfree(hw_blocklist.chans);

	if (!hw_blocklist.more) {
		ret = ath12k_vendor_send_hw_blocklist_update_event_extn(ab, phy_id);
		if (ret)
			ath12k_warn(ab, "Failed to send HW blocklist vendor event: %d\n",
				    ret);
	}

	return 0;
}

static u16 ath12k_reg_hw_blocklist_bw_to_mhz_extn(u8 max_bw)
{
	switch (max_bw) {
	case WMI_HOST_CHAN_WIDTH_5:
		return 5;
	case WMI_HOST_CHAN_WIDTH_10:
		return 10;
	case WMI_HOST_CHAN_WIDTH_20:
		return 20;
	case WMI_HOST_CHAN_WIDTH_40:
		return 40;
	case WMI_HOST_CHAN_WIDTH_80:
		return 80;
	case WMI_HOST_CHAN_WIDTH_160:
		return 160;
	case WMI_HOST_CHAN_WIDTH_165:
		return 165;
	case WMI_HOST_CHAN_WIDTH_320:
		return 320;
	default:
		return 0;
	}
}

static const u16 ath12k_reg_wmi_punc_map_80_extn[] = {
	0x01, 0x02, 0x04, 0x08
};

static const u16 ath12k_reg_wmi_punc_map_160_extn[] = {
	0x01, 0x02, 0x04, 0x08,
	0x10, 0x20, 0x40, 0x80,
	0x0C, 0x03, 0xC0, 0x30
};

static const u16 ath12k_reg_wmi_punc_map_320_extn[] = {
	0x000C, 0x0003, 0x00C0, 0x0030,
	0x0C00, 0x0300, 0xC000, 0x3000,
	0x000F, 0x00F0, 0x0F00, 0xF000,
	0xF003, 0xF00C, 0xF030, 0xF0C0,
	0xF300, 0xFC00, 0x003F, 0x00CF,
	0x030F, 0x0C0F, 0x300F, 0xC00F
};

static const char *ath12k_reg_hw_blocklist_pwr_mode_name_extn(u8 pwr_mode)
{
	switch (pwr_mode) {
	case WMI_REG_INDOOR_AP:
		return "LPI";
	case WMI_REG_STD_POWER_AP:
		return "SP";
	case WMI_REG_VLP_AP:
		return "VLP";
	default:
		return "UNKNOWN";
	}
}

static void
ath12k_reg_hw_blocklist_get_punc_patterns_extn(u16 bw, const u16 **patterns,
					       u8 *num_patterns)
{
	*patterns = NULL;
	*num_patterns = 0;

	switch (bw) {
	case 80:
		*patterns = ath12k_reg_wmi_punc_map_80_extn;
		*num_patterns = ARRAY_SIZE(ath12k_reg_wmi_punc_map_80_extn);
		break;
	case 160:
		*patterns = ath12k_reg_wmi_punc_map_160_extn;
		*num_patterns = ARRAY_SIZE(ath12k_reg_wmi_punc_map_160_extn);
		break;
	case 320:
		*patterns = ath12k_reg_wmi_punc_map_320_extn;
		*num_patterns = ARRAY_SIZE(ath12k_reg_wmi_punc_map_320_extn);
		break;
	default:
		break;
	}
}

static void
ath12k_reg_hw_blocklist_get_pri_freqs_extn(const struct ath12k_hw_blocklist_fb_chan *fb_chan,
					   u16 *pri_freqs, u8 max_freqs, u8 *num_freqs)
{
	u16 bw;
	u16 start_freq;
	u8 max_subch;
	u8 i;

	*num_freqs = 0;
	if (!max_freqs)
		return;

	bw = ath12k_reg_hw_blocklist_bw_to_mhz_extn(fb_chan->max_bw);
	if (bw < 20)
		return;

	if (bw == 20) {
		start_freq = fb_chan->center_freq;
		max_subch = 1;
	} else {
		start_freq = fb_chan->center_freq - (bw / 2) + 10;
		max_subch = min_t(u8, bw / 20, 16);
	}

	for (i = 0; i < max_subch; i++) {
		if (!(fb_chan->pri20_bitmap & BIT(i)))
			continue;
		if (*num_freqs >= max_freqs)
			return;
		pri_freqs[(*num_freqs)++] = start_freq + (i * 20);
	}
}

static void
ath12k_reg_hw_blocklist_get_punc_patterns_list_extn(u16 bw, u32 punc_bitmap,
						    u16 *punc_patterns,
						    u8 max_patterns,
						    u8 *num_patterns)
{
	const u16 *patterns;
	u8 total_patterns;
	u8 i;

	*num_patterns = 0;
	ath12k_reg_hw_blocklist_get_punc_patterns_extn(bw, &patterns,
						       &total_patterns);
	if (!patterns || !total_patterns)
		return;

	for (i = 0; i < total_patterns; i++) {
		if (!(punc_bitmap & BIT(i)))
			continue;
		if (*num_patterns >= max_patterns)
			return;
		punc_patterns[(*num_patterns)++] = patterns[i];
	}
}

#define ATH12K_HWBL_LINE_BUF_LEN 96

static void
ath12k_reg_hw_blocklist_print_wrapped_tokens_extn(struct ath12k_base *ab,
						  const char *row_prefix,
						  char tokens[][32],
						  u8 num_tokens)
{
	char line[ATH12K_HWBL_LINE_BUF_LEN];
	int prefix_len = strlen(row_prefix);
	bool first_line = true;
	u8 i;
	int pos = 0;

	if (!num_tokens) {
		ath12k_info(ab, "%s-\n", row_prefix);
		return;
	}

	line[0] = '\0';
	for (i = 0; i < num_tokens; i++) {
		int token_len = strlen(tokens[i]) + (pos ? 2 : 0);

		if (pos && pos + token_len >= sizeof(line)) {
			if (first_line)
				ath12k_info(ab, "%s%s\n", row_prefix, line);
			else
				ath12k_info(ab, "%*s%s\n",
					    prefix_len, "", line);
			first_line = false;
			pos = 0;
			line[0] = '\0';
		}

		if (pos)
			pos += scnprintf(line + pos, sizeof(line) - pos, ", ");

		pos += scnprintf(line + pos, sizeof(line) - pos, "%s", tokens[i]);
	}

	if (first_line)
		ath12k_info(ab, "%s%s\n", row_prefix, line);
	else
		ath12k_info(ab, "%*s%s\n", prefix_len, "", line);
}

/* Maximum string length for a single display token (frequency or pattern) */
#define ATH12K_HWBL_TOKEN_STR_LEN 32

/* Minimum column width for index display in the blocklist table */
#define ATH12K_HWBL_IDX_MIN_WIDTH 3

static int ath12k_reg_hw_blocklist_idx_width_extn(u32 num_entries)
{
	u32 max_idx;
	int digits = 1;

	if (!num_entries)
		return ATH12K_HWBL_IDX_MIN_WIDTH;

	max_idx = num_entries - 1;
	while (max_idx >= 10) {
		max_idx /= 10;
		digits++;
	}

	return max(digits, ATH12K_HWBL_IDX_MIN_WIDTH);
}

void
ath12k_reg_hw_blocklist_free_snapshot_extn(struct ath12k_hw_blocklist_state *snapshot)
{
	int i;

	for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++)
		ath12k_reg_hw_blocklist_free_chans_extn(&snapshot->pwr[i]);
}

int
ath12k_reg_hw_blocklist_snapshot_extn(struct ath12k_base *ab, u8 phy_id,
				      struct ath12k_hw_blocklist_state *snapshot)
{
	struct ath12k_hw_blocklist_state *st;
	int i;
	int ret = 0;

	memset(snapshot, 0, sizeof(*snapshot));
	spin_lock_bh(&ab->base_lock);
	if (phy_id >= ab->num_radios) {
		ret = -EINVAL;
		goto unlock;
	}

	st = ath12k_reg_hw_blocklist_get_state_extn(ab, phy_id);
	if (!st) {
		ret = -ENODATA;
		goto unlock;
	}

	snapshot->valid = st->valid;
	snapshot->complete = st->complete;
	snapshot->last_cmd = st->last_cmd;
	snapshot->last_seq = st->last_seq;
	snapshot->expected_seq = st->expected_seq;

	for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++) {
		const struct ath12k_hw_blocklist_chans *src = &st->pwr[i];
		struct ath12k_hw_blocklist_chans *dst = &snapshot->pwr[i];

		dst->nfbchans = src->nfbchans;
		dst->npcchans = src->npcchans;

		if (src->nfbchans) {
			u32 fb_len = src->nfbchans * sizeof(*src->fb_chan);
			dst->fb_chan = kmemdup(src->fb_chan, fb_len, GFP_ATOMIC);
			if (!dst->fb_chan) {
				ret = -ENOMEM;
				goto unlock;
			}
		}

		if (src->npcchans) {
			u32 pc_len = src->npcchans * sizeof(*src->pc_chan);
			dst->pc_chan = kmemdup(src->pc_chan, pc_len, GFP_ATOMIC);
			if (!dst->pc_chan) {
				ret = -ENOMEM;
				goto unlock;
			}
		}
	}

unlock:
	spin_unlock_bh(&ab->base_lock);

	if (ret)
		ath12k_reg_hw_blocklist_free_snapshot_extn(snapshot);

	return ret;
}

#define ATH12K_HWBL_DISPLAY_MAX_TOKENS 33

/* Buffer sizes for display row formatting */
#define ATH12K_HWBL_ROW_PREFIX_LEN    128
#define ATH12K_HWBL_BITMAP_STR_LEN     24
#define ATH12K_HWBL_MAX_PRI20_SUBCHANS 16
#define ATH12K_HWBL_MAX_PUNC_VALS      24

static void
ath12k_reg_display_hwbl_fb_chan_row_extn(struct ath12k_base *ab,
					 const struct ath12k_hw_blocklist_fb_chan *fb_chan,
					 u32 idx, int label_width, int idx_width,
					 char (*tokens)[ATH12K_HWBL_TOKEN_STR_LEN])
{
	char row_prefix[ATH12K_HWBL_ROW_PREFIX_LEN];
	char bitmap_str[ATH12K_HWBL_BITMAP_STR_LEN];
	u16 pri_freqs[ATH12K_HWBL_MAX_PRI20_SUBCHANS];
	u8 npri, ntokens, t;
	u16 bw;

	bw = ath12k_reg_hw_blocklist_bw_to_mhz_extn(fb_chan->max_bw);
	ath12k_reg_hw_blocklist_get_pri_freqs_extn(fb_chan, pri_freqs,
						   ARRAY_SIZE(pri_freqs), &npri);
	scnprintf(bitmap_str, sizeof(bitmap_str), "0x%04x", fb_chan->pri20_bitmap);
	scnprintf(row_prefix, sizeof(row_prefix),
		  "%*s%*u | %7u | %11u | %-12s | ",
		  label_width, "", idx_width, idx, bw,
		  fb_chan->center_freq, bitmap_str);

	ntokens = 0;
	for (t = 0; t < npri; t++) {
		scnprintf(tokens[ntokens], sizeof(tokens[ntokens]), "%u", pri_freqs[t]);
		ntokens++;
	}

	ath12k_reg_hw_blocklist_print_wrapped_tokens_extn(ab, row_prefix, tokens, ntokens);
}

static void
ath12k_reg_display_hwbl_pc_chan_row_extn(struct ath12k_base *ab,
					 const struct ath12k_hw_blocklist_pc_chan *pc_chan,
					 u32 idx, int label_width, int idx_width,
					 char (*tokens)[ATH12K_HWBL_TOKEN_STR_LEN])
{
	char row_prefix[ATH12K_HWBL_ROW_PREFIX_LEN];
	char bitmap_str[ATH12K_HWBL_BITMAP_STR_LEN];
	u16 punc_vals[ATH12K_HWBL_MAX_PUNC_VALS];
	u8 npunc, ntokens, t;
	u16 bw;

	bw = ath12k_reg_hw_blocklist_bw_to_mhz_extn(pc_chan->max_bw);
	ath12k_reg_hw_blocklist_get_punc_patterns_list_extn(bw, pc_chan->punc_pat_bmap,
							    punc_vals,
							    ARRAY_SIZE(punc_vals), &npunc);
	scnprintf(bitmap_str, sizeof(bitmap_str), "0x%08x", pc_chan->punc_pat_bmap);
	scnprintf(row_prefix, sizeof(row_prefix),
		  "%*s%*u | %7u | %11u | %-18s | ",
		  label_width, "", idx_width, idx, bw,
		  pc_chan->center_freq, bitmap_str);

	ntokens = 0;
	for (t = 0; t < npunc; t++) {
		scnprintf(tokens[ntokens], sizeof(tokens[ntokens]), "0x%04x", punc_vals[t]);
		ntokens++;
	}

	ath12k_reg_hw_blocklist_print_wrapped_tokens_extn(ab, row_prefix, tokens, ntokens);
}

static void ath12k_reg_display_hw_blocklist_extn(struct ath12k_base *ab, u8 phy_id)
{
	struct ath12k_hw_blocklist_state bl_state;
	const struct ath12k_hw_blocklist_chans *chans;
	char (*tokens)[ATH12K_HWBL_TOKEN_STR_LEN];
	static const char fb_label[] = "FULL-BW   : ";
	int fb_label_width;
	int pc_label_width;
	int fb_idx_width;
	int pc_idx_width;
	int i;
	u32 j;
	int ret;

	if (!ab)
		return;

	if (phy_id >= ab->num_radios) {
		ath12k_warn(ab, "hw blocklist display: invalid phy_id %u\n", phy_id);
		return;
	}

	tokens = kmalloc_array(ATH12K_HWBL_DISPLAY_MAX_TOKENS, sizeof(*tokens), GFP_KERNEL);
	if (!tokens) {
		ath12k_warn(ab, "hw blocklist display: token alloc failed for phy %u\n",
			    phy_id);
		return;
	}

	ret = ath12k_reg_hw_blocklist_snapshot_extn(ab, phy_id, &bl_state);
	if (ret) {
		ath12k_warn(ab, "hw blocklist display: snapshot failed for phy %u ret %d\n",
			    phy_id, ret);
		goto free_tokens;
	}

	ath12k_info(ab,
		    "HW Blocklist Dump | phy=%u valid=%u complete=%u last_cmd=%u last_seq=%u expected_seq=%u\n",
		    phy_id, bl_state.valid, bl_state.complete, bl_state.last_cmd,
		    bl_state.last_seq, bl_state.expected_seq);

	fb_label_width = strlen(fb_label);
	pc_label_width = strlen("PUNCTURED : ");

	for (i = 0; i < WMI_REG_CURRENT_MAX_AP_TYPE; i++) {
		chans = &bl_state.pwr[i];
		ath12k_info(ab,
			    "--------------------------------------------------------------------------------\n");
		ath12k_info(ab,
			    "Power Mode: %s (%u) | Full-BW Entries: %u | Punctured Entries: %u\n",
			    ath12k_reg_hw_blocklist_pwr_mode_name_extn(i),
			    i, chans->nfbchans, chans->npcchans);

		if (!chans->nfbchans) {
			ath12k_info(ab, "FULL-BW   : none\n");
		} else {
			fb_idx_width = ath12k_reg_hw_blocklist_idx_width_extn(chans->nfbchans);
			ath12k_info(ab,
				    "%s%*s | %7s | %11s | %-12s | %s\n",
				    fb_label, fb_idx_width, "idx", "bw(MHz)",
				    "center(MHz)", "pri20_bitmap",
				    "blocked_primary_freqs(MHz)");
			for (j = 0; j < chans->nfbchans; j++)
				ath12k_reg_display_hwbl_fb_chan_row_extn(ab,
									 &chans->fb_chan[j],
									 j, fb_label_width,
									 fb_idx_width, tokens);
		}

		if (!chans->npcchans) {
			ath12k_info(ab, "PUNCTURED : none\n");
			continue;
		}

		pc_idx_width = ath12k_reg_hw_blocklist_idx_width_extn(chans->npcchans);
		ath12k_info(ab,
			    "%s%*s | %7s | %11s | %-18s | %s\n",
			    "PUNCTURED : ", pc_idx_width, "idx", "bw(MHz)",
			    "center(MHz)", "pattern_idx_bitmap",
			    "blocked_patterns");
		for (j = 0; j < chans->npcchans; j++)
			ath12k_reg_display_hwbl_pc_chan_row_extn(ab,
								 &chans->pc_chan[j],
								 j, pc_label_width,
								 pc_idx_width, tokens);
	}

	ath12k_info(ab,
		    "--------------------------------------------------------------------------------\n");

	ath12k_reg_hw_blocklist_free_snapshot_extn(&bl_state);

free_tokens:
	kfree(tokens);
}

static ssize_t
ath12k_read_display_hw_blocklist_extn(struct file *file,
				      char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);

	if (*ppos)
		return 0;

	ath12k_reg_display_hw_blocklist_extn(ar->ab, ar->pdev_idx);
	*ppos = 1;

	return 0;
}

static const struct file_operations fops_display_hw_blocklist_extn = {
	.read = ath12k_read_display_hw_blocklist_extn,
	.open = simple_open,
};

#define AFC_HW_BLOCKLIST_MIN_SUPP_PWR_SUBCMDID_EXTN 150

/**
 * enum ath12k_hw_blocklist_min_supp_pwr_args_extn - AFC unit test args used
 * to override the HW blocklist minimum supported power
 * @HW_BLOCKLIST_MIN_SUPP_PWR_SUBCMDID_EXTN: Subcmdid
 * @HW_BLOCKLIST_MIN_SUPP_PWR_VALUE_EXTN: Min supported power override value
 * @HW_BLOCKLIST_MIN_SUPP_PWR_MAX_ARGS_EXTN: Max test args
 */
enum ath12k_hw_blocklist_min_supp_pwr_args_extn {
	HW_BLOCKLIST_MIN_SUPP_PWR_SUBCMDID_EXTN,
	HW_BLOCKLIST_MIN_SUPP_PWR_VALUE_EXTN,
	HW_BLOCKLIST_MIN_SUPP_PWR_MAX_ARGS_EXTN,
};

/**
 * ath12k_wmi_send_hw_blocklist_min_supp_power_extn() - Send the HW blocklist
 * min supported power override value to FW.
 * @ar: pointer to ath12k radio
 * @value: user configured min supported power override value
 *
 * Reuses the AFC unit test module id/token (AFC_UNIT_TEST_MODULE_ID,
 * AFC_UNIT_TEST_TOKEN) already defined in wmi.h for the AFC grace timer
 * feature, and the shared ath12k_wmi_send_unit_test_cmd() infra used by
 * every other unit test based debug command (grace timer, DFS, M3, AWGN).
 *
 * Return: 0 on success, negative error code on failure
 */
static int
ath12k_wmi_send_hw_blocklist_min_supp_power_extn(struct ath12k *ar, s32 value)
{
	u32 test_args[HW_BLOCKLIST_MIN_SUPP_PWR_MAX_ARGS_EXTN];
	struct wmi_unit_test_cmd wmi_ut;

	test_args[HW_BLOCKLIST_MIN_SUPP_PWR_SUBCMDID_EXTN] =
		AFC_HW_BLOCKLIST_MIN_SUPP_PWR_SUBCMDID_EXTN;
	test_args[HW_BLOCKLIST_MIN_SUPP_PWR_VALUE_EXTN] = (u32)value;

	wmi_ut.vdev_id = cpu_to_le32(0);
	wmi_ut.module_id = cpu_to_le32(AFC_UNIT_TEST_MODULE_ID);
	wmi_ut.num_args = cpu_to_le32(HW_BLOCKLIST_MIN_SUPP_PWR_MAX_ARGS_EXTN);
	wmi_ut.diag_token = cpu_to_le32(AFC_UNIT_TEST_TOKEN);

	ath12k_dbg(ar->ab, ATH12K_DBG_REG,
		   "Sending HW blocklist min supported power override value %d to FW\n",
		   value);

	return ath12k_wmi_send_unit_test_cmd(ar, wmi_ut, test_args);
}

/**
 * ath12k_write_hw_min_supp_power_override_extn() - debugfs write handler
 * for the "hw_min_supp_power_override" debugfs file. Reads the user
 * programmed min supported power override value and sends it to firmware.
 * @file: file pointer
 * @user_buf: user buffer
 * @count: size of the buffer
 * @ppos: file position
 *
 * Return: number of bytes consumed on success, negative error code on failure
 */
static ssize_t
ath12k_write_hw_min_supp_power_override_extn(struct file *file,
					     const char __user *user_buf,
					     size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	int ret;
	s32 value;

	guard(wiphy)(ath12k_ar_to_hw(ar)->wiphy);

	if (!test_bit(ATH12K_FLAG_REGISTERED, &ar->ab->dev_flags))
		return -ENETDOWN;

	if (kstrtos32_from_user(user_buf, count, 10, &value))
		return -EINVAL;

	ret = ath12k_wmi_send_hw_blocklist_min_supp_power_extn(ar, value);
	if (ret)
		return ret;

	return count;
}

static const struct file_operations fops_hw_min_supp_power_override_extn = {
	.write = ath12k_write_hw_min_supp_power_override_extn,
	.open = simple_open,
};

void ath12k_debugfs_register_hw_blocklist_extn(struct ath12k *ar)
{
	debugfs_create_file("display_hw_blocklist", 0400,
			    ar->debug.debugfs_pdev, ar,
			    &fops_display_hw_blocklist_extn);
	debugfs_create_file("hw_min_supp_power_override", 0200,
			    ar->debug.debugfs_pdev, ar,
			    &fops_hw_min_supp_power_override_extn);
}

enum ath12k_6ghz_mode_validate_state {
	ATH12K_6GHZ_MODE_VALIDATE_SKIP = 0,
	ATH12K_6GHZ_MODE_VALIDATE_PASS = 1,
	ATH12K_6GHZ_MODE_VALIDATE_BLOCK = -EINVAL,
};

static struct ieee80211_channel *
ath12k_mac_get_6ghz_chan_for_power_mode(struct wiphy *wiphy,
					const struct ieee80211_channel *chan,
					enum ieee80211_ap_reg_power power_type)
{
	const struct ieee80211_supported_band *sband;
	struct ieee80211_6ghz_channel *chan_6g;
	enum nl80211_regulatory_power_modes pwr_mode;
	int i;

	if (!wiphy || !chan || chan->band != NL80211_BAND_6GHZ)
		return NULL;

	sband = wiphy->bands[NL80211_BAND_6GHZ];
	if (!sband)
		return NULL;

	pwr_mode = ieee80211_mac_to_cfg_power_type(power_type);
	if (pwr_mode >= NL80211_REG_NUM_POWER_MODES)
		return NULL;

	chan_6g = sband->chan_6g[pwr_mode];
	if (!chan_6g || !chan_6g->channels || !chan_6g->n_channels)
		return NULL;

	for (i = 0; i < chan_6g->n_channels; i++) {
		struct ieee80211_channel *tmp = &chan_6g->channels[i];

		if (tmp->center_freq == chan->center_freq &&
		    tmp->freq_offset == chan->freq_offset)
			return tmp;
	}

	return NULL;
}

static int ath12k_reg_get_punc_pattern_index_extn(u16 bw, u16 puncture_pattern)
{
	const u16 *patterns = NULL;
	u8 num_patterns = 0;
	u8 i;

	switch (bw) {
	case 80:
		patterns = ath12k_reg_wmi_punc_map_80_extn;
		num_patterns = ARRAY_SIZE(ath12k_reg_wmi_punc_map_80_extn);
		break;
	case 160:
		patterns = ath12k_reg_wmi_punc_map_160_extn;
		num_patterns = ARRAY_SIZE(ath12k_reg_wmi_punc_map_160_extn);
		break;
	case 320:
		patterns = ath12k_reg_wmi_punc_map_320_extn;
		num_patterns = ARRAY_SIZE(ath12k_reg_wmi_punc_map_320_extn);
		break;
	default:
		return -1;
	}

	for (i = 0; i < num_patterns; i++) {
		if (patterns[i] == puncture_pattern)
			return i;
	}

	return -1;
}

static bool ath12k_reg_is_chan_punc_extn(u16 puncture_pattern, u16 bw)
{
	if (!puncture_pattern)
		return false;

	return ath12k_reg_get_punc_pattern_index_extn(bw, puncture_pattern) >= 0;
}

static bool
ath12k_reg_is_chan_in_full_blocklist_extn(
	const struct ath12k_hw_blocklist_fb_chan *fb_chan,
	u32 nfbchans, u16 freq, u16 center_freq, u16 bw)
{
	u32 i;

	for (i = 0; i < nfbchans; i++) {
		u16 bl_bw = ath12k_reg_hw_blocklist_bw_to_mhz_extn(fb_chan[i].max_bw);
		int start_freq;
		int offset;
		u32 pri20_bit;

		if (!bl_bw || bl_bw != bw || fb_chan[i].center_freq != center_freq)
			continue;

		if (bw == 20) {
			start_freq = freq;
		} else {
			if (bw < 20)
				continue;

			start_freq = (int)center_freq - ((int)bw / 2) + 10;
			if (start_freq <= 0)
				continue;
		}

		offset = (int)freq - start_freq;
		if (offset < 0 || offset % 20)
			continue;

		pri20_bit = offset / 20;
		if (pri20_bit >= 16)
			continue;

		if (fb_chan[i].pri20_bitmap & BIT(pri20_bit))
			return true;
	}

	return false;
}

static bool
ath12k_reg_is_chan_in_punc_blocklist_extn(
	const struct ath12k_hw_blocklist_pc_chan *pc_chan,
	u32 npcchans, u16 puncture_pattern, u16 center_freq, u16 bw)
{
	int punc_idx;
	u32 i;

	punc_idx = ath12k_reg_get_punc_pattern_index_extn(bw, puncture_pattern);
	if (punc_idx < 0)
		return false;

	for (i = 0; i < npcchans; i++) {
		u16 bl_bw = ath12k_reg_hw_blocklist_bw_to_mhz_extn(pc_chan[i].max_bw);

		if (!bl_bw || bl_bw != bw || pc_chan[i].center_freq != center_freq)
			continue;

		if (pc_chan[i].punc_pat_bmap & BIT(punc_idx))
			return true;
	}

	return false;
}

bool ath12k_reg_is_hw_blocklisted_channel_extn(struct ath12k_base *ab, u8 phy_id,
					       u16 freq, u16 center_freq, u16 bw,
					       enum wmi_reg_6g_ap_type pwr_mode,
					       u16 puncture_pattern)
{
	struct ath12k_hw_blocklist_state *st;
	const struct ath12k_hw_blocklist_chans *bl;
	bool blocked = false;

	if (!ab || !freq || !center_freq || !bw)
		return false;

	if (phy_id >= ab->num_radios || pwr_mode >= WMI_REG_CURRENT_MAX_AP_TYPE)
		return false;

	spin_lock_bh(&ab->base_lock);
	st = ath12k_reg_hw_blocklist_get_state_extn(ab, phy_id);
	if (!st || !st->valid || !st->complete)
		goto unlock;

	bl = &st->pwr[pwr_mode];
	if (!ath12k_reg_is_chan_punc_extn(puncture_pattern, bw)) {
		if (!bl->fb_chan || !bl->nfbchans)
			goto unlock;

		blocked =
		    ath12k_reg_is_chan_in_full_blocklist_extn(bl->fb_chan,
							      bl->nfbchans,
							      freq,
							      center_freq,
							      bw);
		goto unlock;
	}

	if (!bl->pc_chan || !bl->npcchans)
		goto unlock;

	blocked =
	    ath12k_reg_is_chan_in_punc_blocklist_extn(bl->pc_chan,
						      bl->npcchans,
						      puncture_pattern,
						      center_freq,
						      bw);
unlock:
	spin_unlock_bh(&ab->base_lock);
	return blocked;
}

static int
ath12k_mac_validate_6ghz_blocklist_for_pwr_mode(struct ath12k *ar,
						struct ieee80211_hw *hw,
						const struct cfg80211_chan_def *chandef,
						u16 center_freq, u16 bw,
						enum ieee80211_ap_reg_power power_type)
{
	struct ieee80211_channel *chan_6ghz;
	struct cfg80211_chan_def tmp_chandef = *chandef;
	enum wmi_reg_6g_ap_type wmi_pwr_type;

	chan_6ghz = ath12k_mac_get_6ghz_chan_for_power_mode(hw->wiphy,
							    chandef->chan,
							    power_type);
	if (!chan_6ghz)
		return ATH12K_6GHZ_MODE_VALIDATE_SKIP;

	tmp_chandef.chan = chan_6ghz;

	if (!cfg80211_chandef_usable(hw->wiphy, &tmp_chandef,
				     IEEE80211_CHAN_DISABLED |
				     IEEE80211_CHAN_NO_IR))
		return ATH12K_6GHZ_MODE_VALIDATE_SKIP;

	wmi_pwr_type = ath12k_ieee80211_ap_pwr_type_convert(power_type);
	if (wmi_pwr_type >= WMI_REG_CURRENT_MAX_AP_TYPE)
		return ATH12K_6GHZ_MODE_VALIDATE_SKIP;

	if (!ath12k_reg_is_hw_blocklisted_channel_extn(ar->ab, ar->pdev_idx,
						       tmp_chandef.chan->center_freq,
						       center_freq, bw,
						       wmi_pwr_type,
						       chandef->punctured))
		return ATH12K_6GHZ_MODE_VALIDATE_PASS;

	ath12k_warn(ar->ab,
		    "Reject 6 GHz chandef due to HW blocklist: pdev %u freq %u center %u bw %u pwr %u punct 0x%04x\n",
		    ar->pdev_idx, chandef->chan->center_freq,
		    center_freq, bw, wmi_pwr_type, chandef->punctured);

	return ATH12K_6GHZ_MODE_VALIDATE_BLOCK;
}

int ath12k_validate_6ghz_chandef_extn(struct ieee80211_hw *hw,
				      struct ieee80211_vif *vif,
				      unsigned int link_id,
				      const struct cfg80211_chan_def *chandef,
				      enum ieee80211_ap_reg_power power_type)
{
	struct ath12k_hw *ah = ath12k_hw_to_ah(hw);
	struct ath12k_vif *ahvif = ath12k_vif_to_ahvif(vif);
	struct ath12k_link_vif *arvif;
	struct ath12k *ar = NULL;
	struct ieee80211_sub_if_data *sdata = vif_to_sdata(vif);
	u16 bw, center_freq;
	bool found = false;
	int i;

	lockdep_assert_wiphy(hw->wiphy);

	if (!chandef || !chandef->chan ||
	    chandef->chan->band != NL80211_BAND_6GHZ)
		return 0;

	if (wdev_is_scan_radio(&sdata->wdev))
		return 0;

	arvif = NULL;
	if (link_id < IEEE80211_MLD_MAX_NUM_LINKS)
		arvif = wiphy_dereference(hw->wiphy, ahvif->link[link_id]);
	if (arvif)
		ar = arvif->ar;

	/* Link may not be created yet (e.g. pre-assoc MLO links). */
	if (!ar) {
		for_each_ar(ah, ar, i) {
			if (chandef->chan->center_freq >=
				    KHZ_TO_MHZ(ar->freq_range.start_freq) &&
			    chandef->chan->center_freq <=
				    KHZ_TO_MHZ(ar->freq_range.end_freq)) {
				found = true;
				break;
			}
		}
		if (!found)
			ar = NULL;
	}
	if (!ar)
		return 0;

	bw = nl80211_chan_width_to_mhz(chandef->width);
	if (!bw)
		return -EINVAL;

	center_freq = chandef->center_freq1 ? chandef->center_freq1 :
					      chandef->chan->center_freq;

	if (power_type == IEEE80211_REG_UNSET_AP) {
		static const enum ieee80211_ap_reg_power pwr_modes[] = {
			IEEE80211_REG_LPI_AP,
			IEEE80211_REG_VLP_AP,
			IEEE80211_REG_SP_AP,
		};

		for (i = 0; i < ARRAY_SIZE(pwr_modes); i++) {
			int ret;

			ret = ath12k_mac_validate_6ghz_blocklist_for_pwr_mode(
				ar, hw, chandef, center_freq, bw,
				pwr_modes[i]);
			if (ret == ATH12K_6GHZ_MODE_VALIDATE_PASS)
				return 0;

			/* For SP mode, bypass blocklist check for a STA before
			 * AFC power event — firmware SP data not yet available.
			 */
			if (pwr_modes[i] == IEEE80211_REG_SP_AP &&
			    ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
			    !ar->afc.is_6ghz_afc_power_event_received)
				return 0;
		}

		return -EINVAL;
	}

	/*
	 * For a STA validating SP power mode before AFC completion, the
	 * firmware has not yet provided blocklist data for SP. Pass the
	 * channel to avoid blocking the connection prematurely.
	 */
	if (ahvif->vdev_type == WMI_VDEV_TYPE_STA &&
	    !ar->afc.is_6ghz_afc_power_event_received &&
	    power_type == IEEE80211_REG_SP_AP)
		return 0;

	/*
	 * Use the power mode decision to handle repeater and AFC-aware cases.
	 * For repeater AP acting as SP client without local AFC, blocklist
	 * data is not yet meaningful — pass the channel.
	 * For all other cases, validate against the effective power mode
	 * derived by the decision (same as power_type for standalone).
	 * power_type is retained for debug prints only.
	 */
	if (arvif) {
		struct ieee80211_bss_conf *bss_conf;
		struct ath12k_6ghz_pwr_mode_decision decision;

		bss_conf = wiphy_dereference(hw->wiphy, vif->link_conf[link_id]);
		if (bss_conf) {
			ath12k_mac_get_6ghz_power_mode_decision(ar, arvif,
								power_type,
								&decision);
			if (decision.ap_repeater_sp_client)
				return 0;

			if (ath12k_mac_validate_6ghz_blocklist_for_pwr_mode(
				    ar, hw, chandef, center_freq, bw,
				    (enum ieee80211_ap_reg_power)
				    decision.reg_6g_power_mode) ==
			    ATH12K_6GHZ_MODE_VALIDATE_BLOCK)
				return -EINVAL;

			return 0;
		}
	}

	/* Fallback when arvif/bss_conf not available (e.g. pre-assoc MLO). */
	if (ath12k_mac_validate_6ghz_blocklist_for_pwr_mode(
		    ar, hw, chandef, center_freq, bw, power_type) ==
	    ATH12K_6GHZ_MODE_VALIDATE_BLOCK)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(ath12k_validate_6ghz_chandef_extn);

/**
 * ath12k_change_6g_txpow_sta_mode_validate_extn - Validate SP power mode
 * against HW blocklist for standalone STA; disconnect if blocked.
 * @ar: Radio instance
 * @wdev: Wireless device of the STA vif
 * @link_id: Link identifier
 *
 * Validates the current STA chandef against the HW blocklist for SP power
 * mode. If blocked, warns and triggers STA disconnect.
 *
 * Return: true if HWBL-blocked (caller should abort power mode change),
 *         false if the channel is valid and caller can proceed.
 */
bool ath12k_change_6g_txpow_sta_mode_validate_extn(struct ath12k *ar,
						    struct wireless_dev *wdev,
						    u8 link_id)
{
	struct ieee80211_vif *vif;
	struct ieee80211_bss_conf *bss_conf;
	int ret;

	if (link_id >= IEEE80211_MLD_MAX_NUM_LINKS)
		return false;

	lockdep_assert_wiphy(ar->ah->hw->wiphy);

	vif = wdev_to_ieee80211_vif(wdev);
	if (!vif)
		return false;

	bss_conf = wiphy_dereference(ar->ah->hw->wiphy, vif->link_conf[link_id]);
	if (!bss_conf)
		return false;

	ret = ath12k_validate_6ghz_chandef_extn(ar->ah->hw, vif, link_id,
						&bss_conf->chanreq.oper,
						IEEE80211_REG_SP_AP);
	if (!ret)
		return false;

	ath12k_warn(ar->ab,
		    "HW blocklist rejected STA 6 GHz SP power mode (err %d), disconnecting\n",
		    ret);
	ieee80211_disconnect(vif, false);
	return true;
}
