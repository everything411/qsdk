/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include "session.h"
#include "init.h"
#include <string.h>
#include <stdio.h>

typedef struct pkcs11_template_st {
        unsigned long allocated;
        unsigned int nattr;
        CK_ATTRIBUTE attrs[32];
} PKCS11_TEMPLATE;


unsigned int qti_pkcs11_addattr(PKCS11_TEMPLATE *tmpl, int type, void *data, size_t size)
{
        unsigned int n = tmpl->nattr;
        CK_ATTRIBUTE_PTR ap;

        ap = &tmpl->attrs[tmpl->nattr++];
        ap->type = type;
        ap->pValue = data;
        ap->ulValueLen = (CK_ULONG)size;
        return n;
}

void qti_pkcs11_addattr_bool(PKCS11_TEMPLATE *tmpl, int type, int value)
{
        static CK_BBOOL _true = CK_TRUE;
        static CK_BBOOL _false = CK_FALSE;
        qti_pkcs11_addattr(tmpl, type, value ? &_true : &_false, sizeof(CK_BBOOL));
}

void qti_pkcs11_addattr_s(PKCS11_TEMPLATE *tmpl, int type, const char *s)
{
        qti_pkcs11_addattr(tmpl, type, (void *)s, s ? strlen(s) : 0);
}

//#define pkcs11_addattr_var(_tmpl, _type, _var) pkcs11_addattr(_tmpl, _type, &(_var), sizeof(_var))

/* Commenting the dummy key support
static void qti_pkcs11_generate_key_dummy(CK_SESSION_HANDLE session)
{

        PKCS11_TEMPLATE tmpl = {0};
        CK_OBJECT_HANDLE object;
        CK_KEY_TYPE key_type_ECDSA = CKK_ECDSA;
	unsigned long class_type = CKO_PRIVATE_KEY;
	char *label;
        unsigned char *id;
        size_t id_len;

	label = "PRIVATE";
	id = "0";
	id_len = sizeof(id);

//        qti_pkcs11_addattr_var(&tmpl, CKA_CLASS, CKO_PRIVATE_KEY);

	qti_pkcs11_addattr(&tmpl, CKA_CLASS, &class_type,sizeof(class_type));

        if (label)
                qti_pkcs11_addattr_s(&tmpl, CKA_LABEL, label);

        if (id && id_len)
                qti_pkcs11_addattr(&tmpl, CKA_ID, id, id_len);

        qti_pkcs11_addattr_bool(&tmpl, CKA_TOKEN, TRUE);
        qti_pkcs11_addattr_bool(&tmpl, CKA_PRIVATE, TRUE);
        qti_pkcs11_addattr_bool(&tmpl, CKA_SENSITIVE, TRUE);
        qti_pkcs11_addattr_bool(&tmpl, CKA_DECRYPT, TRUE);
        qti_pkcs11_addattr_bool(&tmpl, CKA_SIGN, TRUE);
        qti_pkcs11_addattr_bool(&tmpl, CKA_UNWRAP, TRUE);
        qti_pkcs11_addattr(&tmpl, CKA_KEY_TYPE, &key_type_ECDSA, sizeof(key_type_ECDSA));
//        qti_pkcs11_addattr_var(&tmpl,  CKA_KEY_TYPE, key_type_ECDSA);

        C_CreateObject(session, tmpl.attrs, tmpl.nattr, &object);
} */

static qti_pkcs11_session_ctx pkcs11_session_cache[QTI_PKCS11_MAX_SESSIONS_ALLOWED];

static qti_pkcs11_session_ctx_ptr qti_pkcs11_allocate_session_context(void)
{
    qti_pkcs11_session_ctx_ptr rv = NULL_PTR;
    CK_ULONG i;

    for (i = 0; i < QTI_PKCS11_MAX_SESSIONS_ALLOWED; i++)
    {
        if (!pkcs11_session_cache[i].initialized)
        {
            rv = &pkcs11_session_cache[i];
            break;
        }
    }

    return rv;
}

qti_pkcs11_session_ctx_ptr qti_pkcs11_get_session_context(CK_SESSION_HANDLE hSession)
{
    return (qti_pkcs11_session_ctx_ptr)hSession;
}

/* Check if the session is initialized properly */
CK_RV qti_pkcs11_session_check(qti_pkcs11_session_ctx_ptr * ptrSession, CK_SESSION_HANDLE hSession)
{
    CK_RV rv = CKR_SESSION_HANDLE_INVALID;
    qti_pkcs11_session_ctx_ptr pctx = qti_pkcs11_get_session_context(hSession);

    if (pctx)
    {
        if (!pctx->initialized)
        {
	    rv = CKR_SESSION_CLOSED;
        }
        else
        {
	    rv = CKR_OK;
        }
    }

    if (ptrSession)
    {
        if (!rv)
        {
	    *ptrSession = pctx;
        }
        else
        {
	    *ptrSession = NULL_PTR;
        }
    }
    return rv;
}

CK_RV qti_pkcs11_session_open(CK_SLOT_ID slotID, CK_FLAGS flags,
		              CK_VOID_PTR ptrApplication, CK_NOTIFY notify,
			      CK_SESSION_HANDLE_PTR phSession)
{
    qti_pkcs11_slot_ctx_ptr slot_ctx;
    qti_pkcs11_session_ctx_ptr session_pctx;
    pkcs11_lib_ctx_ptr lib_pctx = qti_pkcs11_get_context();

    ((void)ptrApplication);
    ((void)notify);

    if (!lib_pctx || !lib_pctx->initialized)
    {
        return CKR_CRYPTOKI_NOT_INITIALIZED;
    }

    /* PKCS11 Sec 5.6.1, legacy reaons */
    if (!(flags & CKF_SERIAL_SESSION))
    {
        return CKR_SESSION_PARALLEL_NOT_SUPPORTED;
    }

    if (!phSession)
    {
        return CKR_ARGUMENTS_BAD;
    }

    slot_ctx = qti_pkcs11_slot_get_context(lib_pctx, slotID);

    if (!slot_ctx)
    {
        return CKR_SLOT_ID_INVALID;
    }

    if (!slot_ctx->initialized)
    {
        return CKR_TOKEN_NOT_RECOGNIZED;
    }

    /* New session context */
    session_pctx = qti_pkcs11_allocate_session_context();

    /* Check whether the session was created */
    if (!session_pctx)
    {
        return CKR_HOST_MEMORY;
    }

    /* Initialize */
    session_pctx->initialized = TRUE;
    session_pctx->slot = slot_ctx;

    /* Assign the handle */
    session_pctx->handle = (CK_SESSION_HANDLE)session_pctx;
    *phSession = session_pctx->handle;

    /* Generate dummy private key for this session */
//  qti_pkcs11_generate_key_dummy((CK_SESSION_HANDLE)session_pctx);
    qti_pkcs11_config_load_objects(slot_ctx);

    return CKR_OK;
}

CK_RV qti_pkcs11_session_close(CK_SESSION_HANDLE hSession)
{
    qti_pkcs11_session_ctx_ptr session_pctx = qti_pkcs11_get_session_context(hSession);
    pkcs11_lib_ctx_ptr lib_pctx = qti_pkcs11_get_context();
    qti_pkcs11_slot_ctx_ptr slot_ctx;

    if (!lib_pctx || !lib_pctx->initialized)
    {
        return CKR_CRYPTOKI_NOT_INITIALIZED;
    }

    if (!session_pctx)
    {
        return CKR_SESSION_HANDLE_INVALID;
    }

    if (!session_pctx->initialized)
    {
        return CKR_SESSION_CLOSED;
    }

    /* Slot get */
    slot_ctx = (qti_pkcs11_slot_ctx_ptr)session_pctx->slot;

    if (slot_ctx)
    {
        /* Clear the objects with this session if needed */
    }

    /* Session free */
    memset(session_pctx, 0, sizeof(qti_pkcs11_session_ctx));

    return CKR_OK;
}

/* Close all sessions for a given slot - not actually all open sessions */
CK_RV qti_pkcs11_session_closeall(CK_SLOT_ID slotID)
{
    int i;
    qti_pkcs11_slot_ctx_ptr slot_ctx;
    pkcs11_lib_ctx_ptr lib_pctx = qti_pkcs11_get_context();

    if (!lib_pctx || !lib_pctx->initialized)
    {
        return CKR_CRYPTOKI_NOT_INITIALIZED;
    }

    slot_ctx = qti_pkcs11_slot_get_context(lib_pctx, slotID);

    if (!slot_ctx)
    {
        return CKR_SLOT_ID_INVALID;
    }

    /* If multiple sessions open for a slot are supported, then loop over the
     * sessions and close them in order */
    for (i = 0; i < QTI_PKCS11_MAX_SESSIONS_ALLOWED; i++)
    {
        qti_pkcs11_session_close(pkcs11_session_cache[i].handle);
    }

    return CKR_OK;
}

/* Obtains information about a particular session*/
CK_RV qti_pkcs11_session_get_info(CK_SESSION_HANDLE hSession, CK_SESSION_INFO_PTR ptrInfo)
{
    qti_pkcs11_session_ctx_ptr session_pctx = qti_pkcs11_get_session_context(hSession);
    pkcs11_lib_ctx_ptr lib_pctx = qti_pkcs11_get_context();
    qti_pkcs11_slot_ctx_ptr slot_ctx;

    if (!lib_pctx || !lib_pctx->initialized)
    {
        return CKR_CRYPTOKI_NOT_INITIALIZED;
    }

    if (!ptrInfo)
    {
        return CKR_ARGUMENTS_BAD;
    }

    if (!session_pctx)
    {
        return CKR_SESSION_HANDLE_INVALID;
    }

    if (!session_pctx->initialized)
    {
        return CKR_SESSION_CLOSED;
    }

    ptrInfo->ulDeviceError = session_pctx->error;
    ptrInfo->state = session_pctx->state;
    slot_ctx = session_pctx->slot;

    if (slot_ctx)
    {
        ptrInfo->slotID = slot_ctx->slot_id;
    }

    ptrInfo->flags = CKF_RW_SESSION | CKF_SERIAL_SESSION;

    return CKR_OK;
}

CK_RV qti_pkcs11_session_login(CK_SESSION_HANDLE hSession, CK_USER_TYPE userType,
		               CK_UTF8CHAR_PTR prtPin, CK_ULONG ulPinLen)
{
    qti_pkcs11_session_ctx_ptr session_pctx = qti_pkcs11_get_session_context(hSession);
    pkcs11_lib_ctx_ptr lib_pctx = qti_pkcs11_get_context();

    if (!prtPin || !ulPinLen)
    {
        return CKR_ARGUMENTS_BAD;
    }

    if (!lib_pctx || !lib_pctx->initialized)
    {
        return CKR_CRYPTOKI_NOT_INITIALIZED;
    }

    if (!session_pctx)
    {
        return CKR_SESSION_HANDLE_INVALID;
    }

    if (!session_pctx->initialized)
    {
        return CKR_SESSION_CLOSED;
    }

    /* We do not really verify credentials, just return success */
    return CKR_OK;
}
