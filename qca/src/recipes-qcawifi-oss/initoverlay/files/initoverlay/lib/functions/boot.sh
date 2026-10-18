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


mount() {
	MOUNT_CMD=$(/bin/busybox mount &>/dev/null && echo "/bin/busybox mount" || echo "/bin/busybox.suid mount")
	$MOUNT_CMD -o noatime "$@"
}

find_mtd_part() {
	local PART="$(grep "\"$1\"" /proc/mtd | awk -F: '{print $1}')"
	local PREFIX=/dev/mtdblock

	PART="${PART##mtd}"
	[ -d /dev/mtdblock ] && PREFIX=/dev/mtdblock/
	echo "${PART:+$PREFIX$PART}"
}

find_mmc_part() {
	local DEVNAME PARTNAME

	for DEVNAME in $(find /sys/block/mmcblk*/ -name 'mmcblk*p*'); do
		PARTNAME=$(grep PARTNAME ${DEVNAME}/uevent | cut -f2 -d'=')
		[ "$PARTNAME" = "$1" ] && echo "/dev/$(basename $DEVNAME)" && return 0
	done
}

jffs2_ready () {
	mtdpart="$(find_mtd_part rootfs_data)"
	[ -z "$mtdpart" ] && return 1
	magic=$(hexdump $mtdpart -n 4 -e '4/1 "%02x"')
	[ "$magic" != "deadc0de" ]
}

dupe() { # <new_root> <old_root>
	cd $1
	echo -n "creating directories... "
	{
		cd $2
		find . -xdev -type d
		echo "./dev ./overlay ./mnt ./proc ./tmp"
		# xdev skips mounted directories
		cd $1
	} | xargs mkdir -p
	echo "done"

	echo -n "setting up symlinks... "
	for file in $(cd $2; find . -xdev -type f;); do
		case "$file" in
		./rom/note) ;; #nothing
		./etc/config*|\
		./usr/lib/opkg/info/*) cp -af $2/$file $file;;
		*) ln -sf /rom/${file#./*} $file;;
		esac
	done
	for file in $(cd $2; find . -xdev -type l;); do
		cp -af $2/${file#./*} $file
	done
	echo "done"
}

pivot() { # <new_root> <old_root>
	mount -o move /proc $1/proc && \
	pivot_root $1 $1$2 && {
		mount -o move $2/dev /dev
		mount -o move $2/var/volatile /var/volatile 2>&-
		mount -o move $2/tmp /tmp
		mount -o move $2/sys /sys 2>&-
		mount -o move $2/run /run
		mount -o move $2/etc/hostname /etc/hostname 2>&-
		mount -o move $2/etc/hosts /etc/hosts 2>&-
		mount -o move $2/etc/dhcp_static_hosts /etc/dhcp_static_hosts 2>&-
		mount -o move $2/etc/machine-id /etc/machine-id 2>&-
		mount -o move $2/etc/resolv.conf /etc/resolv.conf 2>&-
		mount -o move $2/etc/dibbler /etc/dibbler 2>&-
		mount -o move $2/etc/resolv.dnsmasq /etc/resolv.dnsmasq 2>&-
		mount -o move $2/www /www 2>&-
		mount -o move $2/var/spool/cron /var/spool/cron 2>&-
		mount -o move $2/overlay /overlay 2>&-
		return 0
	}
}

fopivot() { # <rw_root> <ro_root> <dupe?>
	root=/mnt
	{
		if grep -q overlay /proc/filesystems; then
			mount -t overlayfs -o lowerdir=/,upperdir=$1 "overlayfs:$1" /mnt || {
				mkdir -p /overlay/upper
				mkdir -p /overlay/work
				mount -t overlay overlayfs:/overlay /mnt -o lowerdir=/,upperdir=/overlay/upper,workdir=/overlay/work
			}

		elif grep -q mini_fo /proc/filesystems; then
			mount -t mini_fo -o base=/,sto=$1 "mini_fo:$1" /mnt 2>&- && root=/mnt
		else
			mount --bind / /mnt
			mount --bind -o union "$1" /mnt && root=/mnt
		fi
	} || {
		[ "$3" = "1" ] && {
		mount | grep "on $1 type" 2>&- 1>&- || mount -o bind $1 $1
		dupe $1 $rom
		}
	}
	pivot $root $2
}

ramoverlay() {
	mkdir -p /tmp/root
	mount -t tmpfs -o mode=0755 root /tmp/root
	fopivot /tmp/root /rom 1
}

