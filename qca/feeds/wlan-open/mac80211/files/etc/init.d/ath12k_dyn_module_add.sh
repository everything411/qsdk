#!/bin/sh /etc/rc.common

#Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#SPDX-License-Identifier: ISC

START=05
STOP=95

boot()
{
	load_dynamic_modules
}

# Function to conditionally load modules based on board configuration
load_dynamic_modules()
{
	ath12k_config_file="/etc/modules.d/ath12k"
	[ ! -e "$ath12k_config_file" ] && {
		echo "Error: ath12k config file not found at $ath12k_config_file" > /dev/console
		return
	}

	# Conditionally add ath12k_wifi6 module based on board name
	board_name=""
	if [ -f /tmp/sysinfo/board_name ]; then
		board_name=$(cat /tmp/sysinfo/board_name)
	fi

	case "$board_name" in
		*ap-al02-c13*|*ap-mi01.2-qcn9160*|*ap-mi01.7*|*ap-mi01.13*)
			if ! grep -q "ath12k_wifi6" "$ath12k_config_file" 2>/dev/null; then
				echo "ath12k_wifi6" >> "$ath12k_config_file"
			fi
			;;
	esac

	sed -i '/^ath12k_wifi8$/d' "$ath12k_config_file"

	for pci_dev in /sys/bus/pci/devices/*; do
		[ -d "$pci_dev" ] || continue

		ID=$(cat "$pci_dev/device" 2>/dev/null) || continue
		if [ "xx${ID}" = "xx0x1113" ] || [ "xx${ID}" = "xx0x1114" ]; then
			echo "ath12k_wifi8" >> "$ath12k_config_file"
			break
		fi
	done
}
