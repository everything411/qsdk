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

insmod_ath12k_wifi6()
{
	for pci_dev in /sys/bus/pci/devices/*; do
		[ -d "$pci_dev" ] || continue

		ID=$(cat "$pci_dev/device" 2>/dev/null) || continue
		if [ "xx${ID}" = "xx0x1104" ]; then
			insmod ath12k_wifi6
			break
		fi
	done
}

insmod_ath12k_wifi8()
{
	for pci_dev in /sys/bus/pci/devices/*; do
		[ -d "$pci_dev" ] || continue

		ID=$(cat "$pci_dev/device" 2>/dev/null) || continue
		if [ "xx${ID}" = "xx0x1113" ] || [ "xx${ID}" = "xx0x1114" ]; then
			insmod ath12k_wifi8
			break
		fi
	done
}

ipq_wifi_load()
{
	if [ -f /lib/modules/$(uname -r)/ath11k.ko ]; then
		insmod ath11k
		insmod ath11k_ahb
		insmod ath11k_pci
		if [ -f /lib/modules/$(uname -r)/ath12k.ko ]; then
			insmod ath12k

			#check new modules are present and insmod it
			if ls /lib/modules/$(uname -r)/ath12k_wifi*.ko >/dev/null 2>&1; then
				insmod ath12k_wifi7
				insmod_ath12k_wifi8
				insmod_ath12k_wifi6
				insmod qca-wifi-nss-plugins
				if [ "$(ls -1 /sys/kernel/debug/ath11k/ | wc -l)" -gt 0 ]; then
					#wait for two phy entries
					while [ "$(iw dev | grep -c '^phy')" -lt 2 ]; do
						sleep 1
					done
				else
					#wait for one phy entry
					while ! iw dev | grep -q phy; do
						sleep 1
					done
				fi
			fi
		fi
		sleep 2
		#check for prpl
		if [ -f /usr/lib/libwld.so ]; then
			ubus-cli Device.WiFi.Radio.*.Enable=1
		else
			wifi up
		fi
	elif [ -f /lib/modules/$(uname -r)/ath12k.ko ]; then
		insmod ath12k

		#check new modules are present and insmod it
		if ls /lib/modules/$(uname -r)/ath12k_wifi*.ko >/dev/null 2>&1; then
			insmod ath12k_wifi7
			insmod_ath12k_wifi8
			insmod_ath12k_wifi6
			insmod qca-wifi-nss-plugins
			if [ "$(ls -1 /sys/kernel/debug/ath11k/ | wc -l)" -gt 0 ]; then
				#wait for two phy entries
				while [ "$(iw dev | grep -c '^phy')" -lt 2 ]; do
					sleep 1
				done
			else
				#wait for one phy entry
				while ! iw dev | grep -q phy; do
					sleep 1
				done
			fi
		fi
		sleep 2
		# Check if this is a prpl board
		if [ -f /usr/lib/libwld.so ]; then
			ubus-cli Device.WiFi.Radio.*.Enable=1
		else
			wifi up
		fi
	else
		wifi load
	fi
}

ipq_wifi_unload()
{
	if [ $(lsmod | grep ath11k | wc -l) -gt 0 ]; then
		# Check if this is a prpl board
		if [ -f /usr/lib/libwld.so ]; then
			ubus-cli Device.WiFi.Radio.*.Enable=0
		else
			wifi down
		fi
		sleep 2
		rmmod ath11k_ahb
		rmmod ath11k_pci
		rmmod ath11k
		if [ $(lsmod | grep ath12k | wc -l) -gt 0 ]; then
			rmmod qca-wifi-nss-plugins
			#check new modules are present and rmmod it
			if [ $(lsmod | grep ath12k_wifi8 | wc -l) -gt 0 ]; then
				rmmod ath12k_wifi8
			fi
			if [ $(lsmod | grep ath12k_wifi7 | wc -l) -gt 0 ]; then
				rmmod ath12k_wifi7
			fi
			if [ $(lsmod | grep ath12k_wifi6 | wc -l) -gt 0 ]; then
				rmmod ath12k_wifi6
			fi
			rmmod ath12k
		fi
	elif [ $(lsmod | grep ath12k | wc -l) -gt 0 ]; then
		# Check if this is a prpl board
		if [ -f /usr/lib/libwld.so ]; then
			ubus-cli Device.WiFi.Radio.*.Enable=0
		else
			wifi down
		fi
		sleep 2
		rmmod qca-wifi-nss-plugins
		#check new modules are present and rmmod it
		if [ $(lsmod | grep ath12k_wifi8 | wc -l) -gt 0 ]; then
			rmmod ath12k_wifi8
		fi
		if [ $(lsmod | grep ath12k_wifi7 | wc -l) -gt 0 ]; then
			rmmod ath12k_wifi7
		fi
		if [ $(lsmod | grep ath12k_wifi6 | wc -l) -gt 0 ]; then
			rmmod ath12k_wifi6
		fi
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
		ap-al* | db-al* | ap-mi* | db-mi* | ipq5424* | ipq5210* | ipq9650*)
			ipq_wifi_load ;;
		esac ;;
	unload)
		case "$board" in
		ap-al* | db-al* | ap-mi* | db-mi* | ipq5424* | ipq5210* | ipq9650*)
			ipq_wifi_unload ;;
		esac ;;
	reload)
		case "$board" in
		ap-al* | db-al* | ap-mi* | db-mi* | ipq5424* | ipq5210* | ipq9650*)
			ipq_wifi_reload ;;
		esac ;;
esac
