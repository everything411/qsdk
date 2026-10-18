/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2010-2015,2019 The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __QCOM_SCM_INT_H
#define __QCOM_SCM_INT_H

enum qcom_scm_convention {
	SMC_CONVENTION_UNKNOWN,
	SMC_CONVENTION_LEGACY,
	SMC_CONVENTION_ARM_32,
	SMC_CONVENTION_ARM_64,
};

extern enum qcom_scm_convention qcom_scm_convention;

#define MAX_QCOM_SCM_ARGS 10
#define MAX_QCOM_SCM_RETS 3

enum qcom_scm_arg_types {
	QCOM_SCM_VAL,
	QCOM_SCM_RO,
	QCOM_SCM_RW,
	QCOM_SCM_BUFVAL,
};

#define QCOM_SCM_ARGS_IMPL(num, a, b, c, d, e, f, g, h, i, j, ...) (\
			   (((a) & 0x3) << 4) | \
			   (((b) & 0x3) << 6) | \
			   (((c) & 0x3) << 8) | \
			   (((d) & 0x3) << 10) | \
			   (((e) & 0x3) << 12) | \
			   (((f) & 0x3) << 14) | \
			   (((g) & 0x3) << 16) | \
			   (((h) & 0x3) << 18) | \
			   (((i) & 0x3) << 20) | \
			   (((j) & 0x3) << 22) | \
			   ((num) & 0xf))

#define QCOM_SCM_ARGS(...) QCOM_SCM_ARGS_IMPL(__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

#define SCM_QSEEOS_FNID(s, c) (((((s) & 0xFF) << 8) | ((c) & 0xFF)) | \
				0x32000000)

/**
 * struct qcom_scm_desc
 * @arginfo:	Metadata describing the arguments in args[]
 * @args:	The array of arguments for the secure syscall
 */
struct qcom_scm_desc {
	u32 svc;
	u32 cmd;
	u32 arginfo;
	u64 args[MAX_QCOM_SCM_ARGS];
	u32 owner;
};

/**
 * struct qcom_scm_res
 * @result:	The values returned by the secure syscall
 */
struct qcom_scm_res {
	u64 result[MAX_QCOM_SCM_RETS];
};

int qcom_scm_wait_for_wq_completion(u32 wq_ctx);
int scm_get_wq_ctx(u32 *wq_ctx, u32 *flags, u32 *more_pending);

#define SCM_SMC_FNID(s, c)	((((s) & 0xFF) << 8) | ((c) & 0xFF))
extern int __scm_smc_call(struct device *dev, const struct qcom_scm_desc *desc,
			  enum qcom_scm_convention qcom_convention,
			  struct qcom_scm_res *res, bool atomic);
#define scm_smc_call(dev, desc, res, atomic) \
	__scm_smc_call((dev), (desc), qcom_scm_convention, (res), (atomic))

#define SCM_LEGACY_FNID(s, c)	(((s) << 10) | ((c) & 0x3ff))
extern int scm_legacy_call_atomic(struct device *dev,
				  const struct qcom_scm_desc *desc,
				  struct qcom_scm_res *res);
extern int scm_legacy_call(struct device *dev, const struct qcom_scm_desc *desc,
			   struct qcom_scm_res *res);

extern int __qti_scm_tz_hvc_log(struct device *dev, u32 svc_id, u32 cmd_id,
                                void *ker_buf, u32 buf_len);

#define QCOM_SCM_SVC_BOOT		0x01
#define QCOM_SCM_BOOT_SET_ADDR		0x01
#define QCOM_SCM_BOOT_TERMINATE_PC	0x02
#define QCOM_SCM_BOOT_SET_DLOAD_MODE	0x10
#define QCOM_SCM_BOOT_SET_ADDR_MC	0x11
#define QCOM_SCM_BOOT_SET_REMOTE_STATE	0x0a
#define QCOM_SCM_FLUSH_FLAG_MASK	0x3
#define QCOM_SCM_BOOT_MAX_CPUS		4
#define QCOM_SCM_BOOT_MC_FLAG_AARCH64	BIT(0)
#define QCOM_SCM_BOOT_MC_FLAG_COLDBOOT	BIT(1)
#define QCOM_SCM_BOOT_MC_FLAG_WARMBOOT	BIT(2)
#define QCOM_SCM_IS_TZ_LOG_ENCRYPTED	0xb
#define QCOM_SCM_GET_TZ_LOG_ENCRYPTED	0xc
#define QCOM_SCM_ABNORMAL_MAGIC		0x40

#define SCM_CMD_TZ_CONFIG_HW_FOR_RAM_DUMP_ID	0x9

#define QCOM_SCM_SVC_PIL		0x02
#define QCOM_SCM_PIL_PAS_INIT_IMAGE	0x01
#define QCOM_SCM_PAS_INIT_IMAGE_V2_CMD  0x1a
#define QCOM_SCM_PIL_PAS_MEM_SETUP	0x02
#define QCOM_SCM_PIL_PAS_AUTH_AND_RESET	0x05
#define QCOM_SCM_PIL_PAS_SHUTDOWN	0x06
#define QCOM_SCM_PIL_PAS_IS_SUPPORTED	0x07
#define QCOM_QFPROM_IS_AUTHENTICATE_CMD 0x07
#define QCOM_QFPROM_ROW_READ_CMD        0x08
#define QCOM_QFPROM_ROW_WRITE_CMD       0x09
#define QCOM_SCM_SVC_SEC_AUTH           0x01
#define QCOM_SCM_PIL_PAS_MSS_RESET	0x0a

#define QCOM_SCM_SVC_UTIL		0x03
#define QCOM_SCM_CMD_SET_REGSAVE	0x02
#define QCOM_SCM_CDUMP_FEATURE_ID	0x4

#define QCOM_SCM_SVC_IO			0x05
#define QCOM_SCM_IO_READ		0x01
#define QCOM_SCM_IO_WRITE		0x02

#define QCOM_SCM_SVC_INFO		0x06
#define QCOM_SCM_INFO_IS_CALL_AVAIL	0x01
#define QCOM_SCM_IS_FEATURE_AVAIL	0x03
#define QTI_SCM_TZ_DIAG_CMD		0x2
#define QTI_SCM_HVC_DIAG_CMD		0x7
#define QTI_SCM_SMMUSTATE_CMD		0x19
#define QCOM_IS_CALL_AVAIL_CMD		0x1

#define QCOM_SCM_SVC_MP				0x0c
#define QCOM_SCM_MP_RESTORE_SEC_CFG		0x02
#define QCOM_SCM_MP_IOMMU_SECURE_PTBL_SIZE	0x03
#define QCOM_SCM_MP_IOMMU_SECURE_PTBL_INIT	0x04
#define QCOM_SCM_MP_IOMMU_SET_CP_POOL_SIZE	0x05
#define QCOM_SCM_MP_VIDEO_VAR			0x08
#define QCOM_SCM_MP_ASSIGN			0x16

#define QCOM_SCM_SVC_OCMEM		0x0f
#define QCOM_SCM_OCMEM_LOCK_CMD		0x01
#define QCOM_SCM_OCMEM_UNLOCK_CMD	0x02

#define QCOM_SCM_SVC_ES			0x10	/* Enterprise Security */
#define QCOM_SCM_ES_INVALIDATE_ICE_KEY	0x03
#define QCOM_SCM_ES_CONFIG_SET_ICE_KEY	0x04

#define QCOM_SVC_ICE			23
#define QCOM_SCM_ICE_CMD		0x1
#define QCOM_SCM_ICE_CONTEXT_CMD	0x3

#define QCOM_SCM_SVC_HDCP		0x11
#define QCOM_SCM_HDCP_INVOKE		0x01

#define QCOM_SCM_SVC_LMH			0x13
#define QCOM_SCM_LMH_LIMIT_PROFILE_CHANGE	0x01
#define QCOM_SCM_LMH_LIMIT_DCVSH		0x10

#define QCOM_SCM_SVC_SMMU_PROGRAM		0x15
#define QCOM_SCM_SMMU_PT_FORMAT			0x01
#define QCOM_SCM_SMMU_CONFIG_ERRATA1		0x03
#define QCOM_SCM_SMMU_CONFIG_ERRATA1_CLIENT_ALL	0x02

#define QCOM_SCM_PD_LOAD_SVC_ID			0x2
#define QCOM_SCM_PD_LOAD_CMD_ID			0x16
#define QCOM_SCM_PD_LOAD_V2_CMD_ID		0x19
#define QCOM_SCM_INT_RAD_PWR_UP_CMD_ID		0x17
#define QCOM_SCM_INT_RAD_PWR_DN_CMD_ID		0x18

#define QCOM_SCM_SVC_WAITQ			0x24
#define QCOM_SCM_WAITQ_RESUME			0x02
#define QCOM_SCM_WAITQ_GET_WQ_CTX		0x03

/*
 * QCOM_SCM_QCE_SVC - commands related to secure key for secure nand
 */
#define QCOM_SCM_QCE_CMD		0x3
#define QCOM_SCM_QCE_CRYPTO_SIP		0xA
#define QCOM_SCM_QCE_ENC_DEC_CMD	0xB
#define QCOM_SCM_QCE_UNLOCK_CMD		0x4
#define QCOM_SCM_SECCRYPT_CLRKEY_CMD	0xC

/*
 * QCOM_SCM_QWES_SVC - commands related to Qwes feature
 */
#define QCOM_SCM_QWES_SVC_ID				0x1E
#define QCOM_SCM_QWES_INIT_ATTEST			0x01
#define QCOM_SCM_QWES_ATTEST_REPORT			0x02
#define QCOM_SCM_QWES_DEVICE_PROVISION			0x03

extern int __qti_sec_crypt(struct device *dev, void *confBuf, int size);
extern int __qti_seccrypt_clearkey(struct device *dev);
extern int __qti_set_qcekey_sec(struct device *dev, void *confBuf, int size);
extern int __qti_scm_qseecom_remove_xpu(struct device *);
extern int __qti_scm_qseecom_notify(struct device *dev,
				    struct qsee_notify_app *req,
				    size_t req_size,
				    struct qseecom_command_scm_resp *resp,
				    size_t resp_size);
extern int __qti_scm_qseecom_load(struct device *dev,
				  uint32_t smc_id, uint32_t cmd_id,
				  union qseecom_load_ireq *req,
				  size_t req_size,
				  struct qseecom_command_scm_resp *resp,
				  size_t resp_size);
extern int __qti_scm_qseecom_send_data(struct device *dev,
				       union qseecom_client_send_data_ireq *req,
				       size_t req_size,
				       struct qseecom_command_scm_resp *resp,
				       size_t resp_size);
extern int __qti_scm_qseecom_unload(struct device *dev,
				    uint32_t smc_id, uint32_t cmd_id,
				    struct qseecom_unload_ireq *req,
				    size_t req_size,
				    struct qseecom_command_scm_resp *resp,
				    size_t resp_size);
extern int __qti_scm_register_log_buf(struct device *dev,
					 struct qsee_reg_log_buf_req *request,
					 size_t req_size,
					 struct qseecom_command_scm_resp
					 *response, size_t resp_size);
extern int __qti_scm_tls_hardening(struct device *dev, uint32_t req_addr,
				   uint32_t req_size, uint32_t resp_addr,
				   uint32_t resp_size, u32 cmd_id);
extern int __qti_scm_aes(struct device *dev, uint32_t req_addr,
			 uint32_t req_size, u32 cmd_id);
extern int __qti_scm_aes_clear_key_handle(struct device *dev, uint32_t key_handle, u32 cmd_id);
extern int __qcom_remove_xpu_scm_call_available(struct device *dev, u32 svc_id,
						u32 cmd_id);
extern int __qti_scm_get_device_attestation_ephimeral_key(struct device *dev,
			void *key_buf, u32 key_buf_len, u32 *key_len);
extern int __qti_scm_get_device_attestation_response(struct device *dev,
			void *req_buf, u32 req_buf_len,	void *extclaim_buf,
			u32 extclaim_buf_len, void *resp_buf,
			u32 resp_buf_len, u32 *attest_resp_len);
extern int __qti_scm_get_device_provision_response(struct device *dev,
			void *provreq_buf, u32 provreq_buf_len, void *provresp_buf,
			u32 provresp_buf_len, u32 *prov_resp_size);

/* common error codes */
#define QCOM_SCM_V2_EBUSY	-12
#define QCOM_SCM_ENOMEM		-5
#define QCOM_SCM_EOPNOTSUPP	-4
#define QCOM_SCM_EINVAL_ADDR	-3
#define QCOM_SCM_EINVAL_ARG	-2
#define QCOM_SCM_ERROR		-1
#define QCOM_SCM_INTERRUPTED	1
#define QCOM_SCM_WAITQ_SLEEP	2
#define QCOM_SCM_EINVAL_SIZE	18

static inline int qcom_scm_remap_error(int err)
{
	switch (err) {
	case QCOM_SCM_ERROR:
		return -EIO;
	case QCOM_SCM_EINVAL_ADDR:
	case QCOM_SCM_EINVAL_ARG:
		return -EINVAL;
	case QCOM_SCM_EOPNOTSUPP:
		return -EOPNOTSUPP;
	case QCOM_SCM_ENOMEM:
		return -ENOMEM;
	case QCOM_SCM_V2_EBUSY:
		return -EBUSY;
	}
	return -EINVAL;
}

struct scm_priv_data {
	u32 flag;
	u32 milestone_mask;
};
#endif
