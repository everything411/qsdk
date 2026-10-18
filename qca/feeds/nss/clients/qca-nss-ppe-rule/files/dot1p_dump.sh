#!/bin/sh
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: ISC
#

DOT1P_DUMP_FILE=${1:-ppe_dot1p_dump}
DOT1P_DUMP_ROOT=/dev/qca-nss-ppe-rule

#
# usage: dot1p_dot1p.sh
#
module_state_mount() {
        local dot1p_dump_file=$1
        local dot1p_dump_dir=$2
        local stats_file="/sys/kernel/debug/qca-nss-ppe/ppe-rule/ppe-dot1p/ppe_dot1p_dump"

        if [ -e "${dot1p_dump_dir}/${dot1p_dump_file}" ]
        then
                #echo "already mounted"
                return 0
        fi

        if [ ! -e "$stats_file" ]
        then
                #echo "... Dump not supported"
                return 1
        fi

        local major_num="`cat $stats_file`"
        #echo "... Mounting stats $stats_file with major: $major"
        mknod "${dot1p_dump_dir}/${dot1p_dump_file}" c $major_num 0
}


# all state files are mounted under MOUNT_ROOT, so make sure it exists
mkdir -p ${DOT1P_DUMP_ROOT}

#
# attempt to mount state files for the requested module and cat it
# if the mount succeeded
#
module_state_mount ${DOT1P_DUMP_FILE} ${DOT1P_DUMP_ROOT} && {
        cat ${DOT1P_DUMP_ROOT}/${DOT1P_DUMP_FILE}
        exit 0
}

exit 2
