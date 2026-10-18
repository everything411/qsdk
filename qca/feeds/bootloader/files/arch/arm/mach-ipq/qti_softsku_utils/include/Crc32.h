// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef __CRC32_H__
#define __CRC32_H__

#include <linux/types.h>

/* U-Boot compatible type definitions */
#ifndef uint32_t
#define uint32_t u32
#endif

/**
  gQcomTokenSpaceGuid GUID definition.
 */
#define QCOM_TOKEN_SPACE_GUID \
        { 0x882f8c2b, 0x9646, 0x435f, { 0x8d, 0xe5, 0xf2, 0x08, 0xff, 0x80, 0xc1, 0xbd } }

/**
  Initialize CRC32 table.

**/
void
qwes_InitializeCrc32Table (
  void
  );

/**
  Calculate CRC32 for data.

  @param Data            Pointer to the data to calculate CRC on.
  @param DataSize        Size in bytes of the data.
  @param CrcOut          Pointer to the CRC32 of the data.

  @retval  DATA_STORE_SUCCESS            The CRC32 for data is calculated successfully.
  @retval  DATA_STORE_INVALID_PARAMETER  Data or CrcOut is NULL.

**/
uint32_t
qwes_CalculateCrc32 (
  void    *Data,
  uint32_t   DataSize,
  uint32_t  *CrcOut
  );

#endif /* __CRC32_H__ */
