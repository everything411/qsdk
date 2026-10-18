#!/bin/sh
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: ISC
# =============================================================================
# KPI Statistics Collection Script
#
# Usage: kpi_stats.sh {start|end}
#
# Arguments:
#   start - Collect statistics before a test run
#   end   - Collect statistics after a test run
#
# Output: /tmp/kpi_stats_log.txt
# =============================================================================

OUTPUT_FILE="/tmp/kpi_stats_log.txt"

# -----------------------------------------------------------------------------
# Function: print_error
# Description: Print error message to stderr
# -----------------------------------------------------------------------------
print_error() {
    echo "Error: $1" >&2
}

# -----------------------------------------------------------------------------
# Function: print_warning
# Description: Print warning message to stderr
# -----------------------------------------------------------------------------
print_warning() {
    echo "Warning: $1" >&2
}

# -----------------------------------------------------------------------------
# Function: start_background_monitoring
# Description: Start background collection of mpstat and sar on core 3.
#              Writes per-interval section markers directly to OUTPUT_FILE.
#              Must be called AFTER all main-script writes to OUTPUT_FILE are
#              complete for the START phase to guarantee no concurrent access.
# -----------------------------------------------------------------------------
start_background_monitoring() {
    # Check if required commands exist
    if ! command -v mpstat >/dev/null 2>&1; then
        print_warning "mpstat command not found, skipping background monitoring"
        return 1
    fi

    if ! command -v sar >/dev/null 2>&1; then
        print_warning "sar command not found, skipping background monitoring"
        return 1
    fi

    # Terminate any stale monitor from a previous run that was not stopped cleanly
    if [ -f /tmp/kpi_monitor.pid ]; then
        stale_pid=$(cat /tmp/kpi_monitor.pid)
        if kill -0 "$stale_pid" 2>/dev/null; then
            print_warning "Stale background monitor found (PID: $stale_pid), terminating it"
            pkill -P "$stale_pid" 2>/dev/null
            kill "$stale_pid" 2>/dev/null
            sleep 1
        fi
        rm -f /tmp/kpi_monitor.pid
    fi

    # Check for taskset and verify core availability
    if ! command -v taskset >/dev/null 2>&1; then
        print_warning "taskset command not found, running without CPU affinity"
        TASKSET_CMD=""
    else
        num_cores=$(grep -c ^processor /proc/cpuinfo 2>/dev/null || echo "0")
        if [ "$num_cores" -lt 4 ]; then
            print_warning "Core 3 not available (only $num_cores cores), running without CPU affinity"
            TASKSET_CMD=""
        else
            TASKSET_CMD="taskset -c 3"
        fi
    fi

    # Write section open marker before starting background process so the marker
    # is guaranteed to precede all background writes in the output file
    log_section_header "background_monitoring" "START"

    # Start background loop writing per-interval markers directly to OUTPUT_FILE.
    # OUTPUT_FILE is expanded now (double-quoted) so the subshell uses the correct path.
    # Each mpstat and sar sample is wrapped in its own START/END markers so the
    # parser can cleanly extract all samples even though they alternate in the file.
    $TASKSET_CMD sh -c "
        iter=1
        while true; do
            echo \"#mpstat_iteration_\${iter} START#\" >> \"$OUTPUT_FILE\"
            mpstat -P ALL 3 3 >> \"$OUTPUT_FILE\" 2>&1
            echo \"#mpstat_iteration_\${iter} END#\" >> \"$OUTPUT_FILE\"
            echo '' >> \"$OUTPUT_FILE\"
            sleep 3
            echo \"#sar_iteration_\${iter} START#\" >> \"$OUTPUT_FILE\"
            sar -n DEV 3 3 >> \"$OUTPUT_FILE\" 2>&1
            echo \"#sar_iteration_\${iter} END#\" >> \"$OUTPUT_FILE\"
            echo '' >> \"$OUTPUT_FILE\"
            iter=\$(( iter + 1 ))
            sleep 3
        done
    " &

    echo $! > /tmp/kpi_monitor.pid
    echo "Background monitoring started (PID: $!)" >&2
    return 0
}

# -----------------------------------------------------------------------------
# Function: stop_background_monitoring
# Description: Stop background monitoring and write the section close marker.
#              Must be called BEFORE any main-script writes to OUTPUT_FILE in
#              the END phase to guarantee no concurrent access.
# -----------------------------------------------------------------------------
stop_background_monitoring() {
    # Handle missing PID file — monitoring may not have been started or already stopped
    if [ ! -f /tmp/kpi_monitor.pid ]; then
        print_warning "No monitor PID file found, background monitoring may not have been started"
        log_section_header "background_monitoring" "END"
        echo "" >> "$OUTPUT_FILE"
        return 0
    fi

    monitor_pid=$(cat /tmp/kpi_monitor.pid)

    # Handle case where process already exited (e.g., OOM killed)
    if ! kill -0 "$monitor_pid" 2>/dev/null; then
        print_warning "Background monitor (PID: $monitor_pid) is no longer running"
        rm -f /tmp/kpi_monitor.pid
        log_section_header "background_monitoring" "END"
        echo "" >> "$OUTPUT_FILE"
        return 0
    fi

    # Step 1: Kill direct children of the sh wrapper (the running mpstat, sar, or sleep).
    # This prevents orphaned child processes from writing to OUTPUT_FILE after the
    # parent sh wrapper is killed.
    pkill -P "$monitor_pid" 2>/dev/null

    # Step 2: Kill the sh wrapper itself
    kill "$monitor_pid" 2>/dev/null

    # Step 3: Allow up to 2 seconds for any in-flight write to complete.
    # mpstat and sar each run for ~1 second; this margin ensures OUTPUT_FILE
    # is not written to after we proceed.
    sleep 2

    # Step 4: Verify termination; escalate to SIGKILL if process is still alive
    if kill -0 "$monitor_pid" 2>/dev/null; then
        print_warning "Background monitor (PID: $monitor_pid) did not terminate cleanly, sending SIGKILL"
        pkill -P "$monitor_pid" 2>/dev/null
        kill -9 "$monitor_pid" 2>/dev/null
        sleep 1
    fi

    rm -f /tmp/kpi_monitor.pid
    echo "Background monitoring stopped" >&2

    # Write section close marker only after confirmed process termination,
    # guaranteeing no background writes will follow this marker in the file
    log_section_header "background_monitoring" "END"
    echo "" >> "$OUTPUT_FILE"

    return 0
}

# -----------------------------------------------------------------------------
# Function: validate_argument
# Description: Validate script argument
# -----------------------------------------------------------------------------
validate_argument() {
    arg="$1"

    if [ "$arg" != "start" ] && [ "$arg" != "end" ]; then
        print_error "Argument must be 'start' or 'end'"
        echo "Usage: $0 {start|end}" >&2
        exit 1
    fi
}

# -----------------------------------------------------------------------------
# Function: log_command
# Description: Log and execute a command, handling errors gracefully
# Arguments:
#   $1 - Command to execute
#   $2 - Section name (for error reporting)
# -----------------------------------------------------------------------------
log_command() {
    cmd="$1"
    section="${2:-unknown}"

    echo "+ $cmd" >> "$OUTPUT_FILE"

    if eval "$cmd" >> "$OUTPUT_FILE" 2>&1; then
        echo "" >> "$OUTPUT_FILE"
        return 0
    else
        exit_code=$?
        echo "# Command failed with exit code: $exit_code" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        print_warning "Command failed in section '$section': $cmd (exit code: $exit_code)"
        return $exit_code
    fi
}

# -----------------------------------------------------------------------------
# Function: log_section_header
# Description: Log a section header with consistent formatting
# -----------------------------------------------------------------------------
log_section_header() {
    section_name="$1"
    action="${2:-START}"
    echo "#${section_name} ${action}#" >> "$OUTPUT_FILE"
}

# -----------------------------------------------------------------------------
# Function: collect_file_content
# Description: Safely collect content from a file
# -----------------------------------------------------------------------------
collect_file_content() {
    filepath="$1"
    section_name="$2"

    if [ -f "$filepath" ]; then
        log_command "cat $filepath" "$section_name"
    else
        echo "# File not found: $filepath" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        print_warning "File not found: $filepath"
    fi
}

# -----------------------------------------------------------------------------
# Function: collect_directory_files
# Description: Collect content from all files in a directory
# -----------------------------------------------------------------------------
collect_directory_files() {
    dir_path="$1"
    section_name="$2"

    if [ ! -d "$dir_path" ]; then
        echo "# Directory not found: $dir_path" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        print_warning "Directory not found: $dir_path"
        return 1
    fi

    for file in "$dir_path"/*; do
        if [ -f "$file" ]; then
            log_command "cat $file" "$section_name"
        fi
    done
}

# -----------------------------------------------------------------------------
# Function: collect_network_interfaces
# Description: Collect statistics for all active network interfaces
# Arguments:
#   $1 - Command template (use $iface as placeholder for interface name)
#   $2 - Section name
#   $3 - "yes" to skip bridge interfaces (br-*) e.g. for ethtool
# -----------------------------------------------------------------------------
collect_network_interfaces() {
    cmd_template="$1"
    section_name="$2"
    skip_bridges="$3"

    for iface in $(ls /sys/class/net 2>/dev/null); do
        # Skip loopback and bonding_masters
        if [ "$iface" = "lo" ] || [ "$iface" = "bonding_masters" ]; then
            continue
        fi

        # Skip bridge interfaces if requested (for ethtool which doesn't support bridges)
        if [ "$skip_bridges" = "yes" ]; then
            case "$iface" in
                br-*) continue ;;
            esac
        fi

        # Check if interface is up
        if [ -f "/sys/class/net/$iface/operstate" ]; then
            state=$(cat "/sys/class/net/$iface/operstate" 2>/dev/null)
            if [ "$state" = "up" ]; then
                cmd=$(echo "$cmd_template" | sed "s/\$iface/$iface/g")
                log_command "$cmd" "$section_name"
            fi
        fi
    done
}

# -----------------------------------------------------------------------------
# Function: collect_wireless_interfaces
# Description: Collect wireless interface information
# -----------------------------------------------------------------------------
collect_wireless_interfaces() {
    # Get list of wireless interfaces
    interfaces=$(iw dev 2>/dev/null | awk '/^[[:space:]]*Interface[[:space:]]+/ {print $2}')

    if [ -z "$interfaces" ]; then
        echo "# No wireless interfaces found" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        return 0
    fi

    for iface in $interfaces; do
        log_section_header "iw_dev_${iface}_info" "START"
        log_command "iw dev $iface info" "wireless_info"
        log_section_header "iw_dev_${iface}_info" "END"
        echo "" >> "$OUTPUT_FILE"

        log_section_header "iw_station_dump_${iface}" "START"
        log_command "iw dev $iface station dump" "wireless_station_dump"
        log_section_header "iw_station_dump_${iface}" "END"
        echo "" >> "$OUTPUT_FILE"
    done
}

# -----------------------------------------------------------------------------
# Function: collect_htt_stats
# Description: Collect HTT statistics for all available interfaces
# -----------------------------------------------------------------------------
collect_htt_stats() {
    stats_types="1 2 5 6 8 9 10"

    # Find all htt_stats_type files
    htt_files=$(find / -type f -name "htt_stats_type" 2>/dev/null)

    if [ -z "$htt_files" ]; then
        echo "# No htt_stats_type files found" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        return 0
    fi

    echo "$htt_files" | while IFS= read -r htt_stats_type_path; do
        dir=$(dirname "$htt_stats_type_path")
        htt_stats_path="$dir/htt_stats"

        for num in $stats_types; do
            if echo "$num" > "$htt_stats_type_path" 2>/dev/null; then
                echo "+ echo $num > $htt_stats_type_path" >> "$OUTPUT_FILE"

                if [ -f "$htt_stats_path" ]; then
                    log_command "cat $htt_stats_path" "htt_stats"
                else
                    echo "# htt_stats file not found in $dir" >> "$OUTPUT_FILE"
                    echo "" >> "$OUTPUT_FILE"
                fi
            else
                print_warning "Failed to write to $htt_stats_type_path"
            fi
        done
    done
}

# -----------------------------------------------------------------------------
# Function: collect_skb_recycler_cpu_info
# Description: Collect SKB recycler CPU-specific information
# -----------------------------------------------------------------------------
collect_skb_recycler_cpu_info() {
    base_dir="/proc/net/skb_recycler"

    if [ ! -d "$base_dir" ]; then
        echo "# SKB recycler directory not found: $base_dir" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        return 0
    fi

    for cpu_dir in "$base_dir"/cpu*/; do
        if [ -d "$cpu_dir" ]; then
            for file in "$cpu_dir"*; do
                if [ -f "$file" ]; then
                    log_command "cat $file" "skb_recycler_cpuinfo"
                fi
            done
        fi
    done
}

# -----------------------------------------------------------------------------
# Function: collect_ieee80211_stats
# Description: Collect IEEE 802.11 configuration and statistics
# -----------------------------------------------------------------------------
collect_ieee80211_stats() {
    base_dir="/sys/kernel/debug/ieee80211"

    if [ ! -d "$base_dir" ]; then
        echo "# IEEE 802.11 debug directory not found: $base_dir" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        return 0
    fi

    # Find all files except those in statistics subdirectories
    files=$(find "$base_dir" -type f ! -path "*/statistics/*" 2>/dev/null)

    if [ -z "$files" ]; then
        echo "# No IEEE 802.11 debug files found" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        return 0
    fi

    echo "$files" | while IFS= read -r file; do
        log_command "cat $file" "ieee80211_config_and_stats"
    done
}

# -----------------------------------------------------------------------------
# Function: collect_primary_link_info
# Description: Collect primary link information from all locations
# -----------------------------------------------------------------------------
collect_primary_link_info() {
    files=$(find / -type f -name "primary_link" 2>/dev/null)

    if [ -z "$files" ]; then
        echo "# No primary_link files found" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        return 0
    fi

    echo "$files" | while IFS= read -r filepath; do
        log_command "cat $filepath" "primary_link"
    done
}

# -----------------------------------------------------------------------------
# Function: collect_device_dp_stats
# Description: Collect device datapath statistics
# -----------------------------------------------------------------------------
collect_device_dp_stats() {
    files=$(find / -type f -name "device_dp_stats" 2>/dev/null)

    if [ -z "$files" ]; then
        echo "# No device_dp_stats files found" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        return 0
    fi

    echo "$files" | while IFS= read -r filepath; do
        log_command "cat $filepath" "device_dp_stats"
    done
}

# -----------------------------------------------------------------------------
# Function: collect_ecm_debug_stats
# Description: Collect ECM debug statistics
# -----------------------------------------------------------------------------
collect_ecm_debug_stats() {
    base_dir="/sys/kernel/debug/ecm"

    if [ ! -d "$base_dir" ]; then
        echo "# ECM debug directory not found: $base_dir" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        return 0
    fi

    files=$(find "$base_dir" -type f 2>/dev/null)

    if [ -z "$files" ]; then
        echo "# No ECM debug files found" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
        return 0
    fi

    echo "$files" | while IFS= read -r file; do
        log_command "cat $file" "ecm_debug_stats"
    done
}

# =============================================================================
# MAIN COLLECTION FUNCTIONS
# =============================================================================

# -----------------------------------------------------------------------------
# Function: collect_system_info
# Description: Collect basic system information
# -----------------------------------------------------------------------------
collect_system_info() {
    log_section_header "board_info" "START"
    collect_file_content "/tmp/board.json" "board_info"
    log_section_header "board_info" "END"

    log_section_header "proc_version" "START"
    collect_file_content "/proc/version" "proc_version"
    log_section_header "proc_version" "END"

    log_section_header "openwrt_release" "START"
    collect_file_content "/etc/openwrt_release" "openwrt_release"
    log_section_header "openwrt_release" "END"

    log_section_header "openwrt_version" "START"
    collect_file_content "/etc/openwrt_version" "openwrt_version"
    log_section_header "openwrt_version" "END"

    log_section_header "date" "START"
    log_command "date" "date"
    log_section_header "date" "END"

    log_section_header "date_in_seconds" "START"
    log_command "date +%s" "date_in_seconds"
    log_section_header "date_in_seconds" "END"

    log_section_header "uptime" "START"
    log_command "uptime" "uptime"
    log_section_header "uptime" "END"
}

# -----------------------------------------------------------------------------
# Function: collect_firmware_config
# Description: Collect firmware and configuration information
# -----------------------------------------------------------------------------
collect_firmware_config() {
    log_section_header "fw_printenv" "START"
    log_command "fw_printenv" "fw_printenv"
    log_section_header "fw_printenv" "END"

    log_section_header "ecm_config_info" "START"
    collect_file_content "/etc/config/ecm" "ecm_config_info"
    log_section_header "ecm_config_info" "END"

    log_section_header "skb_recycler_config_info" "START"
    collect_file_content "/etc/config/skb_recycler" "skb_recycler_config_info"
    log_section_header "skb_recycler_config_info" "END"

    log_section_header "qca_nss_dp_config" "START"
    collect_file_content "/etc/config/qca_nss_dp" "qca_nss_dp_config"
    log_section_header "qca_nss_dp_config" "END"

    log_section_header "wireless_config" "START"
    collect_file_content "/etc/config/wireless" "wireless_config"
    log_section_header "wireless_config" "END"
}

# -----------------------------------------------------------------------------
# Function: collect_skb_recycler_stats
# Description: Collect SKB recycler statistics
# -----------------------------------------------------------------------------
collect_skb_recycler_stats() {
    log_section_header "skb_recycler_max_skbs" "START"
    collect_file_content "/proc/net/skb_recycler/max_skbs" "skb_recycler_max_skbs"
    log_section_header "skb_recycler_max_skbs" "END"

    log_section_header "skb_recycler_max_spare_skbs" "START"
    collect_file_content "/proc/net/skb_recycler/max_spare_skbs" "skb_recycler_max_spare_skbs"
    log_section_header "skb_recycler_max_spare_skbs" "END"

    log_section_header "skb_recycler_enable" "START"
    collect_file_content "/proc/net/skb_recycler/skb_recycler_enable" "skb_recycler_enable"
    log_section_header "skb_recycler_enable" "END"

    log_section_header "skb_recycler_cpuinfo" "START"
    collect_skb_recycler_cpu_info
    log_section_header "skb_recycler_cpuinfo" "END"
}

# -----------------------------------------------------------------------------
# Function: collect_module_parameters
# Description: Collect kernel module parameters
# -----------------------------------------------------------------------------
collect_module_parameters() {
    modules="ath12k qca_nss_ppe_ds qca_nss_ppe qca_nss_dp mac80211"

    for module in $modules; do
        log_section_header "${module}_module_param" "START"
        collect_directory_files "/sys/module/${module}/parameters" "${module}_module_param"
        log_section_header "${module}_module_param" "END"
    done
}

# -----------------------------------------------------------------------------
# Function: collect_network_stats
# Description: Collect network interface statistics
# -----------------------------------------------------------------------------
collect_network_stats() {
    log_section_header "primary_link" "START"
    collect_primary_link_info
    log_section_header "primary_link" "END"

    log_section_header "ethtool" "START"
    collect_network_interfaces "ethtool -S \$iface" "ethtool" "yes"
    log_section_header "ethtool" "END"

    log_section_header "ethtool_settings" "START"
    collect_network_interfaces "ethtool \$iface" "ethtool_settings" "yes"
    log_section_header "ethtool_settings" "END"

    log_section_header "ifconfig" "START"
    collect_network_interfaces "ifconfig \$iface" "ifconfig" "no"
    log_section_header "ifconfig" "END"

    log_section_header "tc_qdisc_show" "START"
    log_command "tc qdisc show" "tc_qdisc_show"
    log_section_header "tc_qdisc_show" "END"
}

# -----------------------------------------------------------------------------
# Function: collect_system_resources
# Description: Collect system resource information
# -----------------------------------------------------------------------------
collect_system_resources() {
    log_section_header "interrupts" "START"
    collect_file_content "/proc/interrupts" "interrupts"
    log_section_header "interrupts" "END"

    log_section_header "meminfo" "START"
    collect_file_content "/proc/meminfo" "meminfo"
    log_section_header "meminfo" "END"

    log_section_header "top" "START"
    log_command "top -b -n 1" "top"
    log_section_header "top" "END"

    log_section_header "proc_stat" "START"
    collect_file_content "/proc/stat" "proc_stat"
    log_section_header "proc_stat" "END"
}

# -----------------------------------------------------------------------------
# Function: collect_wireless_stats
# Description: Collect wireless statistics
# -----------------------------------------------------------------------------
collect_wireless_stats() {
    log_section_header "iw_dev" "START"
    log_command "iw dev" "iw_dev"
    log_section_header "iw_dev" "END"

    log_section_header "iw_dev_station_dump" "START"
    collect_wireless_interfaces
    log_section_header "iw_dev_station_dump" "END"
}

# -----------------------------------------------------------------------------
# Function: collect_driver_stats
# Description: Collect driver-specific statistics
# -----------------------------------------------------------------------------
collect_driver_stats() {
    log_section_header "ecm_debug_stats" "START"
    collect_ecm_debug_stats
    log_section_header "ecm_debug_stats" "END"

    log_section_header "device_dp_stats" "START"
    collect_device_dp_stats
    log_section_header "device_dp_stats" "END"

    log_section_header "htt_stats" "START"
    collect_htt_stats
    log_section_header "htt_stats" "END"

    log_section_header "edma_tx_stats" "START"
    collect_file_content "/sys/kernel/debug/qca-nss-dp/stats/tx_ring_stats" "edma_tx_stats"
    log_section_header "edma_tx_stats" "END"

    log_section_header "edma_rx_stats" "START"
    collect_file_content "/sys/kernel/debug/qca-nss-dp/stats/rx_ring_stats" "edma_rx_stats"
    log_section_header "edma_rx_stats" "END"

    log_section_header "vp_stats" "START"
    collect_file_content "/sys/kernel/debug/qca-nss-ppe/ppe_vp/vp_stats" "vp_stats"
    log_section_header "vp_stats" "END"

    log_section_header "ds_node_stats" "START"
    collect_file_content "/sys/kernel/debug/qca-nss-ppe/ppe_ds/ppe_ds_node_stats" "ds_node_stats"
    log_section_header "ds_node_stats" "END"
}

# -----------------------------------------------------------------------------
# Function: collect_linux_config
# Description: Collect Linux kernel configuration
# -----------------------------------------------------------------------------
collect_linux_config() {
    log_section_header "linux_config" "START"
    if [ -f "/proc/config.gz" ]; then
        echo "+ zcat /proc/config.gz | grep -v '^#' | grep -v '^\$'" >> "$OUTPUT_FILE"
        zcat /proc/config.gz 2>/dev/null | grep -v '^#' | grep -v '^$' >> "$OUTPUT_FILE" 2>&1
        echo "" >> "$OUTPUT_FILE"
    else
        echo "# /proc/config.gz not found" >> "$OUTPUT_FILE"
        echo "" >> "$OUTPUT_FILE"
    fi
    log_section_header "linux_config" "END"
}

# =============================================================================
# MAIN SCRIPT EXECUTION
# =============================================================================
main() {
    arg="$1"

    # Validate argument
    validate_argument "$arg"

    # Initialize output file
    if [ "$arg" = "start" ]; then
        echo "#KPI Debug Statistics#" > "$OUTPUT_FILE"
        echo "#STATS_COLLECTION_BEFORE_RUN_START#" >> "$OUTPUT_FILE"
    fi

    # -------------------------------------------------------------------------
    # STATIC INFORMATION - Collected only at START
    # These items don't change during test execution
    # -------------------------------------------------------------------------
    if [ "$arg" = "start" ]; then
        echo "Collecting system information..." >&2
        collect_system_info

        echo "Collecting firmware and configuration..." >&2
        collect_firmware_config

        echo "Collecting module parameters..." >&2
        collect_module_parameters

        echo "Collecting Linux configuration..." >&2
        collect_linux_config
    fi

    # -------------------------------------------------------------------------
    # BACKGROUND MONITORING - Stop at END, before writing AFTER_RUN section.
    # Background process must be fully terminated before any main-script writes
    # resume, ensuring strictly sequential access to OUTPUT_FILE.
    # -------------------------------------------------------------------------
    if [ "$arg" = "end" ]; then
        echo "Stopping background monitoring..." >&2
        stop_background_monitoring
        echo "#STATS_COLLECTION_AFTER_RUN_START#" >> "$OUTPUT_FILE"
    fi

    # -------------------------------------------------------------------------
    # DYNAMIC INFORMATION - Collected at both START and END
    # At START: runs before background starts — no concurrent writes possible
    # At END:   runs after background is stopped — no concurrent writes possible
    # -------------------------------------------------------------------------
    echo "Collecting SKB recycler statistics..." >&2
    collect_skb_recycler_stats

    echo "Collecting network statistics..." >&2
    collect_network_stats

    echo "Collecting system resources..." >&2
    collect_system_resources

    echo "Collecting wireless statistics..." >&2
    collect_wireless_stats

    echo "Collecting driver statistics..." >&2
    collect_driver_stats

    # Add final timestamp
    log_section_header "date" "START"
    log_command "date" "date"
    log_section_header "date" "END"

    log_section_header "date_in_seconds" "START"
    log_command "date +%s" "date_in_seconds"
    log_section_header "date_in_seconds" "END"

    # -------------------------------------------------------------------------
    # BACKGROUND MONITORING - Start at END of START phase.
    # All main-script writes to OUTPUT_FILE are complete at this point.
    # From here until the END phase invocation, only the background process
    # writes to OUTPUT_FILE — no concurrent access is possible.
    # -------------------------------------------------------------------------
    if [ "$arg" = "start" ]; then
        echo "#STATS_COLLECTION_BEFORE_RUN_END#" >> "$OUTPUT_FILE"
        echo "Starting background monitoring..." >&2
        start_background_monitoring
        echo "Statistics collection complete. Background monitoring running. Output: $OUTPUT_FILE" >&2
    elif [ "$arg" = "end" ]; then
        echo "#STATS_COLLECTION_AFTER_RUN_END#" >> "$OUTPUT_FILE"
        echo "Statistics collection complete. Output saved to: $OUTPUT_FILE" >&2
    fi

    return 0
}

# Execute main function
main "$@"
