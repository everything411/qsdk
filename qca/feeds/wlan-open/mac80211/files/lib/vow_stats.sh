#!/bin/sh
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: ISC
#
# VoW Stats Collection Script
#
# Usage: vow_stats.sh <phy_iface> [-L <link_id>] [-r] [-h]
#

LINK_ID=0
RESET=0
PHY=""
PHY_VALID=0

usage() {
    echo "Usage: $0 <phy_iface> [-L <link_id>] [-r] [-h]"
    echo ""
    echo "  <phy_iface>   Radio phy interface (e.g., phy00, phy10, phy20)"
    echo "  -L <link_id>  HW link ID for wifitelemetry (default: 0)"
    echo "  -r            Reset all stats first, then collect"
    echo "  -h            Show this help"
    echo ""
    echo "Examples:"
    echo "  $0 phy00"
    echo "  $0 phy10 -L 0 -r"
    exit 0
}

if [ $# -lt 1 ]; then
    usage
fi

PHY="$1"
shift

if ! echo "$PHY" | grep -qE '^phy[0-9]+$'; then
    echo "Error: Invalid PHY interface format. Expected format: phyN (e.g., phy0, phy1, phy10)"
    exit 1
fi

while [ $# -gt 0 ]; do
    case "$1" in
        -L)
            if [ -z "$2" ]; then
                echo "Error: -L requires an argument"
                exit 1
            fi
            if ! echo "$2" | grep -qE '^[0-9]+$'; then
                echo "Error: LINK_ID must be a non-negative integer"
                exit 1
            fi
            LINK_ID="$2"
            shift 2
            ;;
        -r|--reset)
            RESET=1
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            echo "Unknown option: $1"
            usage
            ;;
    esac
done

if [ -d "/sys/class/ieee80211/$PHY" ]; then
    PHY_VALID=1
fi

enable_vow_stats() {
    local phy="$1"
    if [ ! -d "/sys/class/ieee80211/$phy" ]; then
        return 1
    fi
    local current_mask=$(cat "/sys/kernel/debug/ieee80211/$phy/dp_stats_mask" 2>/dev/null || echo 0)
    local new_mask=$((0x$current_mask | 0x41))
    echo $new_mask > "/sys/kernel/debug/ieee80211/$phy/dp_stats_mask" 2>/dev/null
    return 0
}

echo "============================================================"
echo " VoW Stats "
echo " PHY: $PHY  |  Link ID: $LINK_ID  |  Reset: $RESET"
echo " $(date)"
echo "============================================================"

if [ "$RESET" -eq 1 ]; then
    echo ""
    echo "======================================"
    echo "Resetting Stats"
    echo "======================================"

    enable_vow_stats "$PHY"

    if [ "$PHY_VALID" -eq 1 ]; then
        echo 1 > "/sys/kernel/debug/ieee80211/$PHY/reset_dp_stats" 2>/dev/null
    fi

    for soc_path in /sys/kernel/debug/ath12k/pci-*/device_dp_stats \
                    /sys/kernel/debug/ath12k/ahb-*/device_dp_stats; do
        [ -f "$soc_path" ] || continue
        echo reset > "$soc_path"
    done

    echo "Stats reset complete."
fi

echo ""
echo "======================================"
echo "PDEV VoW TID Stats"
echo "======================================"

if [ "$PHY_VALID" -eq 1 ]; then
    enable_vow_stats "$PHY"
    wifitelemetry -ri "$PHY" -L "$LINK_ID" -f TID
else
    echo "phy interface $PHY not found, skipping VoW TID stats"
fi

echo ""
echo "======================================"
echo "HTT Stats (FW Stats)"
echo "======================================"

found_htt=0
for hw_path in /sys/kernel/debug/ath12k/pci-*/mac* \
               /sys/kernel/debug/ath12k/ahb-*/mac*; do
    [ -d "$hw_path" ] || continue
    found_htt=1

    hw=$(echo "$hw_path" | sed 's|.*/ath12k/||')
    echo "--- HW: $hw ---"

    # HTT Stats 1 (TX PDEV Rate Stats)
    echo "  [HTT Stats 1: TX PDEV Rate Stats]"
    echo 1 > "$hw_path/htt_stats_type" 2>/dev/null
    cat "$hw_path/htt_stats" 2>/dev/null

    # HTT Stats 2 (RX PDEV Rate Stats)
    echo "  [HTT Stats 2: RX PDEV Rate Stats]"
    echo 2 > "$hw_path/htt_stats_type" 2>/dev/null
    cat "$hw_path/htt_stats" 2>/dev/null

    # HTT Stats 6 (TQM/MSDU end reason - for drops)
    echo "  [HTT Stats 6: TQM/MSDU End Reason]"
    echo 6 > "$hw_path/htt_stats_type" 2>/dev/null
    cat "$hw_path/htt_stats" 2>/dev/null
    echo ""
done

[ "$found_htt" -eq 0 ] && echo "No HTT stats found"

echo ""
echo "======================================"
echo "WMM Stats"
echo "======================================"

found_wmm=0
for wmm_path in /sys/kernel/debug/ath12k/pci-*/mac*/wmm_stats \
                /sys/kernel/debug/ath12k/ahb-*/mac*/wmm_stats; do
    [ -f "$wmm_path" ] || continue
    found_wmm=1
    
    hw=$(echo "$wmm_path" | sed 's|.*/ath12k/||;s|/wmm_stats||')
    echo "--- HW: $hw ---"
    cat "$wmm_path"
    echo ""
done

[ "$found_wmm" -eq 0 ] && echo "No WMM stats found"

echo ""
echo "======================================"
echo "SOC DP Stats"
echo "======================================"

found_soc=0
for soc_path in /sys/kernel/debug/ath12k/pci-*/device_dp_stats \
                /sys/kernel/debug/ath12k/ahb-*/device_dp_stats; do
    [ -f "$soc_path" ] || continue
    found_soc=1
    soc=$(echo "$soc_path" | sed 's|.*/ath12k/||;s|/device_dp_stats||')
    echo "--- SOC: $soc ---"
    cat "$soc_path"
done

[ "$found_soc" -eq 0 ] && echo "No SOC stats found"

echo ""
echo "======================================"
echo "NSS Stats"
echo "======================================"

for eth in /sys/class/net/eth*; do
    [ -e "$eth" ] || continue
    eth=$(basename "$eth")
    echo "--- $eth ---"
    ethtool -S "$eth" 2>/dev/null || echo "$eth: stats not available"
done

echo ""
echo "======================================"
echo "Station Dump"
echo "======================================"

iw dev | awk '/Interface/{print $2}' | while IFS= read -r wlan; do
    echo "--- iw dev $wlan station dump ---"
    iw dev "$wlan" station dump
done

echo ""
echo "======================================"
echo "VOW stats Done"
echo "======================================"