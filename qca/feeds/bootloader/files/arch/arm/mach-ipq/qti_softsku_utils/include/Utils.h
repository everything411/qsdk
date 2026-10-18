// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef Utils_h
#define Utils_h

/*-----------------------------------------------------------------------------------
 INCLUDE FILES FOR MODULE
 -------------------------------------------------------------------------------------*/
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include "UsefulBuf.h"

/* Forward declarations for functions that would normally be in stringl.h */
size_t memscpy(void *dst, size_t dst_size, const void *src, size_t src_size);

/* Define constants that might not be available */
#ifndef INT32_MAX
#define INT32_MAX 2147483647
#endif
#ifndef INT32_MIN
#define INT32_MIN (-2147483647-1)
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE 1
#endif

#include "object.h"

/*-----------------------------------------------------------------------------------
 LOGGING/DEBUG RELATED FUNCTIONS
 -------------------------------------------------------------------------------------*/
/**
 * @brief  The logging levels that are available.
 */
#define QWES_LOG_MSG_DEBUG2 QSEE_LOG_MSG_DEBUG
#define QWES_LOG_MSG_DEBUG QSEE_LOG_MSG_DEBUG
#define QWES_LOG_MSG_INFO QSEE_LOG_MSG_MED
#define QWES_LOG_MSG_WARN QSEE_LOG_MSG_HIGH
#define QWES_LOG_MSG_ERROR QSEE_LOG_MSG_ERROR
#define QWES_LOG_MSG_FATAL QSEE_LOG_MSG_FATAL
/**
 * @brief  The generic logging API.
 */
#define QWES_LOG(v, fmt, ...)                                     \


/**
 * @brief  These macros control logging/debug
 */
#ifdef OFF_TARGET
#define QWES_LOG_ENABLE_DEBUG2
#define QWES_LOG_ENABLE_DEBUG
#define QWES_LOG_ENABLE_INFO
#endif

#define QWES_LOG_ENABLE_WARN
#define QWES_LOG_ENABLE_ERROR
#define QWES_LOG_ENABLE_FATAL
#define QWES_LOG_ENABLE_INFO
//#define QWES_LOG_TRACK_MEMORY
//#define QWES_DEBUG_ENABLE

/**
 * @brief  Logging definitions available.
 *
 * @note   QWES_LOG and all the verbosity definitions must be given in platform
 *         dependent common utils.
 */
#ifdef QWES_LOG_ENABLE_DEBUG2
#define QWES_LOG_DEBUG2(...) QWES_LOG(QWES_LOG_MSG_DEBUG2, ##__VA_ARGS__)
#else
#define QWES_LOG_DEBUG2(...)
#endif

#ifdef QWES_LOG_ENABLE_DEBUG
#define QWES_LOG_DEBUG(...) QWES_LOG(QWES_LOG_MSG_DEBUG, ##__VA_ARGS__)
#else
#define QWES_LOG_DEBUG(...)
#endif

#ifdef QWES_LOG_ENABLE_INFO
#define QWES_LOG_INFO(...) QWES_LOG(QWES_LOG_MSG_INFO, ##__VA_ARGS__)
#else
#define QWES_LOG_INFO(...)
#endif

#ifdef QWES_LOG_ENABLE_WARN
#define QWES_LOG_WARN(...) QWES_LOG(QWES_LOG_MSG_WARN, ##__VA_ARGS__)
#else
#define QWES_LOG_WARN(...)
#endif

#ifdef QWES_LOG_ENABLE_ERROR
#define QWES_LOG_ERROR(...) QWES_LOG(QWES_LOG_MSG_ERROR, ##__VA_ARGS__)
#else
#define QWES_LOG_ERROR(...)
#endif

#ifdef QWES_LOG_ENABLE_FATAL
#define QWES_LOG_FATAL(...) QWES_LOG(QWES_LOG_MSG_FATAL, ##__VA_ARGS__)
#else
#define QWES_LOG_FATAL(...)
#endif

#ifndef Object_ERROR_MEM
#define Object_ERROR_MEM 5
#endif

	/*-----------------------------------------------------------------------------------
 ERROR CHECKING RELATED FUNCTIONS
 -------------------------------------------------------------------------------------*/

/**
 * @brief  Macro to check the return status of a function and break from the loop
 *         if there is an error
 */
#define CHECK_STATUS(func, fmt, ...)        \
    status = func;                          \
    if (status != 0) {                      \
        QWES_LOG_ERROR(fmt, ##__VA_ARGS__); \
        break;                              \
    }

/**
 * @brief  Macro to check a condition and set error if condition is True
 */
#define CHECK_CONDITION(cond, err, fmt, ...)   \
    if (cond) {                                \
        CHECK_STATUS(err, fmt, ##__VA_ARGS__); \
    }

/**
 * @brief  Macro to check integer addition overflow
 */
#define CHECK_OVERFLOW_UINT32(a, b, c, err, fmt, ...)            \
    CHECK_CONDITION(a > UINT32_MAX - b, err, fmt, ##__VA_ARGS__) \
    c = a + b;

/**
 * @brief  Macro to check integer subtraction underflow
 */
#define CHECK_UNDERFLOW_UINT32(a, b, c, err, fmt, ...) \
    CHECK_CONDITION(a < b, err, fmt, ##__VA_ARGS__)    \
    c = a - b;

/**
 * @brief  Macro to check integer multiplication overflow
 */
#define CHECK_MULTIPLY_OVERFLOW_UINT32(a, b, c, err, fmt, ...) \
    c = a * b;                                                 \
    CHECK_CONDITION(((a != 0) && ((c / a) != b)), err, fmt, ##__VA_ARGS__)

/**
 * @brief  Macro to check integer division overflow
 */
#define CHECK_DIVISION_OVERFLOW_UINT32(a, b, c, err, fmt, ...) \
    if (b != 0) {                                              \
        c = a / b;                                             \
    }                                                          \
    CHECK_CONDITION(((b == 0) || ((c * b) != a)), err, fmt, ##__VA_ARGS__)

/*-----------------------------------------------------------------------------------
 MEMORY RELATED FUNCTIONS
 -------------------------------------------------------------------------------------*/
/**
 * @brief  This function allocates memory based on platform. Similar to "malloc".
 *
 * @param size[in]            The size of memory to be allocated.
 *
 * @return allocated pointer on success, NULL pointer on failure
 */
void* _qwes_malloc(size_t size);

/**
 * @brief  This function deallocates memory pointed to by the pointer given. Similar to
 * "free".
 *
 * @param ptr[in]            The pointer to be deallocated.
 *
 * @return 0 on success, 1 on failure
 */
void _qwes_free(void* ptr);

void _qwes_install_license(void *ptr, size_t len, uint16_t index);

static inline int32_t checked_add_uint64(uint64_t op1, uint64_t op2, uint64_t* res) {
    if (op1 <= UINT64_MAX - op2) {
        *res = op1 + op2;
        return Object_OK;
    }

    return Object_ERROR_SIZE_IN;
}

/**
 * @brief Adds 2 size_t numbers only if they don't overflow
 *
 * @param op1[in] First operand
 * @param op2[in] Second operand
 * @param res[out] Result if op1 + op2 doesn't overflow
 *
 * @return Object_OK on success, Object_ERROR_MAXDATA on failure
 */
static inline int32_t checked_add_size(size_t op1, size_t op2, size_t* res) {
    if (op1 <= SIZE_MAX - op2) {
        *res = op1 + op2;
        return Object_OK;
    }

    return Object_ERROR_SIZE_IN;
}

/**
 * @brief Adds 2 size_t numbers, saturating on overflow
 *
 * @param op1[in] First operand
 * @param op2[in] Second operand
 */
static inline size_t saturate_add_size(size_t op1, size_t op2) {
    if (op1 <= SIZE_MAX - op2) {
        return op1 + op2;
    }
    return SIZE_MAX;
}

/**
 * @brief Multiplies 2 size_t numbers only if they don't overflow
 *
 * @param op1[in] First operand
 * @param op2[in] Second operand
 * @param res[out] Result if op1 * op2 doesn't overflow
 *
 * @return Object_OK on success, Object_ERROR_MAXDATA on failure
 */
static inline int32_t checked_mult_size(size_t op1, size_t op2, size_t* res) {
    if (op2 == 0 || op1 <= SIZE_MAX / op2) {
        *res = op1 * op2;
        return Object_OK;
    }

    return Object_ERROR_SIZE_IN;
}

/**
 * @brief Multiplies 2 signed int numbers only if they don't overflow
 *
 * @param op1[in] First operand
 * @param op2[in] Second operand
 * @param res[out] Result if op1 * op2 doesn't overflow
 *
 * @return Object_OK on success, Object_ERROR_MAXDATA on failure
 */
static inline int32_t checked_mult_int(int op1, int op2, int* res) {
    long long int result = (long long int)op1 * (long long int)op2;

    if ((result <= INT32_MAX) && (result >= INT32_MIN)) {
        *res = op1 * op2;
        return Object_OK;
    }

    return Object_ERROR_SIZE_IN;
}

static inline int32_t checked_mult_uint32(uint32_t op1, uint32_t op2, uint32_t* res) {
    if (op2 == 0 || op1 <= UINT32_MAX / op2) {
        *res = op1 * op2;
        return Object_OK;
    }

    return Object_ERROR_SIZE_IN;
}

#ifdef QWES_LOG_TRACK_MEMORY
#define qwes_malloc(size) \
    _qwes_malloc(size);   \
    QWES_LOG_DEBUG("MLLC")
#else
#define qwes_malloc(size) _qwes_malloc(size)
#endif

#ifdef QWES_LOG_TRACK_MEMORY
#define qwes_free(ptr) \
    _qwes_free(ptr);   \
    QWES_LOG_DEBUG("FREE")
#else
#define qwes_free(ptr) _qwes_free(ptr)
#endif

#define qwes_install_license _qwes_install_license 

static inline void qwes_shred(UsefulBuf* bufferToShred) {
    if (bufferToShred != NULL && bufferToShred->ptr != NULL) {
        UsefulBuf_Set(*bufferToShred, 0);
        qwes_free(bufferToShred->ptr);
        *bufferToShred = NULLUsefulBuf;
    }
}

static inline void qwes_shred_const(UsefulBufC* bufferToShred) {
    UsefulBuf unconstBufferToShred =
        UsefulBuf_Init((void *)bufferToShred->ptr, bufferToShred->len);
    qwes_shred(&unconstBufferToShred);
    *bufferToShred = NULLUsefulBufC;
}

static inline uint64_t ntoh_bytealign_64(void const* ptr, size_t offset) {
    uint8_t const* pVal = (uint8_t const*)ptr + offset;
    // This is highly portable and will always work on any CPU with any compiler
    return ((uint64_t)pVal[0] << 56) | ((uint64_t)pVal[1] << 48) |
           ((uint64_t)pVal[2] << 40) | ((uint64_t)pVal[3] << 32) |
           ((uint64_t)pVal[4] << 24) | ((uint64_t)pVal[5] << 16) |
           ((uint64_t)pVal[6] << 8) | pVal[7];
}

static inline uint32_t ntoh_bytealign_32(void const* ptr, size_t offset) {
    uint8_t const* pVal = (uint8_t const*)ptr + offset;
    // This is highly portable and will always work on any CPU with any compiler
    return ((uint32_t)pVal[0] << 24) | ((uint32_t)pVal[1] << 16) |
           ((uint32_t)pVal[2] << 8) | pVal[3];
}

int32_t roundUp(size_t value, size_t modulus, size_t* result);

// Modifies detination length
static inline int32_t Ext_UsefulBuf_Copy_Mod(UsefulBuf *Dest, UsefulBufC Src) {
    const size_t uOffset = 0;

    if(uOffset > Dest->len || Src.len > Dest->len - uOffset) {
        return Object_ERROR;
    }

    memscpy((uint8_t *)Dest->ptr + uOffset, Dest->len - uOffset, Src.ptr, Src.len);

    Dest->len = Src.len + uOffset;
    return Object_OK;
}

/*-----------------------------------------------------------------------------------
 General-purpose macros
 -------------------------------------------------------------------------------------*/

#define STRINGIFY(x) #x
#define TOSTRING(x) STRINGIFY(x)

#endif /* Utils_h */
