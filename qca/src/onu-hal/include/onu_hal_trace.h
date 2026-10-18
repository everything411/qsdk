/*
*
* Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: ISC
*
*/

#ifndef __onu_hal_trace_h__
#define __onu_hal_trace_h__

#pragma GCC system_header

#include <stdio.h>
#include <string.h> // strrchr()

/* Extract basename from file path */
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)

#define SAH_TRACE_ERROR(format, ...)  printf("[x] " format " (%s@%s:%d)\n", ## __VA_ARGS__, __FUNCTION__, __FILENAME__, __LINE__)
#define SAH_TRACE_NOTICE(format, ...) printf("[n] " format " (%s@%s:%d)\n", ## __VA_ARGS__, __FUNCTION__, __FILENAME__, __LINE__)
#define SAH_TRACE_INFO(format, ...)   printf("[i] " format " (%s@%s:%d)\n", ## __VA_ARGS__, __FUNCTION__, __FILENAME__, __LINE__)
#define SAH_TRACE_DEBUG(format, ...)  printf("[d] " format " (%s@%s:%d)\n", ## __VA_ARGS__, __FUNCTION__, __FILENAME__, __LINE__)

#ifndef when_null_trace
#define when_null_trace(cond, label, level, ...) if((cond) == NULL) {SAH_TRACE_ ## level(__VA_ARGS__); goto label; }
#endif

#endif
