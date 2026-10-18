/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __EDMA_DEBUG_H__
#define __EDMA_DEBUG_H__

#if (EDMA_DEBUG_LEVEL < 1)
#define edma_err(s, ...)
#else
#define edma_err(s, ...) pr_err("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (EDMA_DEBUG_LEVEL < 2)
#define edma_warn(s, ...)
#else
#define edma_warn(s, ...) pr_warn("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (EDMA_DEBUG_LEVEL < 3)
#define edma_info(s, ...)
#else
#define edma_info(s, ...) pr_info("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif

#if (EDMA_DEBUG_LEVEL < 4)
#define edma_debug(s, ...)
#else
#if defined(CONFIG_DYNAMIC_DEBUG)
#define edma_debug(s, ...) pr_debug("%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#else
#define edma_debug(s, ...) printk(KERN_DEBUG"%s[%d]:" s, __func__, __LINE__, ##__VA_ARGS__)
#endif
#endif

#endif	/*__EDMA_DEBUG_H__ */
