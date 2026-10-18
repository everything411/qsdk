/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef INIT_H_
#define INIT_H_

#include "cryptoki.h"
#include "crypto.h"

/** Library Context */
typedef struct _pkcs11_lib_ctx
{
    CK_BBOOL        initialized;
    CK_VOID_PTR     slots;
    CK_ULONG        slot_cnt;
} pkcs11_lib_ctx, *pkcs11_lib_ctx_ptr;

/** Slot Context */
typedef struct _pkcs11_slot_ctx
{
    CK_BBOOL           initialized;
    CK_SLOT_ID         slot_id;
    CK_SESSION_HANDLE  session;
    CK_FLAGS           flags;
} pkcs11_slot_ctx, *qti_pkcs11_slot_ctx_ptr;

/* init functions */
pkcs11_lib_ctx_ptr qti_pkcs11_get_context(void);
CK_RV qti_pkcs11_init(CK_C_INITIALIZE_ARGS_PTR pInitArgs);
CK_RV qti_pkcs11_deinit(CK_VOID_PTR pReserved);
CK_RV qti_pkcs11_get_lib_info(CK_INFO_PTR pInfo);

/* slot/token functions */
qti_pkcs11_slot_ctx_ptr qti_pkcs11_slot_get_context(pkcs11_lib_ctx_ptr lib_ctx, CK_SLOT_ID slotID);

CK_RV qti_pkcs11_slot_get_list(CK_BBOOL tokenPresent, CK_SLOT_ID_PTR pSlotList, CK_ULONG_PTR pulCount);
CK_RV qti_pkcs11_slot_get_info(CK_SLOT_ID slotID, CK_SLOT_INFO_PTR pInfo);
CK_RV qti_pkcs11_token_init(CK_SLOT_ID slotID, CK_UTF8CHAR_PTR pPin, CK_ULONG ulPinLen, CK_UTF8CHAR_PTR pLabel);
CK_RV qti_pkcs11_token_get_info(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR pInfo);

#endif
