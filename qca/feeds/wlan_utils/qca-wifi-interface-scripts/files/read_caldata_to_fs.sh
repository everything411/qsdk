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

is_ftm_conf_supported() {
	local brd_name=$(echo $(board_name) | awk -F '-' '{print $2}')
	local board=$brd_name$(echo $(board_name) | awk -F "$brd_name" '{print$2}')
        local ftm_conf_path=$(get_config_file_path "caldata")

	case "$board" in
       ap-mi*|ap-al02-c4*|ap-al02-c6*|ap-al06*|ap-al05*|ap-al02-c7*|ap-al02-c8*|ap-al02-c9*|ap-al02-c10*|ap-al02-c11*|ap-al02-c12*|ap-al02-c14*|ap-al02-c15*|ap-al02-c16*|ap-al02-c20*|ap-al03-c1*|ap-al03-c2*|db-mi02.1*|ap-sdxpinn-qcn9224*|sdxkova-qcn9224*|rdp466*|rdp485*|rdp487*|rdp498*|rdp500*|rdp501*|tb-mi03.1*|tb-mi05.1*|rdp496*|ap-al02-c13*|ap-al02-c19*)
		ln -s $ftm_conf_path/ftm.conf /tmp/ftm.conf
		;;

#Starting with 11bn RDPs, ART is compressed by default. Add 11bn attach RDPs to below list
	# rdp492 keeps the first 4 KB of ART reserved for raw Ethernet/PHY information.
	# Only the WLAN caldata region after this offset is compressed, and downstream
	# scripts rebuild a single virtual_art.bin view after split/decompress processing.
	rdp492*)
		ln -s $ftm_conf_path/ftm.conf /tmp/ftm.conf
		# ART partition size for legacy platforms.
		cat <<-EOF > /tmp/art.conf
		ART_COMPRESSION=1
		ART_SLOT_OFFSET_KB=4
		ART_PARTITION_SIZE_KB=1024
		EOF
	;;
	# These 11bn RDPs keep the first 10 KB of ART reserved for raw Ethernet
	# information such as MAC address data followed by CALDATA metadata. Only the WLAN
	# caldata region after this offset is compressed, and downstream scripts
	# rebuild a single virtual_art.bin view after split/decompress processing.
	# Selected 11bn RDPs additionally migrate ART to a UBI volume when the ART
	# flash backing is NAND, while preserving the same logical ART payload layout.
	# ART_PARTITION_SIZE_KB only defines the usable size of ART partition, specifically in case
	# of NAND flash where UBIFS is enabled, the actual partition size to be higher
	# to allow for reserved blocks for bad block management
	# ART_UBI_BEB_LIMIT to be defined based on the ART partition size, approximately 1 per MB of flash size
	# i.e., overall 2 PEBs per MB of flash size.
	# IPQ52xx
	rdp499*|rdp502*|rdp503*|rdp504*|rdp505*|rdp511*|rdp512*)
		ln -s $ftm_conf_path/ftm.conf /tmp/ftm.conf
		# Maximum ART partition size among IPQ52xx.
		cat <<-EOF > /tmp/art.conf
		ART_COMPRESSION=1
		ART_SLOT_OFFSET_KB=10
		ART_PARTITION_SIZE_KB=2048
		ART_UBIFS=1
		ART_UBI_BEB_LIMIT=4
		EOF
		ensure_art_ubi_ready >/dev/null 2>&1 || true
	;;
	# IPQ96xx
	rdp488*|rdp489*|rdp506*|rdp507*|rdp490*|rdp491*|rdp525*|rdp529*|rdp530*|rdp532*)
		ln -s $ftm_conf_path/ftm.conf /tmp/ftm.conf
		# Maximum ART partition size among IPQ96xx.
		cat <<-EOF > /tmp/art.conf
		ART_COMPRESSION=1
		ART_SLOT_OFFSET_KB=10
		ART_PARTITION_SIZE_KB=5120
		ART_UBIFS=1
		ART_UBI_BEB_LIMIT=5
		EOF
		ensure_art_ubi_ready >/dev/null 2>&1 || true
	;;
	#Amboseli
	echo-qmp5-v1*)
		ln -s $ftm_conf_path/ftm.conf /tmp/ftm.conf
		cat <<-EOF > /tmp/art.conf
		ART_COMPRESSION=1
		ART_SLOT_OFFSET_KB=10
		ART_PARTITION_SIZE_KB=2250
		ART_UBIFS=1
		ART_UBI_BEB_LIMIT=4
		EOF
		ensure_art_ubi_ready >/dev/null 2>&1 || true
	;;
	*)
		echo "ftm.conf file is not supported for $board " > /dev/console
                rm -rf $ftm_conf_path/ftm.conf
		;;
	esac

	# Record which directory (base or _v2) is the latest-present version of each
	# chip family, so caldata retrieval/symlinking always targets the correct
	# directory when both base and _v2 variants co-exist on the same device.
	if [ -f /tmp/art.conf ]; then
		for chip in qcn9625 qcn9589; do
			if [ -d /lib/firmware/$chip ] || [ -d /lib/firmware/${chip}_v2 ]; then
				local chip_version="$chip"
				[ -d /lib/firmware/${chip}_v2 ] && chip_version="${chip}_v2"
				echo "ART_CHIP_VERSION_${chip}=${chip_version}" >> /tmp/art.conf
			fi
		done
	fi
}

is_ftm_conf_supported

do_load_ipq4019_board_bin()
{

    local brd_name=$(echo $(board_name) | awk -F '-' '{print $2}')
    local board=$brd_name$(echo $(board_name) | awk -F "$brd_name" '{print$2}')
    local mtdblock

    local apdk="/tmp"

    mtdblock=$(get_art_read_device)

    [ -n "$mtdblock" ] || return

    # load board.bin
    case "$board" in
            ap-dk0*)
                    mkdir -p ${apdk}
                    dd if=${mtdblock} of=${apdk}/wifi0.caldata bs=32 count=377 skip=128
                    dd if=${mtdblock} of=${apdk}/wifi1.caldata bs=32 count=377 skip=640
            ;;
            ap16* | ap148*)
                    mkdir -p ${apdk}
                    dd if=${mtdblock} of=${apdk}/wifi0.caldata bs=32 count=377 skip=128
                    dd if=${mtdblock} of=${apdk}/wifi1.caldata bs=32 count=377 skip=640
                    dd if=${mtdblock} of=${apdk}/wifi2.caldata bs=32 count=377 skip=1152
            ;;
            ap-hk14 | ap-hk01-c6)
                    [ -f /lib/firmware/IPQ8074/caldata.bin ] && return
                    FILESIZE=131072
                    mkdir -p ${apdk}/IPQ8074
                    dd if=${mtdblock} of=${apdk}/IPQ8074/caldata.bin bs=1 count=$FILESIZE skip=4096
                    cp ${apdk}/IPQ8074/caldata.bin /lib/firmware/IPQ8074/caldata.bin

                    mkdir -p ${apdk}/qcn9000
                    dd if=${mtdblock} of=${apdk}/qcn9000/caldata_1.bin bs=1 count=$FILESIZE skip=157696
                    cp ${apdk}/qcn9000/caldata_1.bin /lib/firmware/qcn9000/caldata_1.bin
            ;;
            ap-hk01-*)
                    [ -f /lib/firmware/IPQ8074/caldata.bin ] && return
                    HK_BD_FILENAME=/lib/firmware/IPQ8074/bdwlan.bin
                    mkdir -p ${apdk}/IPQ8074
                    if [ -f "$HK_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$HK_BD_FILENAME")
                    else
                        FILESIZE=131072
                    fi
                    dd if=${mtdblock} of=${apdk}/IPQ8074/caldata.bin bs=1 count=$FILESIZE skip=4096
                    [ -L /lib/firmware/IPQ8074/caldata.bin ] || \
                    cp ${apdk}/IPQ8074/caldata.bin /lib/firmware/IPQ8074/caldata.bin
            ;;
            ap-hk10-*)
                    [ -f /lib/firmware/IPQ8074/caldata.bin ] && return
                    FILESIZE=131072
                    mkdir -p ${apdk}/IPQ8074
                    dd if=${mtdblock} of=${apdk}/IPQ8074/caldata.bin bs=1 count=$FILESIZE skip=4096
                    cp ${apdk}/IPQ8074/caldata.bin /lib/firmware/IPQ8074/caldata.bin

                    mkdir -p ${apdk}/qcn9000
                    dd if=${mtdblock} of=${apdk}/qcn9000/caldata_1.bin bs=1 count=$FILESIZE skip=157696
                    dd if=${mtdblock} of=${apdk}/qcn9000/caldata_2.bin bs=1 count=$FILESIZE skip=311296
                    cp ${apdk}/qcn9000/caldata_1.bin /lib/firmware/qcn9000/caldata_1.bin
                    cp ${apdk}/qcn9000/caldata_2.bin /lib/firmware/qcn9000/caldata_2.bin
	    ;;
            ap-hk* | ap-ac* | ap-oa*)
                    [ -f /lib/firmware/IPQ8074/caldata.bin ] && return
                    HK_BD_FILENAME=/lib/firmware/IPQ8074/bdwlan.bin
                    mkdir -p ${apdk}/IPQ8074
                    dd if=${mtdblock} of=${apdk}/wifi1.caldata bs=1 count=12064 skip=208896
                    if [ -f "$HK_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$HK_BD_FILENAME")
                    else
                        FILESIZE=131072
                    fi
                    dd if=${mtdblock} of=${apdk}/IPQ8074/caldata.bin bs=1 count=$FILESIZE skip=4096
                    [ -L /lib/firmware/IPQ8074/caldata.bin ] || \
                    cp ${apdk}/IPQ8074/caldata.bin /lib/firmware/IPQ8074/caldata.bin
            ;;
            ap-cp01-c3*)
                    [ -f /lib/firmware/IPQ6018/caldata.bin ] && return
                    CP_BD_FILENAME=/lib/firmware/IPQ6018/bdwlan.bin
                    mkdir -p ${apdk}/IPQ6018
                    if [ -f "$CP_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$CP_BD_FILENAME")
                    else
                        FILESIZE=65536
                    fi
                    dd if=${mtdblock} of=${apdk}/IPQ6018/caldata.bin bs=1 count=$FILESIZE skip=4096
                    [ -L /lib/firmware/IPQ6018/caldata.bin ] || \
                    cp ${apdk}/IPQ6018/caldata.bin /lib/firmware/IPQ6018/caldata.bin

                    mkdir -p ${apdk}/qcn9000
                    FILESIZE=131072
                    dd if=${mtdblock} of=${apdk}/qcn9000/caldata_1.bin bs=1 count=$FILESIZE skip=157696
                    cp ${apdk}/qcn9000/caldata_1.bin /lib/firmware/qcn9000/caldata_1.bin
            ;;
            ap-cp01-c5*)
                    [ -f /lib/firmware/IPQ6018/caldata.bin ] && return
                    CP_BD_FILENAME=/lib/firmware/IPQ6018/bdwlan.bin
                    mkdir -p ${apdk}/IPQ6018
                    if [ -f "$CP_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$CP_BD_FILENAME")
                    else
                        FILESIZE=65536
                    fi
                    dd if=${mtdblock} of=${apdk}/IPQ6018/caldata.bin bs=1 count=$FILESIZE skip=4096
                    [ -L /lib/firmware/IPQ6018/caldata.bin ] || \
                    cp ${apdk}/IPQ6018/caldata.bin /lib/firmware/IPQ6018/caldata.bin

                    mkdir -p ${apdk}/qcn9000
                    FILESIZE=131072
                    dd if=${mtdblock} of=${apdk}/qcn9000/caldata_1.bin bs=1 count=$FILESIZE skip=157696
                    dd if=${mtdblock} of=${apdk}/qcn9000/caldata_2.bin bs=1 count=$FILESIZE skip=311296
                    cp ${apdk}/qcn9000/caldata_1.bin /lib/firmware/qcn9000/caldata_1.bin
                    cp ${apdk}/qcn9000/caldata_2.bin /lib/firmware/qcn9000/caldata_2.bin
            ;;
            ap-mp02.1*)
                    MP_BD_FILENAME=/lib/firmware/IPQ5018/bdwlan.bin
                    mkdir -p ${apdk}/IPQ5018
                    if [ -f "$MP_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$MP_BD_FILENAME")
                    else
                        FILESIZE=131072
                    fi

                    #FTM Daemon compresses the caldata and writes the lzma file in ART Partition
                    dd if=${mtdblock} of=${apdk}/virtual_art.bin.lzma
                    lzma -fdv --single-stream ${apdk}/virtual_art.bin.lzma || {
                            # Create dummy virtual_art.bin file of size 512K
                            dd if=/dev/zero of=${apdk}/virtual_art.bin bs=1024 count=512
                    }

                    dd if=${apdk}/virtual_art.bin of=${apdk}/IPQ5018/caldata.bin bs=1 count=$FILESIZE skip=4096

                    mkdir -p ${apdk}/qcn6122
                    # Read after 154KB
                    dd if=${apdk}/virtual_art.bin of=${apdk}/qcn6122/caldata_1.bin bs=1 count=$FILESIZE skip=157696
                    # Read after 304KB
                    dd if=${apdk}/virtual_art.bin of=${apdk}/qcn6122/caldata_2.bin bs=1 count=$FILESIZE skip=311296

                    ln -s ${apdk}/IPQ5018/caldata.bin /lib/firmware/IPQ5018/caldata.bin
                    ln -s ${apdk}/qcn6122/caldata_1.bin /lib/firmware/qcn6122/caldata_1.bin
                    ln -s ${apdk}/qcn6122/caldata_2.bin /lib/firmware/qcn6122/caldata_2.bin
            ;;
            ap-mp03.1)
                    [ -f /lib/firmware/IPQ5018/caldata.bin ] && return
                    mkdir -p ${apdk}/IPQ5018
                    FILESIZE=131072

                    if [ -e /sys/firmware/devicetree/base/compressed_art ]
                    then
                        #FTM Daemon compresses the caldata and writes the lzma file in ART Partition
                        dd if=${mtdblock} of=${apdk}/virtual_art.bin.lzma
                        lzma -fdv --single-stream ${apdk}/virtual_art.bin.lzma || {
                        # Create dummy virtual_art.bin file of size 512K
                        dd if=/dev/zero of=${apdk}/virtual_art.bin bs=1024 count=512
                        }

                        dd if=${apdk}/virtual_art.bin of=${apdk}/IPQ5018/caldata.bin bs=1 count=$FILESIZE skip=4096

                        mkdir -p ${apdk}/qcn9000
                        # Read after 154KB
                        dd if=${apdk}/virtual_art.bin of=${apdk}/qcn9000/caldata_1.bin bs=1 count=$FILESIZE skip=157696
                    else
                        dd if=${mtdblock} of=${apdk}/IPQ5018/caldata.bin bs=1 count=$FILESIZE skip=4096

                        mkdir -p ${apdk}/qcn9000
                        dd if=${mtdblock} of=${apdk}/qcn9000/caldata_1.bin bs=1 count=$FILESIZE skip=157696
                    fi

                    cp ${apdk}/IPQ5018/caldata.bin /lib/firmware/IPQ5018/caldata.bin
                    cp ${apdk}/qcn9000/caldata_1.bin /lib/firmware/qcn9000/caldata_1.bin
            ;;
            ap-mp03.1-* | ap-mp03.6*)
                    [ -f /lib/firmware/IPQ5018/caldata.bin ] && return
                    MP_BD_FILENAME=/lib/firmware/IPQ5018/bdwlan.bin
                    mkdir -p ${apdk}/IPQ5018
                    if [ -f "$MP_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$MP_BD_FILENAME")
                    else
                        FILESIZE=131072
                    fi
                    dd if=${mtdblock} of=${apdk}/IPQ5018/caldata.bin bs=1 count=$FILESIZE skip=4096
                    cp ${apdk}/IPQ5018/caldata.bin /lib/firmware/IPQ5018/caldata.bin

                    mkdir -p ${apdk}/qcn9000
                    dd if=${mtdblock} of=${apdk}/qcn9000/caldata_1.bin bs=1 count=$FILESIZE skip=157696
                    cp ${apdk}/qcn9000/caldata_1.bin /lib/firmware/qcn9000/caldata_1.bin
            ;;
            ap-mp03.5*)
                    [ -f /lib/firmware/IPQ5018/caldata.bin ] && return
                    MP_BD_FILENAME=/lib/firmware/IPQ5018/bdwlan.bin
                    mkdir -p ${apdk}/IPQ5018
                    if [ -f "$MP_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$MP_BD_FILENAME")
                    else
                        FILESIZE=131072
                    fi
                    dd if=${mtdblock} of=${apdk}/IPQ5018/caldata.bin bs=1 count=$FILESIZE skip=4096
                    cp ${apdk}/IPQ5018/caldata.bin /lib/firmware/IPQ5018/caldata.bin

                    mkdir -p ${apdk}/qcn6122
                    dd if=${mtdblock} of=${apdk}/qcn6122/caldata_1.bin bs=1 count=$FILESIZE skip=157696

                    cp ${apdk}/qcn6122/caldata_1.bin /lib/firmware/qcn6122/caldata_1.bin

                    dd if=${mtdblock} of=${apdk}/qcn6122/caldata_2.bin bs=1 count=$FILESIZE skip=311296
                    cp ${apdk}/qcn6122/caldata_2.bin /lib/firmware/qcn6122/caldata_2.bin
            ;;
            ap-mp03.3*)
                    [ -f /lib/firmware/IPQ5018/caldata.bin ] && return
                    MP_BD_FILENAME=/lib/firmware/IPQ5018/bdwlan.bin
                    mkdir -p ${apdk}/IPQ5018
                    if [ -f "$MP_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$MP_BD_FILENAME")
                    else
                        FILESIZE=131072
                    fi
                    dd if=${mtdblock} of=${apdk}/IPQ5018/caldata.bin bs=1 count=$FILESIZE skip=4096
                    cp ${apdk}/IPQ5018/caldata.bin /lib/firmware/IPQ5018/caldata.bin

                    mkdir -p ${apdk}/qcn6122
                    dd if=${mtdblock} of=${apdk}/qcn6122/caldata_1.bin bs=1 count=$FILESIZE skip=157696
                    cp ${apdk}/qcn6122/caldata_1.bin /lib/firmware/qcn6122/caldata_1.bin

                    mkdir -p ${apdk}/qcn9000
                    dd if=${mtdblock} of=${apdk}/qcn9000/caldata_2.bin bs=1 count=$FILESIZE skip=311296
                    cp ${apdk}/qcn9000/caldata_2.bin /lib/firmware/qcn9000/caldata_2.bin
            ;;
            ap-mp03.4*)
                    [ -f /lib/firmware/IPQ5018/caldata.bin ] && return
                    MP_BD_FILENAME=/lib/firmware/IPQ5018/bdwlan.bin
                    mkdir -p ${apdk}/IPQ5018
                    if [ -f "$MP_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$MP_BD_FILENAME")
                    else
                        FILESIZE=131072
                    fi
                    dd if=${mtdblock} of=${apdk}/IPQ5018/caldata.bin bs=1 count=$FILESIZE skip=4096
                    cp ${apdk}/IPQ5018/caldata.bin /lib/firmware/IPQ5018/caldata.bin

                    mkdir -p ${apdk}/qcn9000
                    dd if=${mtdblock} of=${apdk}/qcn9000/caldata_1.bin bs=1 count=$FILESIZE skip=157696
                    cp ${apdk}/qcn9000/caldata_1.bin /lib/firmware/qcn9000/caldata_1.bin

                    mkdir -p ${apdk}/qcn9000
                    dd if=${mtdblock} of=${apdk}/qcn9000/caldata_2.bin bs=1 count=$FILESIZE skip=311296
                    cp ${apdk}/qcn9000/caldata_2.bin /lib/firmware/qcn9000/caldata_2.bin
            ;;
            ap-mp*)
                    [ -f /lib/firmware/IPQ5018/caldata.bin ] && return
                    MP_BD_FILENAME=/lib/firmware/IPQ5018/bdwlan.bin
                    mkdir -p ${apdk}/IPQ5018
                    if [ -f "$MP_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$MP_BD_FILENAME")
                    else
                        FILESIZE=131072
                    fi
                    dd if=${mtdblock} of=${apdk}/IPQ5018/caldata.bin bs=1 count=$FILESIZE skip=4096
                    [ -L /lib/firmware/IPQ5018/caldata.bin ] || \
                    cp ${apdk}/IPQ5018/caldata.bin /lib/firmware/IPQ5018/caldata.bin
            ;;
            ap-cp*)
                    [ -f /lib/firmware/IPQ6018/caldata.bin ] && return
                    CP_BD_FILENAME=/lib/firmware/IPQ6018/bdwlan.bin
                    mkdir -p ${apdk}/IPQ6018
                    if [ -f "$CP_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$CP_BD_FILENAME")
                    else
                        FILESIZE=65536
                    fi
                    dd if=${mtdblock} of=${apdk}/IPQ6018/caldata.bin bs=1 count=$FILESIZE skip=4096
                    [ -L /lib/firmware/IPQ6018/caldata.bin ] || \
                    cp ${apdk}/IPQ6018/caldata.bin /lib/firmware/IPQ6018/caldata.bin
            ;;
            ap-al02-c13*)
                    [ -f /lib/firmware/IPQ9574/caldata.bin ] && return
                    mkdir -p ${apdk}/IPQ9574
                    mkdir -p ${apdk}/qcn9000
                    mkdir -p ${apdk}/qcn9224

                    create_cfg_caldata_mr "${mtdblock}" "IPQ9574"
            ;;
            ap-al02-c4*)
                [ -f /lib/firmware/IPQ9574/caldata.bin ] && return
                    mkdir -p ${apdk}/IPQ9574
                    mkdir -p ${apdk}/qcn9224
                    do_ftm_conf_override

                    create_cfg_caldata "${mtdblock}" "IPQ9574" "qcn9224" "0" 
            ;;
            ap-al02-c6*|ap-al06*|ap-al05*|ap-al02-c7*|ap-al02-c8*|ap-al02-c9*|ap-al02-c10*|ap-al02-c11*|ap-al02-c12*|ap-al02-c14*|ap-al02-c15*|ap-al02-c16*|ap-al02-c20*|ap-al03-c1*|ap-al03-c2*|ap-al02-c19*)
                    [ -f /lib/firmware/IPQ9574/caldata.bin ] && return
                    mkdir -p ${apdk}/IPQ9574
                    mkdir -p ${apdk}/qcn9224

                    create_cfg_caldata "${mtdblock}" "IPQ9574" "qcn9224" "0"
            ;;
            ap-al02-c1*)
                    [ -f /lib/firmware/IPQ9574/caldata.bin ] && return
                    AL_BD_FILENAME=/lib/firmware/IPQ9574/bdwlan.bin
                    mkdir -p ${apdk}/IPQ9574
                    if [ -f "$AL_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$AL_BD_FILENAME")
                    else
                        FILESIZE=131072
                    fi
                    dd if=${mtdblock} of=${apdk}/IPQ9574/caldata.bin bs=1 count=$FILESIZE skip=4096
                    [ -L /lib/firmware/IPQ9574/caldata.bin ] || \
                    cp ${apdk}/IPQ9574/caldata.bin /lib/firmware/IPQ9574/caldata.bin

                    mkdir -p ${apdk}/qcn9000
                    dd if=${mtdblock} of=${apdk}/qcn9000/caldata_3.bin bs=1 count=$FILESIZE skip=157696
                    dd if=${mtdblock} of=${apdk}/qcn9000/caldata_4.bin bs=1 count=$FILESIZE skip=311296
                    cp ${apdk}/qcn9000/caldata_3.bin /lib/firmware/qcn9000/caldata_3.bin
                    cp ${apdk}/qcn9000/caldata_4.bin /lib/firmware/qcn9000/caldata_4.bin
            ;;
            ap-al02-c3*)
                    [ -f /lib/firmware/IPQ9574/caldata.bin ] && return
                    AL_BD_FILENAME=/lib/firmware/IPQ9574/bdwlan.bin
                    mkdir -p ${apdk}/IPQ9574
                    if [ -f "$AL_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$AL_BD_FILENAME")
                    else
                        FILESIZE=131072
                    fi
                    dd if=${mtdblock} of=${apdk}/IPQ9574/caldata.bin bs=1 count=$FILESIZE skip=4096
                    [ -L /lib/firmware/IPQ9574/caldata.bin ] || \
                    cp ${apdk}/IPQ9574/caldata.bin /lib/firmware/IPQ9574/caldata.bin

                    mkdir -p ${apdk}/qcn9000
                    dd if=${mtdblock} of=${apdk}/qcn9000/caldata_1.bin bs=1 count=$FILESIZE skip=157696
                    dd if=${mtdblock} of=${apdk}/qcn9000/caldata_3.bin bs=1 count=$FILESIZE skip=311296
                    cp ${apdk}/qcn9000/caldata_1.bin /lib/firmware/qcn9000/caldata_1.bin
                    cp ${apdk}/qcn9000/caldata_3.bin /lib/firmware/qcn9000/caldata_3.bin
            ;;
            ap-al02*|ap-al01-c1*)
                    [ -f /lib/firmware/IPQ9574/caldata.bin ] && return
                    AL_BD_FILENAME=/lib/firmware/IPQ9574/bdwlan.bin
                    mkdir -p ${apdk}/IPQ9574
                    if [ -f "$AL_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$AL_BD_FILENAME")
                    else
                        FILESIZE=131072
                    fi
                    dd if=${mtdblock} of=${apdk}/IPQ9574/caldata.bin bs=1 count=$FILESIZE skip=4096
                    [ -L /lib/firmware/IPQ9574/caldata.bin ] || \
                    cp ${apdk}/IPQ9574/caldata.bin /lib/firmware/IPQ9574/caldata.bin

                    mkdir -p ${apdk}/qcn9000
                    dd if=${mtdblock} of=${apdk}/qcn9000/caldata_1.bin bs=1 count=$FILESIZE skip=157696
                    dd if=${mtdblock} of=${apdk}/qcn9000/caldata_2.bin bs=1 count=$FILESIZE skip=311296
                    cp ${apdk}/qcn9000/caldata_1.bin /lib/firmware/qcn9000/caldata_1.bin
                    cp ${apdk}/qcn9000/caldata_2.bin /lib/firmware/qcn9000/caldata_2.bin
            ;;
            ap-al*)
                    [ -f /lib/firmware/IPQ9574/caldata.bin ] && return
                    AL_BD_FILENAME=/lib/firmware/IPQ9574/bdwlan.bin
                    mkdir -p ${apdk}/IPQ9574
                    if [ -f "$AL_BD_FILENAME" ]; then
                        FILESIZE=$(stat -Lc%s "$AL_BD_FILENAME")
                    else
                        FILESIZE=131072
                    fi
                    dd if=${mtdblock} of=${apdk}/IPQ9574/caldata.bin bs=1 count=$FILESIZE skip=4096
                    [ -L /lib/firmware/IPQ9574/caldata.bin ] || \
                    cp ${apdk}/IPQ9574/caldata.bin /lib/firmware/IPQ9574/caldata.bin
            ;;
            ap-mi01.12*)
                    [ -f /lib/firmware/IPQ5332/caldata.bin ] && return
                    mkdir -p ${apdk}/IPQ5332
                    mkdir -p ${apdk}/qcn6432
                    mkdir -p ${apdk}/qcn9224

                    create_cfg_caldata_mr "${mtdblock}" "IPQ5332"
            ;;
            ap-mi01.13*|ap-mi01.14*)
                    [ -f /lib/firmware/IPQ5332/caldata.bin ] && return
                    mkdir -p ${apdk}/IPQ5332
                    mkdir -p ${apdk}/qcn6432
                    mkdir -p ${apdk}/qcn9224
                    do_ftm_conf_override

                    create_cfg_caldata_mr "${mtdblock}" "IPQ5332"
            ;;
            ap-mi01.2-qcn9160*)
                    [ -f /lib/firmware/IPQ5332/caldata.bin ] && return
                    mkdir -p ${apdk}/IPQ5332
                    mkdir -p ${apdk}/qcn9224
                    mkdir -p ${apdk}/qcn9160

                    create_cfg_caldata_mr "${mtdblock}" "IPQ5332"
            ;;
            ap-mi01.1*|ap-mi01.2*|ap-mi01.4*|ap-mi01.6*|ap-mi01.9*|ap-mi02.1*)
                    [ -f /lib/firmware/IPQ5332/caldata.bin ] && return
                    mkdir -p ${apdk}/IPQ5332
                    mkdir -p ${apdk}/qcn9224

                    create_cfg_caldata "${mtdblock}" "IPQ5332" "qcn9224" "0"
            ;;
            tb-mi03.1*|tb-mi05.1*|ap-mi04.5*|ap-mi01.3-c5)
                    [ -f /lib/firmware/IPQ5332/caldata.bin ] && return
                    mkdir -p ${apdk}/IPQ5332
                    mkdir -p ${apdk}/qcn6432

                    create_cfg_caldata "${mtdblock}" "IPQ5332" "qcn6432" "0"
            ;;
            ap-mi04.3*| ap-mi04.1*| db-mi02.1*| ap-mi01.3*| ap-mi01.7*)
                    [ -f /lib/firmware/IPQ5332/caldata.bin ] && return
                    mkdir -p ${apdk}/IPQ5332
                    mkdir -p ${apdk}/qcn6432
                    do_ftm_conf_override

                    create_cfg_caldata "${mtdblock}" "IPQ5332" "qcn6432" "0"
            ;;
            ap-mi*)
                    [ -f /lib/firmware/IPQ5332/caldata.bin ] && return
                    mkdir -p ${apdk}/IPQ5332
                    create_cfg_caldata "${mtdblock}" "IPQ5332"
            ;;
            ap-sdxpinn-qcn9224-V1)
	            [ -f /data/vendor/wifi/caldata/qcn9224/caldata_1.b0002 ] && \
	            [ -f /data/vendor/wifi/caldata/qcn9224/caldata_2.b0004 ] && \
	            [ -f /data/vendor/wifi/caldata/qcn9224/caldata_3.b0001 ] && \
                    return
                    mkdir -p ${apdk}/qcn9224

                    create_cfg_caldata "${mtdblock}" "" "qcn9224" "0"
            ;;
            ap-sdxpinn-qcn9224-V2)
	            [ -f /data/vendor/wifi/caldata/qcn9224/caldata_1.b1003 ] && \
	            [ -f /data/vendor/wifi/caldata/qcn9224/caldata_2.b0004 ] && \
                    return
                    mkdir -p ${apdk}/qcn9224

                    create_cfg_caldata "${mtdblock}" "" "qcn9224" "0"
            ;;
            sdxkova-qcn9224-V1)
	            [ -f /data/vendor/wifi/caldata/qcn9224/caldata_1.b0002 ] && \
	            [ -f /data/vendor/wifi/caldata/qcn9224/caldata_2.b0004 ] && \
	            [ -f /data/vendor/wifi/caldata/qcn9224/caldata_3.b0001 ] && \
                    return
                    mkdir -p ${apdk}/qcn9224

                    create_cfg_caldata "${mtdblock}" "" "qcn9224" "0"
            ;;
           sdxkova-qcn9224-V2)
	            [ -f /data/vendor/wifi/caldata/qcn9224/caldata_1.b1003 ] && \
	            [ -f /data/vendor/wifi/caldata/qcn9224/caldata_2.b0004 ] && \
                    return
                    mkdir -p ${apdk}/qcn9224

                    create_cfg_caldata "${mtdblock}" "" "qcn9224" "0"
            ;;
            rdp466*|rdp485*|rdp487*|rdp496*)
                    [ -f /lib/firmware/IPQ5424/caldata.bin ] && return
                    mkdir -p ${apdk}/IPQ5424
                    mkdir -p ${apdk}/qcn9224

                    create_cfg_caldata "${mtdblock}" "IPQ5424" "qcn9224" "0"
            ;;
            rdp492*)
                    QCN9625_DIR=/lib/firmware/qcn9625
                    [ -d /lib/firmware/qcn9625_v2 ] && QCN9625_DIR=/lib/firmware/qcn9625_v2
                    ls ${QCN9625_DIR}/caldata*.b* >/dev/null 2>&1 && return
                    mkdir -p ${apdk}/qcn9625

                    create_cfg_caldata_mr "${mtdblock}" "IPQ5424"
            ;;
            rdp499* | rdp502* | rdp503* | rdp504* | rdp505*)
                    QCN9625_DIR=/lib/firmware/qcn9625
                    [ -d /lib/firmware/qcn9625_v2 ] && QCN9625_DIR=/lib/firmware/qcn9625_v2
                    ls ${QCN9625_DIR}/caldata*.b* >/dev/null 2>&1 && return
                    mkdir -p ${apdk}/qcn9625

                    create_cfg_caldata_mr "${mtdblock}" "IPQ5210"
            ;;
            rdp498* | rdp500* | rdp501*)
                    ls /lib/firmware/qcn9224/caldata*.b* >/dev/null 2>&1 && return
                    mkdir -p ${apdk}/qcn9224

                    create_cfg_caldata "${mtdblock}" "" "qcn9224" "0"
            ;;
            rdp488*|rdp489*|rdp506*|rdp529*|rdp530*|rdp532*)
                    QCN9625_DIR=/lib/firmware/qcn9625
                    [ -d /lib/firmware/qcn9625_v2 ] && QCN9625_DIR=/lib/firmware/qcn9625_v2
                    ls ${QCN9625_DIR}/caldata*.b* >/dev/null 2>&1 && return
                    mkdir -p ${apdk}/qcn9625

                    create_cfg_caldata_mr "${mtdblock}" "IPQ9650"
            ;;
            echo-qmp5-v1*)
                    mkdir -p ${apdk}/qcn9625

                    create_cfg_caldata_mr "${mtdblock}" "IPQ9650"
            ;;
            rdp507*|rdp490*|rdp491*|rdp525*)
                    QCN9625_DIR=/lib/firmware/qcn9625
                    QCN9589_DIR=/lib/firmware/qcn9589
                    [ -d /lib/firmware/qcn9625_v2 ] && QCN9625_DIR=/lib/firmware/qcn9625_v2
                    [ -d /lib/firmware/qcn9589_v2 ] && QCN9589_DIR=/lib/firmware/qcn9589_v2
                    ls ${QCN9589_DIR}/caldata*.b* >/dev/null 2>&1 && \
                    ls ${QCN9625_DIR}/caldata*.b* >/dev/null 2>&1 && return
                    mkdir -p ${apdk}/qcn9589
                    mkdir -p ${apdk}/qcn9625
                    create_cfg_caldata_mr "${mtdblock}" "IPQ9650"
            ;;
            rdp511*|rdp512*)
                    QCN9589_DIR=/lib/firmware/qcn9589
                    [ -d /lib/firmware/qcn9589_v2 ] && QCN9589_DIR=/lib/firmware/qcn9589_v2
                    ls ${QCN9589_DIR}/caldata*.b* >/dev/null 2>&1 && return
                    mkdir -p ${apdk}/qcn9589
                    create_cfg_caldata_mr "${mtdblock}" "IPQ5210"
            ;;
   esac
}

