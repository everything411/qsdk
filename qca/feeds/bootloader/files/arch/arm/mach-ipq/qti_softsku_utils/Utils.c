// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

/*===========================================================================

                      EDIT HISTORY FOR FILE

This section contains comments describing changes made to this file.
Notice that changes are listed in reverse chronological order.


when       who     what, where, why
--------   ---     ----------------------------------------------------------
03/28/24   priyma   Initial draft

===========================================================================*/

/*-----------------------------------------------------------------------------------
 INCLUDE FILES FOR MODULE
 -------------------------------------------------------------------------------------*/
#include "include/Utils.h"
#include <malloc.h>
#include <string.h>

/* Forward declaration for boot_license_install function */
extern int boot_license_install(void *ptr, size_t len, uint16_t index);

/*-----------------------------------------------------------------------------------
 MEMORY RELATED FUNCTIONS
 -------------------------------------------------------------------------------------*/
void* _qwes_malloc(size_t size) {
    void* retptr = malloc(size);
    if (retptr == NULL) {
        return NULL;
    }
    memset(retptr, 0, size);
    return retptr;
}

void _qwes_free(void* ptr) {
    if (ptr == NULL)
        return;
    free(ptr);
}

/*-----------------------------------------------------------------------------------
 LICENSE RELATED FUNCTIONS
 -------------------------------------------------------------------------------------*/

void _qwes_install_license(void *ptr, size_t len, uint16_t index) {
    int ret;
    
    if (ptr == NULL) {
        QWES_LOG_ERROR("Invalid license pointer");
        return;
    }
    
    ret = boot_license_install(ptr, len, index);
    if (ret != 0) {
        QWES_LOG_ERROR("License installation failed with error: %d", ret);
    } else {
        QWES_LOG_INFO("License installation completed successfully");
    }
}

/*-----------------------------------------------------------------------------------
 SECURE MEMORY FUNCTIONS
 -------------------------------------------------------------------------------------*/
size_t memscpy(void *dst, size_t dst_size, const void *src, size_t src_size) {
    if (dst == NULL || src == NULL || dst_size == 0) {
        return 0;
    }
    
    size_t copy_size = (src_size < dst_size) ? src_size : dst_size;
    memcpy(dst, src, copy_size);
    return copy_size;
}
/*-----------------------------------------------------------------------------------
  MISC. FUNCTIONS
 -------------------------------------------------------------------------------------*/
// Set result to the next integer >= value that is a multiple of modulus.
// With overflow checks where needed
int32_t roundUp(size_t value, size_t modulus, size_t* result) {
    if (0 == modulus || 0 == value) {
        QWES_LOG_ERROR("range");
        return Object_ERROR;
    }
    size_t remainder = value % modulus;
    size_t needed = remainder ? modulus - remainder : 0;
    if (value <= SIZE_MAX - needed) {
        *result = value + needed;
        return Object_OK;
    }
    QWES_LOG_ERROR("overflow, %zu + %zu", value, needed);
    return Object_ERROR;
}
