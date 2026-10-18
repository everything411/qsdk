/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/*****************************************************************************
   cryptoki.h

   Set the PKCS#11 macros.
*****************************************************************************/

#ifndef _CRYPTOKI_H
#define _CRYPTOKI_H


// Generic helper definitions for shared library support
#if defined _WIN32 || defined __CYGWIN__
#define PKCS11_HELPER_DLL_IMPORT __declspec(dllimport)
#define PKCS11_HELPER_DLL_EXPORT __declspec(dllexport)
#define PKCS11_HELPER_DLL_LOCAL
#else
#if __GNUC__ >= 4
#define PKCS11_HELPER_DLL_IMPORT __attribute__ ((visibility("default")))
#define PKCS11_HELPER_DLL_EXPORT __attribute__ ((visibility("default")))
#define PKCS11_HELPER_DLL_LOCAL  __attribute__ ((visibility("hidden")))
#else
#define PKCS11_HELPER_DLL_IMPORT
#define PKCS11_HELPER_DLL_EXPORT
#define PKCS11_HELPER_DLL_LOCAL
#endif
#endif

// Now we use the generic helper definitions above to define PKCS11_API and PKCS11_LOCAL.
// PKCS11_API is used for the public API symbols. It either DLL imports or DLL exports (or does nothing for static build)
// PKCS11_LOCAL is used for non-api symbols.

#ifdef PKCS11_DLL_EXPORTS // defined if we are building the PKCS11 DLL (instead of using it)
#define PKCS11_API PKCS11_HELPER_DLL_EXPORT
#elif PKCS11_DLL          // defined if PKCS11 is compiled as a DLL
#define PKCS11_API PKCS11_HELPER_DLL_IMPORT
#else // PKCS11_DLL is not defined: this means PKCS11 is a static lib.
#define PKCS11_API
#endif // PKCS11_DLL
#define PKCS11_LOCAL PKCS11_HELPER_DLL_LOCAL

// 1. CK_PTR: The indirection string for making a pointer to an
// object.

#define CK_PTR *

// 2. CK_DECLARE_FUNCTION(returnType, name): A macro which makes
// an importable Cryptoki library function declaration out of a
// return type and a function name.

#define CK_DECLARE_FUNCTION(returnType, name) returnType PKCS11_API name

// 3. CK_DECLARE_FUNCTION_POINTER(returnType, name): A macro
// which makes a Cryptoki API function pointer declaration or
// function pointer type declaration out of a return type and a
// function name.

#define CK_DECLARE_FUNCTION_POINTER(returnType, name) returnType PKCS11_API(*name)

// 4. CK_CALLBACK_FUNCTION(returnType, name): A macro which makes
// a function pointer type for an application callback out of
// a return type for the callback and a name for the callback.

#define CK_CALLBACK_FUNCTION(returnType, name) returnType(*name)

// 5. NULL_PTR: This macro is the value of a NULL pointer.

#ifndef NULL_PTR
#define NULL_PTR 0
#endif

#include "pkcs11.h"

//#if defined(_WIN32) || defined(__GNUC__)
//#pragma pack(pop, cryptoki)
//#endif

#endif // !_CRYPTOKI_H
