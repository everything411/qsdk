/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>

#define ESUCCESS	0
#define EINVAL		1
#define EACCES		2
#define ENOMEM		3
#define ENOSPC		4
#define EIO		5

#define ELIBBAD		6
#define ELIBEXEC	7
#define EOPNOTSUPP	8

#define MAX_DIGEST_SIZE		64

const char *qseecom_device = "/dev/qseecom_device";

struct ecdsa_import_key {
        void *ecdsa_blob;
        uint32_t ecdsa_blob_len;
        uint32_t key_handle;
	uint32_t result;
};

struct ecdsa_sign {
	uint32_t key_handle;
	void *data;
	uint32_t data_len;
	void *signature;
	uint32_t signature_out_len;
	uint32_t result;
};

struct ecdsa_verify {
	uint32_t key_handle;
	void *data;
	uint32_t data_len;
	void *signature;
	uint32_t signature_len;
	uint32_t result;
};

#define FILE_PATH_MAX           100

struct ta_info {
	char mdt_file[FILE_PATH_MAX];
	char seg_file[FILE_PATH_MAX];
};

#define QSEECOM_LOAD_TA_LIB             _IOWR('Q', 1, struct ta_info)
#define QSEECOM_LOAD_TA_APP             _IOWR('Q', 2, struct ta_info)
#define QSEECOM_UNLOAD_TA_LIB		_IO('Q', 3)
#define QSEECOM_UNLOAD_TA_APP           _IO('Q', 4)
#define QSEECOM_ECDSA_IMPORT_KEY	_IOWR('Q', 5, struct ecdsa_import_key)
#define QSEECOM_SET_KEY_HANDLE		_IOWR('Q', 6, uint32_t)
#define QSEECOM_ECDSA_SIGN		_IOWR('Q', 7, struct ecdsa_sign)
#define QSEECOM_ECDSA_VERIFY		_IOWR('Q', 8, struct ecdsa_verify)

