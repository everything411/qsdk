// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "include/Crc32.h"
#include "include/DataStoreMgr.h"

#define CRC32_POLYNOMIAL 0x04c11db7
#define CRC32_SEED       0xffffffff

static uint32_t mCrcTable[256];
static bool CrcInitialized = false;

/**
  This internal function reverses bits for 32-bit data.

  @param  Value                 The data to be reversed.

  @return                       Data reversed.

**/
static uint32_t ReverseBits(uint32_t Value)
{
  uint32_t   Index;
  uint32_t  NewValue;

  NewValue = 0;
  for (Index = 0; Index < 32; Index++)
  {
    if ((Value & (1 << Index)) != 0)
    {
      NewValue = NewValue | (1 << (31 - Index));
    }
  }

  return NewValue;
}

/**
  Initialize the CRC32 table.

**/
void qwes_InitializeCrc32Table(void)
{
  uint32_t TableEntry;
  uint32_t Index;
  uint32_t Value;

  if (CrcInitialized == false)
  {
    for (TableEntry = 0; TableEntry < 256; TableEntry++)
    {
      Value = ReverseBits ((uint32_t) TableEntry);
      for (Index = 0; Index < 8; Index++)
      {
        if ((Value & 0x80000000) != 0)
        {
          Value = (Value << 1) ^ CRC32_POLYNOMIAL;
        }
        else
        {
          Value = Value << 1;
        }
      }

      mCrcTable[TableEntry] = ReverseBits (Value);
    }

    CrcInitialized = true;
  }
}

/**
  Calculate CRC32 for data.

  @param Data            Pointer to the data to calculate CRC on.
  @param DataSize        Size in bytes of the data.
  @param CrcOut          Pointer to the CRC32 of the data.

  @retval  DATA_STORE_SUCCESS            The CRC32 for data is calculated successfully.
  @retval  DATA_STORE_INVALID_PARAMETER  Data or CrcOut is NULL.

**/
uint32_t qwes_CalculateCrc32 (void *Data, uint32_t DataSize, uint32_t *CrcOut)
{
  uint32_t Crc;
  uint32_t Index;
  uint8_t  *Ptr;

  if (Data == NULL || DataSize == 0 || CrcOut == NULL)
  {
    return DATA_STORE_INVALID_PARAMETER;
  }

  if (CrcInitialized == false)
  {
    qwes_InitializeCrc32Table();
  }

  Crc = CRC32_SEED;
  for (Index = 0, Ptr = Data; Index < DataSize; Index++, Ptr++)
  {
    Crc = (Crc >> 8) ^ mCrcTable[(uint8_t) Crc ^ *Ptr];
  }

  *CrcOut = Crc ^ CRC32_SEED;

  return DATA_STORE_SUCCESS;
}
