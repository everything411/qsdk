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


check_skip() {
    if [ "$pi_mount_skip_next" = "true" ]; then
	return 0
    else
	return 1
    fi
}

mount_no_jffs2() {
    echo "jffs2 not ready yet; using ramdisk, and resetting "
	ramoverlay
}

mount_no_rootfs_data() {
    mtd unlock rootfs
    mount -o remount,rw /dev/root /
}

check_for_rootfs_data() {
    check_skip || {
	grep -qs rootfs_data /proc/mtd || grep -qs rootfs_data /sys/class/ubi/ubi0/ubi0_2/name || grep -qs rootfs_data /sys/block/mmcblk0/mmcblk0p*/uevent || {
	    mount_no_rootfs_data && pi_mount_skip_next=true
	}
    }
}


check_for_jffs2() {
    # skip jffs2 mounting even if it's there if we have volume named
    # ubi_rootfs_data
    check_skip && return

    jffs2_ready || {
        grep -wqs rootfs_data /proc/mtd && {
            mount_no_jffs2 && pi_mount_skip_next=true
        }
    }
}

check_for_overlay() {
	check_for_rootfs_data
	check_for_jffs2
}
