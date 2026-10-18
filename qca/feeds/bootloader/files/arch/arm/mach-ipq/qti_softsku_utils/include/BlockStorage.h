// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // for size_t, not part of stdint.h

typedef struct BlockStorage {
    size_t store_len;
    size_t store_count;
    size_t block_size;
    bool read_only;
    int32_t (*read)(struct BlockStorage* self,
                    size_t store,
                    uint8_t* data,
                    size_t data_len);
    int32_t (*write)(struct BlockStorage* self,
                     size_t store,
                     uint8_t const* data,
                     size_t data_len);
    void (*destroy)(struct BlockStorage* self);
} BlockStorage;
