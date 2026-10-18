#!/bin/sh
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: ISC
#
# mem_subsys_capture.sh - Capture /proc/meminfo for a specific subsystem configuration.
#
# Usage:
#   mem_subsys_capture.sh <2|3|4|5>
#
#   UC2 - Generic only       (no NSS, no WiFi stack)
#   UC3 - Generic + NSS      (WiFi stack disabled by removing modules)
#   UC4 - Full platform      (Generic + NSS + WiFi)
#   UC5 - Full platform + VAPs up
#
# Reset strategy: overlay wipe (rm -rf /overlay/*) + module removal replaces
# the old module-shelf (mv) approach.  No skip_wifi bootarg is used.
#
# Boot sequences:
#   UC2:  prep  → reboot → capture
#   UC3:  prep  → reboot → prep2 → reboot → capture
#   UC4:  prep  → reboot → capture
#   UC5:  prep  → reboot → capture
#
# Logs:  /root/mem_logs/uc<N>.log
# State: /root/.uc_target   (target UC number)
#        /root/.uc_stage    (prep | prep2 | capture | DONE)

STAGE_FILE="/root/.uc_stage"
TARGET_FILE="/root/.uc_target"
LOG_DIR="/root/mem_logs"
RC_LOCAL="/etc/rc.local"
SELF="/usr/sbin/mem_subsys_capture.sh"

log()    { echo "[$(date '+%F %T')] $*" | tee -a "$LOG_DIR/master.log"; }
uc_log() { echo "[$(date '+%F %T')] $*" >> "$LOG_FILE"; }
have()   { command -v "$1" >/dev/null 2>&1; }

# 3x drop-caches for stable readings
capture_mem() {
    uc_log "===== MEMORY CAPTURE ====="
    for i in 1 2 3; do
        uc_log "Dropping caches (pass $i)..."
        sync
        echo 3 > /proc/sys/vm/drop_caches
        sleep 2
    done
    uc_log "/proc/meminfo:"
    cat /proc/meminfo >> "$LOG_FILE"
}

reboot_stage() {
    sync
    log "Rebooting for capture..."
    reboot
    sleep 5
    exit 0
}

# Register script in rc.local so it auto-runs on next boot
register_boot() {
    if [ -f "$RC_LOCAL" ]; then
        grep -q "$SELF" "$RC_LOCAL" || sed -i "s|^exit 0|$SELF\nexit 0|" "$RC_LOCAL"
    else
        printf "#!/bin/sh\n%s\nexit 0\n" "$SELF" > "$RC_LOCAL"
        chmod +x "$RC_LOCAL"
    fi
}

# Remove script from rc.local after capture
unregister_boot() {
    [ -f "$RC_LOCAL" ] && sed -i "\|$SELF|d" "$RC_LOCAL" || true
}

_finish() {
    unregister_boot
    echo "DONE" > "$STAGE_FILE"
    log "UC${UC} capture complete. Log: $LOG_FILE"
}

# -----------------------------------------------------------------------
# Entry point
# -----------------------------------------------------------------------
mkdir -p "$LOG_DIR"

# If called with a UC argument, this is the initial invocation.
# Otherwise (called from rc.local on reboot), read state from files.
if [ -n "$1" ]; then
    UC="$1"
    case "$UC" in
        2|3|4|5) ;;
        *) echo "Usage: $0 <2|3|4|5>"; exit 1 ;;
    esac
    echo "$UC" > "$TARGET_FILE"
    echo "prep" > "$STAGE_FILE"
    log "Target set to UC${UC} (phase=prep)"
else
    # Called from rc.local: read saved target
    if [ ! -f "$TARGET_FILE" ]; then
        echo "No target UC set. Run: $0 <2|3|4|5>"
        exit 1
    fi
    UC=$(cat "$TARGET_FILE")
fi

PHASE=$(cat "$STAGE_FILE" 2>/dev/null)
LOG_FILE="$LOG_DIR/uc${UC}.log"
log "UC${UC} phase=${PHASE}"

case "$UC" in
# -----------------------------------------------------------------------
# UC4: Full platform (Generic + NSS + WiFi)
#   prep    — wipe overlay so platform boots clean, reboot
#   capture — capture
# -----------------------------------------------------------------------
4)
    if [ "$PHASE" = "prep" ]; then
        uc_log "UC4: Preparing — reset overlay"
        rm -rf /overlay/* || true
        sync
        echo "capture" > "$STAGE_FILE"
        register_boot
        reboot_stage
    else
        uc_log "UC4: Full platform (WiFi + NSS)"
        lsmod >> "$LOG_FILE" || true
        capture_mem
        _finish
    fi
    ;;
# -----------------------------------------------------------------------
# UC5: Full platform + VAPs up
#   prep    — enable all VAPs persistently, reboot so they come up
#   capture — reload wifi, capture
# -----------------------------------------------------------------------
5)
    if [ "$PHASE" = "prep" ]; then
        uc_log "UC5: Preparing — enabling all VAPs"
        if [ -f /etc/config/wireless ]; then
            sed -i 's/^\(\s*option\s\+disabled\s\+\).*/\10/' /etc/config/wireless || true
            have uci && uci commit wireless 2>/dev/null || true
        fi
        echo "capture" > "$STAGE_FILE"
        register_boot
        reboot_stage
    else
        uc_log "UC5: WiFi VAPs enabled"
        have wifi && { wifi reload || true; sleep 5; }
        have iw && { iw dev >> "$LOG_FILE" || true; }
        lsmod >> "$LOG_FILE" || true
        capture_mem
        _finish
    fi
    ;;
# -----------------------------------------------------------------------
# UC3: Generic + NSS, WiFi stack disabled
#   prep    — wipe overlay (restore clean slate), reboot
#   prep2   — remove WiFi ko files, reboot
#   capture — capture (no WiFi stack present)
# -----------------------------------------------------------------------
3)
    if [ "$PHASE" = "prep" ]; then
        uc_log "UC3: Preparing — reset overlay"
        rm -rf /overlay/* || true
        sync
        echo "prep2" > "$STAGE_FILE"
        register_boot
        reboot_stage
    elif [ "$PHASE" = "prep2" ]; then
        uc_log "UC3: Preparing — remove WiFi modules"
        KDIR="/lib/modules/$(uname -r)"
        if [ -d "$KDIR" ]; then
            cd "$KDIR" || true
            rm -f cfg80211.ko mac80211.ko ath12*.ko ath11*.ko ath.ko compat.ko telemetry_agent.ko || true
            log "Removed WiFi modules for UC3"
        else
            log "Kernel module dir '$KDIR' not found; skipping module removals."
        fi
        sync
        echo "capture" > "$STAGE_FILE"
        register_boot
        reboot_stage
    else
        uc_log "UC3: NSS active; WiFi modules absent"
        lsmod >> "$LOG_FILE" || true
        capture_mem
        _finish
    fi
    ;;
# -----------------------------------------------------------------------
# UC2: Generic only (no NSS, no WiFi stack)
#   prep    — remove NSS + WiFi modules from FS, reboot
#   capture — capture (bare generic platform)
# -----------------------------------------------------------------------
2)
    if [ "$PHASE" = "prep" ]; then
        uc_log "UC2: Preparing — remove NSS/WiFi modules"
        KDIR="/lib/modules/$(uname -r)"
        if [ -d "$KDIR" ]; then
            cd "$KDIR" || true
            # WiFi stack
            rm -f cfg80211.ko mac80211.ko ath12*.ko ath11*.ko ath.ko compat.ko telemetry_agent.ko || true
            # NSS
            rm -f qca-nss-* qca-mcs.ko hyfi-bridging.ko qca-ssdk.ko emesh-sp.ko ecm*.ko phy-*.ko || true
            log "Removed WiFi and NSS modules for UC2"
        else
            log "Kernel module dir '$KDIR' not found; skipping module removals."
        fi
        echo "capture" > "$STAGE_FILE"
        register_boot
        reboot_stage
    else
        uc_log "UC2: Generic only (no NSS, no WiFi stack)"
        lsmod >> "$LOG_FILE" || true
        capture_mem
        _finish
    fi
    ;;
*)
    log "Unknown UC '${UC}'. Usage: $0 <2|3|4|5>"
    exit 1
    ;;
esac
