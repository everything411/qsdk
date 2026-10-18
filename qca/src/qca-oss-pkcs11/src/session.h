/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef SESSION_H_
#define SESSION_H_

#include "cryptoki.h"
#include "init.h"
#include "object.h"

#define QTI_PKCS11_MAX_SESSIONS_ALLOWED 10

/** Session Context */
typedef struct _pkcs11_session_ctx
{
    CK_BBOOL            initialized;
    qti_pkcs11_slot_ctx_ptr slot;
    CK_SESSION_HANDLE   handle;
    CK_STATE            state;
    CK_ULONG            error;
    CK_ATTRIBUTE_PTR    attrib_list;
    CK_ULONG            attrib_count;
    CK_ULONG            object_index;
    CK_ULONG            object_count;
    CK_OBJECT_HANDLE    active_object;
} qti_pkcs11_session_ctx, *qti_pkcs11_session_ctx_ptr;

CK_RV qti_pkcs11_session_check(qti_pkcs11_session_ctx_ptr * pSession, CK_SESSION_HANDLE hSession);
CK_RV qti_pkcs11_session_login(CK_SESSION_HANDLE hSession, CK_USER_TYPE userType, CK_UTF8CHAR_PTR pPin, CK_ULONG ulPinLen);
CK_RV qti_pkcs11_session_get_info(CK_SESSION_HANDLE hSession, CK_SESSION_INFO_PTR pInfo);
CK_RV qti_pkcs11_session_open(CK_SLOT_ID slotID, CK_FLAGS flags, CK_VOID_PTR pApplication, CK_NOTIFY notify, CK_SESSION_HANDLE_PTR phSession);
CK_RV qti_pkcs11_session_close(CK_SESSION_HANDLE hSession);
CK_RV qti_pkcs11_session_closeall(CK_SLOT_ID slotID);

#endif /* SESSION_H_ */
