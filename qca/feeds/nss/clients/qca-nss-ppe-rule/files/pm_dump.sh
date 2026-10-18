#!/bin/sh
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: ISC
#

PM_DUMP_FILE=${1:-ppe_pm_dump}
PM_DUMP_ROOT=/dev/qca-nss-ppe-rule

#
# usage: pm_dump.sh
#
module_state_mount() {
        local pm_dump_file=$1
        local pm_dump_dir=$2
        local stats_file="/sys/kernel/debug/qca-nss-ppe/ppe-rule/ppe-pm/ppe_pm_dump"

        if [ -e "${pm_dump_dir}/${pm_dump_file}" ]
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
        mknod "${pm_dump_dir}/${pm_dump_file}" c $major_num 0
}


# all state files are mounted under MOUNT_ROOT, so make sure it exists
mkdir -p ${PM_DUMP_ROOT}

#
# attempt to mount state files for the requested module and cat it
# if the mount succeeded
#
module_state_mount ${PM_DUMP_FILE} ${PM_DUMP_ROOT} && {
        cat ${PM_DUMP_ROOT}/${PM_DUMP_FILE}
        exit 0
}

exit 2
