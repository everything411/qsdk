#!/bin/sh
: '
 Copyright (c) 2020, The Linux Foundation. All rights reserved.
 Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.

 Permission to use, copy, modify, and/or distribute this software for any
 purpose with or without fee is hereby granted, provided that the above
 copyright notice and this permission notice appear in all copies.

 THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
'
SERVER=$(fw_printenv serverip | cut -c10-24)
TIMESTAMP=$(date +%y%m%d%H%M%S)$(printf "%04d" $((RANDOM % 10000)))
DUMP_LOCATION=/tmp
COMPRESS_ENABLE_sysfs=/sys/kernel/debug/ath12k/compress_fw_dump
ini_path=/ini/global.ini

get_uptime() {
        awk '{print $1}' /proc/uptime
}

if [ -z "$SERVER" ]; then
    echo "Wrong configuration: SERVER is empty" > /dev/console
    exit 1
fi

COREDUMP_PATH="/sys/class/devcoredump"
LATEST_FILE=$(basename "$DEVPATH")

if [ -n "$LATEST_FILE" ] && [ -e "$COREDUMP_PATH/$LATEST_FILE/data" ]; then
    TARGET_PATH="$COREDUMP_PATH/$LATEST_FILE/failing_device"
    hdr=$(dd if=$COREDUMP_PATH/$LATEST_FILE/data bs=4 count=1 2>/dev/null)
    hdr=${hdr:1}
    if [ "$hdr" = "ELF" ]; then
       file_prefix="q6dump"
   else
       file_prefix="q6dbgdump"
   fi
    if [ -e "$TARGET_PATH/subsystem_device" ]; then
        # Extract the hexadecimal value (without '0x') from the file 'subsystem_device'
        target=$(sed -n 's/.*0x\([0-9a-fA-F]*\).*/\1/p' "$TARGET_PATH/subsystem_device")
        TARGET_PATH_N=$(readlink -n "$TARGET_PATH")
        pci_path=$(basename "$TARGET_PATH_N")
        pci_slot=$(echo "$pci_path" | awk '{print substr($0, 4, 1)}')

        FILENAME="${file_prefix}-${target}-pci${pci_slot}-${TIMESTAMP}.bin"
    else
        BOARD_NAME_PATH="/tmp/sysinfo/board_name"
        PD_PATH="$TARGET_PATH"

        if [ -e "$PD_PATH/name" ]; then
            target=$(awk -F'[,-]' '{print $2}' "$BOARD_NAME_PATH")
            if [ -f "$PD_PATH/firmware" ] && grep -q "cdsp" "$PD_PATH/firmware" 2>/dev/null; then
                target_num=$(echo "$target" | sed 's/[^0-9]//g')
                [ -z "$target_num" ] && target_num="0"
                FILENAME="ramdump_cdsp${target_num}_${TIMESTAMP}.elf"
            else
                FILENAME="${file_prefix}-${target}-rootpd-${TIMESTAMP}.bin"
            fi
        else
            target=$(awk -F'[,-]' '{print $2}' "$BOARD_NAME_PATH")
            pd_name=$(awk -F'_' '{print $NF}' "$PD_PATH/of_node/qcom,userpd-subsys-name")
            FILENAME="${file_prefix}-${target}-${pd_name}-${TIMESTAMP}.bin"
        fi
    fi

    DUMPPATH="$COREDUMP_PATH/$LATEST_FILE/data"
fi

if [ -n "$FILENAME" ]; then

    sysfs_param=0
    ini_param=0
    if [ -f "$COMPRESS_ENABLE_sysfs" ]; then
        sysfs_param=$(cat "$COMPRESS_ENABLE_sysfs")
    fi
    if [ -f "$ini_path" ]; then
        ini_param=$(awk -F'=' '/^compress_fw_dump/{print $2}' "$ini_path")
    fi
    if [ "$sysfs_param" = "1" ] || [ "$ini_param" = "1" ]; then
        compress_dump_enable=1
        COMPR_DUMP_NAME=$FILENAME.gz
        #Setting Max size of Compressed Wkk Q6 Memory
        DUMP_MAX_SIZE=30
    else
        compress_dump_enable=0
    fi

    MEM_AVAILABLE=$(df -Ph $DUMP_LOCATION | awk 'NR==2 {print $4}')
    MEM_AVAILABLE=${MEM_AVAILABLE%.*}

    if [ "$compress_dump_enable" = "1" ] && [ ! $MEM_AVAILABLE -gt $DUMP_MAX_SIZE ]; then
        echo "[ $(get_uptime)] Compression skipped: insufficient memory (available=${MEM_AVAILABLE}M, required=${DUMP_MAX_SIZE}M)" > /dev/console
        compress_dump_enable=0
    fi

    if [ "$compress_dump_enable" = "1" ]; then
        $(dd if=$DUMPPATH | gzip -c -1 > $DUMP_LOCATION/$COMPR_DUMP_NAME)
        echo "[ $(get_uptime)] Collecting compressed version" > /dev/console
        COMPR_SIZE=$(du -sk $DUMP_LOCATION/$COMPR_DUMP_NAME | cut -f1)
        if [ "$COMPR_SIZE" -gt "$((DUMP_MAX_SIZE * 1024))" ]; then
            echo "[ $(get_uptime)] Compressed dump size: $COMPR_SIZE greater than expected" > /dev/console
        fi
        echo "[ $(get_uptime)] Collecting $COMPR_DUMP_NAME to $SERVER" > /dev/console
        echo 1 > $DUMPPATH
        (
          if tftp -l "$DUMP_LOCATION/$COMPR_DUMP_NAME" -r "$COMPR_DUMP_NAME" -p "$SERVER" 2>&1; then
                echo "[ $(get_uptime)] $COMPR_DUMP_NAME collected to $SERVER" > /dev/console
          else
                echo "[ $(get_uptime)] $COMPR_DUMP_NAME collection failed to $SERVER" > /dev/console
          fi
          rm $DUMP_LOCATION/$COMPR_DUMP_NAME
        ) &
    else
        echo "[ $(get_uptime)] Collecting dump directly on backbone" > /dev/console
        echo "[ $(get_uptime)] Collecting $FILENAME to $SERVER" > /dev/console
        (
          if tftp -l "$DUMPPATH" -r "$FILENAME" -p "$SERVER" 2>&1; then
                echo "[ $(get_uptime)] $FILENAME collected to $SERVER" > /dev/console
          else
                echo "[ $(get_uptime)] $FILENAME collection failed to $SERVER" > /dev/console
          fi
          echo 1 > $DUMPPATH
        ) &
    fi
fi
