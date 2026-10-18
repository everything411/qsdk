#!/bin/sh
#
# Copyright (c) 2015, 2020, The Linux Foundation. All rights reserved.
# Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.

# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.

# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

get_config_file_path()
{
	local file_type brd_name board
	local ini_path
	local caldata_path

	if [[ $# -ne 1 ]]; then
		return
	fi

	file_type="$1"

	case "$file_type" in
	ini|caldata) ;;
	*) return ;;
	esac

	[ -f /tmp/sysinfo/board_name ] && {
		brd_name=$(echo $(board_name) | awk -F '-' '{print $2}')
		board=$brd_name$(echo $(board_name) | awk -F "$brd_name" '{print$2}')
	}

	case "$board" in
	ap-sdxlemur* | ap-sdxpinn*)
		ini_path="/etc/misc/ipq/ini"
		caldata_path="/data/vendor/wifi/caldata"
	;;
	*sdxkova-*)
		ini_path="/ini"
		caldata_path="/data/vendor/wifi/caldata"
	;;
	*)
		ini_path="/ini"
		caldata_path="/lib/firmware"
	;;
	esac

	case "$file_type" in
	ini)
		echo "$ini_path"
	;;
	caldata)
		echo "$caldata_path"
	;;
	esac
}

# Return 0 (true) if KEY is enabled in CONF, else return 1 (false).
# Accepts: 1, true, yes, on  (case-insensitive)
conf_bool_enabled()
{
	conf_file="$1"
	conf_key="$2"

	[ -f "$conf_file" ] || return 1

	awk -v key="$conf_key" '
		BEGIN { found = 0 }

		/^[[:space:]]*$/ || /^[[:space:]]*#/ { next }

		{
			line = $0
			gsub(/\r/, "", line)

			pos = index(line, "=")
			if (pos == 0)
				next

			k = substr(line, 1, pos-1)
			v = substr(line, pos+1)

			gsub(/^[[:space:]]+|[[:space:]]+$/, "", k)
			gsub(/^[[:space:]]+|[[:space:]]+$/, "", v)

			k = tolower(k)
			v = tolower(v)
			key = tolower(key)

			if (k == key) {
				if (v == "1" || v == "true" || v == "yes" || v == "on")
					found = 1
				exit
			}
		}

	END {
		if (found)
			exit 0
		else
			exit 1
	}
	' "$conf_file"
}

# Return the raw value string for KEY from CONF.
# Prints the value to stdout and returns non-zero if KEY is not present.
conf_get_value()
{
	local conf_file="$1"
	local conf_key="$2"

	[ -f "$conf_file" ] || return 1

	awk -v key="$conf_key" '
		BEGIN { found = 0 }

		/^[[:space:]]*$/ || /^[[:space:]]*#/ { next }

		{
			line = $0
			gsub(/\r/, "", line)

			pos = index(line, "=")
			if (pos == 0)
				next

			k = substr(line, 1, pos-1)
			v = substr(line, pos+1)

			gsub(/^[[:space:]]+|[[:space:]]+$/, "", k)
			gsub(/^[[:space:]]+|[[:space:]]+$/, "", v)

			if (tolower(k) == tolower(key)) {
				print v
				found = 1
				exit
			}
		}

	END {
		if (!found)
			exit 1
	}
	' "$conf_file"
}

# ============================================================================
# ART UBI / NAND helpers for selected 11bn RDPs
# ============================================================================

# find_flash_type(part_name) resolves the flash backing for a partition and
# returns 0 when the partition is found, or 1 when it cannot be resolved.
find_flash_type()
{
	local part_name="$1"
	local emmc_part mtd_part mtd_num mtd_type

	[ -z "$part_name" ] && echo "none" && return 1

	emmc_part=$(find_mmc_part "$part_name" 2>/dev/null)
	[ -n "$emmc_part" ] && echo "emmc" && return 0

	mtd_part=$(find_mtd_part "$part_name" 2>/dev/null)
	[ -z "$mtd_part" ] && echo "none" && return 1

	mtd_num=$(find_mtd_index "$part_name" 2>/dev/null)
	if [ -n "$mtd_num" ] && [ -f "/sys/class/mtd/mtd${mtd_num}/type" ]; then
		mtd_type=$(cat "/sys/class/mtd/mtd${mtd_num}/type" 2>/dev/null)
		echo "${mtd_type:-mtd}"
	else
		echo "mtd"
	fi
	return 0
}

# ubi_find_dev(part_name) resolves the attached UBI device index for an MTD
# partition and returns 0 when attached, or 1 when no attached device is found.
ubi_find_dev()
{
	local part_name="$1"
	local mtd_num ubi_sysfs sysfs_mtd_num

	mtd_num=$(find_mtd_index "$part_name")
	[ -z "$mtd_num" ] && return 1

	for ubi_sysfs in /sys/class/ubi/ubi*; do
		case "$(basename $ubi_sysfs)" in
			*_*) continue ;;
		esac
		[ -f "$ubi_sysfs/mtd_num" ] || continue
		sysfs_mtd_num=$(cat "$ubi_sysfs/mtd_num" 2>/dev/null)
		if [ "$sysfs_mtd_num" = "$mtd_num" ]; then
			basename "$ubi_sysfs" | sed 's/ubi//'
			return 0
		fi
	done
	return 1
}

# ubi_find_vol(ubi_num, vol_name) resolves a named UBI volume to /dev/ubiN_M
# and returns 0 when the volume exists, or 1 when it cannot be found.
ubi_find_vol()
{
	local ubi_num="$1"
	local vol_name="$2"
	local vol_sysfs vol_id dev_path

	[ -z "$ubi_num" ] || [ -z "$vol_name" ] && return 1

	for vol_sysfs in /sys/class/ubi/ubi${ubi_num}_*; do
		[ -f "$vol_sysfs/name" ] || continue
		if [ "$(cat "$vol_sysfs/name" 2>/dev/null)" = "$vol_name" ]; then
			vol_id=$(basename "$vol_sysfs" | sed "s/ubi${ubi_num}_//")
			dev_path="/dev/ubi${ubi_num}_${vol_id}"
			if [ ! -e "$dev_path" ] && [ -f "$vol_sysfs/dev" ]; then
				local devid major minor
				devid=$(cat "$vol_sysfs/dev" 2>/dev/null)
				major="${devid%%:*}"
				minor="${devid##*:}"
				mknod "$dev_path" c "$major" "$minor" 2>/dev/null || true
			fi
			echo "$dev_path"
			return 0
		fi
	done
	return 1
}

# ubi_is_formatted(part_name) checks for UBI magic on the backing MTD partition
# and returns 0 when the partition is already UBI-formatted, or 1 otherwise.
ubi_is_formatted()
{
	local part_name="$1"
	local mtd_num magic
	local ubi_magic="55424923"

	mtd_num=$(find_mtd_index "$part_name")
	[ -z "$mtd_num" ] && return 1

	magic=$(dd if=/dev/mtd${mtd_num} bs=4 count=1 2>/dev/null | hexdump -v -e '/1 "%02x"')
	[ "$magic" = "$ubi_magic" ] && return 0
	return 1
}

# ubi_attach(part_name, [beb_limit]) attaches a UBI-formatted partition to the
# UBI subsystem and returns 0 when attach succeeds/already exists, or 1 on failure.
ubi_attach()
{
	local part_name="$1"
	local beb_opt="$2"
	local mtd_num ubi_num ret

	[ -z "$part_name" ] && return 1

	mtd_num=$(find_mtd_index "$part_name")
	[ -z "$mtd_num" ] && return 1

	ubi_num=$(ubi_find_dev "$part_name" 2>/dev/null)
	[ -n "$ubi_num" ] && return 0

	if [ -n "$beb_opt" ]; then
		ubiattach -m "${mtd_num}" -b "${beb_opt}" 2>/dev/null
	else
		ubiattach -m "${mtd_num}" 2>/dev/null
	fi
	ret=$?

	[ $ret -ne 0 ] && [ $ret -ne 17 ] && return 1

	sync
	return 0
}

# ubi_init(part_name, vol_name, [vol_size_kb], [--force]) formats NAND as UBI when
# needed and ensures the requested static volume exists; it returns 0 on success,
# or 1 on failure.
ubi_init()
{
	local part_name="$1"
	local vol_name="$2"
	local vol_size_kb="$3"
	local force=0

	[ "$3" = "--force" ] && { vol_size_kb=""; force=1; }
	[ "$4" = "--force" ] && force=1

	local flash_type mtd_num ubi_num vol_dev

	[ -z "$part_name" ] || [ -z "$vol_name" ] && return 1

	flash_type=$(find_flash_type "$part_name")
	[ "$flash_type" != "nand" ] && return 1

	mtd_num=$(find_mtd_index "$part_name")
	[ -z "$mtd_num" ] && return 1

	if ubi_is_formatted "$part_name" && [ "$force" = "0" ]; then
		:
	else
		ubi_num=$(ubi_find_dev "$part_name" 2>/dev/null)
		[ -n "$ubi_num" ] && { ubidetach -d "${ubi_num}" 2>/dev/null || true; sleep 1; }

		ubiformat /dev/mtd${mtd_num} -y || return 1
	fi

	ubi_attach "$part_name" || return 1

	ubi_num=$(ubi_find_dev "$part_name")
	[ -z "$ubi_num" ] && return 1

	vol_dev=$(ubi_find_vol "$ubi_num" "$vol_name" 2>/dev/null)
	if [ -n "$vol_dev" ]; then
		return 0
	fi

	if [ -n "$vol_size_kb" ]; then
		ubimkvol /dev/ubi${ubi_num} -N "${vol_name}" -t static -s "${vol_size_kb}KiB" || return 1
	else
		ubimkvol /dev/ubi${ubi_num} -N "${vol_name}" -t static -m || return 1
	fi

	vol_dev=$(ubi_find_vol "$ubi_num" "$vol_name")
	[ -n "$vol_dev" ]
}

# art_ubifs_enabled() checks whether ART UBI handling is enabled by /tmp/art.conf
# and returns 0 when ART_UBIFS is enabled, or 1 when it is absent/disabled.
art_ubifs_enabled()
{
	conf_bool_enabled /tmp/art.conf ART_UBIFS
}

# get_art_ubi_beb_limit() returns the configured numeric BEB limit when present
# and resolves successfully even when no BEB override is configured.
get_art_ubi_beb_limit()
{
	local beb_limit
	beb_limit=$(conf_get_value /tmp/art.conf ART_UBI_BEB_LIMIT)
	[ -n "$beb_limit" ] && echo "$beb_limit"
}

# get_art_volume_size_kb() returns the configured ART volume size in KB and
# defaults to 512 KB when ART_PARTITION_SIZE_KB is absent.
get_art_volume_size_kb()
{
	local volume_size_kb
	volume_size_kb=$(conf_get_value /tmp/art.conf ART_PARTITION_SIZE_KB)
	[ -n "$volume_size_kb" ] || volume_size_kb=512
	echo "$volume_size_kb"
}

# get_raw_art_device() resolves the existing raw ART backing device and returns 0
# when either MTD or MMC ART is found, or 1 when no raw ART device is available.
get_raw_art_device()
{
	local dev

	dev=$(find_mtd_part 0:ART)
	if [ -z "$dev" ]; then
		dev=$(find_mmc_part 0:ART)
	fi

	[ -n "$dev" ] && echo "$dev"
}

# get_art_volume_device() resolves the configured ART UBI volume device path and
# returns 0 when the volume exists, or 1 when the UBI device/volume is missing.
get_art_volume_device()
{
	local ubi_num vol_dev

	ubi_num=$(ubi_find_dev "0:ART" 2>/dev/null)
	[ -n "$ubi_num" ] || return 1

	vol_dev=$(ubi_find_vol "$ubi_num" "art" 2>/dev/null)
	[ -n "$vol_dev" ] || return 1

	echo "$vol_dev"
}

# restore_raw_art_backup(backup_file, raw_art_dev) best-effort restores the saved
# raw ART payload back to the original raw ART device and returns 0 on success.
restore_raw_art_backup()
{
	local backup_file="$1"
	local raw_art_dev="$2"
	local ubi_num

	[ -n "$backup_file" ] || return 1
	[ -s "$backup_file" ] || return 1
	[ -n "$raw_art_dev" ] || return 1

	ubi_num=$(ubi_find_dev "0:ART" 2>/dev/null)
	[ -n "$ubi_num" ] && ubidetach -d "${ubi_num}" 2>/dev/null || true

	dd if="$backup_file" of="$raw_art_dev" || return 1
	sync
	return 0
}

# art_volume_can_fit(part_name, beb_limit, vol_size_kb) estimates whether a new UBI
# volume of the requested size can fit on the ART partition without reformatting.
art_volume_can_fit()
{
	local part_name="$1"
	local beb_limit="$2"
	local vol_size_kb="$3"
	local mtd_num peb_size good_pebs reserve_pebs available_bytes requested_bytes
	local reserve_count reserve_overhead

	[ -n "$part_name" ] || return 1
	[ -n "$vol_size_kb" ] || return 1

	mtd_num=$(find_mtd_index "$part_name")
	[ -n "$mtd_num" ] || return 1

	[ -f /sys/class/mtd/mtd${mtd_num}/erasesize ] || return 1
	[ -f /sys/class/mtd/mtd${mtd_num}/size ] || return 1

	peb_size=$(cat /sys/class/mtd/mtd${mtd_num}/erasesize 2>/dev/null)
	[ -n "$peb_size" ] || return 1

	good_pebs=$(( $(cat /sys/class/mtd/mtd${mtd_num}/size 2>/dev/null) / peb_size ))
	[ "$good_pebs" -gt 0 ] || return 1

	reserve_count=0
	case "$beb_limit" in
		'')
			reserve_count=20
			;;
		*)
			reserve_count="$beb_limit"
			;;
	esac
	[ -n "$reserve_count" ] || reserve_count=20

	# Reserve estimation:
	# - UBI bad block handling reserves roughly 2 PEBs per configured BEB unit.
	# - Add 2 more PEBs for internal/layout overhead during fresh volume creation.
	# This is intentionally conservative so we avoid destructive ubiformat when the
	# requested logical ART payload is unlikely to fit.
	reserve_overhead=$((reserve_count * 2 + 2))
	[ "$good_pebs" -gt "$reserve_overhead" ] || return 1

	# UBI data area subtracts EC+VID/data header overhead. On our NAND targets PEB
	# is expected to be much larger than 4096 bytes; if not, treat the geometry as
	# unsupported for this estimation and avoid destructive migration.
	[ "$peb_size" -gt 4096 ] || return 1
	available_bytes=$(((good_pebs - reserve_overhead) * (peb_size - 4096)))
	requested_bytes=$((vol_size_kb * 1024))

	[ "$available_bytes" -ge "$requested_bytes" ]
}

# ensure_art_ubi_ready() prepares the NAND ART UBI backend, including first-boot
# raw ART migration when needed, and returns 0 when the volume is ready, or 1 on failure.
ensure_art_ubi_ready()
{
	local flash_type raw_art beb_limit vol_dev backup_file
	local need_recovery recovery_reason mtd_num ubi_num vol_size_kb
	local backup_count_kb

	art_ubifs_enabled || return 1

	flash_type=$(find_flash_type "0:ART")
	[ "$flash_type" = "nand" ] || return 1

	raw_art=$(get_raw_art_device)
	[ -n "$raw_art" ] || return 1

	beb_limit=$(get_art_ubi_beb_limit)
	vol_size_kb=$(get_art_volume_size_kb)
	need_recovery=0
	recovery_reason=""

	if ubi_is_formatted "0:ART"; then
		echo "ART UBI: Existing UBI formatting detected for 0:ART" > /dev/console
		if ubi_attach "0:ART" "$beb_limit"; then
			vol_dev=$(get_art_volume_device 2>/dev/null)
			if [ -n "$vol_dev" ]; then
				echo "ART UBI: Using existing volume ${vol_dev}" > /dev/console
				return 0
			fi
			need_recovery=1
			recovery_reason="attached UBI is missing volume 'art'"
			echo "ART UBI: ${recovery_reason}, preserving ART and rebuilding UBI volume" > /dev/console
		else
			need_recovery=1
			recovery_reason="attach failed despite UBI magic"
			echo "ART UBI: ${recovery_reason}, preserving ART and re-ubinizing 0:ART" > /dev/console
		fi
	else
		need_recovery=1
		recovery_reason="first boot migration required"
		echo "ART UBI: ${recovery_reason}, backing up raw ART from ${raw_art}" > /dev/console
	fi

	if ! art_volume_can_fit "0:ART" "$beb_limit" "$vol_size_kb"; then
		echo "ART UBI: Requested size ${vol_size_kb}KiB cannot fit on 0:ART, skipping destructive migration" > /dev/console
		return 1
	fi

	backup_file="/tmp/raw_art_backup.bin"
	backup_count_kb="$vol_size_kb"
	if ! dd if="$raw_art" of="$backup_file" bs=1024 count="$backup_count_kb"; then
		echo "ART UBI: Failed to back up raw ART from ${raw_art}" > /dev/console
		return 1
	fi

	if [ ! -s "$backup_file" ]; then
		echo "ART UBI: Raw ART backup is empty, aborting migration" > /dev/console
		return 1
	fi

	# This path should never be reached with need_recovery != 1 because all earlier
	# successful flows return immediately. Keep this as a defensive sanity check in
	# case future control-flow changes accidentally drop into the destructive path.
	if [ "$need_recovery" != "1" ]; then
		echo "ART UBI: Internal error, recovery path not selected" > /dev/console
		rm -f "$backup_file"
		return 1
	fi

	mtd_num=$(find_mtd_index "0:ART")
	if [ -z "$mtd_num" ]; then
		echo "ART UBI: Failed to resolve MTD index for 0:ART" > /dev/console
		rm -f "$backup_file"
		return 1
	fi

	ubi_num=$(ubi_find_dev "0:ART" 2>/dev/null)
	if [ -n "$ubi_num" ]; then
		ubidetach -d "${ubi_num}" 2>/dev/null || true
	fi

	echo "ART UBI: Formatting 0:ART and creating UBI volume 'art'" > /dev/console
	if ! ubiformat /dev/mtd${mtd_num} -y; then
		echo "ART UBI: ubiformat failed during recovery" > /dev/console
		rm -f "$backup_file"
		return 1
	fi

	if ! ubi_attach "0:ART" "$beb_limit"; then
		echo "ART UBI: Failed to attach UBI backend after formatting" > /dev/console
		restore_raw_art_backup "$backup_file" "$raw_art" || \
			echo "ART UBI: Failed to roll back raw ART after post-format attach failure" > /dev/console
		rm -f "$backup_file"
		return 1
	fi

	ubi_num=$(ubi_find_dev "0:ART")
	if [ -z "$ubi_num" ]; then
		echo "ART UBI: Failed to resolve attached UBI device for 0:ART" > /dev/console
		restore_raw_art_backup "$backup_file" "$raw_art" || \
			echo "ART UBI: Failed to roll back raw ART after missing attached UBI device" > /dev/console
		rm -f "$backup_file"
		return 1
	fi

	if ! ubimkvol /dev/ubi${ubi_num} -N "art" -t static -s "${vol_size_kb}KiB"; then
		echo "ART UBI: Failed to create UBI volume 'art'" > /dev/console
		restore_raw_art_backup "$backup_file" "$raw_art" || \
			echo "ART UBI: Failed to roll back raw ART after volume creation failure" > /dev/console
		rm -f "$backup_file"
		return 1
	fi

	vol_dev=$(get_art_volume_device)
	if [ -z "$vol_dev" ]; then
		echo "ART UBI: Failed to resolve ART volume '${vol_name}' after creation" > /dev/console
		restore_raw_art_backup "$backup_file" "$raw_art" || \
			echo "ART UBI: Failed to roll back raw ART after volume resolution failure" > /dev/console
		rm -f "$backup_file"
		return 1
	fi

	echo "ART UBI: Restoring raw ART backup into ${vol_dev}" > /dev/console
	if ! ubiupdatevol "$vol_dev" "$backup_file"; then
		echo "ART UBI: Failed to restore raw ART backup into ${vol_dev}" > /dev/console
		restore_raw_art_backup "$backup_file" "$raw_art" || \
			echo "ART UBI: Failed to roll back raw ART after volume restore failure" > /dev/console
		rm -f "$backup_file"
		return 1
	fi

	rm -f "$backup_file"
	echo "ART UBI: Recovery complete, using volume ${vol_dev}" > /dev/console
	return 0
}

# get_art_read_device() returns the ART source path that readers should consume
# and returns 0 when the selected backend is ready, or 1 when it cannot be resolved.
get_art_read_device()
{
	local flash_type vol_dev raw_dev

	flash_type=$(find_flash_type "0:ART")
	if [ "$flash_type" = "nand" ] && art_ubifs_enabled; then
		if ensure_art_ubi_ready; then
			vol_dev=$(get_art_volume_device)
			[ -n "$vol_dev" ] || return 1
			echo "$vol_dev"
			return 0
		fi

		raw_dev=$(get_raw_art_device)
		if [ -n "$raw_dev" ]; then
			echo "ART UBI: Falling back to raw ART device ${raw_dev} for read path" > /dev/console
			echo "$raw_dev"
			return 0
		fi
		return 1
	fi

	raw_dev=$(get_raw_art_device)
	[ -n "$raw_dev" ] || return 1
	echo "$raw_dev"
}

# write_art_file(infile) persists a complete ART payload to the selected backend
# and returns 0 on successful write, or non-zero when backend setup/write fails.
write_art_file()
{
	local infile="$1"
	local flash_type vol_dev raw_dev

	[ -n "$infile" ] || return 1

	flash_type=$(find_flash_type "0:ART")
	if [ "$flash_type" = "nand" ] && art_ubifs_enabled; then
		if ensure_art_ubi_ready; then
			vol_dev=$(get_art_volume_device)
			[ -n "$vol_dev" ] || return 1
			ubiupdatevol "$vol_dev" "$infile"
			return $?
		fi

		raw_dev=$(get_raw_art_device)
		[ -n "$raw_dev" ] || return 1
		echo "ART UBI: Falling back to raw ART device ${raw_dev} for write path" > /dev/console
		dd if="$infile" of="$raw_dev"
		return $?
	fi

	raw_dev=$(get_raw_art_device)
	[ -n "$raw_dev" ] || return 1

	dd if="$infile" of="$raw_dev"
	return $?
}

create_cfg_caldata() {
	local brd_name=$(echo $(board_name) | awk -F '-' '{print $2}')
	local brd=$brd_name$(echo $(board_name) | awk -F "$brd_name" '{print$2}')
	local fw_caldata=$(get_config_file_path "caldata")
       case "$brd" in
               *sdxkova-*) brd="ap-$brd" ;;
               *) ;;
       esac
	awk -F ',' -v apdk='/tmp/' -v mtdblock=$1 -v ahb_dir=$2 -v pci_dir=$3 -v pci1_dir=$4 -v board=$brd -v fw_path=$fw_caldata '{
		if ($1 == board) {
			print $1 "\t" $2 "\t" $3 "\t" $4 "\t" $5 "\t" $6
                        file_suffix=$6+1
			BDF_SIZE=0
			if ($6 == 255) {
				print "Internal radio"
				cmd ="stat -Lc%s " fw_path "/" ahb_dir "/bdwlan.b" $2 " 2> /dev/null"
				cmd | getline BDF_SIZE
				close(cmd)
				if(!BDF_SIZE) {
					print "BDF file for Board id " $2 " not found. Using default value"
					BDF_SIZE=131072
				}
				cmd = "dd if="mtdblock" of=" apdk ahb_dir "/caldata.bin bs=1 count=" BDF_SIZE " skip=" $4
				system(cmd)
				cmd = "cp " apdk ahb_dir "/caldata.bin " fw_path "/" ahb_dir "/"
				system(cmd)
			} else {
				print "PCI radio"
				dir_lib=pci_dir
				if ($3 == 2){
					print "Inside slot instance 2"
					if (pci1_dir != 0) {
						dir_lib=pci1_dir
					}
				}
				cmd ="stat -Lc%s " fw_path "/" dir_lib "/bdwlan.b" $2 " 2> /dev/null"
				cmd | getline BDF_SIZE
				close(cmd)
				if(!BDF_SIZE) {
					print "BDF file for Board id " $2 " not found. Using default value"
					if (dir_lib == "qcn9224")
						BDF_SIZE=184320
					#Adding additional condition check for pebble wideband case
					else if (dir_lib == "qcn6432" && $2 == 0070)
						BDF_SIZE=168960
					else
						BDF_SIZE=131072
				}
				cmd = "mkdir -p " fw_path "/" dir_lib "/"
				system(cmd)
				cmd = "dd if="mtdblock" of=" apdk dir_lib "/caldata_" file_suffix ".b" $2 " bs=1 count=" BDF_SIZE " skip=" $4
				system(cmd)
				cmd = "cp " apdk dir_lib "/caldata_" file_suffix ".b" $2 " " fw_path "/" dir_lib "/"
				system(cmd)
			}
		}
	}' $fw_caldata/ftm.conf

	case "$brd" in
       *sdxpinn* | *sdxkova*)
		;;
	*)
		[ -f $fw_caldata/$2/caldata.bin ] || touch $fw_caldata/$2/caldata.bin
		;;
	esac

}

do_ftm_conf_override()
{
        #Necessary conditon check, This method will be invoked only for below mentioned RDP's
        #Inside this API, we will update the ftm.conf file with DTS board ID values maintained.
        #This is applicable only for below mentioned RDP's, For other RDP's return [Do nothing]
        local brd_name=$(echo $(board_name) | awk -F '-' '{print $2}')
        local board=$brd_name$(echo $(board_name) | awk -F "$brd_name" '{print$2}')
        local ftm_conf_path=$(get_config_file_path "caldata")
        local board_id_2g
        local board_id_5g
        local board_id_6g
        local ker_ver=`uname -r |cut -d. -f1`
        #Check for presence of ATH module to differentiate the folder read of dts file b/w PROP and ATH
        ath12k="/etc/modules.d/ath12k"
        if [ $ker_ver -ge 6 ]; then
            case "$board" in
                    ap-mi04.3*|ap-mi04.1*|ap-mi01.3*|ap-mi01.14)
                        board_id_2g=`hexdump -C /proc/device-tree/soc@0/wifi@c0000000/qcom,board_id | awk '{print $5}'`
                        board_id_5g=`hexdump -C /proc/device-tree/soc@0/wifi1@c0000000/qcom,board_id | awk '{print $5}'`
                        board_id_6g=`hexdump -C /proc/device-tree/soc@0/wifi2@c0000000/qcom,board_id | awk '{print $5}'`
                        case "$board" in
                            ap-mi01.14)
                            board_id_6g=`hexdump -C /proc/device-tree/soc@0/wifi3@f00000/board_id | awk '{print $5}'`
                        ;;
                        esac
                    ;;
                    ap-al02-c4) #check for RDP433
                    if [ -e "$ath12k" ]; then   #override for ATH   
                        board_id_2g=`hexdump -C /proc/device-tree/soc@0/pci@10000000/pcie@0/wifi@0/qcom,board_id | awk '{print $5}'`    #Read dts file for 2G board_id
                        board_id_5g=`hexdump -C /proc/device-tree/soc@0/pci@18000000/pcie@0/wifi@0/qcom,board_id | awk '{print $5}'`    #Read dts file for 5G board_id
                        board_id_6g=`hexdump -C /proc/device-tree/soc@0/pci@20000000/pcie@0/wifi@0/qcom,board_id | awk '{print $5}'`    #Read dts file for 6G board_id
                    else    #override for PROP
                        board_id_2g=`hexdump -C /proc/device-tree/soc@0/wifi5@f00000/board_id | awk '{print $5}'`   #Read dts file for 2G board_id
                        board_id_5g=`hexdump -C /proc/device-tree/soc@0/wifi7@f00000/board_id | awk '{print $5}'`   #Read dts file for 5G board_id
                        board_id_6g=`hexdump -C /proc/device-tree/soc@0/wifi6@f00000/board_id | awk '{print $5}'`   #Read dts file for 6G board_id
                    fi
                    ;;
                    *)
                            echo "Board name is $board -do_ftm_conf_override API not applicable" > /dev/console && return
                    ;;
            esac
        else
            case "$board" in
                    ap-mi04.3*|ap-mi04.1*|ap-mi01.3*|ap-mi01.14)
                        board_id_2g=`hexdump -C /proc/device-tree/soc/wifi@c0000000/qcom,board_id | awk '{print $5}'`
                        board_id_5g=`hexdump -C /proc/device-tree/soc/wifi4@f00000/qcom,board_id | awk '{print $5}'`
                        board_id_6g=`hexdump -C /proc/device-tree/soc/wifi5@f00000/qcom,board_id | awk '{print $5}'`
                        case "$board" in
                            ap-mi01.14)
                            board_id_5g=`hexdump -C /proc/device-tree/soc/wifi1@f00000/qcom,board_id | awk '{print $5}'`
                            board_id_6g=`hexdump -C /proc/device-tree/soc/wifi2@f00000/board_id | awk '{print $5}'`
                        ;;
                        esac
                    ;;
                    *)
                            echo "Board name is $board -do_ftm_conf_override API not applicable" > /dev/console && return
                    ;;
            esac
        fi

        case "$board" in
            ap-mi04.3*|ap-mi04.1*|ap-mi01.3*|ap-mi01.14)
                awk -F',' -v board=$board -v board_id_2g=$board_id_2g -v board_id_5g=$board_id_5g -v board_id_6g=$board_id_6g -v ftm_conf_path=$ftm_conf_path '{
                    if ($1 == board) {
                            print $1 "\t" $2 "\t" $3 "\t" $4 "\t" $5 "\t" $6 "\t" NR
                            lineNumber=NR
                            if ($3 == 0){
                                print "2G slot Instance -lineNumber" lineNumber "DTS board ID - "board_id_2g
                                cmd = "sed -i " lineNumber"s" "\/" $2 "\/" board_id_2g "\/ " ftm_conf_path "/ftm.conf"
                            }
                            if ($3 == 1){
                                print "5G slot Instance -lineNumber" lineNumber "DTS board ID - "board_id_5g
                                cmd = "sed -i " lineNumber"s" "\/" $2 "\/" "00" board_id_5g "\/ " ftm_conf_path "/ftm.conf"
                            }
                            else if($3 == 2)
                            {
                                print "6G slot Instance -lineNumber" lineNumber "DTS board ID - "board_id_6g
                                cmd = "sed -i " lineNumber"s" "\/" $2 "\/" "00" board_id_6g "\/ " ftm_conf_path "/ftm.conf"
                            }
                            system(cmd)
                    }
                }' $ftm_conf_path/ftm.conf
            ;;
            ap-al02-c4) #Update ftm.conf file with the overridden board_id for RDP433
                awk -F',' -v board=$board -v board_id_2g=$board_id_2g -v board_id_5g=$board_id_5g -v board_id_6g=$board_id_6g -v ftm_conf_path=$ftm_conf_path '{
                    if ($1 == board) {  
                            print $1 "\t" $2 "\t" $3 "\t" $4 "\t" $5 "\t" $6 "\t" NR
                            lineNumber=NR #store the current line number
                            if ($3 == 2){
                                print "2G slot Instance -lineNumber" lineNumber "DTS board ID - "board_id_2g
                                # Construct sed command to replace board ID for 2G.
                                # $2 contains existing board_id in ftm.conf file
                                # board_id_2g contains the overridden board_id from dts file
                                # In dts file, board_id will be 0x01 and caldata naming convention will be caldata_2.b0001. So, append 00 before board_id while constructing sed command
                                # Same convention followed for 5G and 6G as well
                                cmd = "sed -i " lineNumber"s" "\/" $2 "\/" "00" board_id_2g "\/ " ftm_conf_path "/ftm.conf"
                            }
                            if ($3 == 4){
                                print "5G slot Instance -lineNumber" lineNumber "DTS board ID - "board_id_5g
                                # Construct sed command to replace board ID for 5G
                                # Same as 2G
                                cmd = "sed -i " lineNumber"s" "\/" $2 "\/" "00" board_id_5g "\/ " ftm_conf_path "/ftm.conf"
                            }
                            else if($3 == 3)
                            {
                                print "6G slot Instance -lineNumber" lineNumber "DTS board ID - "board_id_6g
                                # Construct sed command to replace board ID for 6G
                                # Same as 2G
                                cmd = "sed -i " lineNumber"s" "\/" $2 "\/" "00" board_id_6g "\/ " ftm_conf_path "/ftm.conf"
                            }
                            system(cmd)
                    }
                }' $ftm_conf_path/ftm.conf
            ;;
            esac
}

#create_cfg_caldata_mr is the new api added for multi radio support
# For 11bn attach RDPs, uncompression is handled if art_compression.conf is available
#To call this API, ftm.conf entry should have DIR argument with the existing arguments 
#while calling it should have 2 aruguments mtdblock and integrated radio
#Ex : create_cfg_caldata_mr "${mtdblock}" "Integrated radio"

create_cfg_caldata_mr()
{
    local brd_name=$(echo $(board_name) | awk -F '-' '{print $2}')
    local brd=$brd_name$(echo $(board_name) | awk -F "$brd_name" '{print$2}')
    local ftm_conf_path=$(get_config_file_path "caldata")
    local grep_val=$(grep $brd $ftm_conf_path/ftm.conf)
    local num_rows="$(grep -w -c $brd $ftm_conf_path/ftm.conf)"
    local apdk="/tmp"

    local ART_COMPRESSION_ENABLED=0
    local ART_SLOT_OFFSET_KB=0
    local ART_PARTITION_SIZE_KB=512
    local READ_IF=$1
    local WLAN_CALDATA_PARTITION_SIZE=0


    if conf_bool_enabled /tmp/art.conf ART_COMPRESSION; then
       ART_COMPRESSION_ENABLED=1
       ART_SLOT_OFFSET_KB=$(conf_get_value /tmp/art.conf ART_SLOT_OFFSET_KB)
       [ -n "$ART_SLOT_OFFSET_KB" ] || ART_SLOT_OFFSET_KB=0
       ART_PARTITION_SIZE_KB=$(conf_get_value /tmp/art.conf ART_PARTITION_SIZE_KB)
       [ -n "$ART_PARTITION_SIZE_KB" ] || ART_PARTITION_SIZE_KB=512

       if [ "$ART_SLOT_OFFSET_KB" -gt 0 ]; then
           WLAN_CALDATA_PARTITION_SIZE=$((ART_PARTITION_SIZE_KB - ART_SLOT_OFFSET_KB))
           # Design note:
           # ART on 11bn RDPs is a mixed-format image. The first ART_SLOT_OFFSET_KB KB hold
           # Ethernet MAC address region + CALDATA metadata and must remain raw so the
           # booted system can read them directly from flash without LZMA decompression.
           # Only the WLAN calibration area after that offset is stored in compressed form.
           #
           # We therefore split the image into:
           #   1. virtual_art_ethphy.bin              -> raw prefix from flash
           #   2. virtual_art_wlan_caldata.bin.lzma  -> compressed WLAN calibration suffix
           #
           # After decompressing the WLAN part, we manually concatenate both sections back
           # into virtual_art.bin so every downstream caldata consumer still sees a single
           # logical ART replica in /tmp while preserving the raw-on-flash layout contract.
           dd if=$READ_IF of=${apdk}/virtual_art_ethphy.bin bs=1024 count=$ART_SLOT_OFFSET_KB
           dd if=$READ_IF of=${apdk}/virtual_art_wlan_caldata.bin.lzma bs=1024 skip=$ART_SLOT_OFFSET_KB
           lzma -fdv --single-stream ${apdk}/virtual_art_wlan_caldata.bin.lzma || {
               dd if=/dev/zero of=${apdk}/virtual_art_wlan_caldata.bin bs=1024 count=$WLAN_CALDATA_PARTITION_SIZE
           }
           cat ${apdk}/virtual_art_ethphy.bin ${apdk}/virtual_art_wlan_caldata.bin > ${apdk}/virtual_art.bin
       else
           #FTM Daemon compresses the caldata and writes the lzma file in ART Partition
           dd if=$READ_IF of=${apdk}/virtual_art.bin.lzma
           lzma -fdv --single-stream ${apdk}/virtual_art.bin.lzma || {
               dd if=/dev/zero of=${apdk}/virtual_art.bin bs=1024 count=$ART_PARTITION_SIZE_KB
           }
       fi
       READ_IF=${apdk}/virtual_art.bin
    fi

    # Loop to process the output
    for i in `seq 1 $num_rows`
    do

        #Parse the FTM.conf file and Get the Values
        ROW_VAL=$(echo $grep_val | awk -v i=$i '{print $i}')
        BOARD_ID=$(echo $ROW_VAL | awk -F ',' '{print $2}')
        SLOT_ID=$(echo $ROW_VAL | awk -F ',' '{print $3}')
        OFFSET=$(echo $ROW_VAL | awk -F ',' '{print $4}')
        SIZE=$(echo $ROW_VAL | awk -F ',' '{print $5}')
        IS_PCI=$(echo $ROW_VAL | awk -F ',' '{print $6}')
        DIR_LIB=$(echo $ROW_VAL | awk -F ',' '{print $7}')
        FILE_SUFFIX=$((IS_PCI + 1))

        # Resolve DIR_LIB to the latest-present versioned directory (e.g. qcn9625_v2)
        # when /tmp/art.conf tags one for this chip family; only accept the tagged
        # value if it is actually a version of the same chip (prefix-matches DIR_LIB).
        CHIP_VERSION=$(conf_get_value /tmp/art.conf "ART_CHIP_VERSION_${DIR_LIB}")
        case "$CHIP_VERSION" in
            "${DIR_LIB}"*)
                DIR_LIB="$CHIP_VERSION"
                ;;
        esac
        mkdir -p "$apdk"/"$DIR_LIB"

        echo -e $brd "\t" $BOARD_ID "\t"  $SLOT_ID "\t" $OFFSET "\t" $SIZE "\t" $IS_PCI "\t" $DIR_LIB

        #Get the BDF size
        BDF_FILE="/lib/firmware/${DIR_LIB}/bdwlan.b${BOARD_ID}"

        if [ -f "$BDF_FILE" ]; then
            BDF_SIZE=$(stat -Lc%s "$BDF_FILE")
        else
            BDF_SIZE=$SIZE
        fi

        echo "BDF_SIZE -" $BDF_SIZE

        if [ $IS_PCI == "255" ]
        then
            cmd=$(dd if=$READ_IF of="$apdk"/"$DIR_LIB"/caldata.bin bs=1 count="$BDF_SIZE" skip="$OFFSET")
            cp -f "$apdk"/"$DIR_LIB"/caldata.bin /lib/firmware/"$DIR_LIB"/
        else
            if [ "$DIR_LIB" == "qcn9160" ]
            then
                cmd=$(dd if=$READ_IF of="$apdk"/"$DIR_LIB"/caldata_"$FILE_SUFFIX".bin bs=1 count="$BDF_SIZE" skip="$OFFSET")
                cp -f "$apdk"/"$DIR_LIB"/caldata_"$FILE_SUFFIX".bin /lib/firmware/"$DIR_LIB"/
            else
                cmd=$(dd if=$READ_IF of="$apdk"/"$DIR_LIB"/caldata_"$FILE_SUFFIX".b"$BOARD_ID" bs=1 count="$BDF_SIZE" skip="$OFFSET")
                cp -f "$apdk"/"$DIR_LIB"/caldata_"$FILE_SUFFIX".b"$BOARD_ID" /lib/firmware/"$DIR_LIB"/
            fi
        fi

        [ -f $ftm_conf_path/$2/caldata.bin ] || touch $ftm_conf_path/$2/caldata.bin
    done
}