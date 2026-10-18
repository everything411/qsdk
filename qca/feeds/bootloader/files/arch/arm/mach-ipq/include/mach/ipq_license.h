// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _IPQ_LICENSE_H_
#define _IPQ_LICENSE_H_

#include <linux/types.h>

/* License feature IDs */
#define SOFTSKU_USB3_INTERFACE_FEATURE_ID       3005
#define SOFTSKU_APPS_CPU_FREQUENCY_FEATURE_ID   3006
#define SOFTSKU_AUDIO_INTERFACE_FEATURE_ID      3007
#define SOFTSKU_DDR_SPACE_LIMIT_FEATURE_ID      3008
#define SOFTSKU_NSS_CRYPTO_ENGINE_FEATURE_ID    3009
#define SOFTSKU_PCIE_RC0_FEATURE_ID             3010
#define SOFTSKU_PCIE_RC1_FEATURE_ID             3011
#define SOFTSKU_PCIE_RC2_FEATURE_ID             3012
#define SOFTSKU_PCIE_RC3_FEATURE_ID             3013
#define SOFTSKU_UNIPHY0_FEATURE_ID              3014
#define SOFTSKU_UNIPHY1_FEATURE_ID              3015
#define SOFTSKU_UNIPHY2_FEATURE_ID              3016
#define SOFTSKU_APSS_PERF_CPU_FREQ_FEATURE_ID   3028
#define SOFTSKU_NSP_FEATURE_ID                  3029
#define SOFTSKU_PCIE_RC4_FEATURE_ID             3031
#define SOFTSKU_PRIME_SUBSYSTEM_FEATURE_ID      3032
#define SOFTSKU_UNIPHY1_SPEED_CONFIG_FEATURE_ID 3033
#define SOFTSKU_UNIPHY2_SPEED_CONFIG_FEATURE_ID 3034

#define SOFTSKU_FEATURE_ID_COUNT                18

/* License constants */
#define MAX_LICENSE_LEN         0xc00  /* 3KB */
#define MAX_LICENSE_COUNT       61
#define SLOT_SIZE               4096   /* 4KB */

/* License flags */
#define FLAG_UPDATE_NONE            0x0
#define FLAG_UPDATE_LICENSESTORE    0x1
#define FLAG_UPDATE_CACHE           0x10

/* Feature status types */
typedef enum {
	SEC_FEATURE_STATUS_ACTIVE = 0x00,
	SEC_FEATURE_STATUS_NOTACTIVE = 0x01,
	SEC_FEATURE_STATUS_NOTPRESENT = 0x02,
	SEC_FEATURE_STATUS_DISABLED = 0x03,
} sec_feature_status_type;

/* Quality of Time types */
typedef enum {
	SEC_QOT_NOT_AVAILABLE = 0x0,
	SEC_QOT_LOW_TRUST = 0x01,
	SEC_QOT_MEDIUM_TRUST = 0x02,
	SEC_QOT_HIGH_TRUST = 0x03,
} sec_QoT_type;

/* Feature value structure */
typedef struct {
	u32 encoding_type;
	u32 feature_value;
} sec_featureValue_type;

/* Time information structure */
typedef struct {
	sec_QoT_type QoT;
	u64 currentTime;
} sec_timeInfo_type;

/* Feature response structure */
typedef struct {
	u32 featureId;
	sec_feature_status_type featureStatus;
	bool isTimeBound;
	u64 validAfter;
	u64 validUntil;
	u64 graceUntil;
	sec_featureValue_type feature_value;
	u8 metaData[8];
	u64 bindings;
	u64 antiReplayCounter;
} sec_feature_response_type;

/* FID response structure */
typedef struct {
	u32 version;
	sec_timeInfo_type time;
	sec_feature_response_type feature_status;
} sec_fid_response_type;

/* FID info structure */
struct sec_fid_info {
	u32 fid;
	sec_fid_response_type rcvd_response;
	bool fid_updated;
};

/* HW feature enforcement structure */
struct sec_enforceHWFeatureId {
	u32 feature_id;
	bool HWFeatureStatus;
};

/* SMEM structure for softsku info */
typedef struct {
	bool fid_updated;
	u32 featureId;
	bool HWFeatureStatus;
	sec_feature_status_type featureStatus;
	bool isTimeBound;
	u64 graceUntil;
	sec_featureValue_type feature_value;
} softsku_info_smem;

/* License storage handle */
typedef void *license_handle_t;

/* Function prototypes */
int ipq_spl_mibib_getpart(const char *part_name, uint32_t *start_blk,
			   uint32_t *blk_cnt);
int ipq_spl_license_init(void *ctx);
int ipq_spl_save_fids_smem(struct udevice *smem);
int ipq_license_install_tme(void *license, size_t licenseLen,
			     u64 *flags, u8 *identifier,
			     size_t identifierLen, size_t *identifierLenOut);
int ipq_license_enforce_hw_features_tme(struct sec_enforceHWFeatureId *fidBuff,
					size_t fidBuffLen, size_t *fidBuffLenOut,
					u32 *HWRegisterInterfaceVersion);
int ipq_license_check_fid_info_tme(struct sec_fid_info *fidInfo, size_t fidInfoLen);
int ipq_license_limit_ddr_tme(void);

#endif /* _IPQ_LICENSE_H_ */
