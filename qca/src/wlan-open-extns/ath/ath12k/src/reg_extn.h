/* SPDX-License-Identifier: BSD-3-Clause-Clear */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef REG_EXTN_H
#define REG_EXTN_H

#include "ath12k_cmn_extn.h"
#include "../wmi.h"

/**
 * enum ath12k_hw_blocklist_cmd - HW blocklist command types from FW
 *
 * Firmware sends these codes in the HW blocklist WMI event to indicate
 * what kind of update should be applied to the per-pdev blocklist state.
 *
 * UPDATE commands carry channel entries that must be merged/replaced.
 * CLEAR commands remove all entries for the indicated power mode.
 * ALL variants apply to both SP and VLP power modes simultaneously.
 *
 * Typical scenarios:
 *   - @ATH12K_HWBL_CMD_SP_UPDATE: FW reports new SP-mode blocked channels
 *     after a regulatory change or AFC response.
 *   - @ATH12K_HWBL_CMD_VLP_CLEAR: FW clears VLP blocklist (e.g. no VLP
 *     channels are restricted in the current regulatory domain).
 *   - @ATH12K_HWBL_CMD_UPDATE_ALL: Both SP and VLP are updated together.
 */
enum ath12k_hw_blocklist_cmd {
	ATH12K_HWBL_CMD_SP_UPDATE = 0,
	ATH12K_HWBL_CMD_VLP_UPDATE = 1,
	ATH12K_HWBL_CMD_SP_CLEAR = 2,
	ATH12K_HWBL_CMD_VLP_CLEAR = 3,
	ATH12K_HWBL_CMD_UPDATE_ALL = 4,
	ATH12K_HWBL_CMD_CLEAR_ALL = 5,
	ATH12K_HWBL_CMD_INVALID = 15,
};

/**
 * struct ath12k_hw_blocklist_fb_chan - Full-bandwidth (FB) blocked channel entry
 * @pri20_bitmap: Bitmask of blocked primary-20 MHz sub-channels within the
 *                channel.  Bit N set means the sub-channel at
 *                center_freq - bw/2 + 10 + N*20 MHz is blocked.
 * @center_freq: Center frequency of the channel in MHz.
 * @max_bw: Maximum bandwidth encoded as a @wmi_host_chan_width value.
 */
struct ath12k_hw_blocklist_fb_chan {
	u16 pri20_bitmap;
	u16 center_freq;
	u8 max_bw;
};

/**
 * struct ath12k_hw_blocklist_pc_chan - Punctured-channel (PC) blocked channel entry
 * @center_freq: Center frequency of the channel in MHz.
 * @max_bw: Maximum bandwidth encoded as a @wmi_host_chan_width value.
 * @punc_pat_bmap: Bitmask of blocked puncture patterns.  Each set bit N
 *                 selects the Nth entry from the per-bandwidth puncture
 *                 pattern table (e.g. ath12k_reg_wmi_punc_map_80_extn[]).
 */
struct ath12k_hw_blocklist_pc_chan {
	u16 center_freq;
	u8 max_bw;
	u32 punc_pat_bmap;
};

/**
 * struct ath12k_hw_blocklist_chans - Per-power-mode blocked channel lists
 * @nfbchans: Number of valid entries in @fb_chan.
 * @npcchans: Number of valid entries in @pc_chan.
 * @fb_chan: Array of full-bandwidth blocked channel entries (heap-allocated).
 * @pc_chan: Array of punctured blocked channel entries (heap-allocated).
 */
struct ath12k_hw_blocklist_chans {
	u32 nfbchans;
	u32 npcchans;
	struct ath12k_hw_blocklist_fb_chan *fb_chan;
	struct ath12k_hw_blocklist_pc_chan *pc_chan;
};

/**
 * struct ath12k_hw_blocklist_state - Per-pdev HW blocklist state
 *
 * Tracks the current blocklist state for one radio (pdev).  "State" here
 * refers to the full lifecycle of a blocklist update: whether the data is
 * valid (FW has sent at least one event), whether all fragments have been
 * received (complete), and the sequencing variables used to detect
 * out-of-order or missing fragments.
 *
 * @valid: True once FW has sent at least one blocklist event for this pdev.
 * @complete: True when all fragments of the current update have been received.
 * @last_cmd: The most recent @ath12k_hw_blocklist_cmd value applied.
 * @last_seq: Sequence number of the most recent fragment processed.
 * @expected_seq: Next sequence number expected from FW (0 when idle).
 * @pwr: Per-power-mode channel lists, indexed by @wmi_reg_6g_ap_type.
 */
struct ath12k_hw_blocklist_state {
	bool valid;
	bool complete;
	u8 last_cmd;
	u8 last_seq;
	u8 expected_seq;
	struct ath12k_hw_blocklist_chans pwr[WMI_REG_CURRENT_MAX_AP_TYPE];
};

/**
 * struct ath12k_hw_blocklist_chan_info - Parsed per-channel info from a WMI fragment
 * @pri20_bitmap: Blocked primary-20 sub-channel bitmap (FB channels only).
 * @center_freq: Channel center frequency in MHz.
 * @max_bw: Maximum bandwidth as a @wmi_host_chan_width value.
 * @power_mode: AP power mode (@wmi_reg_6g_ap_type) for this entry.
 * @punc_pat_bmap: Puncture pattern bitmap (PC channels only; 0 for FB).
 */
struct ath12k_hw_blocklist_chan_info {
	u16 pri20_bitmap;
	u16 center_freq;
	u8 max_bw;
	u8 power_mode;
	u32 punc_pat_bmap;
};

/**
 * struct ath12k_hw_blocklist_update - Parsed HW blocklist WMI event fragment
 *
 * Carries one fragment of a (possibly multi-fragment) HW blocklist update
 * after it has been parsed from the WMI TLV stream.  The @cmd field uses
 * the same @ath12k_hw_blocklist_cmd values as @ath12k_hw_blocklist_state.
 *
 * @valid: True if the fixed-param TLV was present and contained channel data.
 * @cmd: Blocklist command type (@ath12k_hw_blocklist_cmd).
 * @phy_id: pdev index this update targets.
 * @is_first: True if this is the first fragment (seq_num == 1).
 * @more: True if additional fragments follow.
 * @seq_num: Fragment sequence number (1-based).
 * @total_num: Total number of fragments expected in this update.
 * @total_num_chan: Total channel count across all fragments.
 * @current_num_chan: Channel count in this fragment.
 * @num_chans: Number of valid entries in @chans.
 * @chans: Heap-allocated array of parsed channel entries for this fragment.
 */
struct ath12k_hw_blocklist_update {
	bool valid;
	u8 cmd;
	u8 phy_id;
	bool is_first;
	bool more;
	u8 seq_num;
	u8 total_num;
	u16 total_num_chan;
	u16 current_num_chan;
	u32 num_chans;
	struct ath12k_hw_blocklist_chan_info *chans;
};

/**
 * enum reg_unii_band
 * @REG_UNII_BAND_1: Disable UNII-1 band channels
 * @REG_UNII_BAND_2A: Disable UNII-2A band channels
 */
enum reg_unii_band {
	REG_UNII_BAND_1 = 0x0,
	REG_UNII_BAND_2A = 0x1,
};

#endif /* REG_EXTN_H */
