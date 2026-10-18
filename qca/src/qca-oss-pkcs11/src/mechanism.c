/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <string.h>
#include "mechanism.h"
#include "init.h"
#include "debug.h"

typedef struct _qti_pcks11_mech_table_e
{
    CK_MECHANISM_TYPE type;
    CK_MECHANISM_INFO info;
} qti_pcks11_mech_table_e, *qti_pcks11_mech_table_ptr;

#define PCKS11_MECH_ECC508_EC_CAPABILITY   (CKF_EC_F_P | CKF_EC_NAMEDCURVE | CKF_EC_UNCOMPRESS)

/* CK_MECHANISM_TYPE, MinKeySize, MaxKeySize, Flags */
static qti_pcks11_mech_table_e pkcs11_mech_list_Trusted_app[] = {
//    { CKM_EC_KEY_PAIR_GEN,{ 256, 256, CKF_HW | CKF_GENERATE | CKF_GENERATE_KEY_PAIR | PCKS11_MECH_ECC508_EC_CAPABILITY } },
    { CKM_ECDSA,{ 384, 521, CKF_HW | CKF_SIGN } },
    { CKM_ECDSA_SHA384, {384, 521, CKF_HW | CKF_SIGN } },
};

#define TABLE_SIZE(x)   sizeof(x) / sizeof(x[0])

static CK_MECHANISM_INFO_PTR qti_pkcs11_mech_find_info(qti_pcks11_mech_table_ptr ptrMechanismInfoList,
		                                       CK_ULONG count, CK_MECHANISM_TYPE type)
{
    CK_ULONG i;
    CK_MECHANISM_INFO_PTR rv = NULL_PTR;

    if (ptrMechanismInfoList)
    {
        for (i = 0; i < count; i++)
        {
            if (type == ptrMechanismInfoList[i].type)
            {
                rv = &ptrMechanismInfoList[i].info;
                break;
            }
        }
    }

    return rv;
}

static void qti_pkcs11_mech_fill_list(CK_MECHANISM_TYPE_PTR ptrMechanismList,
		                      qti_pcks11_mech_table_ptr ptrMechanismInfoList,
				      CK_ULONG count)
{
    CK_ULONG i;

    if (ptrMechanismList && ptrMechanismInfoList)
    {
        for (i = 0; i < count; i++)
        {
            ptrMechanismList[i] = ptrMechanismInfoList[i].type;
        }
    }
}

CK_RV qti_pkcs11_mech_get_list(CK_SLOT_ID slotID, CK_MECHANISM_TYPE_PTR ptrMechanismList,
		               CK_ULONG_PTR pulCount)
{
    pkcs11_lib_ctx_ptr lib_pctx = qti_pkcs11_get_context();
    qti_pcks11_mech_table_ptr mech_list_ptr = NULL_PTR;
    qti_pkcs11_slot_ctx_ptr slot_ctx;
    CK_ULONG mech_cnt = 0;

    if (!lib_pctx)
    {
        return CKR_CRYPTOKI_NOT_INITIALIZED;
    }

    /* C_GetMechanismList - Ref PKCS11 Sec 5.5 */
    if (!pulCount)
    {
        return CKR_ARGUMENTS_BAD;
    }

    /* Retreive the slot context */
    slot_ctx = qti_pkcs11_slot_get_context(lib_pctx, slotID);

    if (!slot_ctx)
    {
        return CKR_SLOT_ID_INVALID;
    }

    mech_list_ptr = pkcs11_mech_list_Trusted_app;
    mech_cnt = TABLE_SIZE(pkcs11_mech_list_Trusted_app);

    /* C_GetMechanismList Requirement #2 - Ref PKCS11 Sec 5.5 */
    if (ptrMechanismList)
    {
        if (mech_cnt > *pulCount)
        {
            *pulCount = mech_cnt;
            return CKR_BUFFER_TOO_SMALL;
        }
        else
        {
            qti_pkcs11_mech_fill_list(ptrMechanismList, mech_list_ptr, mech_cnt);
        }
    }

    *pulCount = mech_cnt;

    return CKR_OK;
}

CK_RV qti_pkcs_mech_get_info(CK_SLOT_ID slotID, CK_MECHANISM_TYPE type, CK_MECHANISM_INFO_PTR ptrInfo)
{
    pkcs11_lib_ctx_ptr lib_pctx = qti_pkcs11_get_context();
    CK_MECHANISM_INFO_PTR ptr_mech_info = NULL_PTR;
    qti_pkcs11_slot_ctx_ptr slot_ctx;

    if (!lib_pctx || !lib_pctx->initialized)
    {
        return CKR_CRYPTOKI_NOT_INITIALIZED;
    }

    if (!ptrInfo)
    {
        return CKR_ARGUMENTS_BAD;
    }

    /* Retreive the slot context */
    slot_ctx = qti_pkcs11_slot_get_context(lib_pctx, slotID);
    if (!slot_ctx)
    {
        return CKR_SLOT_ID_INVALID;
    }

    ptr_mech_info = qti_pkcs11_mech_find_info(pkcs11_mech_list_Trusted_app,
		                              TABLE_SIZE(pkcs11_mech_list_Trusted_app),
					      type);
    if (!ptr_mech_info)
    {
        return CKR_MECHANISM_INVALID;
    }

    memcpy(ptrInfo, ptr_mech_info, sizeof(CK_MECHANISM_INFO));

    return CKR_OK;
}
