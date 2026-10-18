#!/bin/sh

# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: ISC

HOSTAPD_DIR="/var/run/hostapd"
WPA_SUPPLICANT_DIR="/var/run/wpa_supplicant"
CONF_FILE="/var/run/rptr_mgr.conf"

# Clear the config file
> "$CONF_FILE"

#sample rptr_mgr.conf file
#mld_ap=phy00-mld2
#links=phy00-mld2_link0 phy00-mld2_link1

#mld_sta=phy00-mld1

#comm_type=1
#athnewind=1
#skip_cac=1


# Process hostapd/wpa_supplicant interfaces
for entry in $(ls "$HOSTAPD_DIR"); do
    case "$entry" in
        *_link*) continue ;;  # Skip link interfaces
        global) continue ;;   # Skip global
        hostapd_if_eloop*) continue ;;
    esac

    echo "mld_ap=$entry" >> "$CONF_FILE"

    # Find corresponding links (if any)
    links=$(ls "$HOSTAPD_DIR" 2>/dev/null | grep "^${entry}_link" | tr '\n' ' ')
    echo "links=$links" >> "$CONF_FILE"
    echo "" >> "$CONF_FILE"
done

# Process wpa_supplicant interfaces
for entry in $(ls "$WPA_SUPPLICANT_DIR"); do
    case "$entry" in
        global) continue ;;   # Skip global
    esac

    echo "mld_sta=$entry" >> "$CONF_FILE"
    echo "" >> "$CONF_FILE"
done

# Get skip_cac: 1/0 - Enable/Disable
echo "skip_cac=$1" >> "$CONF_FILE"
# Get rptr_mgr_mode: UBUS-based communication(1) or Socket-based communication (2)
echo "comm_type=$2" >> "$CONF_FILE"
# Get athnewind: 1/0 - Independent Repeater Mode Enable/Disable
echo "athnewind=$3" >> "$CONF_FILE"
