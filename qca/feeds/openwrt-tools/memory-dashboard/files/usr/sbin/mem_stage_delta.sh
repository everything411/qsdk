#!/bin/sh
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: ISC
#
# mem_stage_delta.sh - Memory consumption by boot stage (platform/NSS/WiFi)
#
# Reads /proc/meminfo snapshots captured at three stages and reports
# memory consumed (MemTotal - MemFree+FreePCP) and Slab growth per stage.
#
# Snapshot files:
#   /tmp/meminfo.failsafe  - captured at preinit (platform baseline)
#   /tmp/meminfo.net       - captured at lan/wan ifup (after NSS init)
#   /tmp/meminfo.wifi      - captured at first WiFi interface add

FAILSAFE_FILE="/tmp/meminfo.failsafe"
NET_FILE="/tmp/meminfo.net"
WIFI_FILE="/tmp/meminfo.wifi"

# get_field <pattern> <file>
# Returns the numeric value (kB) for the matching line, or 0 if missing.
get_field() {
	awk -v pat="$1" '$0 ~ pat { print $2; exit }' "$2" 2>/dev/null
}

read_stage() {
	file="$1"
	if [ ! -f "$file" ]; then
		echo "0 0 0 0"
		return
	fi
	mf=$(get_field 'MemFree[+]FreePCP' "$file")
	sl=$(get_field '^Slab:' "$file")
	sr=$(get_field '^SReclaimable:' "$file")
	su=$(get_field '^SUnreclaim:' "$file")
	[ -z "$mf" ] && mf=0
	[ -z "$sl" ] && sl=0
	[ -z "$sr" ] && sr=0
	[ -z "$su" ] && su=0
	echo "$mf $sl $sr $su"
}

# pct <numerator> <denominator>: percentage with 1 decimal, or n/a if denominator is 0
pct() {
	awk -v n="$1" -v d="$2" 'BEGIN { if (d==0) print "n/a"; else printf "%.1f", n*100/d }'
}

# Read all three stages
set -- $(read_stage "$FAILSAFE_FILE")
fs_mf=$1 fs_sl=$2 fs_sr=$3 fs_su=$4

set -- $(read_stage "$NET_FILE")
net_mf=$1 net_sl=$2 net_sr=$3 net_su=$4

set -- $(read_stage "$WIFI_FILE")
wf_mf=$1 wf_sl=$2 wf_sr=$3 wf_su=$4

# Platform consumption: MemTotal - MemFree+FreePCP at failsafe
mt=$(get_field '^MemTotal:' "$FAILSAFE_FILE")
[ -z "$mt" ] && mt=0
mt_net=$(get_field '^MemTotal:' "$NET_FILE")
[ -z "$mt_net" ] && mt_net=0
mt_wf=$(get_field '^MemTotal:' "$WIFI_FILE")
[ -z "$mt_wf" ] && mt_wf=0
plat_mf=$(expr $mt - $fs_mf)

# NSS consumed: free mem that disappeared between failsafe and net
nss_mf=$(expr $fs_mf - $net_mf)
nss_sl=$(expr $net_sl - $fs_sl)
nss_sr=$(expr $net_sr - $fs_sr)
nss_su=$(expr $net_su - $fs_su)

# WiFi consumed: free mem that disappeared between net and wifi
wif_mf=$(expr $net_mf - $wf_mf)
wif_sl=$(expr $wf_sl - $net_sl)
wif_sr=$(expr $wf_sr - $net_sr)
wif_su=$(expr $wf_su - $net_su)

# Total consumed: failsafe to wifi
tot_mf=$(expr $fs_mf - $wf_mf)
tot_sl=$(expr $wf_sl - $fs_sl)
tot_sr=$(expr $wf_sr - $fs_sr)
tot_su=$(expr $wf_su - $fs_su)

echo "Snapshot Raw Values (kB)"
echo "========================================================================================"
printf "%-18s %-16s %-16s %-16s\n" "Field" "meminfo.failsafe" "meminfo.net" "meminfo.wifi"
echo "----------------------------------------------------------------------------------------"
printf "%-18s %-16d %-16d %-16d\n" "MemTotal"        "$mt"     "$mt_net"  "$mt_wf"
printf "%-18s %-16d %-16d %-16d\n" "MemFree+FreePCP" "$fs_mf"  "$net_mf"  "$wf_mf"
printf "%-18s %-16d %-16d %-16d\n" "Slab"            "$fs_sl"  "$net_sl"  "$wf_sl"
printf "%-18s %-16d %-16d %-16d\n" "SReclaimable"    "$fs_sr"  "$net_sr"  "$wf_sr"
printf "%-18s %-16d %-16d %-16d\n" "SUnreclaim"      "$fs_su"  "$net_su"  "$wf_su"
echo "========================================================================================"
echo ""
echo "Memory Consumption by Stage (kB)"
echo "========================================================================================"
printf "%-18s %-16s %-12s %-8s %-14s %-12s\n" \
	"Stage" "MemConsumed" "Slab" "Slab%" "SReclaimable" "SUnreclaim"
echo "----------------------------------------------------------------------------------------"

# Platform row: MemTotal - MemFree+FreePCP, Slab% = platform slab / final slab
if [ ! -f "$FAILSAFE_FILE" ]; then
	printf "%-18s [snapshot not found: %s]\n" "Platform" "$FAILSAFE_FILE"
else
	printf "%-18s %-16d %-12d %-8s %-14d %-12d\n" \
		"Platform" "$plat_mf" "$fs_sl" "$(pct $fs_sl $wf_sl)" "$fs_sr" "$fs_su"
fi

# NSS row: Slab% = NSS slab delta / total slab growth
if [ ! -f "$NET_FILE" ]; then
	printf "%-18s [snapshot not found: %s]\n" "NSS" "$NET_FILE"
else
	printf "%-18s %-16d %-12d %-8s %-14d %-12d\n" \
		"NSS" "$nss_mf" "$nss_sl" "$(pct $nss_sl $tot_sl)" "$nss_sr" "$nss_su"
fi

# WiFi row: Slab% = WiFi slab delta / total slab growth
if [ ! -f "$WIFI_FILE" ]; then
	printf "%-18s [snapshot not found: %s]\n" "WiFi" "$WIFI_FILE"
else
	printf "%-18s %-16d %-12d %-8s %-14d %-12d\n" \
		"WiFi" "$wif_mf" "$wif_sl" "$(pct $wif_sl $tot_sl)" "$wif_sr" "$wif_su"
fi

echo "----------------------------------------------------------------------------------------"
printf "%-18s %-16d %-12d %-8s %-14d %-12d\n" \
	"Total" "$tot_mf" "$tot_sl" "100.0" "$tot_sr" "$tot_su"
echo "========================================================================================"
echo "Note: MemConsumed = MemTotal - MemFree+FreePCP (platform) or delta between stages."
echo "      Slab% (Platform) = platform slab / final slab. Slab% (NSS/WiFi) = delta / total growth."
