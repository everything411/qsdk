#!/bin/sh
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: ISC
#
# memory_dashboard.sh — Static memory snapshot for IPQ dashboard
#
# Usage:
#   memory_dashboard.sh                  — run all sections
#   memory_dashboard.sh -s SECTION,...   — run only specified section(s)
#   memory_dashboard.sh -h               — show help
#
# Sections: meminfo, subsys, kernel, wifi, wifibins, reserved, cma, iomem,
#           memblock, dmesg, vmstat, sysinfo, lsmod, slabinfo,
#           vmallocinfo, procmem, arch

usage() {
	cat <<EOF
Usage: $0 [-s SECTION[,SECTION,...]] [-h]

Options:
  -s SECTION,...  Run only the specified section(s) (comma-separated)
  -h              Show this help message

Sections:
  meminfo     Board info, /proc/meminfo, free -h
  subsys      Memory Consumption by SubSystem
  kernel      Kernel size
  wifi        WiFi Firmware Memory Calculator
  wifibins    WiFi Firmware Binary Sizes (/lib/firmware/*/WIFI_FW)
  reserved    Reserved Memory Size Calculator
  cma         Memory Consumption by CMA
  iomem       I/O Memory Map (/proc/iomem)
  memblock    Memblock Reserved Regions
  dmesg       Reserved Memory (dmesg)
  vmstat      /proc/vmstat
  sysinfo     System Information (mtd, df, mount)
  lsmod       Loadable Modules
  slabinfo    Slab Allocator Info (/proc/slabinfo)
  vmallocinfo Vmalloc Allocations (/proc/vmallocinfo)
  procmem     Process Memory Analyzer
  arch        System Architecture (uname -m)

Examples:
  $0 -s meminfo,vmstat
  $0 -s procmem
EOF
	exit 0
}

get_physical_ddr_mb() {
	local reg_file
	reg_file=$(ls /sys/firmware/devicetree/base/memory@*/reg 2>/dev/null | head -1)

	if [ -z "$reg_file" ]; then
		awk '/MemTotal/ {printf "%d", $2/1024}' /proc/meminfo
		return
	fi

	local vals size_high size_low size_hex size_dec size_mb
	vals=$(hexdump -v -e '4/1 "%02x" "\n"' "$reg_file")

	# reg layout: [addr_high][addr_low][size_high][size_low]
	size_high=$(echo "$vals" | sed -n '3p')
	size_low=$(echo "$vals"  | sed -n '4p')

	size_hex="0x${size_high}${size_low}"
	size_dec=$((size_hex))
	size_mb=$((size_dec / 1024 / 1024))

	echo "$size_mb"
}

RUN_ALL=1
SECTIONS=""

while getopts "s:h" opt; do
	case "$opt" in
		s)
			RUN_ALL=0
			SECTIONS=$(echo "$OPTARG" | tr ',' ' ')
			;;
		h)
			usage
			;;
		*)
			usage
			;;
	esac
done

run_section() {
	[ "$RUN_ALL" = "1" ] && return 0
	for s in $SECTIONS; do
		[ "$s" = "$1" ] && return 0
	done
	return 1
}

format_fw_size() {
	bytes=$1
	awk -v b="$bytes" 'BEGIN {
		if (b >= 1048576)
			printf "%.2f MB (%d bytes)\n", b/1048576, b
		else if (b >= 1024)
			printf "%.2f KB (%d bytes)\n", b/1024, b
		else
			printf "%d bytes\n", b
	}'
}

get_physical_ddr() {
	total=0
	for mem in /sys/firmware/devicetree/base/memory@*/reg; do
		[ -f "$mem" ] || continue
		hex=$(hexdump -v -e '1/1 "%02x"' "$mem")
		while [ ${#hex} -ge 32 ]; do
			cell=${hex:0:32}
			size_hi=${cell:16:8}
			size_lo=${cell:24:8}
			size=$((0x${size_hi}${size_lo}))
			total=$((total + size))
			hex=${hex:32}
		done
	done
	echo "$((total / 1024 / 1024)) MB"
}

get_cmdline_profile() {
	profile=$(grep -o 'mem-profile=[^ ]*' /proc/cmdline 2>/dev/null | head -1 | cut -d= -f2)
	if [ -z "$profile" ]; then
		profile=$(fw_printenv bootargs 2>/dev/null | grep -o 'mem-profile=[^ ]*' | cut -d= -f2)
	fi
	echo "$profile"
}

if run_section meminfo; then
	echo 3 > /proc/sys/vm/drop_caches
	echo ""
	echo "  Board        : $(cat /tmp/sysinfo/board_name)"
	echo "  Physical DDR : $(get_physical_ddr_mb) MB"
	echo ""
	echo "  Total Physical DDR : $(get_physical_ddr)"
	echo ""
	echo "  Runtime Profile : $(get_cmdline_profile)"
	echo ""
	echo "cat /proc/meminfo"
	cat /proc/meminfo
	echo " free -h"
	free -h
	echo ""
fi

if run_section subsys; then
	echo "========================================================================"
	echo "                    Memory Consumption by SubSystem"
	echo "========================================================================"
	echo ""
	sh /usr/sbin/mem_stage_delta.sh
	echo ""
fi

DBG_BASE="/sys/kernel/debug/ath12k"

if run_section kernel; then
	echo "========================================================================"
	echo "                    Kernel Size"
	echo "========================================================================"
	echo ""
	grep -i kernel /proc/iomem | while read line; do
		range=$(echo "$line" | awk '{print $1}')
		name=$(echo "$line" | awk '{print $3, $4}')
		start=$(echo "$range" | cut -d'-' -f1)
		end=$(echo "$range" | cut -d'-' -f2)
		size=$(( (0x$end - 0x$start + 1) / 1024 ))
		echo "$size KB  $name"
	done
	echo ""
fi

if run_section wifi; then
	echo "========================================================================"
	echo "                    WiFi Firmware Memory Calculator"
	echo "========================================================================"
	echo ""

	for dev in "$DBG_BASE"/pci-*; do
		[ -d "$dev" ] || continue
		echo "$dev"
		if [ -f "$dev/mac0/wmi_ctrl_stats" ]; then
			echo ""
			echo 3 1 > "$dev/mac0/wmi_ctrl_stats"
			cat "$dev/mac0/wmi_ctrl_stats"
		fi
	done
fi

if run_section wifibins; then
	echo "========================================================================"
	echo "                    WiFi Firmware Binary Sizes"
	echo "========================================================================"
	echo ""
	found=0
	total_bytes=0
	for fw_bin in /lib/firmware/*/WIFI_FW/*/amss*.bin; do
		[ -f "$fw_bin" ] || continue
		found=1
		size=$(wc -c < "$fw_bin")
		printf "  %-58s %s\n" "$fw_bin" "$(format_fw_size "$size")"
		total_bytes=$(( total_bytes + size ))
	done
	if [ "$found" = "0" ]; then
		echo "  No amss*.bin files found under /lib/firmware/*/WIFI_FW"
	else
		echo ""
		printf "  %-58s %s\n" "TOTAL" "$(format_fw_size "$total_bytes")"
	fi
	echo ""
fi

if run_section reserved; then
	echo "========================================================================"
	echo "                    Reserved Memory Size Calculator"
	echo "========================================================================"
	echo ""

	RESERVED_BASE="/sys/firmware/devicetree/base/reserved-memory"

	for dev in "$RESERVED_BASE"/*; do
		[ -d "$dev" ] || continue
		if [ -f "$dev/reg" ]; then
			sh /lib/dashboard/reserved_memory.sh $dev/reg
		fi
		if [ -f "$dev/size" ]; then
			sh /lib/dashboard/reserved_memory.sh $dev/size
		fi
	done
fi

if run_section cma; then
	echo "========================================================================"
	echo "                    Memory Consumption by CMA"
	echo "========================================================================"
	echo ""
	awk '/^CmaTotal:/ {print "Total CMA Memory: " $2 " " $3}
	     /^CmaFree:/  {print "Total Free CMA Memory: " $2 " " $3}' /proc/meminfo
	echo ""

	CMA_DBG_BASE="/sys/kernel/debug/cma"
	PAGE_KB=4

	if [ -d "$CMA_DBG_BASE" ]; then
		for count_file in "$CMA_DBG_BASE"/*/count; do
			[ -f "$count_file" ] || continue
			cma_dir=$(dirname "$count_file")
			cma_name=$(basename "$cma_dir")
			used_file="$cma_dir/used"

			total_count=$(cat "$count_file")
			used_count=0
			[ -f "$used_file" ] && used_count=$(cat "$used_file")

			echo "CMA Reserved Memory By $cma_name :"
			echo "Total Reserved : $((total_count * PAGE_KB)) KB"
			echo "Utilized       : $((used_count * PAGE_KB)) KB"
			echo ""
		done
	else
		echo "$CMA_DBG_BASE not found"
		echo ""
	fi
fi

if run_section iomem; then
	echo "========================================================================"
	echo "                    I/O Memory Map (/proc/iomem)"
	echo "========================================================================"
	echo ""
	cat /proc/iomem
fi

if run_section memblock; then
	echo "========================================================================"
	echo "                    Memblock Reserved Regions"
	echo "========================================================================"
	echo ""
	cat /sys/kernel/debug/memblock/reserved
fi

if run_section dmesg; then
	echo "========================================================================"
	echo "                    Reserved Memory (dmesg)"
	echo "========================================================================"
	echo ""
	dmesg | grep "Reserved memory"
fi

if run_section vmstat; then
	echo "========================================================================"
	echo "                    /proc/vmstat"
	echo "========================================================================"
	echo ""
	cat /proc/vmstat
fi

if run_section sysinfo; then
	echo "========================================================================"
	echo "                    System Information"
	echo "========================================================================"
	echo ""
	echo "cat /proc/mtd"
	cat /proc/mtd
	echo ""
	echo "df -h"
	df -h
	echo ""
	echo "mount "
	mount
fi

if run_section lsmod; then
	echo "========================================================================"
	echo "                    Loadable Modules (lsmod)"
	echo "========================================================================"
	echo ""
	awk '{deps=($4=="-"?"":$4); printf "%-23s %7d %3d %s\n", $1, $2, $3, deps}' /proc/modules | sort
fi

if run_section slabinfo; then
	echo "========================================================================"
	echo "                    Slab Allocator Info (/proc/slabinfo)"
	echo "========================================================================"
	echo ""
	if [ -f /proc/slabinfo ]; then
		cat /proc/slabinfo
	else
		echo "/proc/slabinfo not found"
	fi
fi

if run_section vmallocinfo; then
	echo "========================================================================"
	echo "                    Vmalloc Allocations (/proc/vmallocinfo)"
	echo "========================================================================"
	echo ""
	cat /proc/vmallocinfo
fi

if run_section procmem; then
	echo "========================================================================"
	echo "                    Process Memory Analyzer"
	echo "========================================================================"
	echo ""
	sh /usr/sbin/mem_analyzer.sh -a -t
fi

if run_section arch; then
	echo "========================================================================"
	echo "                    System Architecture"
	echo "========================================================================"
	echo ""
	echo "uname -m: $(uname -m)"
	echo ""
fi
