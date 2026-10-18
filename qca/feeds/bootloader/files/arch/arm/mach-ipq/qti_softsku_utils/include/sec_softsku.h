// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __SEC_SOFTSKU_H__
#define __SEC_SOFTSKU_H__

#include <linux/types.h>

/* License flags */
enum license_flags {
	FLAG_UPDATE_NONE = 0x0,
	FLAG_UPDATE_LICENSESTORE = 0x1,
	FLAG_UPDATE_CACHE = 0x10,
};

/* Quality of Time */
enum sec_qot_type {
	SEC_QOT_NOT_AVAILABLE = 0x0,	/* TTime not available */
	SEC_QOT_LOW_TRUST = 0x01,
	SEC_QOT_MEDIUM_TRUST = 0x02,
	SEC_QOT_HIGH_TRUST = 0x03,	/* QCWES Cloud time */
};

/* Feature status */
enum sec_feature_status_type {
	SEC_FEATURE_STATUS_ACTIVE = 0x00,
	SEC_FEATURE_STATUS_NOTACTIVE = 0x01,
	SEC_FEATURE_STATUS_NOTPRESENT = 0x02,
	SEC_FEATURE_STATUS_DISABLED = 0x03,
};

/**
 * struct sec_time_info - Time information
 * @qot: Quality of time
 * @current_time: Current time value
 */
struct sec_time_info {
	enum sec_qot_type qot;
	u64 current_time;
};

/**
 * struct sec_feature_value - Feature value
 * @encoding_type: Encoding type
 * @feature_value: Feature value
 */
struct sec_feature_value {
	u32 encoding_type;
	u32 feature_value;
};

/**
 * struct sec_feature_response - Feature response
 * @feature_id: Feature ID
 * @feature_status: Feature status
 * @is_time_bound: Is time bound flag
 * @valid_after: Valid after timestamp
 * @valid_until: Valid until timestamp
 * @grace_until: Grace period until timestamp
 * @feature_value: Feature value
 * @meta_data: Metadata (8 bytes)
 * @bindings: Bindings
 * @anti_replay_counter: Anti-replay counter
 */
struct sec_feature_response {
	u32 feature_id;
	enum sec_feature_status_type feature_status;
	bool is_time_bound;
	u64 valid_after;
	u64 valid_until;
	u64 grace_until;
	struct sec_feature_value feature_value;
	u8 meta_data[8];
	u64 bindings;
	u64 anti_replay_counter;
};

/**
 * struct sec_fid_response - FID response
 * @version: Version
 * @time: Time information
 * @feature_status: Feature status
 */
struct sec_fid_response {
	u32 version;
	struct sec_time_info time;
	struct sec_feature_response feature_status;
};

/**
 * struct sec_fid_info - FID information
 * @fid: Feature ID
 * @rcvd_response: Received response
 * @fid_updated: FID updated flag
 */
struct sec_fid_info {
	u32 fid;
	struct sec_fid_response rcvd_response;
	bool fid_updated;
};

/**
 * struct sec_enforce_hw_feature_id - HW feature enforcement
 * @feature_id: Feature ID
 * @hw_feature_status: HW feature status (enforced or not)
 */
struct sec_enforce_hw_feature_id {
	u32 feature_id;
	bool hw_feature_status;
};

/**
 * struct softsku_info_smem - Softsku info stored in SMEM
 * @fid_updated: FID updated flag
 * @feature_id: Feature ID
 * @hw_feature_status: HW feature enforcement status
 * @feature_status: Feature status
 * @is_time_bound: Is time bound flag
 * @grace_until: Grace period until timestamp
 * @feature_value: Feature value
 */
struct softsku_info_smem {
	bool fid_updated;
	u32 feature_id;
	bool hw_feature_status;
	enum sec_feature_status_type feature_status;
	bool is_time_bound;
	u64 grace_until;
	struct sec_feature_value feature_value;
};

#endif /* __SEC_SOFTSKU_H__ */
