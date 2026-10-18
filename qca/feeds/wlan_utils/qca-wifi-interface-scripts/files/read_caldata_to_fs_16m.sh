#!/bin/sh
#
# Copyright (c) 2015, 2020, The Linux Foundation. All rights reserved.
# Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.

# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.

# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

. /lib/functions.sh
. /lib/create_cfg_caldata.sh

#This API is used only for 16M cases.
#16M platform has very little busy box, so the existing system commands used in primary script
# cannot be used here. So, writing a simple routine to parse the ftm.conf and create caldata
create_cfg_caldata_16m()
{
    local brd_name=$(echo $(board_name) | awk -F '-' '{print $2}')
    local brd=$brd_name$(echo $(board_name) | awk -F "$brd_name" '{print$2}')
    local ftm_conf_path=$(get_config_file_path "caldata")
    local grep_val=$(grep $brd $ftm_conf_path/ftm.conf)
    local num_rows="$(grep -w -c $brd $ftm_conf_path/ftm.conf)"
    local apdk="/tmp"

    # Always initialize with Integrated/Platform FW directory
    ahb_dir=$2

    # Loop to process the output
    for i in `seq 1 $num_rows`
    do
        if [ $i == "2" ]
        then
            ahb_dir=$3
        fi

        #Parse the FTM.conf file and Get the Values
        ROW_VAL=$(echo $grep_val | awk -v i=$i '{print $i}')
        BOARD_ID=$(echo $ROW_VAL | awk -F ',' '{print $2}')
        SLOT_ID=$(echo $ROW_VAL | awk -F ',' '{print $3}')
        OFFSET=$(echo $ROW_VAL | awk -F ',' '{print $4}')
        SIZE=$(echo $ROW_VAL | awk -F ',' '{print $5}')

        echo -e $brd "\t" $BOARD_ID "\t"  $SLOT_ID "\t" $OFFSET "\t" $SIZE

        #Get the BDF size
        BDF_SIZE=$(stat -Lc%s /lib/firmware/"$ahb_dir"/bdwlan.b"$BOARD_ID")

        if [ -z $BDF_SIZE ]
        then
            BDF_SIZE=131072
        fi

        echo "BDF_SIZE -" $BDF_SIZE

        if [ $i == "2" ]
        then
            cmd=$(dd if=$1 of="$apdk"/"$ahb_dir"/caldata_"$SLOT_ID".b"$BOARD_ID" bs=1 count="$BDF_SIZE" skip="$OFFSET")
            cp -f "$apdk"/"$ahb_dir"/caldata_"$SLOT_ID".b"$BOARD_ID" /lib/firmware/"$ahb_dir"/
        else
            cmd=$(dd if=$1 of="$apdk"/"$ahb_dir"/caldata.bin bs=1 count="$BDF_SIZE" skip="$OFFSET")
            cp -f "$apdk"/"$ahb_dir"/caldata.bin /lib/firmware/"$ahb_dir"/
        fi
    done
}

do_load_ipq4019_board_bin()
{
    local brd_name=$(echo $(board_name) | awk -F '-' '{print $2}')
    local board=$brd_name$(echo $(board_name) | awk -F "$brd_name" '{print$2}')
    local mtdblock=$(find_mtd_part 0:ART)

    local apdk="/tmp"

    if [ -z "$mtdblock" ]; then
        # read from mmc
        mtdblock=$(find_mmc_part 0:ART)
    fi

    [ -n "$mtdblock" ] || return

    # load board.bin
    case "$board" in
            ap-mi01.3*|ap-mi04.1*)
                    [ -f /lib/firmware/IPQ5332/caldata.bin ] && return
                    mkdir -p ${apdk}/IPQ5332
                    mkdir -p ${apdk}/qcn6432
                    do_ftm_conf_override

                    if [ -e /sys/firmware/devicetree/base/compressed_art ]
                    then
                        #FTM Daemon compresses the caldata and writes the lzma file in ART Partition
                        dd if=${mtdblock} of=${apdk}/virtual_art.bin.lzma
                        lzma -fdv --single-stream ${apdk}/virtual_art.bin.lzma || {
                        # Create dummy virtual_art.bin file of size 256K
                        dd if=/dev/zero of=${apdk}/virtual_art.bin bs=1024 count=256
                        }

                        create_cfg_caldata_16m "${apdk}/virtual_art.bin" "IPQ5332" "qcn6432" "0" 
                    else
                    	create_cfg_caldata_16m "${mtdblock}" "IPQ5332" "qcn6432" "0" 
                    fi
            ;;
            ap-mi*)
                    [ -f /lib/firmware/IPQ5332/caldata.bin ] && return
                    mkdir -p ${apdk}/IPQ5332
                    create_cfg_caldata_16m "${mtdblock}" "IPQ5332"
            ;;
   esac
}

