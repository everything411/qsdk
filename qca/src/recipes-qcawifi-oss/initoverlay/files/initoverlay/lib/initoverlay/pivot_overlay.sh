#!/bin/sh
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


. /lib/functions/boot.sh


rootfs_pivot_ext4() {
    check_skip || ext4_not_mounted || {
	echo "switching to ext4"
	mount -o move /tmp/overlay /overlay 2>&-
	fopivot /overlay /rom && pi_mount_skip_next=true
    }
}


rootfs_pivot_jffs2() {
    check_skip || jffs2_not_mounted || {
	echo "switching to jffs2"
	mount -o move /tmp/overlay /overlay 2>&-
	fopivot /overlay /rom && pi_mount_skip_next=true
    }
}


rootfs_pivot_ubifs() {
    check_skip || ubifs_not_mounted || {
	echo "switching to ubifs"
	mount -o move /tmp/overlay /overlay 2>&-
	fopivot /overlay /rom && pi_mount_skip_next=true
    }
}


rootfs_pivot_overlay() {
	rootfs_pivot_jffs2
	rootfs_pivot_ubifs
	rootfs_pivot_ext4
}


do_mount_no_jffs2() {
    check_skip || {
	mount_no_jffs2 && pi_mount_skip_next=true
    }
}


do_mount_no_mtd() {
    check_skip || {
	mount_no_mtd
    }
}

do_mount_overlay() {
	check_for_overlay
	do_mount_rootfs_data
	rootfs_pivot_overlay
	do_mount_no_jffs2
	do_mount_no_mtd
}
