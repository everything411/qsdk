/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "cryptoki.h"
#include "debug.h"
#include "init.h"
#include "session.h"
#include "object.h"
#include "crypto.h"

static CK_BYTE qti_pkcs11_find_template_cache[QTI_PKCS11_SEARCH_CACHE_SIZE];
static const CK_BBOOL cbFalse = CK_FALSE;
static const CK_BBOOL cbTrue = CK_TRUE;

CK_RV qti_pkcs11_init_check(pkcs11_lib_ctx_ptr * ppContext, CK_BBOOL lock)
{
    pkcs11_lib_ctx_ptr ctx = qti_pkcs11_get_context();

    if (!ctx && !ctx->initialized)
	return CKR_CRYPTOKI_NOT_INITIALIZED;
    else
        return CKR_OK;
}

static CK_RV qti_pkcs11_find_copy_template(CK_BYTE_PTR ptrBuffer, CK_ULONG ulBuf_Size, CK_ATTRIBUTE_PTR ptrTemplate, const CK_ULONG ulCount)
{
    CK_ULONG required;
    CK_ATTRIBUTE_PTR ptrCopy;
    CK_ULONG i;

    if (!ptrBuffer || !ulBuf_Size || !ptrTemplate || !ulCount)
    {
        return CKR_ARGUMENTS_BAD;
    }

    /* Start with the required memory to hold the Array */
    required = sizeof(CK_ATTRIBUTE) * ulCount;

    /* Check the pValue requirements */
    for (i = 0; i < ulCount; i++)
    {
        if (ptrTemplate[i].pValue && ptrTemplate[i].ulValueLen)
        {
            required += ptrTemplate[i].ulValueLen;

            if (required < ptrTemplate[i].ulValueLen)
            {
                return CKR_HOST_MEMORY;
            }
        }
    }

    if (ulBuf_Size < required)
    {
        return CKR_HOST_MEMORY;
    }

    /* Copy */
    required = sizeof(CK_ATTRIBUTE) * ulCount;

    /* Set up the Copied Template */
    ptrCopy = (CK_ATTRIBUTE_PTR)ptrBuffer;

    /* Move to the data section */
    ptrBuffer += required;

    /* Copy the template */
    for (i = 0; i < ulCount; i++)
    {
        ptrCopy[i].type = ptrTemplate[i].type;

        /* Sanity check the input to see if we need to allocate any memory for this */
        if (ptrTemplate[i].pValue && ptrTemplate[i].ulValueLen)
        {
            ptrCopy[i].pValue = ptrBuffer;
            ptrCopy[i].ulValueLen = ptrTemplate[i].ulValueLen;
            ptrBuffer += ptrTemplate[i].ulValueLen;
            memcpy(ptrCopy[i].pValue, ptrTemplate[i].pValue, ptrTemplate[i].ulValueLen);
        }
        else
        {
            ptrCopy[i].pValue = NULL_PTR;
            ptrCopy[i].ulValueLen = 0;
        }
    }

    return CKR_OK;
}

static qti_pkcs11_attrib_model_ptr qti_pkcs11_find_attrib(const qti_pkcs11_attrib_model_ptr ptrAttributeList, const CK_ULONG ulCount, const CK_ATTRIBUTE_PTR ptrTemplate)
{
    CK_ULONG i;
    qti_pkcs11_attrib_model_ptr ptrAttribute = NULL_PTR;

    if (ptrAttributeList && ulCount && ptrTemplate)
    {
        for (i = 0; i < ulCount; i++)
        {
            ptrAttribute = &ptrAttributeList[i];
            if (ptrAttribute->type == ptrTemplate->type)
            {
                return ptrAttribute;
            }
        }
    }

    return NULL_PTR;
}

static qti_pkcs11_attrib_model_ptr qti_pkcs11_find_attrib_match(qti_pkcs11_object_ptr ptrObject, const qti_pkcs11_attrib_model_ptr ptrAttributeList, const CK_ULONG ulCount, const CK_ATTRIBUTE_PTR ptrTemplate)
{
    CK_BBOOL found = FALSE;
    qti_pkcs11_attrib_model_ptr ptrAttribute = NULL_PTR;

    ptrAttribute = qti_pkcs11_find_attrib(ptrAttributeList, ulCount, ptrTemplate);

    if (ptrAttribute && ptrObject)
    {
        CK_BBOOL must_full_match = FALSE;

        /* Special rules - only return CKO_HW_FEATURE (Sec 4.3.2) or CKO_MECHANISM (Sec 4.12.2) if the search attributes contain them */
        if (CKA_CLASS == ptrAttribute->type)
        {
            if ((CKO_HW_FEATURE == ptrObject->class_id) || (CKO_MECHANISM == ptrObject->class_id))
            {
                must_full_match = TRUE;
            }
        }

        /* Search criteria has a value to we must match it */
        if (ptrTemplate->pValue && ptrAttribute->func)
        {
            CK_UTF8CHAR buf[QTI_PKCS11_MAX_LABEL_SIZE];
            CK_ATTRIBUTE temp = { 0, buf, sizeof(buf) };

	    temp.type = ptrTemplate->type;
            /* Get the attribute */
            if (!ptrAttribute->func(ptrObject, &temp))
            {
                if ((temp.ulValueLen == ptrTemplate->ulValueLen))
                {
                    if (!memcmp(temp.pValue, ptrTemplate->pValue, ptrTemplate->ulValueLen))
                    {
                        found = TRUE;
                    }
                }
            }
        }
        else if (!must_full_match)
        {
            found = TRUE;
        }
    }

    if (found)
    {
        return ptrAttribute;
    }

    return NULL_PTR;
}

static CK_OBJECT_HANDLE qti_pkcs11_find_handle(const CK_ATTRIBUTE_PTR ptrTemplate, CK_ULONG ulCount, CK_ULONG_PTR index)
{
    CK_ULONG i;
    CK_ULONG j;
    CK_OBJECT_HANDLE rv = NULL_PTR;

    /* Finds the first or next match - Iterate through objects */
    for (i = (index) ? *index : 0; i < QTI_PKCS11_MAX_OBJECTS_ALLOWED; i++)
    {
        qti_pkcs11_object_ptr ptrObject = qti_pkcs11_object_cache[i].object;
        if (ptrObject)
        {
            /* Iterate through attribute list */
            for (j = 0; j < ulCount; j++)
            {
                /* See if a match failed */
                if (!qti_pkcs11_find_attrib_match(ptrObject, (const qti_pkcs11_attrib_model_ptr)ptrObject->attributes,
                                              ptrObject->count, &ptrTemplate[j]))
                {
                    break;
                }
            }
            if (j == ulCount)
            {
                /* Full match */
                if (ulCount)
                {
                    rv = qti_pkcs11_object_cache[i].handle;
                    break;
                }
                else if ((CKO_HW_FEATURE != ptrObject->class_id) && (CKO_MECHANISM != ptrObject->class_id))
                {
                    /* Special condition where ulCount is zero we match all
                       objects except HW Features and Mechanisms */
                    rv = qti_pkcs11_object_cache[i].handle;
                    break;
                }
            }
        }
    }

    if (index)
    {
        *index = i;
    }

    return rv;
}

CK_RV qti_pkcs11_find_init(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR ptrTemplate, CK_ULONG ulCount)
{
    qti_pkcs11_session_ctx_ptr ptrSession;
    CK_ULONG index = 0;
    CK_RV rv;

    rv = qti_pkcs11_init_check(NULL, FALSE);
    if (rv)
    {
        return rv;
    }

    if (!ptrTemplate && ulCount)
    {
        return CKR_ARGUMENTS_BAD;
    }

    rv = qti_pkcs11_session_check(&ptrSession, hSession);
    if (rv)
    {
        return rv;
    }

    qti_pkcs11_debug_attributes(ptrTemplate, ulCount);

    /* Check Public/Private Session info - although I don't think we can actually
        get private unless we're using a shared key system - and that will only be
        for secured data and not key info */

    if (qti_pkcs11_find_handle(ptrTemplate, ulCount, &index))
    {
        if (ulCount)
        {
            rv = qti_pkcs11_find_copy_template(qti_pkcs11_find_template_cache, QTI_PKCS11_SEARCH_CACHE_SIZE, ptrTemplate, ulCount);
            if (rv)
            {
                return rv;
            }
            ptrSession->attrib_list = (CK_ATTRIBUTE_PTR)qti_pkcs11_find_template_cache;
        }
        else
        {
            ptrSession->attrib_list = NULL_PTR;
        }
        ptrSession->attrib_count = ulCount;
        ptrSession->object_index = index;
        ptrSession->object_count = 1;
        index++;
        while (qti_pkcs11_find_handle(ptrSession->attrib_list, ulCount, &index))
        {
            ptrSession->object_count++;
            index++;
        }
    }

    return CKR_OK;
}

CK_RV qti_pkcs11_find_continue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE_PTR phObject, CK_ULONG ulMaxObjectCount, CK_ULONG_PTR pulObjectCount)
{
    qti_pkcs11_session_ctx_ptr ptrSession;
    CK_ULONG i;
    CK_RV rv;

    rv = qti_pkcs11_init_check(NULL, FALSE);
    if (rv)
    {
        return rv;
    }

    if (!phObject || !pulObjectCount)
    {
        return CKR_ARGUMENTS_BAD;
    }

    rv = qti_pkcs11_session_check(&ptrSession, hSession);
    if (rv)
    {
        return rv;
    }

    *pulObjectCount = (ptrSession->object_count < ulMaxObjectCount) ? ptrSession->object_count : ulMaxObjectCount;

    for (i = 0; i < *pulObjectCount; i++)
    {
        phObject[i] = qti_pkcs11_find_handle(ptrSession->attrib_list, ptrSession->attrib_count, &ptrSession->object_index);
        if (!phObject[i])
        {
            ptrSession->object_count = 0;
            *pulObjectCount = i;
            break;
        }

        ptrSession->object_index++;

        if (ptrSession->object_count)
        {
            ptrSession->object_count--;
        }
    }

    return CKR_OK;
}

CK_RV qti_pkcs11_find_finish(CK_SESSION_HANDLE hSession)
{
    qti_pkcs11_session_ctx_ptr ptrSession;
    CK_RV rv;

    rv = qti_pkcs11_init_check(NULL, FALSE);
    if (rv)
    {
        return rv;
    }

    rv = qti_pkcs11_session_check(&ptrSession, hSession);
    if (rv)
    {
        return rv;
    }

    ptrSession->attrib_list = NULL_PTR;
    ptrSession->attrib_count = 0;
    ptrSession->object_count = 0;

    return CKR_OK;
}

static qti_pkcs11_object qti_pkcs11_object_store[QTI_PKCS11_MAX_OBJECTS_ALLOWED];
qti_pkcs11_object_cache_t qti_pkcs11_object_cache[QTI_PKCS11_MAX_OBJECTS_ALLOWED];

static CK_OBJECT_HANDLE qti_pkcs11_object_alloc_handle(void)
{
    static CK_OBJECT_HANDLE qti_pkcs11_object_last_handle = 1;

    if (qti_pkcs11_object_last_handle)
    {
        qti_pkcs11_object_last_handle++;
    }

    return qti_pkcs11_object_last_handle;
}

CK_RV qti_pkcs11_object_alloc(qti_pkcs11_object_ptr * ptrObject)
{
    CK_RV rv = CKR_OK;
    CK_ULONG i;

    if (!ptrObject)
    {
        rv = CKR_ARGUMENTS_BAD;
    }
    else
    {
        *ptrObject = NULL;
    }

    for (i = 0; i < QTI_PKCS11_MAX_OBJECTS_ALLOWED && CKR_OK == rv; i++)
    {
        if (!qti_pkcs11_object_cache[i].object)
        {
            *ptrObject = &qti_pkcs11_object_store[i];
            if (*ptrObject)
            {
                memset(*ptrObject, 0, sizeof(qti_pkcs11_object));
                qti_pkcs11_object_cache[i].handle = qti_pkcs11_object_alloc_handle();
                qti_pkcs11_object_cache[i].object = *ptrObject;
            }
            break;
        }
    }

    if (QTI_PKCS11_MAX_OBJECTS_ALLOWED == i)
    {
        rv = CKR_HOST_MEMORY;
    }

    return i;
}

CK_RV qti_pkcs11_object_free(qti_pkcs11_slot_ctx_ptr ptrSlot, qti_pkcs11_object_ptr ptrObject)
{
    CK_ULONG i;
    char cwd[100] = {0};
    char filename[200];

    for (i = 0; i < QTI_PKCS11_MAX_OBJECTS_ALLOWED; i++)
    {
        if (ptrObject == qti_pkcs11_object_cache[i].object)
        {
            qti_pkcs11_object_cache[i].object = NULL_PTR;
            qti_pkcs11_object_cache[i].handle = 0;

            if (getcwd(cwd, sizeof(cwd)) == NULL) {
		    printf("Error: Unable to save in cwd \n");
		    continue;
	    }

	    (void)snprintf(filename, sizeof(filename), "%s%lu.%ld.conf", cwd, ptrSlot->slot_id, i);
	    if (remove(filename) == 0)
		    printf("Deleted object %s\n", filename);
	    else
		    printf("Error deleting object %s\n", filename);
        }
    }

    if (ptrObject)
    {
        memset(ptrObject, 0, sizeof(qti_pkcs11_object));
    }

    return CKR_OK;
}

CK_RV qti_pkcs11_object_get_handle(qti_pkcs11_object_ptr ptrObject, CK_OBJECT_HANDLE_PTR phObject)
{
    CK_ULONG i;

    if (!phObject || !ptrObject)
    {
        return CKR_ARGUMENTS_BAD;
    }

    for (i = 0; i < QTI_PKCS11_MAX_OBJECTS_ALLOWED; i++)
    {
        if (ptrObject == qti_pkcs11_object_cache[i].object)
        {
            *phObject = qti_pkcs11_object_cache[i].handle;
            break;
        }
    }
    if (i == QTI_PKCS11_MAX_OBJECTS_ALLOWED)
    {
        return CKR_OBJECT_HANDLE_INVALID;
    }

    return CKR_OK;
}

CK_RV qti_pkcs11_object_check(qti_pkcs11_object_ptr * ptrObject, CK_OBJECT_HANDLE hObject)
{
    CK_ULONG i;

    if (!hObject)
    {
        return CKR_OBJECT_HANDLE_INVALID;
    }

    for (i = 0; i < QTI_PKCS11_MAX_OBJECTS_ALLOWED; i++)
    {
        if (hObject == qti_pkcs11_object_cache[i].handle)
        {
            break;
        }
    }
    if (i == QTI_PKCS11_MAX_OBJECTS_ALLOWED)
    {
        return CKR_OBJECT_HANDLE_INVALID;
    }
    else if (ptrObject)
    {
        *ptrObject = qti_pkcs11_object_cache[i].object;
    }

    return CKR_OK;
}

CK_RV qti_pkcs11_attrib_fill(CK_ATTRIBUTE_PTR ptrAttribute, const CK_VOID_PTR ptrData, const CK_ULONG ulSize)
{

    if (!ptrAttribute)
    {
        return CKR_ARGUMENTS_BAD;
    }

    if (ptrAttribute->pValue)
    {
        if (ulSize <= ptrAttribute->ulValueLen)
        {
            if (!ptrData)
            {
		memset(ptrAttribute->pValue, 0, ptrAttribute->ulValueLen);
            }
            else
            {
                memcpy(ptrAttribute->pValue, ptrData, ulSize);
            }
        }
        else
        {
            ptrAttribute->ulValueLen = CK_UNAVAILABLE_INFORMATION;

            return CKR_BUFFER_TOO_SMALL;
        }
    }

    ptrAttribute->ulValueLen = ulSize;

    return CKR_OK;
}

CK_RV qti_pkcs11_attrib_value(CK_ATTRIBUTE_PTR ptrAttribute, const CK_ULONG ulValue, const CK_ULONG ulSize)
{
    return qti_pkcs11_attrib_fill(ptrAttribute, (const CK_VOID_PTR)&ulValue, ulSize);
}

CK_RV qti_pkcs11_attrib_false(const CK_VOID_PTR ptrObject, CK_ATTRIBUTE_PTR ptrAttribute)
{
    ((void)ptrObject);
    return qti_pkcs11_attrib_fill(ptrAttribute, (const CK_VOID_PTR)&cbFalse, sizeof(cbFalse));
}

CK_RV qti_pkcs11_attrib_true(const CK_VOID_PTR ptrObject, CK_ATTRIBUTE_PTR ptrAttribute)
{
    ((void)ptrObject);
    return qti_pkcs11_attrib_fill(ptrAttribute, (const CK_VOID_PTR)&cbTrue, sizeof(cbTrue));
}

CK_RV qti_pkcs11_attrib_empty(const CK_VOID_PTR ptrObject, CK_ATTRIBUTE_PTR ptrAttribute)
{
    ((void)ptrObject);
    return qti_pkcs11_attrib_fill(ptrAttribute, NULL, 0);
}

CK_RV qti_pkcs11_key_get_private_key(CK_VOID_PTR ptrObject, CK_ATTRIBUTE_PTR ptrAttribute)
{
    return CKR_ATTRIBUTE_SENSITIVE;
}

CK_RV qti_pkcs11_key_get_public_key(CK_VOID_PTR ptrObject, CK_ATTRIBUTE_PTR ptrAttribute)
{
    return CKR_FUNCTION_NOT_SUPPORTED;
}

/* Hard coded EC params for secp384r1 */
static const CK_UTF8CHAR qti_pkcs11_key_ec_params[] = {
	0x30,0x82,0x01,0x57,0x02,0x01,0x01,0x30,0x3C,0x06,0x07,0x2A,0x86,0x48,0xCE,0x3D,0x01,0x01,0x02,0x31,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,
	0xFF,0xFF,0xFF,0x30,0x7B,0x04,0x30,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,0xFF,0xFF,0xFF,0xFF,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFC,0x04,0x30,0xB3,0x31,0x2F,0xA7,0xE2,0x3E,0xE7,0xE4,0x98,
	0x8E,0x05,0x6B,0xE3,0xF8,0x2D,0x19,0x18,0x1D,0x9C,0x6E,0xFE,0x81,0x41,0x12,0x03,0x14,0x08,0x8F,0x50,0x13,0x87,0x5A,0xC6,0x56,0x39,0x8D,0x8A,0x2E,0xD1,0x9D,0x2A,0x85,
	0xC8,0xED,0xD3,0xEC,0x2A,0xEF,0x03,0x15,0x00,0xA3,0x35,0x92,0x6A,0xA3,0x19,0xA2,0x7A,0x1D,0x00,0x89,0x6A,0x67,0x73,0xA4,0x82,0x7A,0xCD,0xAC,0x73,0x04,0x61,0x04,0xAA,
	0x87,0xCA,0x22,0xBE,0x8B,0x05,0x37,0x8E,0xB1,0xC7,0x1E,0xF3,0x20,0xAD,0x74,0x6E,0x1D,0x3B,0x62,0x8B,0xA7,0x9B,0x98,0x59,0xF7,0x41,0xE0,0x82,0x54,0x2A,0x38,0x55,0x02,
	0xF2,0x5D,0xBF,0x55,0x29,0x6C,0x3A,0x54,0x5E,0x38,0x72,0x76,0x0A,0xB7,0x36,0x17,0xDE,0x4A,0x96,0x26,0x2C,0x6F,0x5D,0x9E,0x98,0xBF,0x92,0x92,0xDC,0x29,0xF8,0xF4,0x1D,
	0xBD,0x28,0x9A,0x14,0x7C,0xE9,0xDA,0x31,0x13,0xB5,0xF0,0xB8,0xC0,0x0A,0x60,0xB1,0xCE,0x1D,0x7E,0x81,0x9D,0x7A,0x43,0x1D,0x7C,0x90,0xEA,0x0E,0x5F,0x02,0x31,0x00,0xFF,
	0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xC7,0x63,0x4D,0x81,0xF4,0x37,0x2D,0xDF,0x58,0x1A,
	0x0D,0xB2,0x48,0xB0,0xA7,0x7A,0xEC,0xEC,0x19,0x6A,0xCC,0xC5,0x29,0x73,0x02,0x01,0x01 };

CK_RV qti_pkcs11_key_get_ec_params(CK_VOID_PTR ptrObject, CK_ATTRIBUTE_PTR ptrAttribute)
{
    return qti_pkcs11_attrib_fill(ptrAttribute, (CK_VOID_PTR)qti_pkcs11_key_ec_params, sizeof(qti_pkcs11_key_ec_params));
}

CK_RV qti_pkcs11_key_get_ec_point(CK_VOID_PTR ptrObject, CK_ATTRIBUTE_PTR ptrAttribute)
{
	CK_UTF8CHAR ec_asn1_key[MAX_ECC_PUB_KEY_LEN];
	struct ecdsa_points ec_points = {0};
	CK_RV rv;
	int fd;

	fd = open(qseecom_device, O_RDWR);
	if (fd < 0) {
		printf("Cannot open device file: %s\n", qseecom_device);
		return CKR_DEVICE_ERROR;
	}

	rv = ioctl(fd, QSEECOM_GET_EC_POINTS, &ec_points);
	if (rv) {
		printf("Failed to get the EC_POINTS, %ld, len %d pont[0] %d\n", rv, ec_points.public_key_len, ec_points.public_key[0]);
		close(fd);
		return CKR_FUNCTION_FAILED;
	}

	close(fd);

	/* ASN.1 format */
	ec_asn1_key[0] = 0x04;
	ec_asn1_key[1] = (unsigned char)ec_points.public_key_len;

	memcpy(ec_asn1_key + 2, ec_points.public_key, ec_asn1_key[1]);

	return qti_pkcs11_attrib_fill(ptrAttribute, ec_asn1_key, ec_points.public_key_len + 2);
}

static const CK_MECHANISM_TYPE qti_pkcs11_key_508_public_mech[] = {
    CKM_ECDSA,
    CKM_ECDSA_SHA256
};
 
static const CK_MECHANISM_TYPE qti_pkcs11_key_508_private_mech[] = {
    CKM_ECDSA,
    CKM_ECDSA_SHA256
};
 
CK_RV qti_pkcs11_key_get_allowed_mechanisms_priv(CK_VOID_PTR ptrObject, CK_ATTRIBUTE_PTR ptrAttribute)
{
            return qti_pkcs11_attrib_fill(ptrAttribute, (CK_VOID_PTR)qti_pkcs11_key_508_private_mech,
                                      sizeof(qti_pkcs11_key_508_private_mech));
}
 
CK_RV qti_pkcs11_key_get_allowed_mechanisms_pub(CK_VOID_PTR ptrObject, CK_ATTRIBUTE_PTR ptrAttribute)
{
            return qti_pkcs11_attrib_fill(ptrAttribute, (CK_VOID_PTR)qti_pkcs11_key_508_public_mech,
                                      sizeof(qti_pkcs11_key_508_public_mech));
}

CK_RV qti_pkcs11_object_get_class(CK_VOID_PTR ptrObject, CK_ATTRIBUTE_PTR ptrAttribute)
{
    qti_pkcs11_object_ptr obj_ptr = (qti_pkcs11_object_ptr)ptrObject;

    if (!obj_ptr)
    {
        return CKR_ARGUMENTS_BAD;
    }

    return qti_pkcs11_attrib_fill(ptrAttribute, &obj_ptr->class_id, sizeof(obj_ptr->class_id));
}

CK_RV qti_pkcs11_object_get_name(CK_VOID_PTR ptrObject, CK_ATTRIBUTE_PTR ptrAttribute)
{
    qti_pkcs11_object_ptr obj_ptr = (qti_pkcs11_object_ptr)ptrObject;

    if (!obj_ptr)
    {
        return CKR_ARGUMENTS_BAD;
    }

    if (obj_ptr->name[0] != '\0')
    {
        return qti_pkcs11_attrib_fill(ptrAttribute, (const CK_VOID_PTR)obj_ptr->name, strlen((char*)obj_ptr->name));
    }
    else
    {
        return qti_pkcs11_attrib_fill(ptrAttribute, NULL_PTR, 0);
    }
}

CK_RV qti_pkcs11_object_get_destroyable(CK_VOID_PTR ptrObject, CK_ATTRIBUTE_PTR ptrAttribute)
{
    qti_pkcs11_object_ptr obj_ptr = (qti_pkcs11_object_ptr)ptrObject;

    if (!obj_ptr)
    {
        return CKR_ARGUMENTS_BAD;
    }

    if (obj_ptr->flags & PKCS11_OBJECT_FLAG_DESTROYABLE)
    {
        return qti_pkcs11_attrib_true(ptrObject, ptrAttribute);
    }
    else
    {
        return qti_pkcs11_attrib_false(ptrObject, ptrAttribute);
    }
}

CK_RV qti_pkcs11_object_get_type(CK_VOID_PTR ptrObject, CK_ATTRIBUTE_PTR ptrAttribute)
{
    qti_pkcs11_object_ptr obj_ptr = (qti_pkcs11_object_ptr)ptrObject;

    if (!obj_ptr)
    {
        return CKR_ARGUMENTS_BAD;
    }

    return qti_pkcs11_attrib_fill(ptrAttribute, &obj_ptr->class_type, sizeof(obj_ptr->class_type));
}

CK_RV qti_pkcs11_object_find(qti_pkcs11_object_ptr * ptrObject, CK_ATTRIBUTE_PTR ptrTemplate, CK_ULONG ulCount)
{
    CK_OBJECT_CLASS class = CKO_PRIVATE_KEY;
    CK_ATTRIBUTE_PTR ptrName = NULL;
    int i;

    if (!ptrObject || !ptrTemplate || !ulCount)
    {
        return CKR_ARGUMENTS_BAD;
    }

    for (i = 0; i < ulCount; i++, ptrTemplate++)
    {
        switch (ptrTemplate->type)
        {
            case CKA_LABEL:
                ptrName = ptrTemplate;
                break;
            case CKA_CLASS:
                class = *((CK_OBJECT_CLASS_PTR)ptrTemplate->pValue);
                break;
            default:
                break;
        }
    }

    if (ptrName)
    {
        for (i = 0; i < QTI_PKCS11_MAX_OBJECTS_ALLOWED; i++)
        {
            qti_pkcs11_object_ptr ptrObj = qti_pkcs11_object_cache[i].object;
            if (ptrObj)
            {
                if ((ptrObj->class_id == class) && (strlen((const char *)ptrObj->name) == ptrName->ulValueLen))
                {
                    if (!memcmp(ptrObj->name, ptrName->pValue, ptrName->ulValueLen))
                    {
                        *ptrObject = ptrObj;
                        break;
                    }
                }
            }
        }
    }

    return CKR_OK;
}

CK_RV qti_pkcs11_object_get_size(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, CK_ULONG_PTR pulSize)
{
    qti_pkcs11_object_ptr ptrObject;
    CK_RV rv;

    rv = qti_pkcs11_init_check(NULL, FALSE);
    if (rv)
    {
        return rv;
    }

    if (!pulSize)
    {
        return CKR_ARGUMENTS_BAD;
    }

    rv = qti_pkcs11_session_check(NULL_PTR, hSession);
    if (rv)
    {
        return rv;
    }

    rv = qti_pkcs11_object_check(&ptrObject, hObject);
    if (rv)
    {
        return rv;
    }

    *pulSize = ptrObject->size;

    return CKR_OK;
}

const qti_pkcs11_attrib_model qti_pkcs11_key_private_attributes[] = {
    /** Object Class - CK_OBJECT_CLASS */
    { CKA_CLASS,               qti_pkcs11_object_get_class                                                               },
    /** CK_TRUE if object is a token object; CK_FALSE if object is a session object. Default is CK_FALSE. */
    { CKA_TOKEN,               qti_pkcs11_attrib_true                                                                    },
    /** CK_TRUE if object is a private object; CK_FALSE if object is a public object. */
    { CKA_PRIVATE,             qti_pkcs11_attrib_true                                                                    },
    /** Description of the object(default empty). */
    { CKA_LABEL,               qti_pkcs11_object_get_name                                                                },
    /** CK_TRUE if object can be copied using C_CopyObject.Defaults to CK_TRUE. */
    { CKA_COPYABLE,            qti_pkcs11_attrib_false                                                                   },
    /** CK_TRUE if the object can be destroyed using C_DestroyObject. Default is CK_TRUE. */
    { CKA_DESTROYABLE,         qti_pkcs11_object_get_destroyable                                                         },
    /** Type of key */
    { CKA_KEY_TYPE,            qti_pkcs11_object_get_type                                                                },
    /** Key identifier for key (default empty) */
    { CKA_ID,                  NULL_PTR                                                                                  },
    { CKA_SIGN,                qti_pkcs11_attrib_true                                                                    },
    { CKA_VALUE,               NULL_PTR                                                                                  },
    { CKA_EC_PARAMS,           qti_pkcs11_key_get_ec_params	                                                                 },
    { CKA_EC_POINT,            qti_pkcs11_key_get_ec_point	                                                                 },
    { CKA_ALLOWED_MECHANISMS,  qti_pkcs11_key_get_allowed_mechanisms_priv                                                    },
};

const qti_pkcs11_attrib_model qti_pkcs11_key_public_attributes[] = {
    /** Object Class - CK_OBJECT_CLASS */
    { CKA_CLASS,               qti_pkcs11_object_get_class                                                               },
    /** CK_TRUE if object is a token object; CK_FALSE if object is a session object. Default is CK_FALSE. */
    { CKA_TOKEN,               qti_pkcs11_attrib_true                                                                    },
    /** CK_TRUE if object is a private object; CK_FALSE if object is a public object. */
    { CKA_PRIVATE,             qti_pkcs11_attrib_false                                                                    },
    /** Description of the object(default empty). */
    { CKA_LABEL,               qti_pkcs11_object_get_name                                                                },
    /** CK_TRUE if object can be copied using C_CopyObject.Defaults to CK_TRUE. */
    { CKA_COPYABLE,            qti_pkcs11_attrib_false                                                                   },
    /** CK_TRUE if the object can be destroyed using C_DestroyObject. Default is CK_TRUE. */
    { CKA_DESTROYABLE,         qti_pkcs11_object_get_destroyable                                                         },
    /** Type of key */
    { CKA_KEY_TYPE,            qti_pkcs11_object_get_type                                                                },
    /** Key identifier for key (default empty) */
    { CKA_ID,                  qti_pkcs11_attrib_empty                                                                   },
    { CKA_SIGN,                qti_pkcs11_attrib_true                                                                    },
    { CKA_VALUE,               NULL_PTR                                                                                  },
    { CKA_EC_PARAMS,           qti_pkcs11_key_get_ec_params	                                                                 },
    { CKA_EC_POINT,            qti_pkcs11_key_get_ec_point	                                                                 },
    { CKA_ALLOWED_MECHANISMS,  qti_pkcs11_key_get_allowed_mechanisms_pub                                                     },
};

const CK_ULONG qti_pkcs11_key_private_attributes_count = sizeof(qti_pkcs11_key_private_attributes)/sizeof(qti_pkcs11_key_private_attributes[0]);
const CK_ULONG qti_pkcs11_key_public_attributes_count = sizeof(qti_pkcs11_key_public_attributes)/sizeof(qti_pkcs11_key_public_attributes[0]);

static void qti_pkcs11_save_obj_store(qti_pkcs11_slot_ctx_ptr pSlot, char *objtype, qti_pkcs11_object_ptr ptrObject, int index)
{
    FILE* fp;
    char filename[200];
    char cwd[100] = {0};

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
	 printf("Error: Unable to save in cwd \n");
  	 return;
    }

    if (index < QTI_PKCS11_MAX_OBJECTS_ALLOWED)
    {
        (void)snprintf(filename, sizeof(filename), "%s%lu.%d.conf", cwd, pSlot->slot_id, index);
        fp = fopen(filename, "wb");
        if (fp)
        {
            fprintf(fp, "type = %s\n", objtype);
            fprintf(fp, "label = %s\n", ptrObject->name);
            fclose(fp);
        }
    }
}


void qti_pkcs11_config_init_private(qti_pkcs11_object_ptr ptrObject, char * label, size_t len)
//* Static file size of max 1000 bytes to hold object attributes */
{
    if (len >= QTI_PKCS11_MAX_LABEL_SIZE)
    {
        len = QTI_PKCS11_MAX_LABEL_SIZE - 1;
    }
    memcpy(ptrObject->name, label, len);
    ptrObject->name[len] = '\0';
    ptrObject->class_id = CKO_PRIVATE_KEY;
    ptrObject->class_type = CKK_EC;
    ptrObject->attributes = qti_pkcs11_key_private_attributes;
    ptrObject->count = qti_pkcs11_key_private_attributes_count;
    ptrObject->data = NULL;
    ptrObject->size = 16;
    ptrObject->flags = PKCS11_OBJECT_FLAG_DESTROYABLE;
}

void qti_pkcs11_config_init_public(qti_pkcs11_object_ptr ptrObject, char * label, size_t len)
{
    if (len >= QTI_PKCS11_MAX_LABEL_SIZE)
    {
        len = QTI_PKCS11_MAX_LABEL_SIZE - 1;
    }
    memcpy(ptrObject->name, label, len);
    ptrObject->name[len] = '\0';
    ptrObject->class_id = CKO_PUBLIC_KEY;
    ptrObject->class_type = CKK_EC;
    ptrObject->attributes = qti_pkcs11_key_public_attributes;
    ptrObject->count = qti_pkcs11_key_public_attributes_count;
    ptrObject->data = NULL;
    ptrObject->size = 64;
    ptrObject->flags = PKCS11_OBJECT_FLAG_DESTROYABLE;
}

/**
 * \brief Create a new object on the token in the specified session using the given attribute template
 */
CK_RV qti_pkcs11_object_create(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR ptrTemplate,
		               CK_ULONG ulCount, CK_OBJECT_HANDLE_PTR phObject)
{
    qti_pkcs11_session_ctx_ptr ptrSession = NULL;
    CK_OBJECT_CLASS_PTR ptrClass = NULL;
    pkcs11_lib_ctx_ptr ptrLibCtx = NULL;
    qti_pkcs11_object_ptr ptrObject;
    CK_ATTRIBUTE_PTR ptrData = NULL;
    CK_ATTRIBUTE_PTR ptrLabel = NULL;
    CK_ATTRIBUTE_PTR ptrValue = NULL;
    qti_pkcs11_slot_ctx_ptr slot_ctx;
    int i, index;
    CK_RV rv;


    (void) ptrData;
    (void) ptrValue;

    rv = qti_pkcs11_init_check(&ptrLibCtx, FALSE);
    if (rv)
    {
        return rv;
    }

    rv = qti_pkcs11_session_check(&ptrSession, hSession);
    if (rv)
    {
        return rv;
    }

    for (i = 0; i < ulCount; i++)
    {
        switch (ptrTemplate[i].type)
        {
            case CKA_LABEL:
                ptrLabel = &ptrTemplate[i];
                break;
            case CKA_CLASS:
                ptrClass = ptrTemplate[i].pValue;
                break;
            case CKA_VALUE:
                ptrValue = &ptrTemplate[i];
            case CKA_EC_POINT:
                ptrData = &ptrTemplate[i];
                break;
            default:
                break;
        }
    }

    slot_ctx = (qti_pkcs11_slot_ctx_ptr)ptrSession->slot;

    index = qti_pkcs11_object_alloc(&ptrObject);
    if (ptrObject && ptrClass && ptrLabel && ptrLabel->pValue)
    {
        switch (*ptrClass)
        {
        case CKO_PUBLIC_KEY:
            ptrObject->class_id = CKO_PUBLIC_KEY;
            ptrObject->slot = i;
            ptrObject->flags = PKCS11_OBJECT_FLAG_DESTROYABLE;
            qti_pkcs11_config_init_public(ptrObject, ptrLabel->pValue, ptrLabel->ulValueLen);
	    qti_pkcs11_save_obj_store(slot_ctx, "public", ptrObject, index);
            break;
        case CKO_PRIVATE_KEY:
            ptrObject->class_id = CKO_PRIVATE_KEY;
            ptrObject->slot = i;
            ptrObject->flags = PKCS11_OBJECT_FLAG_DESTROYABLE;
            qti_pkcs11_config_init_private(ptrObject, ptrLabel->pValue, ptrLabel->ulValueLen);
	    qti_pkcs11_save_obj_store(slot_ctx, "private", ptrObject, index);
            break;
        default:
            break;
        }

	rv = qti_pkcs11_object_get_handle(ptrObject, phObject);
    }

    return rv;
}

/**
 * \brief Destroy the specified object
 */
CK_RV qti_pkcs11_object_destroy(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject)
{
    qti_pkcs11_session_ctx_ptr ptrSession = NULL;
    pkcs11_lib_ctx_ptr ptrLibCtx = NULL;
    qti_pkcs11_object_ptr ptrObject;
    CK_RV rv;

    rv = qti_pkcs11_init_check(&ptrLibCtx, FALSE);
    if (rv)
    {
        return rv;
    }

    rv = qti_pkcs11_session_check(&ptrSession, hSession);
    if (rv)
    {
        return rv;
    }

    rv = qti_pkcs11_object_check(&ptrObject, hObject);
    if (rv)
    {
        return rv;
    }

    if (ptrObject->flags & PKCS11_OBJECT_FLAG_DESTROYABLE)
    {
        return qti_pkcs11_object_free((qti_pkcs11_slot_ctx_ptr)ptrSession->slot, ptrObject);
    }
    else
    {
        return CKR_ACTION_PROHIBITED;
    }
}

CK_RV qti_pkcs11_object_deinit(pkcs11_lib_ctx_ptr pContext)
{
    qti_pkcs11_slot_ctx_ptr slot_ctx;
    CK_RV rv = CKR_OK;
    int i;


    slot_ctx = qti_pkcs11_slot_get_context(pContext, 0);

    for (i = 0; i < QTI_PKCS11_MAX_OBJECTS_ALLOWED; i++)
    {
        qti_pkcs11_object_ptr ptrObj = qti_pkcs11_object_cache[i].object;
        if (ptrObj)
        {
            CK_RV tmp = qti_pkcs11_object_free(slot_ctx, ptrObj);
            if (!rv)
            {
                rv = tmp;
            }
        }
    }
    return rv;
}

CK_RV qti_pkcs11_find_get_attribute(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR ptrTemplate, CK_ULONG ulCount)
{
    qti_pkcs11_session_ctx_ptr ptrSession;
    qti_pkcs11_object_ptr ptrObject;
    pkcs11_lib_ctx_ptr ptrLibCtx;
    CK_ULONG i;
    CK_RV rv;

    if (!ptrTemplate || !ulCount)
    {
        return CKR_ARGUMENTS_BAD;
    }

    rv = qti_pkcs11_init_check(&ptrLibCtx, FALSE);
    if (rv)
    {
        return rv;
    }

    rv = qti_pkcs11_session_check(&ptrSession, hSession);
    if (rv)
    {
        return rv;
    }

    rv = qti_pkcs11_object_check(&ptrObject, hObject);
    if (rv)
    {
        return rv;
    }

    for (i = 0; i < ulCount; i++)
    {
        qti_pkcs11_attrib_model_ptr ptrAttribute = qti_pkcs11_find_attrib((const qti_pkcs11_attrib_model_ptr)ptrObject->attributes,
                                                                ptrObject->count, &ptrTemplate[i]);

        if (!ptrAttribute)
        {
            ptrTemplate[i].ulValueLen = CK_UNAVAILABLE_INFORMATION;
            if (!rv)
            {
                rv = CKR_ATTRIBUTE_TYPE_INVALID;
            }
        }
        else if (ptrAttribute->func)
        {
                CK_RV temp = ptrAttribute->func(ptrObject, &ptrTemplate[i]);
                if (!rv)
                {
                    rv = temp;
                }
        }
        else
        {
            ptrTemplate[i].ulValueLen = CK_UNAVAILABLE_INFORMATION;
            if (!rv)
            {
                rv = CKR_ATTRIBUTE_SENSITIVE;
            }
        }
    }

    return rv;
}

/* Static file size of max 1000 bytes to hold object attributes */
#define MAX_OBJ_FILE_SZ 1000

static size_t qti_pkcs11_config_load_file(FILE* fp, char * buf)
{
    size_t sz = 0;

    if (buf)
    {
        /* Get file size */
        fseek(fp, 0L, SEEK_END);
        sz = ftell(fp);
        fseek(fp, 0L, SEEK_SET);
        memset(buf, 0, sz);
        if (sz != fread(buf, 1, sz, fp))
             sz = 0;
    }

    return sz;
}

/* Parse the buffer and return value should be even */
static int qti_pkcs11_config_parse_buffer(char* buf, size_t length, int argc, char* argv[])
{
    bool comment = FALSE;
    int num_args = 0;
    bool arg = FALSE;
    bool v = FALSE;
    char* string;

    if (!argc || !argv || !buf || !length)
    {
        return 0;
    }

    for (string = buf; string < (buf + length) && num_args < argc; string++)
    {
        switch (*string)
        {
        case '\n':
        case '\r':
            /* Line End */
            if (arg && !v && !comment)
            {
                /* All are key value pair */
                return 0;
            }
            else
            {
                *string = '\0';
                arg = FALSE;
                v = FALSE;
                comment = FALSE;
            }
            break;
        case '=':
            v = TRUE;
        case ' ':
        case '\t':
            arg = 0;
            *string = '\0';
            break;
        default:
            if (!comment)
            {
                if (*string == '#')
                {
                    comment = 1;
                }
                else if (!arg)
                {
                    arg = TRUE;
                    argv[num_args++] = string;
                }
            }
            break;
        }
    }

    if (num_args & 0x1)
    {
        /* Parsing error */
        num_args = 0;
    }

    return num_args;
}

static CK_RV qti_pkcs11_config_parse_object_file(qti_pkcs11_slot_ctx_ptr slot_ctx, CK_BYTE slot, int argc, char * argv[])
{
    CK_RV rv;
    int i;
    qti_pkcs11_object_ptr ptrObject = NULL;
    bool privkey = FALSE;

    (void) privkey;

    rv = qti_pkcs11_object_alloc(&ptrObject);
    if (rv != CKR_OK || ptrObject == NULL)
      return rv;

    if (ptrObject)
    {
        ptrObject->slot = slot;
        ptrObject->flags = PKCS11_OBJECT_FLAG_DESTROYABLE;
        memset(ptrObject->name, 0, sizeof(ptrObject->name));
    }

    for (i = 0; i < argc; i += 2)
    {
        if (!strcmp(argv[i], "type"))
        {
            if (!strcmp(argv[i + 1], "private"))
            {
                privkey = TRUE;
                qti_pkcs11_config_init_private(ptrObject, "", 0);
            }
            else if (!strcmp(argv[i + 1], "public"))
            {
                qti_pkcs11_config_init_public(ptrObject, "", 0);
            }
        }
        else if (!strcmp(argv[i], "label"))
        {
            strlcpy((char *)ptrObject->name, argv[i + 1], sizeof(ptrObject->name));
        }
    }

#ifdef XXX //TBD
    if (!rv && privkey)
    {
        qti_pkcs11_object_ptr pPubkey = NULL;
        rv = qti_pkcs11_object_alloc(&pPubkey);
        if (pPubkey)
        {
            pPubkey->slot = slot;
            pPubkey->flags = ptrObject->flags;
            qti_pkcs11_config_init_public(pPubkey, ptrObject->name, strlen(ptrObject->name));
        }
        else
        {
            qti_pkcs11_object_free(ptrObject);
        }
    }
#endif

    return rv;
}

/* Load objects from the file store for a given slot */
CK_RV qti_pkcs11_config_load_objects(qti_pkcs11_slot_ctx_ptr pSlot)
{
    FILE* fp;
    char buf[MAX_OBJ_FILE_SZ];
    size_t buflen;
    char* argv[QTI_PKCS11_MAX_OBJECTS_ALLOWED + 1];
    int argc = 0;
    char filename[200];
    int j;
    CK_RV rv = CKR_GENERAL_ERROR;
    char cwd[100] = {0};

    if (getcwd(cwd, sizeof(cwd)) == NULL) {
	 printf("Error: Unable to save in cwd \n");
	 return rv;
    }

    for (j = 0; j < QTI_PKCS11_MAX_OBJECTS_ALLOWED; j++)
    {
            (void)snprintf(filename, sizeof(filename), "%s%lu.%d.conf", cwd, pSlot->slot_id, j);
            fp = fopen(filename, "rb");

            if (fp)
            {
                buflen = qti_pkcs11_config_load_file(fp, buf);
                fclose(fp);
                fp = NULL;

                /* Remove the slot from the free list*/
                pSlot->flags &= ~(1 << j);

                if (0 < buflen)
                {
                    if (0 < (argc = qti_pkcs11_config_parse_buffer(buf, buflen, sizeof(argv) / sizeof(argv[0]), argv)))
                    {
                        rv = qti_pkcs11_config_parse_object_file(pSlot, j, argc, argv);
                    }
                    else
                    {
                        QTI_PKCS11_DBG("Failed to parse the slot configuration file");
                    }
                }
            }
     }
    return rv;
}

CK_RV qti_pkcs11_key_generate_pair(CK_SESSION_HANDLE hSession,CK_MECHANISM_PTR pMechanism,
                                   CK_ATTRIBUTE_PTR ptrPublicKeyTemplate, CK_ULONG ulPublicKeyAttributeCount,
				   CK_ATTRIBUTE_PTR ptrPrivateKeyTemplate, CK_ULONG ulPrivateKeyAttributeCount,
				   CK_OBJECT_HANDLE_PTR phPublicKey,CK_OBJECT_HANDLE_PTR phPrivateKey)
{
    qti_pkcs11_session_ctx_ptr ptrSession;
    qti_pkcs11_object_ptr ptrPrivate = NULL;
    qti_pkcs11_object_ptr ptrPublic = NULL;
    qti_pkcs11_slot_ctx_ptr slot_ctx;
    CK_ATTRIBUTE_PTR ptrName = NULL;
    pkcs11_lib_ctx_ptr ptrLibCtx;
    int i, pub_index, prv_index;
    CK_RV rv = CKR_OK;

    if (!pMechanism || !ptrPublicKeyTemplate || !ulPublicKeyAttributeCount ||
        !ptrPrivateKeyTemplate || !ulPrivateKeyAttributeCount ||
        !phPublicKey || !phPrivateKey)
    {
        return CKR_ARGUMENTS_BAD;
    }

    rv = qti_pkcs11_init_check(&ptrLibCtx, FALSE);
    if (rv)
    {
        return rv;
    }

    rv = qti_pkcs11_session_check(&ptrSession, hSession);
    if (rv)
    {
        return rv;
    }

    /* @todo Perform the various mechanism and key attribute checks */

    if (CKM_EC_KEY_PAIR_GEN != pMechanism->mechanism)
    {
        return CKR_MECHANISM_INVALID;
    }


//    CK_OBJECT_CLASS class = CKO_PRIVATE_KEY;    /* Unless specified assume private key object */

    /* Match Name and Class */
    for (i = 0; i < ulPrivateKeyAttributeCount; i++)
    {
        if (CKA_LABEL == ptrPrivateKeyTemplate[i].type)
        {
            ptrName = &ptrPrivateKeyTemplate[i];
            break;
        }
    }

    if (!ptrName || ptrName->ulValueLen > QTI_PKCS11_MAX_LABEL_SIZE)
    {
        return CKR_TEMPLATE_INCONSISTENT;
    }

    prv_index = qti_pkcs11_object_alloc(&ptrPrivate);

    pub_index = qti_pkcs11_object_alloc(&ptrPublic);

    if (!ptrPublic || !ptrPrivate)
    {
        rv = CKR_TEMPLATE_INCONSISTENT;
    }

    slot_ctx = (qti_pkcs11_slot_ctx_ptr)ptrSession->slot;

    if (CKR_OK == rv)
    {
        ptrPrivate->class_id = CKO_PRIVATE_KEY;
        qti_pkcs11_config_init_private(ptrPrivate, ptrName->pValue, ptrName->ulValueLen);
	qti_pkcs11_save_obj_store(slot_ctx, "private", ptrPrivate, prv_index);
    }

    if (CKR_OK == rv)
    {
        memcpy(ptrPublic->name, ptrName->pValue, ptrName->ulValueLen);
        ptrPublic->class_id = CKO_PUBLIC_KEY;
        ptrPublic->class_type = CKK_EC;
        ptrPublic->slot = ptrPrivate->slot;
        ptrPublic->flags = ptrPrivate->flags;
        ptrPublic->attributes = qti_pkcs11_key_public_attributes;
        ptrPublic->count = qti_pkcs11_key_public_attributes_count;
        ptrPublic->size = 64;
	ptrPublic->flags = PKCS11_OBJECT_FLAG_DESTROYABLE;

        qti_pkcs11_config_init_public(ptrPublic, ptrName->pValue, ptrName->ulValueLen);
        qti_pkcs11_save_obj_store(slot_ctx, "public", ptrPublic, pub_index);
    }

    if (CKR_OK == rv)
    {
        qti_pkcs11_object_get_handle(ptrPrivate, phPrivateKey);
        qti_pkcs11_object_get_handle(ptrPublic, phPublicKey);
    }
    else
    {
        if (ptrPrivate)
        {
            qti_pkcs11_object_free(slot_ctx, ptrPrivate);
        }
        if (ptrPublic)
        {
            qti_pkcs11_object_free(slot_ctx, ptrPublic);
        }
    }

    return rv;
}
