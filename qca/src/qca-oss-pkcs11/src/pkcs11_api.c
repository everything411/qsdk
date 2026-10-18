/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */
/* SPDX-License-Identifier: BSD-2-Clause */
/*
 * Copyright (c) 2018-2020, Linaro Limited
 */
#include "pkcs11.h"
#include <stdbool.h>
#include <stddef.h>
#include "cryptoki.h"
#include "debug.h"
#include "mechanism.h"
#include "init.h"
#include "session.h"
#include "object.h"
#include "crypto.h"

static const CK_FUNCTION_LIST pkcs11FunctionList = {
	.version = {
		.major = CRYPTOKI_VERSION_MAJOR,
		.minor = CRYPTOKI_VERSION_MINOR,
	},
	.C_Initialize = C_Initialize,
	.C_Finalize = C_Finalize,
	.C_GetInfo = C_GetInfo,
	.C_GetFunctionList = C_GetFunctionList,
	.C_GetSlotList = C_GetSlotList,
	.C_GetSlotInfo = C_GetSlotInfo,
	.C_GetTokenInfo = C_GetTokenInfo,
	.C_GetMechanismList = C_GetMechanismList,
	.C_GetMechanismInfo = C_GetMechanismInfo,
	.C_InitToken = C_InitToken,
	.C_InitPIN = C_InitPIN,
	.C_SetPIN = C_SetPIN,
	.C_OpenSession = C_OpenSession,
	.C_CloseSession = C_CloseSession,
	.C_CloseAllSessions = C_CloseAllSessions,
	.C_GetSessionInfo = C_GetSessionInfo,
	.C_GetOperationState = C_GetOperationState,
	.C_SetOperationState = C_SetOperationState,
	.C_Login = C_Login,
	.C_Logout = C_Logout,
	.C_CreateObject = C_CreateObject,
	.C_CopyObject = C_CopyObject,
	.C_DestroyObject = C_DestroyObject,
	.C_GetObjectSize = C_GetObjectSize,
	.C_GetAttributeValue = C_GetAttributeValue,
	.C_SetAttributeValue = C_SetAttributeValue,
	.C_FindObjectsInit = C_FindObjectsInit,
	.C_FindObjects = C_FindObjects,
	.C_FindObjectsFinal = C_FindObjectsFinal,
	.C_EncryptInit = C_EncryptInit,
	.C_Encrypt = C_Encrypt,
	.C_EncryptUpdate = C_EncryptUpdate,
	.C_EncryptFinal = C_EncryptFinal,
	.C_DecryptInit = C_DecryptInit,
	.C_Decrypt = C_Decrypt,
	.C_DecryptUpdate = C_DecryptUpdate,
	.C_DecryptFinal = C_DecryptFinal,
	.C_DigestInit = C_DigestInit,
	.C_Digest = C_Digest,
	.C_DigestUpdate = C_DigestUpdate,
	.C_DigestKey = C_DigestKey,
	.C_DigestFinal = C_DigestFinal,
	.C_SignInit = C_SignInit,
	.C_Sign = C_Sign,
	.C_SignUpdate = C_SignUpdate,
	.C_SignFinal = C_SignFinal,
	.C_SignRecoverInit = C_SignRecoverInit,
	.C_SignRecover = C_SignRecover,
	.C_VerifyInit = C_VerifyInit,
	.C_Verify = C_Verify,
	.C_VerifyUpdate = C_VerifyUpdate,
	.C_VerifyFinal = C_VerifyFinal,
	.C_VerifyRecoverInit = C_VerifyRecoverInit,
	.C_VerifyRecover = C_VerifyRecover,
	.C_DigestEncryptUpdate = C_DigestEncryptUpdate,
	.C_DecryptDigestUpdate = C_DecryptDigestUpdate,
	.C_SignEncryptUpdate = C_SignEncryptUpdate,
	.C_DecryptVerifyUpdate = C_DecryptVerifyUpdate,
	.C_GenerateKey = C_GenerateKey,
	.C_GenerateKeyPair = C_GenerateKeyPair,
	.C_WrapKey = C_WrapKey,
	.C_UnwrapKey = C_UnwrapKey,
	.C_DeriveKey = C_DeriveKey,
	.C_SeedRandom = C_SeedRandom,
	.C_GenerateRandom = C_GenerateRandom,
	.C_GetFunctionStatus = C_GetFunctionStatus,
	.C_CancelFunction = C_CancelFunction,
	.C_WaitForSlotEvent = C_WaitForSlotEvent,
};

/* Initializes Cryptoki library
 * notes If pInitArgs is a non-NULL_PTR is must dereference to a CK_C_INITIALIZE_ARGS structure */
CK_RV C_Initialize(CK_VOID_PTR ptrInitArgs)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_init((CK_C_INITIALIZE_ARGS_PTR)ptrInitArgs));
}

/* Clean up miscellaneous Cryptoki-associated resources */
CK_RV C_Finalize(CK_VOID_PTR ptrReserved)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_deinit(ptrReserved));
}

/* Obtains general information about Cryptoki */
CK_RV C_GetInfo(CK_INFO_PTR ptrInfo)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_get_lib_info(ptrInfo));
}

/* Obtains entry points of Cryptoki library functions */
CK_RV C_GetFunctionList(CK_FUNCTION_LIST_PTR_PTR ppFuncList)
{
    QTI_PKCS11_DBG("\r\n");
    if (!ppFuncList)
    {
        return CKR_ARGUMENTS_BAD;
    }

    *ppFuncList = (CK_FUNCTION_LIST_PTR)&pkcs11FunctionList;

    return CKR_OK;
}

/* Obtains a list of slots in the system */
CK_RV C_GetSlotList(CK_BBOOL tokenPresent, CK_SLOT_ID_PTR ptrSlotList, CK_ULONG_PTR ptrulCount)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_slot_get_list(tokenPresent, ptrSlotList, ptrulCount));
}

/* Obtains information about a particular slot */
CK_RV C_GetSlotInfo(CK_SLOT_ID slotID, CK_SLOT_INFO_PTR ptrInfo)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_slot_get_info(slotID, ptrInfo));
}

/* Obtains information about a particular token */
CK_RV C_GetTokenInfo(CK_SLOT_ID slotID, CK_TOKEN_INFO_PTR ptrInfo)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_token_get_info(slotID, ptrInfo));
}

/* Obtains a list of mechanisms supported by a token (in a slot) */
CK_RV C_GetMechanismList(CK_SLOT_ID slotID, CK_MECHANISM_TYPE_PTR ptrMechanismList, CK_ULONG_PTR ptrulCount)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_mech_get_list(slotID, ptrMechanismList, ptrulCount));
}

/* Obtains information about a particular mechanism of a token (in a slot) */
CK_RV C_GetMechanismInfo(CK_SLOT_ID slotID, CK_MECHANISM_TYPE type, CK_MECHANISM_INFO_PTR ptrInfo)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs_mech_get_info(slotID, type, ptrInfo));
}

/* Initializes a token (in a slot) */
CK_RV C_InitToken(CK_SLOT_ID slotID, CK_UTF8CHAR_PTR ptrPin, CK_ULONG ulPinLen, CK_UTF8CHAR_PTR ptrLabel)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_token_init(slotID, ptrPin, ulPinLen, ptrLabel));
}

/* Initializes the normal user's PIN */
CK_RV C_InitPIN(CK_SESSION_HANDLE hSession, CK_UTF8CHAR_PTR ptrPin, CK_ULONG ulPinLen)
{
    ((void)ulPinLen);
    ((void)hSession);
    ((void)ptrPin);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Modifies the PIN of the current user */
CK_RV C_SetPIN(CK_SESSION_HANDLE hSession, CK_UTF8CHAR_PTR ptrOldPin, CK_ULONG ulOldLen,
		CK_UTF8CHAR_PTR ptrNewPin, CK_ULONG ulNewLen)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Opens a connection between an application and a particular token or
 * sets up an application callback for token insertion */
CK_RV C_OpenSession(CK_SLOT_ID slotID, CK_FLAGS flags, CK_VOID_PTR ptrApplication,
		    CK_NOTIFY notify, CK_SESSION_HANDLE_PTR phSession)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_session_open(slotID, flags, ptrApplication, notify, phSession));
}

/* Close the given session */
CK_RV C_CloseSession(CK_SESSION_HANDLE hSession)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_session_close(hSession));
}

/* Close all open sessions */
CK_RV C_CloseAllSessions(CK_SLOT_ID slotID)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_session_closeall(slotID));
}

/* Retrieve information about the specified session */
CK_RV C_GetSessionInfo(CK_SESSION_HANDLE hSession, CK_SESSION_INFO_PTR ptrInfo)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_session_get_info(hSession, ptrInfo));
}

/* Obtains the cryptographic operations state of a session */
CK_RV C_GetOperationState(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrOperationState, CK_ULONG_PTR pulOperationStateLen)
{
    ((void)hSession);
    ((void)ptrOperationState);
    ((void)pulOperationStateLen);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Sets the cryptographic operations state of a session */
CK_RV C_SetOperationState(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrOperationState,
		          CK_ULONG ulOperationStateLen, CK_OBJECT_HANDLE  hEncryptionKey,
			  CK_OBJECT_HANDLE  hAuthenticationKey)
{
    ((void)hSession);
    ((void)hEncryptionKey);
    ((void)ptrOperationState);
    ((void)hAuthenticationKey);
    ((void)ulOperationStateLen);

    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Login on the token in the specified session */
CK_RV C_Login(CK_SESSION_HANDLE hSession, CK_USER_TYPE userType, CK_UTF8CHAR_PTR ptrPin, CK_ULONG ulPinLen)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_session_login(hSession, userType, ptrPin, ulPinLen));
}

/* Log out of the token in the specified session */
CK_RV C_Logout(CK_SESSION_HANDLE hSession)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Create a new object on the token in the specified session using the given attribute template */
CK_RV C_CreateObject(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR ptrTemplate,
		     CK_ULONG ulCount, CK_OBJECT_HANDLE_PTR phObject)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_object_create(hSession, ptrTemplate, ulCount, phObject));
}

/* Create a copy of the object with the specified handle */
CK_RV C_CopyObject(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR ptrTemplate,
		   CK_ULONG ulCount, CK_OBJECT_HANDLE_PTR phNewObject)
{
    ((void)ulCount);
    ((void)hObject);
    ((void)hSession);
    ((void)ptrTemplate);
    ((void)phNewObject);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Destroy the specified object */
CK_RV C_DestroyObject(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_object_destroy(hSession, hObject));
}

/* Obtains the size of an object in bytes */
CK_RV C_GetObjectSize(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, CK_ULONG_PTR ptrulSize)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_object_get_size(hSession, hObject, ptrulSize));
}

/* Obtains an attribute value of an object */
CK_RV C_GetAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR ptrTemplate, CK_ULONG ulCount)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_find_get_attribute(hSession, hObject, ptrTemplate, ulCount));
}

/* Change or set the value of the specified attributes on the specified object */
CK_RV C_SetAttributeValue(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject, CK_ATTRIBUTE_PTR ptrTemplate, CK_ULONG ulCount)
{
    ((void)hObject);
    ((void)ulCount);
    ((void)hSession);
    ((void)ptrTemplate);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Initializes an object search in the specified session using the specified attribute template as search parameters */
CK_RV C_FindObjectsInit(CK_SESSION_HANDLE hSession, CK_ATTRIBUTE_PTR ptrTemplate, CK_ULONG ulCount)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_find_init(hSession, ptrTemplate, ulCount));
}

/* Continue the search for objects in the specified session */
CK_RV C_FindObjects(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE_PTR phObject, CK_ULONG ulMaxObjectCount, CK_ULONG_PTR pulObjectCount)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_find_continue(hSession, phObject, ulMaxObjectCount, pulObjectCount));
}

/* Finishes an object search operation (and cleans up) */
CK_RV C_FindObjectsFinal(CK_SESSION_HANDLE hSession)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_find_finish(hSession));
}

/* Initializes an encryption operation using the specified mechanism and session */
CK_RV C_EncryptInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR  ptrMechanism, CK_OBJECT_HANDLE  hObject)
{
    ((void)hObject);
    ((void)hSession);
    ((void)ptrMechanism);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Perform a single operation encryption operation in the specified session */
CK_RV C_Encrypt(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrData, CK_ULONG ulDataLen, CK_BYTE_PTR ptrEncryptedData, CK_ULONG_PTR pulEncryptedDataLen)
{
    ((void)ptrData);
    ((void)hSession);
    ((void)ulDataLen);
    ((void)ptrEncryptedData);
    ((void)pulEncryptedDataLen);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Continues a multiple-part encryption operation */
CK_RV C_EncryptUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrData, CK_ULONG ulDataLen, CK_BYTE_PTR ptrEncryptedData, CK_ULONG_PTR pulEncryptedDataLen)
{
    ((void)ptrData);
    ((void)hSession);
    ((void)ulDataLen);
    ((void)ptrEncryptedData);
    ((void)pulEncryptedDataLen);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Finishes a multiple-part encryption operation */
CK_RV C_EncryptFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrEncryptedData, CK_ULONG_PTR pulEncryptedDataLen)
{
    ((void)hSession);
    ((void)ptrEncryptedData);
    ((void)pulEncryptedDataLen);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Initialize decryption using the specified object */
CK_RV C_DecryptInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR ptrMechanism, CK_OBJECT_HANDLE hObject)
{
    ((void)hObject);
    ((void)hSession);
    ((void)ptrMechanism);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Perform a single operation decryption in the given session */
CK_RV C_Decrypt(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrEncryptedData, CK_ULONG ulEncryptedDataLen, CK_BYTE_PTR ptrData,CK_ULONG_PTR pulDataLen)
{
    ((void)ptrData);
    ((void)hSession);
    ((void)pulDataLen);
    ((void)ptrEncryptedData);
    ((void)ulEncryptedDataLen);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Continues a multiple-part decryption operation */
CK_RV C_DecryptUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrEncryptedData,
		      CK_ULONG ulEncryptedDataLen, CK_BYTE_PTR ptrData, CK_ULONG_PTR ptrDataLen)
{
    ((void)ptrData);
    ((void)hSession);
    ((void)ptrDataLen);
    ((void)ptrEncryptedData);
    ((void)ulEncryptedDataLen);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Finishes a multiple-part decryption operation */
CK_RV C_DecryptFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrData, CK_ULONG_PTR ptrDataLen)
{
    ((void)ptrData);
    ((void)hSession);
    ((void)ptrDataLen);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Initializes a message-digesting operation using the specified mechanism in the specified session */
CK_RV C_DigestInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR ptrMechanism)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Digest the specified data in a one-pass operation and return the resulting digest */
CK_RV C_Digest(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrData, CK_ULONG ulDataLen, CK_BYTE_PTR ptrDigest, CK_ULONG_PTR pulDigestLen)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Continues a multiple-part digesting operation */
CK_RV C_DigestUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrPart, CK_ULONG ulPartLen)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Update a running digest operation by digesting a secret key with the specified handle */
CK_RV C_DigestKey(CK_SESSION_HANDLE hSession, CK_OBJECT_HANDLE hObject)
{
    ((void)hObject);
    ((void)hSession);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Finishes a multiple-part digesting operation */
CK_RV C_DigestFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrDigest, CK_ULONG_PTR pulDigestLen)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Initialize a signing operation using the specified key and mechanism */
CK_RV C_SignInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR ptrMechanism, CK_OBJECT_HANDLE hKey)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_signature_sign_init(hSession, ptrMechanism, hKey));
}

/* Sign the data in a single pass operation */
CK_RV C_Sign(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrData, CK_ULONG ulDataLen, CK_BYTE_PTR ptrSignature, CK_ULONG_PTR pulSignatureLen)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_signature_sign(hSession, ptrData, ulDataLen, ptrSignature, pulSignatureLen));
}

/* Continues a multiple-part signature operation */
CK_RV C_SignUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrPart, CK_ULONG ulPartLen)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Finishes a multiple-part signature operation */
CK_RV C_SignFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrSignature, CK_ULONG_PTR pulSignatureLen)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Initializes a signature operation, where the data can be recovered from the signature */
CK_RV C_SignRecoverInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR ptrMechanism, CK_OBJECT_HANDLE hKey)
{
    ((void)hKey);
    ((void)hSession);
    ((void)ptrMechanism);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Signs single-part data, where the data can be recovered from the signature */
CK_RV C_SignRecover(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrData, CK_ULONG ulDataLen, CK_BYTE_PTR ptrSignature, CK_ULONG_PTR pulSignatureLen)
{
    ((void)ptrData);
    ((void)hSession);
    ((void)ulDataLen);
    ((void)ptrSignature);
    ((void)pulSignatureLen);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Initializes a verification operation using the specified key and mechanism */
CK_RV C_VerifyInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR ptrMechanism, CK_OBJECT_HANDLE hKey)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Verifies a signature on single-part data */
CK_RV C_Verify(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrData, CK_ULONG ulDataLen, CK_BYTE_PTR ptrSignature, CK_ULONG ulSignatureLen)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Continues a multiple-part verification operation */
CK_RV C_VerifyUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrPart, CK_ULONG ulPartLen)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Finishes a multiple-part verification operation */
CK_RV C_VerifyFinal(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrSignature, CK_ULONG ulSignatureLen)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Initializes a verification operation where the data is recovered from the signature */
CK_RV C_VerifyRecoverInit(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR ptrMechanism, CK_OBJECT_HANDLE hKey)
{
    ((void)hKey);
    ((void)hSession);
    ((void)ptrMechanism);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Verifies a signature on single-part data, where the data is recovered from the signature */
CK_RV C_VerifyRecover(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrSignature,
		      CK_ULONG ulSignatureLen, CK_BYTE_PTR ptrData, CK_ULONG_PTR pulDataLen)
{
    ((void)ptrData);
    ((void)hSession);
    ((void)pulDataLen);
    ((void)ptrSignature);
    ((void)ulSignatureLen);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Continues simultaneous multiple-part digesting and encryption operations */
CK_RV C_DigestEncryptUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrPart, CK_ULONG ulPartLen,
		            CK_BYTE_PTR pEncryptedPart, CK_ULONG_PTR pulEncryptedPartLen)
{
    ((void)ptrPart);
    ((void)hSession);
    ((void)ulPartLen);
    ((void)pEncryptedPart);
    ((void)pulEncryptedPartLen);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Continues simultaneous multiple-part decryption and digesting operations */
CK_RV C_DecryptDigestUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrPart,
		            CK_ULONG ulPartLen, CK_BYTE_PTR ptrDecryptedPart,
			    CK_ULONG_PTR pulDecryptedPartLen)
{
    ((void)ptrPart);
    ((void)hSession);
    ((void)ulPartLen);
    ((void)ptrDecryptedPart);
    ((void)pulDecryptedPartLen);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Continues simultaneous multiple-part signature and encryption operations */
CK_RV C_SignEncryptUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrPart, CK_ULONG ulPartLen, CK_BYTE_PTR ptrEncryptedPart, CK_ULONG_PTR pulEncryptedPartLen)
{
    ((void)ptrPart);
    ((void)hSession);
    ((void)ulPartLen);
    ((void)ptrEncryptedPart);
    ((void)pulEncryptedPartLen);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Continues simultaneous multiple-part decryption and verification operations */
CK_RV C_DecryptVerifyUpdate(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrEncryptedPart, CK_ULONG ulEncryptedPartLen, CK_BYTE_PTR ptrPart, CK_ULONG_PTR pulPartLen)
{
    ((void)ptrPart);
    ((void)hSession);
    ((void)pulPartLen);
    ((void)ptrEncryptedPart);
    ((void)ulEncryptedPartLen);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Generates a secret key using the specified mechanism */
CK_RV C_GenerateKey(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR ptrMechanism, CK_ATTRIBUTE_PTR ptrTemplate, CK_ULONG ulCount, CK_OBJECT_HANDLE_PTR phKey)
{
    ((void)phKey);
    ((void)ulCount);
    ((void)hSession);
    ((void)ptrTemplate);
    ((void)ptrMechanism);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Generates a public-key/private-key pair using the specified mechanism */
CK_RV C_GenerateKeyPair(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR ptrMechanism, CK_ATTRIBUTE_PTR ptrPublicKeyTemplate,
		        CK_ULONG ulPublicKeyAttributeCount, CK_ATTRIBUTE_PTR ptrPrivateKeyTemplate, CK_ULONG ulPrivateKeyAttributeCount,
			CK_OBJECT_HANDLE_PTR phPublicKey, CK_OBJECT_HANDLE_PTR phPrivateKey)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(qti_pkcs11_key_generate_pair(hSession, ptrMechanism, ptrPublicKeyTemplate,
                            ulPublicKeyAttributeCount, ptrPrivateKeyTemplate, ulPrivateKeyAttributeCount,
			    phPublicKey, phPrivateKey));
}

/* Wraps (encrypts) the specified key using the specified wrapping key and mechanism */
CK_RV C_WrapKey(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR ptrMechanism, CK_OBJECT_HANDLE hWrappingKey, CK_OBJECT_HANDLE hKey, CK_BYTE_PTR ptrWrappedKey, CK_ULONG_PTR pulWrappedKeyLen)
{
    ((void)hKey);
    ((void)hSession);
    ((void)hWrappingKey);
    ((void)ptrMechanism);
    ((void)ptrWrappedKey);
    ((void)pulWrappedKeyLen);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Unwraps (decrypts) the specified key using the specified unwrapping key */
CK_RV C_UnwrapKey(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR ptrMechanism, CK_OBJECT_HANDLE hUnwrappingKey, CK_BYTE_PTR ptrWrappedKey, CK_ULONG ulWrappedKeyLen, CK_ATTRIBUTE_PTR ptrTemplate, CK_ULONG ulCount, CK_OBJECT_HANDLE_PTR phKey)
{
    ((void)phKey);
    ((void)ulCount);
    ((void)hSession);
    ((void)ptrTemplate);
    ((void)ptrMechanism);
    ((void)ptrWrappedKey);
    ((void)hUnwrappingKey);
    ((void)ulWrappedKeyLen);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Derive a key from the specified base key */
CK_RV C_DeriveKey(CK_SESSION_HANDLE hSession, CK_MECHANISM_PTR ptrMechanism, CK_OBJECT_HANDLE hBaseKey, CK_ATTRIBUTE_PTR ptrTemplate, CK_ULONG ulCount, CK_OBJECT_HANDLE_PTR phKey)
{
    ((void)phKey);
    ((void)ulCount);
    ((void)hBaseKey);
    ((void)hSession);
    ((void)ptrTemplate);
    ((void)ptrMechanism);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Mixes in additional seed material to the random number generator */
CK_RV C_SeedRandom(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrSeed, CK_ULONG ulSeedLen)
{
    ((void)ptrSeed);
    ((void)hSession);
    ((void)ulSeedLen);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_RANDOM_SEED_NOT_SUPPORTED);
}

/* Generate the specified amount of random data */
CK_RV C_GenerateRandom(CK_SESSION_HANDLE hSession, CK_BYTE_PTR ptrRandomData, CK_ULONG ulRandomLen)
{
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}

/* Legacy function - see PKCS#11 v2.40 */
CK_RV C_GetFunctionStatus(CK_SESSION_HANDLE hSession)
{
    ((void)hSession);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_PARALLEL);
}

/* Legacy function */
CK_RV C_CancelFunction(CK_SESSION_HANDLE hSession)
{
    ((void)hSession);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_PARALLEL);
}

/* Wait for a slot event (token insertion, removal, etc) on the specified slot to occur */
CK_RV C_WaitForSlotEvent(CK_FLAGS flags, CK_SLOT_ID_PTR pSlot, CK_VOID_PTR pReserved)
{
    ((void)pSlot);
    ((void)flags);
    ((void)pReserved);
    QTI_PKCS11_DBG("\r\n");
    QTI_PKCS11_DBG_RET(CKR_FUNCTION_NOT_SUPPORTED);
}
