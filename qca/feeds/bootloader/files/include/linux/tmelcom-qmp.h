/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#ifndef _LINUX_TMELCOM_QMP_H
#define _LINUX_TMELCOM_QMP_H
#include <linux/bitfield.h>
#include <dt-bindings/interrupt-controller/irq.h>

/*
 * Macro used to define unique TMEL Message Identifier based on
 * message type and action identifier.
 */
#define MSGTYPE_MASK GENMASK(15, 8)
#define ACTIONID_MASK GENMASK(7, 0)

#define TMEL_MSG_UID_CREATE(m, a) ((u32)((((m) & 0xff) << 8) | ((a) & 0xff)))

#define TMEL_ACTION_LOOPBACK_TEST_MBOX_ADD_VAL                  0x01
#define TMEL_MSG_LOOPBACK_TEST           0xFF
/*
 * Helper macro to extract the messageType from TMEL_MSG_UID
 */
#define TMEL_MSG_UID_MSG_TYPE(v)	FIELD_GET(MSGTYPE_MASK, v)

/*
 * Helper macro to extract the actionID from TMEL_MSG_UID
 */
#define TMEL_MSG_UID_ACTION_ID(v)	FIELD_GET(ACTIONID_MASK, v)

/*
 * All definitions of supported messageTypes.
 */
#define TMEL_MSG_SECBOOT	0x00
#define TMEL_MSG_FUSE		0x03
#define TMEL_MSG_KM		0x07
#define TMEL_MSG_HCS		0x0B

/*
 * Action IDs for TMEL_MSG_SECBOOT
 */
#define TMEL_ACTION_SECBOOT_SEC_AUTH		0x04
#define TMEL_ACTION_SECBOOT_SEC_AUTH_V2		0x0F
#define TMEL_ACTION_SECBOOT_SS_TEAR_DOWN	0x0a
#define TMEL_ACTION_SECBOOT_GET_STATE		0x0C

/*
 *   Action ID's for TMEL_MSG_FUSE
 */
#define TMEL_ACTION_FUSE_READ_SINGLE                     0x00    /* Deprecated */
#define TMEL_ACTION_FUSE_READ_MULTIPLE                   0x01    /* Deprecated */
#define TMEL_ACTION_FUSE_WRITE_SINGLE                    0x02    /* Deprecated */
#define TMEL_ACTION_FUSE_WRITE_MULTIPLE                  0x03    /* Deprecated */
#define TMEL_ACTION_FUSE_WRITE_SECURE                    0x04    /* Deprecated */
#define TMEL_ACTION_FUSE_READ_SINGLE_ROW                 0x05
#define TMEL_ACTION_FUSE_READ_MULTIPLE_ROW               0x06
#define TMEL_ACTION_FUSE_WRITE_SINGLE_ROW                0x07
#define TMEL_ACTION_FUSE_WRITE_MULTIPLE_ROW              0x08
#define TMEL_ACTION_FUSE_ROM_PATCH_REQ                   0x09

/*
 * Action ID's for TMEL_MSG_HCS (Host Crypto Services)
 */
#define TMEL_ACTION_HCS_AES_ENCRYPT			0x0A
#define TMEL_ACTION_HCS_AES_DECRYPT			0x0B
#define TMEL_ACTION_HCS_PRNG_GET			0x0C
#define TMEL_ACTION_HCS_AES_DERIVE			0x04
#define TMEL_ACTION_HCS_AES_CLEAR			0x01

/*
 * TME Key IDs
 */
#define TME_KID_ALLOC					0xAAAAAAAA
#define TME_KID_INVALID					0xFFFFFFFF
#define TME_KID_CHIP_RAND_BASE				0x9
#define TME_KID_OEM_PRODUCT_SEED			0xC
#define TME_KID_L2_KEYWRAPSVC				0x6
#define TME_KID_L2_SECURESTRGSVC			0x7
#define TME_KID_L2_CLIENTEXTSVC				0x3

/*
 * TME KDF and Algorithm IDs
 */
#define TME_KAL_KDF_NIST				0x80000
#define TME_KAL_SHA512_HMAC				0x58000
#define TME_KAL_AES256_ECB				0xC
#define TME_KAL_AES256_CBC				0x8

/*
 * TME Key Lineage IDs
 */
#define TME_KLI_NP_CU					0x800
#define TME_KLI_NA					0x0


/*
 * UIDs for TMEL_MSG_SECBOOT
 */
#define TMEL_MSG_UID_SECBOOT_SEC_AUTH	TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
					TMEL_ACTION_SECBOOT_SEC_AUTH)

#define TMEL_MSG_UID_SECBOOT_SEC_AUTH_V2	TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
						TMEL_ACTION_SECBOOT_SEC_AUTH_V2)

#define TMEL_MSG_UID_SECBOOT_SS_TEAR_DOWN	TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
						TMEL_ACTION_SECBOOT_SS_TEAR_DOWN)

#define TMEL_MSG_UID_SECBOOT_GET_STATE		TMEL_MSG_UID_CREATE(TMEL_MSG_SECBOOT,\
						TMEL_ACTION_SECBOOT_GET_STATE)

#define TMEL_MSG_UID_LOOPBACK_TEST_MBOX_ADD_VAL	TMEL_MSG_UID_CREATE(TMEL_MSG_LOOPBACK_TEST,\
						TMEL_ACTION_LOOPBACK_TEST_MBOX_ADD_VAL)

/*
 * Read multiple fuses
 */
#define TMEL_MSG_UID_FUSE_READ_MULTIPLE_ROW	TMEL_MSG_UID_CREATE(TMEL_MSG_FUSE,\
						TMEL_ACTION_FUSE_READ_MULTIPLE_ROW)

/*
 * UIDs for TMEL_MSG_HCS (Host Crypto Services)
 */
#define TMEL_MSG_UID_HCS_AES_ENCRYPT		TMEL_MSG_UID_CREATE(TMEL_MSG_HCS,\
						TMEL_ACTION_HCS_AES_ENCRYPT)

#define TMEL_MSG_UID_HCS_AES_DECRYPT		TMEL_MSG_UID_CREATE(TMEL_MSG_HCS,\
						TMEL_ACTION_HCS_AES_DECRYPT)

#define TMEL_MSG_UID_HCS_PRNG_GET		TMEL_MSG_UID_CREATE(TMEL_MSG_HCS,\
						TMEL_ACTION_HCS_PRNG_GET)

#define TMEL_MSG_UID_HCS_AES_DERIVE		TMEL_MSG_UID_CREATE(TMEL_MSG_KM,\
						TMEL_ACTION_HCS_AES_DERIVE)

#define TMEL_MSG_UID_HCS_AES_CLEAR		TMEL_MSG_UID_CREATE(TMEL_MSG_KM,\
						TMEL_ACTION_HCS_AES_CLEAR)

#define TMEL_MSG_UID_AES_ENCRYPT		TMEL_MSG_UID_HCS_AES_ENCRYPT
#define TMEL_MSG_UID_AES_DECRYPT		TMEL_MSG_UID_HCS_AES_DECRYPT
#define TMEL_MSG_UID_AES_DERIVE_KEY		TMEL_MSG_UID_HCS_AES_DERIVE
#define TMEL_MSG_UID_AES_CLEAR_KEY		TMEL_MSG_UID_HCS_AES_CLEAR

/*
 * Parameter ID for HCS PRNG GET
 */
#define TMEL_MSG_UID_HCS_PRNG_GET_PARAM_ID	0x08

#define TMEL_MAX_FUSE_ADDR_SIZE 8

/*
 * All definitions for TME_MSG_QWES (QWES services)
 */
#define TME_MSG_QWES				0x0C

#define TME_ACTION_QWES_INIT_ATTESTATION	0x00
#define TME_ACTION_QWES_DEVICE_ATTESTATION	0x01
#define TME_ACTION_QWES_DEVICE_PROVISIONING	0x02
#define TME_ACTION_QWES_LICENSING_INSTALL	0x03
#define TME_ACTION_QWES_LICENSING_CHECK		0x04
#define TME_ACTION_QWES_LICENSING_ENFORCEHWFEATURES	0x05
#define TME_ACTION_QWES_LICENSING_CHECKLICBUFFER	0x06

/*
 * UIDs for TME_MSG_QWES (following reference implementation)
 */
#define TME_MSG_UID_QWES_LICENSING_INSTALL \
	TMEL_MSG_UID_CREATE(TME_MSG_QWES, TME_ACTION_QWES_LICENSING_INSTALL)

#define TME_MSG_UID_QWES_LICENSING_ENFORCEHWFEATURES \
	TMEL_MSG_UID_CREATE(TME_MSG_QWES, TME_ACTION_QWES_LICENSING_ENFORCEHWFEATURES)

#define TME_MSG_UID_QWES_LICENSING_CHECK \
	TMEL_MSG_UID_CREATE(TME_MSG_QWES, TME_ACTION_QWES_LICENSING_CHECK)

/* Legacy aliases for backward compatibility */
#define TMEL_MSG_UID_LICENSE_INSTALL		TME_MSG_UID_QWES_LICENSING_INSTALL
#define TMEL_MSG_UID_LICENSE_ENFORCE_HW		TME_MSG_UID_QWES_LICENSING_ENFORCEHWFEATURES
#define TMEL_MSG_UID_LICENSE_CHECK_FID		TME_MSG_UID_QWES_LICENSING_CHECK
//#define TMEL_MSG_UID_LICENSE_LIMIT_DDR		TMEL_MSG_UID_CREATE(TME_MSG_QWES, 0x07)  /* Custom DDR limit action */

struct tmel_qmp_msg {
	void *msg;
	u32 msg_id;
	size_t size;
};

struct tmel_add_val {
	u32 val1;
	u32 val2;
};

struct tmel_msg_param_type_buf_in {
	u32 buf;
	u32 buf_len;
};

struct tmel_msg_param_type_buf_out {
	u32 buf;
	u32 buf_len;
	u32 out_buf_len;
};

struct tmel_msg_param_type_buf_in_out {
	u32 buf;
	u32 buf_len;
	u32 out_buf_len;
};

struct tmel_sec_auth {
	u32 sw_id;
	struct tmel_msg_param_type_buf_in elf_buf;
	struct tmel_msg_param_type_buf_in region_list;
	u32 relocate;
};

struct tmel_sec_auth_v2 {
	u32 sw_id;
	struct tmel_msg_param_type_buf_in elf_buf;
	struct tmel_msg_param_type_buf_in region_list;
	u32 relocate;
	u32 nsIntegrityCheck:1;
	u32 reservedBits:31;
	struct tmel_msg_param_type_buf_in reservedBuf;
	u32 keyHandle;
};

struct tmel_fuse_payload {
	u32 fuse_addr;
	u32 lsb_val;
	u32 msb_val;
} __packed;

struct tmel_fuse_read_multiple_msg {
	u32 status;
	struct tmel_msg_param_type_buf_in_out fuse_read_data;
} __packed;

struct tmelcom {
	struct mbox_chan mbox;
};

/*
 * TME PATCH VERSION structures
 */

/*
 * TME Response Buffer structure
 * Matches TMEResponseCBuffer from TME firmware
 */
struct tmel_response_buffer {
	u32 pdata;              /* Pointer to the buffer */
	u32 length;             /* Length of the buffer */
	u32 length_used;        /* Actual length of the buffer used */
} __packed;

/*
 * TME Patch Version Length
 */
#define TME_PATCH_VERSION_LENGTH        128

/*
 * TME State Type structure
 * Matches tmeStateType_t from TME firmware
 */
struct tmel_state_type {
	u32 tme_patch_status : 1;       /* Only 1 bit tmePatchStatus_t value */
	u32 tme_mode : 1;               /* Only 1 bit tmeModeType_t value */
	u32 reserve : 30;               /* 30 bits reserved */
} __packed;

/*
 * Get TME State Response structure
 * Matches tmeGetStateRsp_t from TME firmware
 */
struct tmel_get_state_rsp {
	u32 state;                      /* Get TMEL State */
	struct tmel_response_buffer patch_version;      /* TMEL Patch version buffer */
	u32 status;                                     /* TME Get State Status */
} __packed;

/*
 * Get TME State structure
 * Matches tmeGetState_t from TME firmware
 */
struct tmel_get_state {
	struct tmel_get_state_rsp rsp;
} __packed;

/*
 * Simplified TME Get State structure for internal use
 * Used when calling tmel_qmp_msg and tmel_qmp_send
 */
struct tmel_get_tme_version {
	u32 pdata;              /* Pointer to patch version buffer */
	u32 length;             /* Length of the buffer (should be >= TME_PATCH_VERSION_LENGTH) */
} __packed;

/*
 * PRNG service structures
 */

/*
 * Sequencer status response structure
 * Matches TMESequencerStatusRsp_t from TME firmware
 */
struct tmel_sequencer_status_resp {
	u32 tme_error_status;		/* TME FW Response status */
	u32 seq_error_status;		/* Contents of CSR_CMD_ERROR_STATUS */
	u32 seq_kp_error_status0;	/* CRYPTO_ENGINE_CRYPTO_KEY_POLICY_ERROR_STATUS0 */
	u32 seq_kp_error_status1;	/* CRYPTO_ENGINE_CRYPTO_KEY_POLICY_ERROR_STATUS1 */
	u32 seq_rsp_status;		/* Contents of CSR_CMD_RESPONSE_STATUS */
} __packed;

/*
 * Get PRNG request structure
 */
struct tmel_prng_get_req {
	u32 length;		/* Length of random data to be generated */
} __packed;

/*
 * Get PRNG response structure
 * Matches TMEPRNGGetResponse_t from TME firmware
 */
struct tmel_prng_get_resp {
	struct tmel_response_buffer prng_buf;		/* Buffer to store random data */
	u32 status;				/* TME-FW IPC layer status */
	struct tmel_sequencer_status_resp seq_status;	/* Sequencer status */
} __packed;

/*
 * Get PRNG message structure
 */
struct tmel_prng_get_msg {
	struct tmel_prng_get_req input;
	struct tmel_prng_get_resp output;
} __packed;

/*
 * Simplified PRNG Get structure for internal use
 */
struct tmel_get_prng {
	u32 length;
	u32 pdata;
} __packed;
/* License install request structure (matches QwesLicensingInstallMsg_t) */
struct tmel_license_install_req {
	u32 status;
	struct tmel_msg_param_type_buf_in license_buf;
	u32 flags;  /* Changed from u64 to u32 to match reference */
	struct tmel_msg_param_type_buf_out identifier_buf;
} __packed;

/* License install response structure */
struct tmel_license_install_resp {
	u32 status;
	u64 flags;
	u32 identifier_len;
} __packed;

/* HW feature enforcement request (matches QwesLicensingEnfHWFeaturesMsg_t) */
struct tmel_license_enforce_hw_req {
	u32 status;
	struct tmel_msg_param_type_buf_in_out features_buf;
	u32 hw_reg_version;
} __packed;

/* Forward declarations for license types - actual definitions in mach/ipq_license.h */
struct sec_enforceHWFeatureId;
struct sec_fid_info;

#ifdef CONFIG_CMD_AES_256
/*
 * AES Crypto Service structures
 */

/* Common buffer structures for AES operations */
struct tmel_cbuffer {
	u32 buf;
	u32 buf_len;
} __packed;

struct tmel_cbuffer_resp {
	u32 buf;
	u32 length;
	u32 length_used;
} __packed;

/* AES Encrypt structures */
struct tmel_aes_encrypt_req {
	u32 algo;
	u32 key_id;
	struct tmel_cbuffer in_aad;
	struct tmel_cbuffer in_plain_txt;
} __packed;

struct tmel_aes_encrypt_resp {
	struct tmel_cbuffer_resp out_aad;
	struct tmel_cbuffer_resp out_iv;
	struct tmel_cbuffer_resp out_tag;
	struct tmel_cbuffer_resp out_cipher_txt;
	u32 status;
	u32 seq_status[5];
} __packed;

struct tmel_aes_encrypt_msg {
	struct tmel_aes_encrypt_req req;
	struct tmel_aes_encrypt_resp resp;
} __packed;

/* AES Decrypt structures */
struct tmel_aes_decrypt_req {
	u32 algo;
	u32 key_id;
	struct tmel_cbuffer in_aad;
	struct tmel_cbuffer in_iv;
	struct tmel_cbuffer in_tag;
	struct tmel_cbuffer in_cipher_txt;
} __packed;

struct tmel_aes_decrypt_resp {
	struct tmel_cbuffer_resp out_aad;
	struct tmel_cbuffer_resp out_plain_txt;
	u32 status;
	u32 seq_status[5];
} __packed;

struct tmel_aes_decrypt_msg {
	struct tmel_aes_decrypt_req req;
	struct tmel_aes_decrypt_resp resp;
} __packed;

/* AES Key Derivation structures */

/* TME KDF spec constants */
#define TME_KDF_SW_CONTEXT_BYTES_MAX		128
#define TME_KDF_SALT_LABEL_BYTES_MAX		64

/* TME KDF spec structure with 128-byte context */
struct tme_kdf_spec {
	u32 kdf_algo;
	u32 input_key;
	u32 mix_key;
	u32 l2_key;
	struct {
		u32 low;
		u32 high;
	} policy;
	u8 sw_context[TME_KDF_SW_CONTEXT_BYTES_MAX];
	u32 sw_context_len;
	u32 security_context;
	u8 salt_label[TME_KDF_SALT_LABEL_BYTES_MAX];
	u32 salt_label_len;
	u32 prf_digest_algo;
} __packed;

/* TME derive key request/response structures */
struct tme_derive_req {
	u32 key_id;
	u32 kdf_buf;
	u32 kdf_len;
	u32 cred_slot;
} __packed;

struct tme_derive_resp {
	u32 key_id;
	u32 status;
	u32 seq_status[5];
} __packed;

struct tme_derive_msg {
	struct tme_derive_req req;
	struct tme_derive_resp resp;
} __packed;

/* AES Clear Key structures */
struct tmel_aes_clear_key_req {
	u32 key_id;
} __packed;

struct tmel_aes_clear_key_resp {
	u32 status;
	u32 seq_status[5];
} __packed;

struct tmel_aes_clear_key_msg {
	struct tmel_aes_clear_key_req req;
	struct tmel_aes_clear_key_resp resp;
} __packed;
#endif /* CONFIG_CMD_AES_256 */

void tmel_secboot_sec_free(void *ptr);
int ipq_get_tmelcom_device(struct tmelcom **tmelcom_priv);
int ipq_list_fuse_tme_impl(void *params);
int ipq_dump_fuse_tme_impl(void *params);
int ipq_check_secure_boot_tme_impl(void *params);
int ipq_secure_auth_tme_impl(void *params);
int ipq_get_tme_version_impl(void *params);
int ipq_prng_get_tme_impl(void *params);
int ipq_aes_256_enc_tme_impl(void *params);
int ipq_aes_256_dec_tme_impl(void *params);
int ipq_aes_derive_key_tme_impl(void *params);
int ipq_aes_derive_key_max_ctxt_tme_impl(void *params);
int ipq_aes_clear_key_tme_impl(void *params);

#ifdef CONFIG_IPQ_SOFTSKU_SUPPORT
int ipq_license_install_tme(void *license, size_t licenseLen,
			     u64 *flags, u8 *identifier,
			     size_t identifierLen, size_t *identifierLenOut);
int ipq_license_enforce_hw_features_tme(struct sec_enforceHWFeatureId *fidBuff,
					size_t fidBuffLen, size_t *fidBuffLenOut,
					u32 *HWRegisterInterfaceVersion);
#endif

#endif  /* _LINUX_TMELCOM_QMP_H */
