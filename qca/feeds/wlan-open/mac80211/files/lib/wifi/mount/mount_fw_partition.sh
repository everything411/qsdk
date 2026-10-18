#!/bin/sh
#
#Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#SPDX-License-Identifier: BSD-3-Clause-Clear
#

# This script will be used in lowmem profiles for FW_UMOUNT feature.
# This helps mounting back the FW Partition to DDR

. /etc/init.d/wifi_fw_mount
. /lib/functions.sh

is_fw_umount_supported()
{
        local platform=$(grep -o "IPQ.*" /proc/device-tree/model | awk -F[' '/] '{print $1}')

        if [[ "$platform" == "IPQ5332" || "$platform" == "IPQ5424" ]]; then
                echo "1"
        else
                echo "0"
        fi
}

check_already_mounted=$(mount | grep -c WIFI_FW)
[ "$check_already_mounted" -gt 0 ] && return

supported_arch=$( is_fw_umount_supported )

if [ "$supported_arch" == "1" ] && [ -e /sys/firmware/devicetree/base/MP_256 ] ; then
        echo " WIFI FW mount started " > /dev/console 2>&1
        boot
fi
