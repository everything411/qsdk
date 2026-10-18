#!/bin/sh
#
# Copyright (c) 2015-2016, 2019, The Linux Foundation. All rights reserved.
# Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL  WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
# RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
# CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

type ipq_board_name &>/dev/null  || ipq_board_name() {
	local board="$(echo $(board_name) | sed 's/^\([^-]*-\)\{1\}//g')"
	if [[ "$board" == *rdp* ]]; then
		board=$(cat /tmp/sysinfo/board_name | awk -F, '{print$2}')
	fi
	echo "$board"
}

. /lib/functions.sh

ipq_wifi_load()
{
	if [ -f /lib/modules/$(uname -r)/ath11k.ko ]; then
		insmod ath11k
		insmod ath11k_ahb
		insmod ath11k_pci
		if [ -f /lib/modules/$(uname -r)/ath12k.ko ]; then
			insmod ath12k
		fi
		sleep 2
		wifi up
	elif [ -f /lib/modules/$(uname -r)/ath12k.ko ]; then
		insmod ath12k
		sleep 2
		wifi up
	else
		wifi load
	fi
}

ipq_wifi_unload()
{
	if [ $(lsmod | grep ath11k | wc -l) -gt 0 ]; then
		wifi down
		sleep 2
		rmmod ath11k_ahb
		rmmod ath11k_pci
		rmmod ath11k
		if [ $(lsmod | grep ath12k | wc -l) -gt 0 ]; then
			rmmod ath12k
		fi
	elif [ $(lsmod | grep ath12k | wc -l) -gt 0 ]; then
		wifi down
		sleep 2
		rmmod ath12k
	else
		wifi unload
	fi
}

ipq_wifi_reload()
{
	ipq_wifi_unload
	ipq_wifi_load
}

board=$(ipq_board_name)
case "$1" in
	load)
		case "$board" in
		ap-al* | db-al* | ap-mi* | db-mi* | ipq5424*)
			ipq_wifi_load ;;
		esac ;;
	unload)
		case "$board" in
		ap-al* | db-al* | ap-mi* | db-mi* | ipq5424*)
			ipq_wifi_unload ;;
		esac ;;
	reload)
		case "$board" in
		ap-al* | db-al* | ap-mi* | db-mi* | ipq5424*)
			ipq_wifi_reload ;;
		esac ;;
esac
