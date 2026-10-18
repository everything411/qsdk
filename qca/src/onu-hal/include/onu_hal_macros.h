/*
*
* Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: ISC
*
*/

#ifndef __onu_hal_macros_h__
#define __onu_hal_macros_h__

#ifndef UNUSED
#define UNUSED __attribute__((unused))
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef when_null
#define when_null(x, l) if(x == NULL) { goto l; }
#endif

#endif
