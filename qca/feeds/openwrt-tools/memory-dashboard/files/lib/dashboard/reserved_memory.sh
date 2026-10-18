#!/bin/sh
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: ISC
#
# Script to calculate reserved memory size from device tree reg property
# Usage: ./calculate_reserved_memory.sh [path_to_reg_file]
#        ./calculate_reserved_memory.sh /sys/firmware/devicetree/base/reserved-memory/qcn9224_pcie1@50f00000/reg
#

# Function to format size in human-readable format
format_size() {
    local bytes=$1
    local gb_int=$((bytes / 1024 / 1024 / 1024))
    local mb_int=$((bytes / 1024 / 1024))
    local kb_int=$((bytes / 1024))

    if [ $gb_int -ge 1 ]; then
        local gb=$(awk "BEGIN {printf \"%.2f\", $bytes / 1073741824}")
        echo "$gb GB ($bytes bytes)"
    elif [ $mb_int -ge 1 ]; then
        local mb=$(awk "BEGIN {printf \"%.2f\", $bytes / 1048576}")
        echo "$mb MB ($bytes bytes)"
    elif [ $kb_int -ge 1 ]; then
        local kb=$(awk "BEGIN {printf \"%.2f\", $bytes / 1024}")
        echo "$kb KB ($bytes bytes)"
    else
        echo "$bytes bytes"
    fi
}

# Main script
echo "========================================================================"
echo "                    Reserved Memory Size Calculator"
echo "========================================================================"
echo ""

# Determine input source
if [ -n "$1" ]; then
    REG_FILE="$1"
    if [ ! -f "$REG_FILE" ]; then
        echo "Error: File not found: $REG_FILE"
        exit 1
    fi
    echo "Reading from: $REG_FILE"
else
    # Default path
    REG_FILE="/sys/firmware/devicetree/base/reserved-memory/qcn9224_pcie1@50f00000/reg"
    if [ ! -f "$REG_FILE" ]; then
        echo "Error: Default file not found: $REG_FILE"
        echo "Usage: $0 [path_to_reg_file]"
        exit 1
    fi
    echo "Reading from: $REG_FILE (default)"
fi

echo ""

# Read binary data from reg file and convert to hex
# Device tree reg format (big-endian):
# Bytes 0-3:   Address High (32-bit)
# Bytes 4-7:   Address Low (32-bit)
# Bytes 8-11:  Size High (32-bit)
# Bytes 12-15: Size Low (32-bit)

# Read the file as binary and convert to hex
hex_data=$(hexdump -v -e '4/1 "%02x" " "' "$REG_FILE")

# Parse the hex values using awk
addr_high=$(echo "$hex_data" | awk '{print $1}')
addr_low=$(echo "$hex_data" | awk '{print $2}')
size_high=$(echo "$hex_data" | awk '{print $3}')
size_low=$(echo "$hex_data" | awk '{print $4}')

if [ -z "$size_high" ]; then
    size_high="0"
fi

if [ -z "$size_low" ]; then
    size_low="0"
fi

echo "Raw hex data from reg file:"
echo "  Address High: 0x$addr_high"
echo "  Address Low:  0x$addr_low"
echo "  Size High:    0x$size_high"
echo "  Size Low:     0x$size_low"
echo ""

# Convert to decimal
addr_high_dec=$((0x$addr_high))
addr_low_dec=$((0x$addr_low))
size_high_dec=$((0x$size_high))
size_low_dec=$((0x$size_low))

# For 64-bit calculation without bc, we use awk for large number arithmetic
# Calculate: (high << 32) | low = high * 4294967296 + low
address_dec=$(awk "BEGIN {printf \"%.0f\", $addr_high_dec * 4294967296 + $addr_low_dec}")
size_dec=$(awk "BEGIN {printf \"%.0f\", $size_high_dec * 4294967296 + $size_low_dec}")
end_address_dec=$(awk "BEGIN {printf \"%.0f\", $address_dec + $size_dec}")

# Calculate end address hex components
end_addr_high=$(awk "BEGIN {printf \"%.0f\", int(($addr_high_dec + $size_high_dec + ($addr_low_dec + $size_low_dec) / 4294967296))}")
end_addr_low=$(awk "BEGIN {printf \"%.0f\", ($addr_low_dec + $size_low_dec) % 4294967296}")
end_addr_hex=$(printf "%08x%08x" $end_addr_high $end_addr_low)

echo "Parsed Device Tree Register Property:"
echo "------------------------------------------------------------------------"
echo "Base Address (64-bit): 0x${addr_high}${addr_low}"
echo "                       $address_dec bytes"
echo "                       $(format_size $address_dec)"
echo ""
echo "Size (64-bit):         0x${size_high}${size_low}"
echo "                       $size_dec bytes"
echo "                       $(format_size $size_dec)"
echo ""
echo "End Address (64-bit):  0x${end_addr_hex}"
echo "                       $end_address_dec bytes"
if [ $(basename $REG_FILE) = size ]; then
size_dec="$address_dec"
fi
echo "========================================================================"
echo ""
echo "RESULT: Reserved Memory Size = $(format_size $size_dec)"
if [ -f "$(dirname $REG_FILE)/status" ]; then
 status="$(tr -d '\0' < "$(dirname $REG_FILE)/status")"
 if [ -z "$status" ]; then
echo "RESULT: Status : okay"
 else
echo "RESULT: Status : $status"
 fi
else
echo "RESULT: Status : okay"
fi
echo ""
echo "========================================================================"
