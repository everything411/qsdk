#!/bin/sh
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: ISC
#

VLAN_RULE_DUMP_FILE=${1:-ppe_vlan_rule_dump}
VLAN_RULE_DUMP_ROOT=/dev/qca-nss-ppe-rule

#
# usage: vlan_rule_dump.sh
#
module_state_mount() {
        local vlan_rule_dump_file=$1
        local vlan_rule_dump_dir=$2
        local stats_file="/sys/kernel/debug/qca-nss-ppe/ppe-rule/ppe-vlan/ppe_vlan_dump"

        if [ -e "${vlan_rule_dump_dir}/${vlan_rule_dump_file}" ]
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
        mknod "${vlan_rule_dump_dir}/${vlan_rule_dump_file}" c $major_num 0
}


# all state files are mounted under MOUNT_ROOT, so make sure it exists
mkdir -p ${VLAN_RULE_DUMP_ROOT}

#
# attempt to mount state files for the requested module and cat it
# if the mount succeeded
#
module_state_mount ${VLAN_RULE_DUMP_FILE} ${VLAN_RULE_DUMP_ROOT} && {
        cat ${VLAN_RULE_DUMP_ROOT}/${VLAN_RULE_DUMP_FILE}
        exit 0
}

exit 2
