// SPDX-License-Identifier: GPL-2.0+
/*
 * OPTEE Communication Implementation
 *
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <asm/io.h>
#include <linux/delay.h>
#include <dm/device-internal.h>
#include <dm/device.h>
#include <dm/uclass.h>
#include <mach/ipq.h>
#include <cpu_func.h>
#include <command.h>
#include <errno.h>
#include <tee.h>
#include <vsprintf.h>

/* OPTEE fuseipq implementation */
#define PTA_CMD_QFPROM_BLOW_SECELF 0
#define TA_FUSEIPQ_UUID \
	{ 0x7e5e8c5d, 0x5375, 0x4ab0, \
	{ 0x8b, 0x11, 0x95, 0x14, 0x96, 0x65, 0xdc, 0x88} }

/* OPTEE ICE implementation */
#define ICE_TA_CMD_SET_CONTEXT		0x0
#define ICE_TA_CMD_CONFIG_HW_KEY	0x1
#define ICE_TA_UUID \
	{ 0x29e87b9e, 0x012a, 0x4878, \
	{ 0xa1, 0xe1, 0xa1, 0xb9, 0x0a, 0x21, 0x5b, 0x16} }

/* ICE OP-TEE Cipher definitions */
#define QCOM_OPTEE_ICE_CIPHER_AES_128_XTS	0
#define QCOM_OPTEE_ICE_CIPHER_AES_128_CBC	1
#define QCOM_OPTEE_ICE_CIPHER_AES_128_ECB	2
#define QCOM_OPTEE_ICE_CIPHER_AES_256_XTS	3
#define QCOM_OPTEE_ICE_CIPHER_AES_256_CBC	4
#define QCOM_OPTEE_ICE_CIPHER_AES_256_ECB	5

/* Hardware Key Configuration Constants */
#define ICE_OPTEE_CRYPTO_ALGO_MODE_HW_AES_XTS	0x3
#define ICE_OPTEE_CRYPTO_ALGO_MODE_HW_AES_ECB	0x0
#define ICE_OPTEE_CRYPTO_KEY_SIZE_HW_128	0x0
#define ICE_OPTEE_CRYPTO_KEY_SIZE_HW_256	0x2
#define ICE_OPTEE_CRYPTO_USE_KEY0_HW_KEY	0x0

#define TEEC_SUCCESS	0x00000000
#define OEM_SEED_TYPE	0x1

/* Hardware Key Configuration Structure */
struct ice_hw_key_config {
	u32 index;
	u8 key_size;
	u8 algo_mode;
	u8 key_mode;
};

/* ICE Context Configuration Structure */
struct ice_context_config {
	u32 seed_type;
	u8 key_size;
	u8 algo_mode;
	u32 data_ctxt_len;
	u32 salt_ctxt_len;
};

int ipq_fuseipq_optee_impl(void *params)
{
	const struct tee_optee_ta_uuid uuid = TA_FUSEIPQ_UUID;
	struct tee_open_session_arg session_arg;
	struct udevice *dev = NULL;
	struct tee_shm *shm_elf, *shm_dat;
	struct tee_invoke_arg arg;
	struct tee_param param[4];
	struct fuseipq_params *fuseipq_params = (struct fuseipq_params *)params;
	int ret = 0;

	dev = tee_find_device(dev, NULL, NULL, NULL);
	if (!dev) {
		printf("tee_find_session(): failed(%d)\n", ret);
		return -ENODEV;
	}

	memset(&session_arg, 0, sizeof(session_arg));
	tee_optee_ta_uuid_to_octets(session_arg.uuid, &uuid);

	ret = tee_open_session(dev, &session_arg, 0, NULL);
	if (ret) {
		printf("tee_open_session(): failed(%d)\n", ret);
		return ret;
	}

	memset(&arg, 0, sizeof(arg));
	memset(param, 0, sizeof(param));

	arg.func = PTA_CMD_QFPROM_BLOW_SECELF;
	arg.session = session_arg.session;

	ret = tee_shm_register(dev, (void *)(ulong)fuseipq_params->addr,
			       fuseipq_params->meta_data_size, 0x0, &shm_elf);
	if (ret < 0) {
		printf("Cannot register input elf memory 0x%X\n", ret);
		goto error;
	}

	param[0].u.memref.shm	= shm_elf;
	param[0].u.memref.size	= shm_elf->size;
	param[0].attr		= TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;

	/* Use 64-bit struct buffer for OPTEE */
	ret = tee_shm_register(dev, (void *)(ulong)fuseipq_params->load_seg_buff_64,
			       fuseipq_params->load_seg_cnt_64, 0x0, &shm_dat);
	if (ret < 0) {
		printf("Cannot register input dat memory 0x%X\n", ret);
		tee_shm_free(shm_elf);
		goto error;
	}

	param[1].u.memref.shm	= shm_dat;
	param[1].u.memref.size	= shm_dat->size;
	param[1].attr		= TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;

	param[2].attr = TEE_PARAM_ATTR_TYPE_VALUE_OUTPUT;
	param[3].attr = TEE_PARAM_ATTR_TYPE_NONE;

	ret = tee_invoke_func(dev, &arg, 4, param);
	if (ret) {
		printf("tee_invoke_func(): failed(%d)\n", ret);
		tee_shm_free(shm_elf);
		tee_shm_free(shm_dat);
		goto error;
	}

	fuseipq_params->fuse_status = param[2].u.value.a;
	tee_shm_free(shm_elf);
	tee_shm_free(shm_dat);
error:
	ret = tee_close_session(dev, arg.session);
	if (ret < 0)
		printf("tee_close_session(): failed(%d)\n", ret);

	return ret;
}

#if IS_ENABLED(CONFIG_IPQ_INLINE_ENCRYPTION)
int ipq_ice_configure_optee_impl(void *params)
{
	const struct tee_optee_ta_uuid uuid = ICE_TA_UUID;
	struct tee_open_session_arg session_arg;
	struct udevice *dev = NULL;
	struct tee_shm *hwkey_shm;
	struct tee_invoke_arg arg;
	struct tee_param tee_param[4];
	struct ice_hw_key_config *hwkey_config;
	struct ice_configure_params *ice_params =
		(struct ice_configure_params *)params;
	struct ice_config_sec *ice = ice_params->ice;
	int ret = 0;

	dev = tee_find_device(dev, NULL, NULL, NULL);
	if (!dev) {
		printf("tee_find_device(): failed\n");
		return -ENODEV;
	}

	memset(&session_arg, 0, sizeof(session_arg));
	tee_optee_ta_uuid_to_octets(session_arg.uuid, &uuid);

	ret = tee_open_session(dev, &session_arg, 0, NULL);
	if (ret) {
		printf("tee_open_session(): failed(%d)\n", ret);
		return ret;
	}

	ret = tee_shm_alloc(dev, sizeof(*hwkey_config),
			    TEE_SHM_ALLOC, &hwkey_shm);
	if (ret) {
		printf("Failed to allocate shared memory for HW key config\n");
		goto close_session;
	}

	hwkey_config = (struct ice_hw_key_config *)hwkey_shm->addr;
	memset(hwkey_config, 0, sizeof(*hwkey_config));

	hwkey_config->index = ice->index;
	hwkey_config->key_size = ice->key_size;
	hwkey_config->algo_mode = ice->algo_mode;
	hwkey_config->key_mode = ice->key_mode;

	memset(&arg, 0, sizeof(arg));
	memset(tee_param, 0, sizeof(tee_param));

	arg.func = ICE_TA_CMD_CONFIG_HW_KEY;
	arg.session = session_arg.session;

	tee_param[0].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;
	tee_param[0].u.memref.shm = hwkey_shm;
	tee_param[0].u.memref.size = sizeof(*hwkey_config);
	tee_param[0].u.memref.shm_offs = 0;

	ret = tee_invoke_func(dev, &arg, 1, tee_param);
	if (ret < 0 || arg.ret != TEEC_SUCCESS) {
		printf("Config HW key failed, ret=0x%x\n", arg.ret);
		ret = -EINVAL;
	} else {
		printf("HW key configured successfully via OP-TEE\n");
		ret = 0;
	}

	tee_shm_free(hwkey_shm);

close_session:
	tee_close_session(dev, arg.session);

	return ret;
}

int ipq_ice_key_configure_optee_impl(void *params)
{
	const struct tee_optee_ta_uuid uuid = ICE_TA_UUID;
	struct tee_open_session_arg session_arg;
	struct udevice *dev = NULL;
	struct tee_shm *config_shm, *data_shm = NULL, *salt_shm = NULL;
	struct tee_invoke_arg arg;
	struct tee_param tee_param[4];
	struct ice_context_config *config;
	struct ice_key_configure_params *key_params =
		(struct ice_key_configure_params *)params;
	int ret = 0;

	dev = tee_find_device(dev, NULL, NULL, NULL);
	if (!dev) {
		printf("tee_find_device(): failed\n");
		return -ENODEV;
	}

	memset(&session_arg, 0, sizeof(session_arg));
	tee_optee_ta_uuid_to_octets(session_arg.uuid, &uuid);

	ret = tee_open_session(dev, &session_arg, 0, NULL);
	if (ret) {
		printf("tee_open_session(): failed(%d)\n", ret);
		return ret;
	}

	ret = tee_shm_alloc(dev, sizeof(*config), TEE_SHM_ALLOC, &config_shm);
	if (ret) {
		printf("Failed to allocate shared memory for context config\n");
		goto close_session;
	}

	config = (struct ice_context_config *)config_shm->addr;
	memset(config, 0, sizeof(*config));
	config->seed_type = key_params->seedtype;
	config->key_size = key_params->key_size;
	config->algo_mode = key_params->algo_mode;

	memset(&arg, 0, sizeof(arg));
	memset(tee_param, 0, sizeof(tee_param));

	arg.func = ICE_TA_CMD_SET_CONTEXT;
	arg.session = session_arg.session;

	tee_param[0].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;
	tee_param[0].u.memref.shm = config_shm;
	tee_param[0].u.memref.size = sizeof(*config);
	tee_param[0].u.memref.shm_offs = 0;
	if (key_params->seedtype == OEM_SEED_TYPE &&
	    key_params->hex_data_context && key_params->hex_data_len > 0) {
		ret = tee_shm_alloc(dev, key_params->hex_data_len,
				    TEE_SHM_ALLOC, &data_shm);
		if (ret) {
			printf("Failed to allocate shared memory for data context\n");
			goto free_config_shm;
		}

		memcpy((void *)data_shm->addr, key_params->hex_data_context,
		       key_params->hex_data_len);

		tee_param[1].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;
		tee_param[1].u.memref.shm = data_shm;
		tee_param[1].u.memref.size = key_params->hex_data_len;
		tee_param[1].u.memref.shm_offs = 0;
		if (key_params->algo_mode == ICE_OPTEE_CRYPTO_ALGO_MODE_HW_AES_XTS &&
		    key_params->hex_salt_context && key_params->hex_salt_len > 0) {
			ret = tee_shm_alloc(dev, key_params->hex_salt_len,
					    TEE_SHM_ALLOC, &salt_shm);
			if (ret) {
				printf("Failed to allocate shared memory for salt context\n");
				goto free_data_shm;
			}

			memcpy((void *)salt_shm->addr, key_params->hex_salt_context,
			       key_params->hex_salt_len);

			tee_param[2].attr = TEE_PARAM_ATTR_TYPE_MEMREF_INPUT;
			tee_param[2].u.memref.shm = salt_shm;
			tee_param[2].u.memref.size = key_params->hex_salt_len;
			tee_param[2].u.memref.shm_offs = 0;
			ret = tee_invoke_func(dev, &arg, 3, tee_param);
		} else {
			ret = tee_invoke_func(dev, &arg, 2, tee_param);
		}
	} else {
		ret = tee_invoke_func(dev, &arg, 1, tee_param);
	}

	if (ret < 0 || arg.ret != TEEC_SUCCESS) {
		printf("Set context failed, ret=0x%x\n", arg.ret);
		ret = -EINVAL;
		goto cleanup;
	}

	printf("Context set successfully via OP-TEE\n");
	ret = 0;

cleanup:
	if (salt_shm)
		tee_shm_free(salt_shm);
free_data_shm:
	if (data_shm)
		tee_shm_free(data_shm);
free_config_shm:
	tee_shm_free(config_shm);
close_session:
	tee_close_session(dev, arg.session);

	return ret;
}
#endif /* CONFIG_IPQ_INLINE_ENCRYPTION */
