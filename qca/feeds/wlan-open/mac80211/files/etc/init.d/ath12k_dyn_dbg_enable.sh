#!/bin/sh /etc/rc.common
#
# Copyright (c) 2020 The Linux Foundation. All rights reserved.
# Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

START=04
STOP=94

boot()
{
	# Add path so ini framework will read the files from the /ini folder
	if ! echo -n "/ini" > /sys/module/firmware_class/parameters/path; then
		echo "Failed to set path for ini framework" > /dev/console
	fi

	update_ath12k_module_parameters
	board_name=$(cat /tmp/sysinfo/board_name)
	case "$board_name" in
		ap-sdxkova*)
			create_caldata
			caldata_symlink_creation "$board_name" "1"
			caldata_symlink_creation "$board_name" "2"
			caldata_symlink_creation "$board_name" "3"
		;;
	esac
}

# this function is to parse the bootargs and update ath12k
# module params

update_ath12k_module_parameters()
{
	ath12k_config_file="/etc/modules.d/ath12k"
	[ ! -e "$ath12k_config_file" ] && {
		echo "Error: ath12k config file not found at $ath12k_config_file" > /dev/console
		return
	}

	boot_arguments=$(cat /proc/cmdline)
	ftm_mode_enabled=0
	module_params=""

	# Parse boot arguments to extract ath12k-specific parameters
	for argument in $boot_arguments; do
		case "$argument" in
			wifi_ftm_mode|ath12k_ftm_mode=1)
				ftm_mode_enabled=1
				;;
			ath12k_*=*)
				param="${argument#ath12k_}"
				module_params="$module_params $param"
				;;
		esac
	done

	first_line="ath12k dyndbg=+p"
	[ "$ftm_mode_enabled" -eq 1 ] && first_line="$first_line ftm_mode=1"

	# Append all unique ath12k_* parameters to the first line
	for param in $module_params; do
		if ! echo "$first_line" | grep -qw "$param"; then
			first_line="$first_line $param"
		fi
	done

	# Replace only the first line of the config file, preserving the rest
	tail -n +2 "$ath12k_config_file" > /tmp/ath12k_rest
	{
		echo "$first_line"
		cat /tmp/ath12k_rest
	} > "$ath12k_config_file"
	rm -f /tmp/ath12k_rest

	fsync "$ath12k_config_file"

	# Set board name for sdx kobuk single-phy and split-phy
	if [ ! -f /tmp/sysinfo/model ] || [ $(grep -c "SDXKOVA" /sys/firmware/devicetree/base/model) != 0 ]; then
		if [ ! -f /tmp/sysinfo/model ] || [ $(grep -c "V2" /tmp/sysinfo/model) = 0 ]; then
			echo "ap-sdxkova-qcn9224-V1" > /tmp/sysinfo/board_name
		else
			echo "ap-sdxkova-qcn9224-V2" > /tmp/sysinfo/board_name
		fi
	fi
}

create_caldata() {
	if [ -e /lib/read_caldata_to_fs.sh ]; then
		. /lib/read_caldata_to_fs.sh
		do_load_ipq4019_board_bin
	fi
}

caldata_symlink_creation(){
	var=0
	while read -r line
	do
		board=$(echo $line | cut -f1 -d',')
		if [[ "$board" == "$1" ]]; then
			var=$((var+1))
			if [[ $var == $2 ]]; then
				local brdid=$(echo $line | cut -f2 -d',')
				local art_slot=$(echo $line | cut -f3 -d',')
				local pciid=$(echo $line | cut -f6 -d',')
				break
			fi
		fi
	done < /lib/firmware/ftm.conf

	if [ -e /lib/firmware/qcn9224/caldata_$art_slot.b$brdid ]; then
		ln -sf /lib/firmware/qcn9224/caldata_$art_slot.b$brdid /lib/firmware/ath12k/QCN92XX/hw1.0/cal-pci-000$pciid:01:00.0.bin
	fi
}
