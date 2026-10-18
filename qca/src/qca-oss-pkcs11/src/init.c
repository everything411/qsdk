/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "init.h"
#include <stdio.h>
#include <string.h>

/* static context for now */
static pkcs11_lib_ctx pkcs11_context;
static pkcs11_slot_ctx pkcs11_slot_context;

const char pkcs11_lib_manufacturer_id[] = "QCOM";
const char pkcs11_lib_model[] = "QCOM";
const char pkcs11_lib_serialnumber[] = "QCOM";
const char pkcs11_lib_label[] = "QCOM";

const char pkcs11_lib_description[] = "TA Crypto pkcs11 interface";

#define TA_VER_MAJOR 1
#define TA_VER_MINOR 0

/* Single default active slot/token */
#define PKCS11_MAX_SLOTS_ALLOWED 1

/**
 * \brief Retrieve the current library context
 */
pkcs11_lib_ctx_ptr qti_pkcs11_get_context(void)
{
    return &pkcs11_context;
}

qti_pkcs11_slot_ctx_ptr qti_pkcs11_slot_get_context(pkcs11_lib_ctx_ptr lib_ctx,
					    CK_SLOT_ID slotID)
{
   return &pkcs11_slot_context;
}

void qti_pkcs11_util_escape_string(CK_UTF8CHAR_PTR buf, CK_ULONG buf_len)
{
    if (buf && buf_len)
    {
        CK_ULONG i;
        for (i = 0; i < buf_len; i++)
        {
            if (0x20 > buf[i] || 0x7E < buf[i])
            {
                buf[i] = ' ';
            }
        }
    }
}

/**
 * \brief Obtains general information about Cryptoki
 */
CK_RV qti_pkcs11_get_lib_info(CK_INFO_PTR pInfo)
{
    pkcs11_lib_ctx_ptr ctx = qti_pkcs11_get_context();

    if (!ctx || !ctx->initialized)
    {
        return CKR_CRYPTOKI_NOT_INITIALIZED;
    }

    if (!pInfo)
    {
        return CKR_ARGUMENTS_BAD;
    }

    /* Must be zero for 3.0 */
    pInfo->flags = 0;

    /* PKCS11 Specification Version */
    pInfo->cryptokiVersion.major = CRYPTOKI_VERSION_MAJOR;
    pInfo->cryptokiVersion.minor = CRYPTOKI_VERSION_MINOR;

    /* Cryptoauthlib Version */
    pInfo->libraryVersion.major = TA_VER_MAJOR;
    pInfo->libraryVersion.minor = TA_VER_MINOR;

    /* Set up the identifier strings */
    snprintf((char*)pInfo->manufacturerID, sizeof(pInfo->manufacturerID), pkcs11_lib_manufacturer_id);
    snprintf((char*)pInfo->libraryDescription, sizeof(pInfo->libraryDescription), pkcs11_lib_description);

    qti_pkcs11_util_escape_string(pInfo->manufacturerID, sizeof(pInfo->manufacturerID));
    qti_pkcs11_util_escape_string(pInfo->libraryDescription, sizeof(pInfo->libraryDescription));

    printf("%s returning OK \n", __func__);
    return CKR_OK;
}

/*
 *  Initialise pkcs11 lib
 *   -> No support for Multi threading/locking
 *   -> Single Slot/Token, active by default
 */
CK_RV qti_pkcs11_init(CK_C_INITIALIZE_ARGS_PTR pInitArgs)
{
	CK_BBOOL allset = FALSE;
	CK_BBOOL allunset = FALSE;

	pkcs11_lib_ctx_ptr lib_ctx = qti_pkcs11_get_context();
	qti_pkcs11_slot_ctx_ptr slot_ctx;

	if (lib_ctx->initialized)
		return CKR_CRYPTOKI_ALREADY_INITIALIZED;

	if (pInitArgs) {

	         allset = (pInitArgs->CreateMutex && pInitArgs->DestroyMutex && pInitArgs->LockMutex && pInitArgs->UnlockMutex);
        	 allunset = (!pInitArgs->CreateMutex && !pInitArgs->DestroyMutex && !pInitArgs->LockMutex && !pInitArgs->UnlockMutex);

		/* If Locking args are partially set, return arguments are bad */
		if ((!allset && !allunset) || pInitArgs->pReserved)
			return CKR_ARGUMENTS_BAD;

		/* Locking is not supported, only single threaded */
		if (allset)
			return CKR_CANT_LOCK;
	}

	/*
	 * Slot/Token is the TA residing in the secure world,
	 * controlled by the TZ. TA is loaded and init during
         * boot. So no init is required from pkcs lib.
	 * Just return the static data.
	 */
	if (!qti_pkcs11_slot_init(0)) {
		lib_ctx->slot_cnt = PKCS11_MAX_SLOTS_ALLOWED;
		lib_ctx->initialized = TRUE; 

	        slot_ctx = qti_pkcs11_slot_get_context(lib_ctx, 0);
		slot_ctx->initialized = TRUE;
	}

	return CKR_OK;
}

/* Close the library */
CK_RV qti_pkcs11_deinit(CK_VOID_PTR pReserved)
{
    pkcs11_lib_ctx_ptr lib_ctx = qti_pkcs11_get_context();

    if (pReserved)
    {
        return CKR_ARGUMENTS_BAD;
    }

    if (!lib_ctx->initialized)
    {
        return CKR_CRYPTOKI_NOT_INITIALIZED;
    }

    lib_ctx->initialized = FALSE;

    return CKR_OK;
}

CK_RV qti_pkcs11_slot_get_list(CK_BBOOL tokenPresent, CK_SLOT_ID_PTR pSlotList, CK_ULONG_PTR pulCount)
{
    pkcs11_lib_ctx_ptr lib_ctx = qti_pkcs11_get_context();
    CK_ULONG slot_cnt = 0;

    if (!lib_ctx)
    {
        return CKR_CRYPTOKI_NOT_INITIALIZED;
    }

    if (!pulCount)
    {
        return CKR_ARGUMENTS_BAD;
    }

    /* Single active slot/token by default */
    slot_cnt = lib_ctx->slot_cnt;

    if (pSlotList)
    {
        if (slot_cnt > *pulCount)
        {
            *pulCount = slot_cnt;
            return CKR_BUFFER_TOO_SMALL;
        }
        else
        {
	    pSlotList[0] = 0;
        }
    }

    *pulCount = slot_cnt;

    return CKR_OK;
}

/**
 * \brief Obtains information about a particular slot
 */
CK_RV qti_pkcs11_slot_get_info(CK_SLOT_ID slotID, CK_SLOT_INFO_PTR pInfo)
{
    pkcs11_lib_ctx_ptr lib_ctx = qti_pkcs11_get_context();
    qti_pkcs11_slot_ctx_ptr slot_ctx;

    if (!lib_ctx || !lib_ctx->initialized)
    {
        return CKR_CRYPTOKI_NOT_INITIALIZED;
    }

    if (!pInfo)
    {
        return CKR_ARGUMENTS_BAD;
    }

    /* Initialize the input structure */
    memset(pInfo, 0, sizeof(CK_SLOT_INFO));

    slot_ctx = qti_pkcs11_slot_get_context(lib_ctx, slotID);

    if (!slot_ctx)
    {
        return CKR_SLOT_ID_INVALID;
    }

    /* Default version information */
    pInfo->hardwareVersion.major = (CK_BYTE)CRYPTOKI_VERSION_MAJOR;
    pInfo->hardwareVersion.minor = (CK_BYTE)CRYPTOKI_VERSION_MINOR;
    pInfo->firmwareVersion.major = (CK_BYTE)TA_VER_MAJOR;
    pInfo->firmwareVersion.minor = (CK_BYTE)TA_VER_MINOR;

    /* Set up the flags - Single default slot with Token active by default */
    pInfo->flags = CKF_HW_SLOT | CKF_TOKEN_PRESENT;

    snprintf((char*)pInfo->slotDescription, sizeof(pInfo->slotDescription), "%d", (int)slotID);

    /* Use the same manufacturer ID we use throughout */
    snprintf((char*)pInfo->manufacturerID, sizeof(pInfo->manufacturerID), "%s", pkcs11_lib_manufacturer_id);

    return CKR_OK;
}

/**
 * \brief Obtains information about a particular token
 */
CK_RV qti_pkcs11_token_get_info(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR pInfo)
{
    pkcs11_lib_ctx_ptr lib_ctx = qti_pkcs11_get_context();
    qti_pkcs11_slot_ctx_ptr slot_ctx;

    if (!lib_ctx || !lib_ctx->initialized)
    {
        return CKR_CRYPTOKI_NOT_INITIALIZED;
    }

    if (!pInfo)
    {
        return CKR_ARGUMENTS_BAD;
    }

    /* Initialize the target structure */
    memset(pInfo, 0, sizeof(CK_TOKEN_INFO));

    slot_ctx = qti_pkcs11_slot_get_context(lib_ctx, slotID);

    if (!slot_ctx)
    {
        return CKR_SLOT_ID_INVALID;
    }

    /* Default Info Fields */
    pInfo->hardwareVersion.major = (CK_BYTE)CRYPTOKI_VERSION_MAJOR;
    pInfo->hardwareVersion.minor = (CK_BYTE)CRYPTOKI_VERSION_MINOR;
    pInfo->firmwareVersion.major = (CK_BYTE)TA_VER_MAJOR;
    pInfo->firmwareVersion.minor = (CK_BYTE)TA_VER_MINOR;
    pInfo->ulTotalPublicMemory = CK_UNAVAILABLE_INFORMATION;
    pInfo->ulFreePublicMemory = CK_UNAVAILABLE_INFORMATION;
    pInfo->ulTotalPrivateMemory = CK_UNAVAILABLE_INFORMATION;
    pInfo->ulFreePrivateMemory = CK_UNAVAILABLE_INFORMATION;
    pInfo->ulMaxPinLen = 0;
    pInfo->ulMinPinLen = 0;
    pInfo->flags = CKF_RNG | CKF_USER_PIN_INITIALIZED | CKF_TOKEN_INITIALIZED;
    pInfo->ulMaxSessionCount = 1;
    pInfo->ulMaxRwSessionCount = 1;

    pInfo->ulSessionCount = (slot_ctx->session) ? TRUE : FALSE;
    pInfo->ulRwSessionCount = (slot_ctx->session) ? TRUE : FALSE;

    /* Use the same manufacturer ID we use throughout */
//    snprintf((char*)pInfo->manufacturerID, sizeof(pInfo->manufacturerID), "%s", pkcs11_lib_manufacturer_id);
  //  snprintf((char*)pInfo->model, sizeof(pInfo->model), "%s", pkcs11_lib_model);
    snprintf((char*)pInfo->label, sizeof(pInfo->label), "%s", pkcs11_lib_label);
    //snprintf((char*)pInfo->serialNumber, sizeof(pInfo->serialNumber), "%s", pkcs11_lib_serialnumber);

    return CKR_OK;
}

