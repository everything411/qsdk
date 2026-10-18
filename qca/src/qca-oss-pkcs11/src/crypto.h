/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
#ifndef CRYPTO_H_
#define CRYPTO_H_

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include "cryptoki.h"

CK_RV qti_pkcs11_slot_init(CK_SLOT_ID slotID);

struct ecdsa_sign {
	unsigned int key_handle;
	void *data;
	unsigned int data_len;
	void *signature;
	unsigned int signature_out_len;
	unsigned int result;
};

struct ecdsa_verify {
        unsigned int key_handle;
        void *data;
        unsigned int data_len;
        void *signature;
        unsigned int signature_len;
	unsigned int result;
};

extern const char *qseecom_device;

#define MAX_ECC_PUB_KEY_LEN		133
struct ecdsa_points {
	unsigned char public_key[MAX_ECC_PUB_KEY_LEN];
	unsigned int public_key_len;
};

#define QSEECOM_ECDSA_SIGN              _IOWR('Q', 7, struct ecdsa_sign)
#define QSEECOM_ECDSA_VERIFY            _IOWR('Q', 8, struct ecdsa_verify)
#define QSEECOM_GET_EC_POINTS		_IOWR('Q', 9, struct ecdsa_points)

CK_RV pkcs11_slot_init(CK_SLOT_ID slotID);
CK_RV qti_pkcs11_signature_sign_init(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
				     CK_OBJECT_HANDLE hKey);
CK_RV qti_pkcs11_signature_sign(CK_SESSION_HANDLE hSession, CK_BYTE_PTR pData,
				CK_ULONG ulDataLen, CK_BYTE_PTR pSignature,
				CK_ULONG_PTR pulSignatureLen);
#endif
