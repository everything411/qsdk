/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef DEBUG_H_
#define DEBUG_H_

//#define QTI_PKCS11_DBG_ENABLE

#ifdef QTI_PKCS11_DBG_ENABLE
const char * qti_pkcs11_debug_get_ckr_name(CK_RV rv);
void qti_pkcs11_debug_attributes(CK_ATTRIBUTE_PTR ptrTemplate, CK_ULONG ulCount);

#if defined(__linux__) || defined(__CYGWIN__)
#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <sys/types.h>

#define QTI_PKCS11_DBG_NOFILE(fmt, ...) { fflush(stdout); fprintf(stderr, "%d:%d:" fmt, getpid(), (int)syscall(SYS_gettid), ## __VA_ARGS__); fflush(stderr); }
#else
#define QTI_PKCS11_DBG_NOFILE(fmt, ...) { printf(fmt, ## __VA_ARGS__); }
#endif

#define QTI_PKCS11_DBG(fmt, ...)  QTI_PKCS11_DBG_NOFILE("%s:%d:" fmt, __FUNCTION__, __LINE__, ## __VA_ARGS__)
#define QTI_PKCS11_DBG_RET(x)     { CK_RV __y = x; QTI_PKCS11_DBG("%s(%x)\n", qti_pkcs11_debug_get_ckr_name(__y), (unsigned int)__y); return __y; }

#else
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/syscall.h>
#include <sys/types.h>

#define QTI_PKCS11_DBG(...) printf("function %s line %d\n", __FUNCTION__, __LINE__);
#define QTI_PKCS11_DBG_RET(x)     { return x; }
#define qti_pkcs11_debug_attributes(x, y)

#endif

#endif /* DEBUG_H_ */
