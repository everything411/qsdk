/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef OBJECT_H_
#define OBJECT_H_

#include <stddef.h>
#include "cryptoki.h"

#define QTI_PKCS11_SEARCH_CACHE_SIZE        128
#define QTI_PKCS11_MAX_LABEL_SIZE           30
#define QTI_PKCS11_MAX_OBJECTS_ALLOWED      16
#define PKCS11_OBJECT_FLAG_DESTROYABLE      0x01
#define PKCS11_OBJECT_FLAG_MODIFIABLE       0x02
#define PKCS11_OBJECT_FLAG_DYNAMIC          0x04


typedef CK_RV (*attrib_f)(CK_VOID_PTR ptrObject, CK_ATTRIBUTE_PTR ptrAttribute);

typedef struct _qti_pkcs11_attrib_model
{
    const CK_ATTRIBUTE_TYPE type;
    const attrib_f          func;
} qti_pkcs11_attrib_model, *qti_pkcs11_attrib_model_ptr;

typedef struct _qti_pkcs11_object
{
    CK_ULONG class_type;
    CK_OBJECT_CLASS class_id;
    CK_ULONG     size;
    CK_ULONG     count;
    CK_BYTE      slot;
    CK_VOID_PTR  config;
    CK_FLAGS     flags;
    void const * data;
    qti_pkcs11_attrib_model const * attributes;
    CK_UTF8CHAR  name[QTI_PKCS11_MAX_LABEL_SIZE + 1];
} qti_pkcs11_object, *qti_pkcs11_object_ptr;

typedef struct _qti_pkcs11_object_cache_t
{
    qti_pkcs11_object_ptr object;
    CK_OBJECT_HANDLE handle;
} qti_pkcs11_object_cache_t;

extern qti_pkcs11_object_cache_t qti_pkcs11_object_cache[];

extern const qti_pkcs11_attrib_model pkcs11_object_monotonic_attributes[];
extern const CK_ULONG pkcs11_object_monotonic_attributes_count;

CK_RV qti_pkcs11_object_alloc(qti_pkcs11_object_ptr * ppObject);
CK_RV qti_pkcs11_object_free(qti_pkcs11_slot_ctx_ptr pSlot, qti_pkcs11_object_ptr pObject);
CK_RV qti_pkcs11_object_check(qti_pkcs11_object_ptr * ppObject, CK_OBJECT_HANDLE handle);
CK_RV qti_pkcs11_object_find(qti_pkcs11_object_ptr * ppObject, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount);

CK_RV qti_pkcs11_object_get_class(CK_VOID_PTR pObject, CK_ATTRIBUTE_PTR pAttribute);
CK_RV qti_pkcs11_object_get_name(CK_VOID_PTR pObject, CK_ATTRIBUTE_PTR pAttribute);
CK_RV qti_pkcs11_object_get_type(CK_VOID_PTR pObject, CK_ATTRIBUTE_PTR pAttribute);
CK_RV qti_pkcs11_object_get_destroyable(CK_VOID_PTR pObject, CK_ATTRIBUTE_PTR pAttribute);
CK_RV qti_pkcs11_object_get_size(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, CK_ULONG_PTR pulSize);
CK_RV qti_pkcs11_object_get_handle(qti_pkcs11_object_ptr pObject, CK_OBJECT_HANDLE_PTR phObject);

CK_RV qti_pkcs11_object_create(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount, CK_OBJECT_HANDLE_PTR phObject);
CK_RV qti_pkcs11_object_destroy(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject);

CK_RV qti_pkcs11_object_deinit(pkcs11_lib_ctx_ptr pContext);

CK_RV qti_pkcs11_find_get_attribute(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount);
CK_RV qti_pkcs11_find_init(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR pTemplate, CK_ULONG ulCount);
CK_RV qti_pkcs11_find_continue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE_PTR phObject, CK_ULONG ulMaxObjectCount, CK_ULONG_PTR pulObjectCount);
CK_RV qti_pkcs11_find_finish(CK_SESSION_HANDLE hSession);

CK_RV qti_pkcs11_init_check(pkcs11_lib_ctx_ptr * ppContext, CK_BBOOL lock);
CK_RV qti_pkcs11_config_load_objects(qti_pkcs11_slot_ctx_ptr pSlot);
CK_RV qti_pkcs11_key_generate_pair (CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR pMechanism,
				    CK_ATTRIBUTE_PTR pPublicKeyTemplate, CK_ULONG ulPublicKeyAttributeCount,
				    CK_ATTRIBUTE_PTR pPrivateKeyTemplate, CK_ULONG ulPrivateKeyAttributeCount,
				    CK_OBJECT_HANDLE_PTR phPublicKey, CK_OBJECT_HANDLE_PTR phPrivateKey);
#endif /* OBJECT_H_ */
