#
# Copyright (c) 2016, The Linux Foundation. All rights reserved.
#
#  Permission to use, copy, modify, and/or distribute this software for any
#  purpose with or without fee is hereby granted, provided that the above
#  copyright notice and this permission notice appear in all copies.
#
#  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
#  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
#  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
#  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
#  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
#  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
#  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

check_for_firstboot() {
	mkdir -p /tmp/test
	mtd_dev=$(echo $(find_mtd_part "rootfs_data"))
	if [ -n "$mtd_dev" ]; then
		mount "$(find_mtd_part rootfs_data)" /tmp/test -t jffs2 || {
			mtd erase "$mtd_dev"
		}
	else
		mount "$(find_mmc_part rootfs_data)" /tmp/test -t ext4 || {
			mkfs.ext4 -F "$(find_mmc_part rootfs_data)"
		}
	fi
		umount /tmp/test &>/dev/null
}

ext4_not_mounted() {
    if [ "$pi_ext4_mount_success" != "true" ]; then
	return 0
    else
	return 1
    fi
}

jffs2_not_mounted() {
    if [ "$pi_jffs2_mount_success" != "true" ]; then
	return 0
    else
	return 1
    fi
}

ubifs_not_mounted() {
    if [ "$pi_ubifs_mount_success" != "true" ]; then
	return 0
    else
	return 1
    fi
}

do_mount_ext4() {
    check_skip && return
    grep -wqs rootfs_data /sys/block/mmcblk*/*/uevent || return 1

    mkdir -p /tmp/overlay
    mount "$(find_mmc_part rootfs_data)" /tmp/overlay -t ext4 &&
        pi_ext4_mount_success=true
}

find_mount_jffs2() {
    mkdir -p /tmp/overlay
    mount "$(find_mtd_part rootfs_data)" /tmp/overlay -t jffs2
    mtd -qq unlock rootfs_data
}

do_mount_jffs2() {
    # skip jffs2 mounting even if it's there if we have volume named
    # ubi_rootfs_data
    check_skip && return
    grep -wqs rootfs_data /proc/mtd || return 1

    find_mount_jffs2 && pi_jffs2_mount_success=true
}

do_mount_ubifs() {
    check_skip && return
    grep -wqs rootfs_data /sys/class/ubi/ubi0/ubi0_*/name || return 1

    mkdir -p /tmp/overlay
    mount -t ubifs ubi0:rootfs_data /tmp/overlay &&
	    pi_ubifs_mount_success=true
}

do_mount_rootfs_data() {
	do_mount_ubifs
	ubifs_not_mounted && check_for_firstboot
	ubifs_not_mounted && do_mount_jffs2
	jffs2_not_mounted && ubifs_not_mounted && do_mount_ext4
}
