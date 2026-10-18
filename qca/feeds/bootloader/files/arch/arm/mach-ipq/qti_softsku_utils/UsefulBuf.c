// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/*==============================================================================
 Copyright (c) 2016-2018, The Linux Foundation.
 Copyright (c) 2018-2019, Laurence Lundblade.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.
    * Neither the name of The Linux Foundation nor the names of its
      contributors, nor the name "Laurence Lundblade" may be used to
      endorse or promote products derived from this software without
      specific prior written permission.

THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ==============================================================================*/

#include <stddef.h>
#include <stdint.h>
#include "include/UsefulBuf.h"

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

#ifdef HAS_QTEE_STDLIB
#include "stringl.h"
#else
size_t memsmove(void *dst, size_t dst_size, const void *src, size_t src_size);
void * secure_memset(void *ptr, int value, size_t len);
size_t memscpy(void *dst, size_t dst_size, const void  *src, size_t src_size);
#endif

#define USEFUL_OUT_BUF_MAGIC  (0x0B0F) // used to catch use of uninitialized or corrupted UOBs


UsefulBufC UsefulBuf_Set(UsefulBuf pDest, uint8_t value)
{
   memset(pDest.ptr, value, pDest.len);

   return UsefulBufC_Init(pDest.ptr, pDest.len);
}


UsefulBufC UsefulBuf_Secure_Set(UsefulBuf pDest, uint8_t value)
{
   secure_memset(pDest.ptr, value, pDest.len);

   return UsefulBufC_Init(pDest.ptr, pDest.len);
}


int UsefulBuf_IsNULL(UsefulBuf UB)
{
   return !UB.ptr;
}


int UsefulBuf_IsNULLC(UsefulBufC UB)
{
   return !UB.ptr;
}


int UsefulBuf_IsEmpty(UsefulBuf UB)
{
   return !UB.len;
}


int UsefulBuf_IsEmptyC(UsefulBufC UB)
{
   return !UB.len;
}


int UsefulBuf_IsNULLOrEmpty(UsefulBuf UB)
{
   return UsefulBuf_IsEmpty(UB) || UsefulBuf_IsNULL(UB);
}


int UsefulBuf_IsNULLOrEmptyC(UsefulBufC UB)
{
   return UsefulBuf_IsEmptyC(UB) || UsefulBuf_IsNULLC(UB);
}


UsefulBufC UsefulBuf_Const(const UsefulBuf UB)
{
   return UsefulBufC_Init(UB.ptr, UB.len);
}


/*
The function below violates good practices and has been removed from the code base:
UsefulBuf UsefulBuf_Unconst(const UsefulBufC UBC)
{
   return UsefulBuf_Init((void *)UBC.ptr, UBC.len);
}
*/


UsefulBufC UsefulBuf_FromSZ(const char *szString)
{
   // Don't crash by passing NULL to strlen
   return UsefulBufC_Init(szString, szString ? strlen(szString) : 0);
}


UsefulBufC UsefulBuf_Copy(UsefulBuf Dest, const UsefulBufC Src)
{
   return UsefulBuf_CopyOffset(Dest, 0, Src);
}


UsefulBufC UsefulBuf_CopyPtr(UsefulBuf Dest, const void *ptr, size_t len)
{
   return UsefulBuf_Copy(Dest, UsefulBufC_Init(ptr, len ));
}


UsefulBufC UsefulBuf_Head(UsefulBufC UB, size_t uAmount)
{
   if(uAmount > UB.len) {
      return NULLUsefulBufC;
   }

   return UsefulBufC_Init(UB.ptr, uAmount);
}


UsefulBufC UsefulBuf_Tail(UsefulBufC UB, size_t uAmount)
{
   UsefulBufC ReturnValue = {NULL, 0};

   if(uAmount > UB.len) {
      ReturnValue = NULLUsefulBufC;
   } else if(UB.ptr == NULL) {
      ReturnValue = UsefulBufC_Init(NULL, UB.len - uAmount);
   } else {
      ReturnValue = UsefulBufC_Init((const uint8_t *)UB.ptr + uAmount, UB.len - uAmount);
   }

   return ReturnValue;
}


#ifdef FEATURE_ENABLE_QCBOR_FLOAT
uint32_t UsefulBufUtil_CopyFloatToUint32(float f)
{
   uint32_t u32;
   memscpy(&u32, sizeof(u32), &f, sizeof(f));
   return u32;
}

uint64_t UsefulBufUtil_CopyDoubleToUint64(double d)
{
   uint64_t u64;
   memscpy(&u64, sizeof(u64), &d, sizeof(d));
   return u64;
}

double UsefulBufUtil_CopyUint64ToDouble(uint64_t u64)
{
   double d;
   memscpy(&d, sizeof(d), &u64, sizeof(u64));
   return d;
}

float UsefulBufUtil_CopyUint32ToFloat(uint32_t u32)
{
   float f;
   memscpy(&f, sizeof(f), &u32, sizeof(u32));
   return f;
}
#endif


void UsefulOutBuf_Reset(UsefulOutBuf *pMe)
{
   pMe->data_len = 0;
   pMe->err      = 0;
}


size_t UsefulOutBuf_GetEndPosition(UsefulOutBuf *pMe)
{
   return pMe->data_len;
}


int UsefulOutBuf_AtStart(UsefulOutBuf *pMe)
{
   return 0 == pMe->data_len;
}


void UsefulOutBuf_InsertData(UsefulOutBuf *pMe,
                                           const void *pBytes,
                                           size_t uLen,
                                           size_t uPos)
{
   UsefulBufC Data = {pBytes, uLen};
   UsefulOutBuf_InsertUsefulBuf(pMe, Data, uPos);
}


void UsefulOutBuf_InsertString(UsefulOutBuf *pMe,
                                             const char *szString,
                                             size_t uPos)
{
   UsefulOutBuf_InsertUsefulBuf(pMe, UsefulBuf_FromSZ(szString), uPos);
}


void UsefulOutBuf_InsertByte(UsefulOutBuf *me,
                                           uint8_t byte,
                                           size_t uPos)
{
   UsefulOutBuf_InsertData(me, &byte, 1, uPos);
}


void UsefulOutBuf_InsertUint16(UsefulOutBuf *me,
                                             uint16_t uInteger16,
                                             size_t uPos)
{
   // See UsefulOutBuf_InsertUint64() for comments on this code

   const void *pBytes;

#if defined(USEFULBUF_CONFIG_BIG_ENDIAN)
   pBytes = &uInteger16;

#elif defined(USEFULBUF_CONFIG_HTON)
   uint16_t uTmp = htons(uInteger16);
   pBytes        = &uTmp;

#elif defined(USEFULBUF_CONFIG_LITTLE_ENDIAN) && defined(USEFULBUF_CONFIG_BSWAP)
   uint16_t uTmp = __builtin_bswap16(uInteger16);
   pBytes = &uTmp;

#else
   uint8_t aTmp[2];

   aTmp[0] = (uint8_t) ((uInteger16 & 0xff00) >> 8);
   aTmp[1] = (uint8_t)  (uInteger16 & 0xff);

   pBytes = aTmp;
#endif

   UsefulOutBuf_InsertData(me, pBytes, 2, uPos);
}


void UsefulOutBuf_InsertUint32(UsefulOutBuf *pMe,
                                             uint32_t uInteger32,
                                             size_t uPos)
{
   // See UsefulOutBuf_InsertUint64() for comments on this code

   const void *pBytes;

#if defined(USEFULBUF_CONFIG_BIG_ENDIAN)
   pBytes = &uInteger32;

#elif defined(USEFULBUF_CONFIG_HTON)
   uint32_t uTmp = htonl(uInteger32);
   pBytes = &uTmp;

#elif defined(USEFULBUF_CONFIG_LITTLE_ENDIAN) && defined(USEFULBUF_CONFIG_BSWAP)
   uint32_t uTmp = __builtin_bswap32(uInteger32);

   pBytes = &uTmp;

#else
   uint8_t aTmp[4];

   aTmp[0] = (uint8_t) ((uInteger32 & 0xff000000) >> 24);
   aTmp[1] = (uint8_t) ((uInteger32 & 0xff0000) >> 16);
   aTmp[2] = (uint8_t) ((uInteger32 & 0xff00) >> 8);
   aTmp[3] = (uint8_t)  (uInteger32 & 0xff);

   pBytes = aTmp;
#endif

   UsefulOutBuf_InsertData(pMe, pBytes, 4, uPos);
}

void UsefulOutBuf_InsertUint64(UsefulOutBuf *pMe,
                                             uint64_t uInteger64,
                                             size_t uPos)
{
   const void *pBytes;

#if defined(USEFULBUF_CONFIG_BIG_ENDIAN)
   // We have been told explicitly we are running on a big-endian
   // machine. Network byte order is big endian, so just copy.  There
   // is no issue with alignment here because uInter64 is always
   // aligned (and it doesn't matter if pBytes is aligned).
   pBytes = &uInteger64;

#elif defined(USEFULBUF_CONFIG_HTON)
   // Use system function to handle big- and little-endian. This works
   // on both big- and little-endian machines, but hton() is not
   // always available or in a standard place so it is not used by
   // default. With some compilers and CPUs the code for this is very
   // compact through use of a special swap instruction and on
   // big-endian machines hton() will reduce to nothing.
   uint64_t uTmp = htonll(uInteger64);

   pBytes = &uTmp;

#elif defined(USEFULBUF_CONFIG_LITTLE_ENDIAN) && defined(USEFULBUF_CONFIG_BSWAP)
   // Use built-in function for byte swapping. This usually compiles
   // to an efficient special byte swap instruction. Unlike hton() it
   // does not do this conditionally on the CPU endianness, so this
   // code is also conditional on USEFULBUF_CONFIG_LITTLE_ENDIAN
   uint64_t uTmp = __builtin_bswap64(uInteger64);

   pBytes = &uTmp;

#else
   // Default which works on every CPU with no dependency on anything
   // from the CPU, compiler, libraries or OS.  This always works, but
   // it is usually a little larger and slower than hton().
   uint8_t aTmp[8];

   aTmp[0] = (uint8_t)((uInteger64 & 0xff00000000000000) >> 56);
   aTmp[1] = (uint8_t)((uInteger64 & 0xff000000000000) >> 48);
   aTmp[2] = (uint8_t)((uInteger64 & 0xff0000000000) >> 40);
   aTmp[3] = (uint8_t)((uInteger64 & 0xff00000000) >> 32);
   aTmp[4] = (uint8_t)((uInteger64 & 0xff000000) >> 24);
   aTmp[5] = (uint8_t)((uInteger64 & 0xff0000) >> 16);
   aTmp[6] = (uint8_t)((uInteger64 & 0xff00) >> 8);
   aTmp[7] = (uint8_t)((uInteger64 & 0xff));

   pBytes = aTmp;
#endif

   // Do the insert
   UsefulOutBuf_InsertData(pMe, pBytes, sizeof(uint64_t), uPos);
}


#ifdef FEATURE_ENABLE_QCBOR_FLOAT
void UsefulOutBuf_InsertFloat(UsefulOutBuf *pMe,
                                            float f,
                                            size_t uPos)
{
   UsefulOutBuf_InsertUint32(pMe, UsefulBufUtil_CopyFloatToUint32(f), uPos);
}


void UsefulOutBuf_InsertDouble(UsefulOutBuf *pMe,
                                             double d,
                                             size_t uPos)
{
   UsefulOutBuf_InsertUint64(pMe, UsefulBufUtil_CopyDoubleToUint64(d), uPos);
}
#endif


void UsefulOutBuf_AppendUsefulBuf(UsefulOutBuf *pMe,
                                                UsefulBufC NewData)
{
   // An append is just a insert at the end
   UsefulOutBuf_InsertUsefulBuf(pMe, NewData, UsefulOutBuf_GetEndPosition(pMe));
}


void UsefulOutBuf_AppendData(UsefulOutBuf *pMe,
                                           const void *pBytes,
                                           size_t uLen)
{
   UsefulBufC Data = {pBytes, uLen};
   UsefulOutBuf_AppendUsefulBuf(pMe, Data);
}


void UsefulOutBuf_AppendString(UsefulOutBuf *pMe,
                                             const char *szString)
{
   UsefulOutBuf_AppendUsefulBuf(pMe, UsefulBuf_FromSZ(szString));
}


void UsefulOutBuf_AppendByte(UsefulOutBuf *pMe,
                                           uint8_t byte)
{
   UsefulOutBuf_AppendData(pMe, &byte, 1);
}


void UsefulOutBuf_AppendUint16(UsefulOutBuf *pMe,
                                             uint16_t uInteger16)
{
   UsefulOutBuf_InsertUint16(pMe, uInteger16, UsefulOutBuf_GetEndPosition(pMe));
}

void UsefulOutBuf_AppendUint32(UsefulOutBuf *pMe,
                                             uint32_t uInteger32)
{
   UsefulOutBuf_InsertUint32(pMe, uInteger32, UsefulOutBuf_GetEndPosition(pMe));
}


void UsefulOutBuf_AppendUint64(UsefulOutBuf *pMe,
                                             uint64_t uInteger64)
{
   UsefulOutBuf_InsertUint64(pMe, uInteger64, UsefulOutBuf_GetEndPosition(pMe));
}


#ifdef FEATURE_ENABLE_QCBOR_FLOAT
void UsefulOutBuf_AppendFloat(UsefulOutBuf *pMe,
                                            float f)
{
   UsefulOutBuf_InsertFloat(pMe, f, UsefulOutBuf_GetEndPosition(pMe));
}


void UsefulOutBuf_AppendDouble(UsefulOutBuf *pMe,
                                             double d)
{
   UsefulOutBuf_InsertDouble(pMe, d, UsefulOutBuf_GetEndPosition(pMe));
}
#endif

int UsefulOutBuf_GetError(UsefulOutBuf *pMe)
{
   return pMe->err;
}


size_t UsefulOutBuf_RoomLeft(UsefulOutBuf *pMe)
{
   return pMe->UB.len - pMe->data_len;
}


int UsefulOutBuf_WillItFit(UsefulOutBuf *pMe, size_t uLen)
{
   return uLen <= UsefulOutBuf_RoomLeft(pMe);
}


int UsefulOutBuf_IsBufferNULL(UsefulOutBuf *pMe)
{
   return pMe->UB.ptr == NULL;
}



void UsefulInputBuf_Init(UsefulInputBuf *pMe, UsefulBufC UB)
{
   pMe->cursor = 0;
   pMe->err    = 0;
   pMe->magic  = UIB_MAGIC;
   pMe->UB     = UB;
}

size_t UsefulInputBuf_Tell(UsefulInputBuf *pMe)
{
   return pMe->cursor;
}


void UsefulInputBuf_Seek(UsefulInputBuf *pMe, size_t uPos)
{
   if(uPos > pMe->UB.len) {
      pMe->err = 1;
   } else {
      pMe->cursor = uPos;
   }
}


size_t UsefulInputBuf_BytesUnconsumed(UsefulInputBuf *pMe)
{
   // Code Reviewers: THIS FUNCTION DOES POINTER MATH

   // Magic number is messed up. Either the structure got overwritten
   // or was never initialized.
   if(pMe->magic != UIB_MAGIC) {
      return 0;
   }

   // The cursor is off the end of the input buffer given.
   // Presuming there are no bugs in this code, this should never happen.
   // If it so, the struct was corrupted. The check is retained as
   // as a defense in case there is a bug in this code or the struct is
   // corrupted.
   if(pMe->cursor > pMe->UB.len) {
      return 0;
   }

   // subtraction can't go neative because of check above
   return pMe->UB.len - pMe->cursor;
}


int UsefulInputBuf_BytesAvailable(UsefulInputBuf *pMe, size_t uLen)
{
   return UsefulInputBuf_BytesUnconsumed(pMe) >= uLen ? 1 : 0;
}


UsefulBufC UsefulInputBuf_GetUsefulBuf(UsefulInputBuf *pMe, size_t uNum)
{
   const void *pResult = UsefulInputBuf_GetBytes(pMe, uNum);
   if(!pResult) {
      return NULLUsefulBufC;
   } else {
      return UsefulBufC_Init(pResult, uNum);
   }
}


uint8_t UsefulInputBuf_GetByte(UsefulInputBuf *pMe)
{
   const void *pResult = UsefulInputBuf_GetBytes(pMe, sizeof(uint8_t));

   return pResult ? *(const uint8_t *)pResult : 0;
}

uint16_t UsefulInputBuf_GetUint16(UsefulInputBuf *pMe)
{
   const uint8_t *pResult = (const uint8_t *)UsefulInputBuf_GetBytes(pMe, sizeof(uint16_t));

   if(!pResult) {
      return 0;
   }

   // See UsefulInputBuf_GetUint64() for comments on this code
#if defined(USEFULBUF_CONFIG_BIG_ENDIAN) || defined(USEFULBUF_CONFIG_HTON) || defined(USEFULBUF_CONFIG_BSWAP)
   uint16_t uTmp = 0;
   memscpy(&uTmp, sizeof(uTmp), pResult, sizeof(uint16_t));

#if defined(USEFULBUF_CONFIG_BIG_ENDIAN)
   return uTmp;

#elif defined(USEFULBUF_CONFIG_HTON)
   return ntohs(uTmp);

#else
   return __builtin_bswap16(uTmp);

#endif

#else
   return  (uint16_t) (((uint16_t)(pResult[0] << 8)) + (uint16_t)pResult[1]);

#endif
}


uint32_t UsefulInputBuf_GetUint32(UsefulInputBuf *pMe)
{
   const uint8_t *pResult = (const uint8_t *)UsefulInputBuf_GetBytes(pMe, sizeof(uint32_t));

   if(!pResult) {
      return 0;
   }

   // See UsefulInputBuf_GetUint64() for comments on this code

#if defined(USEFULBUF_CONFIG_BIG_ENDIAN) || defined(USEFULBUF_CONFIG_HTON) || defined(USEFULBUF_CONFIG_BSWAP)
   uint32_t uTmp = 0;
   memscpy(&uTmp, sizeof(uTmp), pResult, sizeof(uint32_t));

#if defined(USEFULBUF_CONFIG_BIG_ENDIAN)
   return uTmp;

#elif defined(USEFULBUF_CONFIG_HTON)
   return ntohl(uTmp);

#else
   return __builtin_bswap32(uTmp);

#endif

#else
   return ((uint32_t)pResult[0]<<24) +
          ((uint32_t)pResult[1]<<16) +
          ((uint32_t)pResult[2]<<8)  +
           (uint32_t)pResult[3];
#endif
}


uint64_t UsefulInputBuf_GetUint64(UsefulInputBuf *pMe)
{
   const uint8_t *pResult = (const uint8_t *)UsefulInputBuf_GetBytes(pMe, sizeof(uint64_t));

   if(!pResult) {
      return 0;
   }

#if defined(USEFULBUF_CONFIG_BIG_ENDIAN) || defined(USEFULBUF_CONFIG_HTON) || defined(USEFULBUF_CONFIG_BSWAP)
   // pResult will probably not be aligned.  This memscpy() moves the
   // bytes into a temp variable safely for CPUs that can or can't do
   // unaligned memory access. Many compilers will optimize the
   // memscpy() into a simple move instruction.
   uint64_t uTmp = 0;
   memscpy(&uTmp, sizeof(uTmp), pResult, sizeof(uint64_t));

#if defined(USEFULBUF_CONFIG_BIG_ENDIAN)
   // We have been told expliclity this is a big-endian CPU.  Since
   // network byte order is big-endian, there is nothing to do.
   return uTmp;


#elif defined(USEFULBUF_CONFIG_HTON)
   // We have been told to use ntoh(), the system function to handle
   // big- and little-endian. This works on both big- and
   // little-endian machines, but ntoh() is not always available or in
   // a standard place so it is not used by default. On some CPUs the
   // code for this is very compact through use of a special swap
   // instruction.

   return ntohll(uTmp);

#else
   // Little-endian (since it is not USEFULBUF_CONFIG_BIG_ENDIAN) and
   // USEFULBUF_CONFIG_BSWAP (since it is not USEFULBUF_CONFIG_HTON).
   // __builtin_bswap64() and friends are not conditional on CPU
   // endianness so this must only be used on little-endian machines.

   return __builtin_bswap64(uTmp);


#endif

#else
   // This is the default code that works on every CPU and every
   // endianness with no dependency on ntoh().  This works on CPUs
   // that either allow or do not allow unaligned access. It will
   // always work, but usually is a little less efficient than ntoh().

   return   ((uint64_t)pResult[0]<<56) +
            ((uint64_t)pResult[1]<<48) +
            ((uint64_t)pResult[2]<<40) +
            ((uint64_t)pResult[3]<<32) +
            ((uint64_t)pResult[4]<<24) +
            ((uint64_t)pResult[5]<<16) +
            ((uint64_t)pResult[6]<<8)  +
            (uint64_t)pResult[7];
#endif
}


#ifdef FEATURE_ENABLE_QCBOR_FLOAT
float UsefulInputBuf_GetFloat(UsefulInputBuf *pMe)
{
   uint32_t uResult = UsefulInputBuf_GetUint32(pMe);

   return uResult ? UsefulBufUtil_CopyUint32ToFloat(uResult) : 0;
}


double UsefulInputBuf_GetDouble(UsefulInputBuf *pMe)
{
   uint64_t uResult = UsefulInputBuf_GetUint64(pMe);

   return uResult ? UsefulBufUtil_CopyUint64ToDouble(uResult) : 0;
}
#endif


int UsefulInputBuf_GetError(UsefulInputBuf *pMe)
{
   return pMe->err;
}


/*
 Public function -- see UsefulBuf.h
 */
UsefulBufC UsefulBufC_Init(const void *ptr, size_t len)
{
   return (UsefulBufC) { ptr, len };
}

/*
 Public function -- see UsefulBuf.h
 */
UsefulBuf UsefulBuf_Init(void *ptr, size_t len)
{
   return (UsefulBuf) {ptr, len};
}

/*
 Public function -- see UsefulBuf.h
 */
UsefulBufC UsefulBuf_CopyOffset(UsefulBuf Dest, size_t uOffset, const UsefulBufC Src)
{
   // Do this with subtraction so it doesn't give erroneous result if uOffset + Src.len overflows
   if(uOffset > Dest.len || Src.len > Dest.len - uOffset) { // uOffset + Src.len > Dest.len
      return NULLUsefulBufC;
   }

   memscpy((uint8_t *)Dest.ptr + uOffset, Dest.len - uOffset, Src.ptr, Src.len);

   return (UsefulBufC){Dest.ptr, Src.len + uOffset};
}


/*
   Public function -- see UsefulBuf.h
 */
int UsefulBuf_Compare(const UsefulBufC UB1, const UsefulBufC UB2)
{
   // use the comparisons rather than subtracting lengths to
   // return an int instead of a size_t
   if(UB1.len < UB2.len) {
      return -1;
   } else if (UB1.len > UB2.len) {
      return 1;
   } // else UB1.len == UB2.len

   return memcmp(UB1.ptr, UB2.ptr, UB1.len);
}


/*
 Public function -- see UsefulBuf.h
 */
size_t UsefulBuf_IsValue(const UsefulBufC UB, uint8_t uValue)
{
   if(UsefulBuf_IsNULLOrEmptyC(UB)) {
      /* Not a match */
      return 0;
   }

   const uint8_t * const pEnd = (const uint8_t *)UB.ptr + UB.len;
   for(const uint8_t *p = UB.ptr; p < pEnd; p++) {
      if(*p != uValue) {
         /* Byte didn't match */
         return (size_t) (p - (const uint8_t *)UB.ptr);
      }
   }

   /* Success. All bytes matched */
   return SIZE_MAX;
}


/*
 Public function -- see UsefulBuf.h
 */
size_t UsefulBuf_FindBytes(UsefulBufC BytesToSearch, UsefulBufC BytesToFind)
{
   if(BytesToSearch.len < BytesToFind.len) {
      return SIZE_MAX;
   }

   for(size_t uPos = 0; uPos <= BytesToSearch.len - BytesToFind.len; uPos++) {
      if(!UsefulBuf_Compare((UsefulBufC){((const uint8_t *)BytesToSearch.ptr) + uPos, BytesToFind.len}, BytesToFind)) {
         return uPos;
      }
   }

   return SIZE_MAX;
}


/*
 Public function -- see UsefulBuf.h

 Code Reviewers: THIS FUNCTION DOES POINTER MATH
 */
void UsefulOutBuf_Init(UsefulOutBuf *pMe, UsefulBuf Storage)
{
    pMe->magic  = USEFUL_OUT_BUF_MAGIC;
    UsefulOutBuf_Reset(pMe);
    pMe->UB     = Storage;

#if 0
   // This check is off by default.

   // The following check fails on ThreadX

    // Sanity check on the pointer and size to be sure we are not
    // passed a buffer that goes off the end of the address space.
    // Given this test, we know that all unsigned lengths less than
    // me->size are valid and won't wrap in any pointer additions
    // based off of pStorage in the rest of this code.
    const uintptr_t ptrM = UINTPTR_MAX - Storage.len;
    if(Storage.ptr && (uintptr_t)Storage.ptr > ptrM) // Check #0
        me->err = 1;
#endif
}



/*
 Public function -- see UsefulBuf.h

 The core of UsefulOutBuf -- put some bytes in the buffer without writing off the end of it.

 Code Reviewers: THIS FUNCTION DOES POINTER MATH

 This function inserts the source buffer, NewData, into the destination buffer, me->UB.ptr.

 Destination is represented as:
   me->UB.ptr -- start of the buffer
   me->UB.len -- size of the buffer UB.ptr
   me->data_len -- length of value data in UB

 Source is data:
   NewData.ptr -- start of source buffer
   NewData.len -- length of source buffer

 Insertion point:
   uInsertionPos.

 Steps:

 0. Corruption checks on UsefulOutBuf

 1. Figure out if the new data will fit or not

 2. Is insertion position in the range of valid data?

 3. If insertion point is not at the end, slide data to the right of the insertion point to the right

 4. Put the new data in at the insertion position.

 */
void UsefulOutBuf_InsertUsefulBuf(UsefulOutBuf *pMe, UsefulBufC NewData, size_t uInsertionPos)
{
   if(pMe->err) {
      // Already in error state.
      return;
   }

   /* 0. Sanity check the UsefulOutBuf structure */
   // A "counter measure". If magic number is not the right number it
   // probably means me was not initialized or it was corrupted. Attackers
   // can defeat this, but it is a hurdle and does good with very
   // little code.
   if(pMe->magic != USEFUL_OUT_BUF_MAGIC) {
      pMe->err = 1;
      return;  // Magic number is wrong due to uninitalization or corrption
   }

   // Make sure valid data is less than buffer size. This would only occur
   // if there was corruption of me, but it is also part of the checks to
   // be sure there is no pointer arithmatic under/overflow.
   if(pMe->data_len > pMe->UB.len) {  // Check #1
      pMe->err = 1;
      return; // Offset of valid data is off the end of the UsefulOutBuf due to uninitialization or corruption
   }

   /* 1. Will it fit? */
   // WillItFit() is the same as: NewData.len <= (me->size - me->data_len)
   // Check #1 makes sure subtraction in RoomLeft will not wrap around
   if(! UsefulOutBuf_WillItFit(pMe, NewData.len)) { // Check #2
      // The new data will not fit into the the buffer.
      pMe->err = 1;
      return;
   }

   /* 2. Check the Insertion Position */
   // This, with Check #1, also confirms that uInsertionPos <= me->data_len
   if(uInsertionPos > pMe->data_len) { // Check #3
      // Off the end of the valid data in the buffer.
      pMe->err = 1;
      return;
   }

   /* 3. Slide existing data to the right */
   uint8_t *pSourceOfMove       = ((uint8_t *)pMe->UB.ptr) + uInsertionPos; // PtrMath #1
   size_t   uNumBytesToMove     = pMe->data_len - uInsertionPos; // PtrMath #2
   uint8_t *pDestinationOfMove  = pSourceOfMove + NewData.len; // PtrMath #3

   if(uNumBytesToMove && pMe->UB.ptr) {
      // To know memmove won't go off end of destination, see PtrMath #4
      memsmove(pDestinationOfMove, uNumBytesToMove, pSourceOfMove, uNumBytesToMove);
   }

   /* 4. Put the new data in */
   uint8_t *pInsertionPoint = ((uint8_t *)pMe->UB.ptr) + uInsertionPos; // PtrMath #5
   if(pMe->UB.ptr) {
      // To know memmove won't go off end of destination, see PtrMath #6
      memsmove(pInsertionPoint, NewData.len, NewData.ptr, NewData.len);
   }
   pMe->data_len += NewData.len ;
}


/*
 Rationale that describes why the above pointer math is safe

 PtrMath #1 will never wrap around over because
    Check #0 in UsefulOutBuf_Init makes sure me->UB.ptr + me->UB.len doesn't wrap
    Check #1 makes sure me->data_len is less than me->UB.len
    Check #3 makes sure uInsertionPos is less than me->data_len

 PtrMath #2 will never wrap around under because
    Check #3 makes sure uInsertionPos is less than me->data_len

 PtrMath #3 will never wrap around over because   todo
    PtrMath #1 is checked resulting in pSourceOfMove being between me->UB.ptr and a maximum valid ptr
    Check #2 that NewData.len will fit

 PtrMath #4 will never wrap under because
    Calculation for extent or memmove is uRoomInDestination  = me->UB.len - (uInsertionPos + NewData.len)
    Check #3 makes sure uInsertionPos is less than me->data_len
    Check #3 allows Check #2 to be refactored as NewData.Len > (me->size - uInsertionPos)
    This algebraically rearranges to me->size > uInsertionPos + NewData.len

 PtrMath #5 is exactly the same as PtrMath #1

 PtrMath #6 will never wrap under because
    Calculation for extent of memove is uRoomInDestination = me->UB.len - uInsertionPos;
    Check #1 makes sure me->data_len is less than me->size
    Check #3 makes sure uInsertionPos is less than me->data_len
 */


/*
 Public function -- see UsefulBuf.h
 */
UsefulBufC UsefulOutBuf_OutUBuf(UsefulOutBuf *pMe)
{
   if(pMe->err) {
      return NULLUsefulBufC;
   }

   if(pMe->magic != USEFUL_OUT_BUF_MAGIC) {
      pMe->err = 1;
      return NULLUsefulBufC;
   }

   return (UsefulBufC){pMe->UB.ptr, pMe->data_len};
}


/*
 Public function -- see UsefulBuf.h

 Copy out the data accumulated in to the output buffer.
 */
UsefulBufC UsefulOutBuf_CopyOut(UsefulOutBuf *pMe, UsefulBuf pDest)
{
   const UsefulBufC Tmp = UsefulOutBuf_OutUBuf(pMe);
   if(UsefulBuf_IsNULLC(Tmp)) {
      return NULLUsefulBufC;
   }
   return UsefulBuf_Copy(pDest, Tmp);
}




/*
 Public function -- see UsefulBuf.h

 The core of UsefulInputBuf -- consume some bytes without going off the end of the buffer.

 Code Reviewers: THIS FUNCTION DOES POINTER MATH
 */
const void * UsefulInputBuf_GetBytes(UsefulInputBuf *pMe, size_t uAmount)
{
   // Already in error state. Do nothing.
   if(pMe->err) {
      return NULL;
   }

   if(!UsefulInputBuf_BytesAvailable(pMe, uAmount)) {
      // The number of bytes asked for at current position are more than available
      pMe->err = 1;
      return NULL;
   }

   // This is going to succeed
   const void * const result = ((const uint8_t *)pMe->UB.ptr) + pMe->cursor;
   pMe->cursor += uAmount; // this will not overflow because of check using UsefulInputBuf_BytesAvailable()
   return result;
}
