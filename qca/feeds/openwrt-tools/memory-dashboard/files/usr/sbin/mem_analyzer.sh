#!/bin/sh
#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: ISC
#
# memory_analyzer.sh - Measure real physical memory usage in OpenWrt
# Usage: ./memory_analyzer.sh [options]
#   -a    Show all processes (default: only show user processes)
#   -s    Sort by PSS (default), RSS, USS, VSZ, DATA, BSS, or STACK
#   -c    Show memory in KB (default), MB, or B
#   -t    Include totals row
#   -h    Show this help message

# Process command line options
SHOW_ALL=0
SORT_BY="pss"
UNIT="KB"
SHOW_TOTALS=0

while getopts "as:c:th" opt; do
  case $opt in
    a) SHOW_ALL=1 ;;
    s) SORT_BY=$OPTARG ;;
    c) UNIT=$OPTARG ;;
    t) SHOW_TOTALS=1 ;;
    h) echo "Usage: $0 [-a] [-s pss|rss|uss|vsz|data|bss|stack] [-c KB|MB|B] [-t] [-h]"
       echo "  -a    Show all processes (default: only show user processes)"
       echo "  -s    Sort by PSS (default), RSS, USS, VSZ, DATA, BSS, or STACK"
       echo "  -c    Show memory in KB (default), MB, or B"
       echo "  -t    Include totals row"
       echo "  -h    Show this help message"
       exit 0 ;;
    *) echo "Invalid option: -$OPTARG" >&2
       exit 1 ;;
  esac
done

echo 3 > /proc/sys/vm/drop_caches
cat /proc/meminfo
# Function to convert units
convert_units() {
  value=$1
  if [ "$UNIT" = "MB" ]; then
    # Use printf for better precision without bc dependency
    printf "%.2f" $(echo "$value / 1024" | bc -l)
  elif [ "$UNIT" = "B" ]; then
    echo "$((value * 1024))"
  else
    echo $value
  fi
}

# Function to format value with unit
format_with_unit() {
  value=$1
  converted=$(convert_units $value)
  echo "${converted}${UNIT}"
}

# Check if smaps is available
if [ ! -r /proc/1/smaps ]; then
  echo "Error: /proc/pid/smaps not available. Kernel may not support this feature." >&2
  exit 1
fi

# Create a more reliable temporary file
TEMP_FILE="/tmp/memory_analyzer_$$"
touch "$TEMP_FILE"
if [ ! -f "$TEMP_FILE" ]; then
  echo "Error: Could not create temporary file" >&2
  exit 1
fi

# Make sure we clean up the temp file on exit
trap "rm -f $TEMP_FILE" EXIT INT TERM

# Print header
printf "%-8s %-20s %-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s\n" \
  "PID" "COMMAND" "VSZ(${UNIT})" "RSS(${UNIT})" "PSS(${UNIT})" "USS(${UNIT})" "SHR(${UNIT})" \
  "DATA(${UNIT})" "BSS(${UNIT})" "STACK(${UNIT})" "DIRTY(${UNIT})"

# Process each PID
for pid in $(ls -1 /proc/ | grep -E '^[0-9]+$'); do
  # Skip kernel threads if SHOW_ALL=0
  if [ $SHOW_ALL -eq 0 ]; then
    # More efficient check for kernel threads
    if [ ! -s /proc/$pid/cmdline ] && ! grep -q "VmSize" /proc/$pid/status 2>/dev/null; then
      continue
    fi
  fi

  if [ -r /proc/$pid/smaps ]; then
    # Get command name more efficiently
    if [ -s /proc/$pid/cmdline ]; then
      cmd=$(tr '\0' ' ' < /proc/$pid/cmdline | sed 's/ $//' | head -c 20)
    else
      cmd=$(cat /proc/$pid/comm 2>/dev/null)
    fi
    
    if [ ! -z "$cmd" ]; then
      # Get memory metrics without eval
      VmSize=$(grep -m1 "VmSize:" /proc/$pid/status 2>/dev/null | awk '{print $2}')
      VmRSS=$(grep -m1 "VmRSS:" /proc/$pid/status 2>/dev/null | awk '{print $2}')
      VmData=$(grep -m1 "VmData:" /proc/$pid/status 2>/dev/null | awk '{print $2}')
      VmStk=$(grep -m1 "VmStk:" /proc/$pid/status 2>/dev/null | awk '{print $2}')
      VmExe=$(grep -m1 "VmExe:" /proc/$pid/status 2>/dev/null | awk '{print $2}')
      
      # Default values if not found
      [ -z "$VmSize" ] && VmSize=0
      [ -z "$VmRSS" ] && VmRSS=0
      [ -z "$VmData" ] && VmData=0
      [ -z "$VmStk" ] && VmStk=0
      [ -z "$VmExe" ] && VmExe=0
      
      # Calculate PSS, USS, and Dirty more efficiently
      pss=$(awk '/^Pss:/ {total += $2} END {print total}' /proc/$pid/smaps 2>/dev/null)
      uss=$(awk '/^Private/ {total += $2} END {print total}' /proc/$pid/smaps 2>/dev/null)
      dirty=$(awk '/^Dirty:/ {total += $2} END {print total}' /proc/$pid/smaps 2>/dev/null)
      
      # Calculate BSS (uninitialized data)
      private_clean=$(awk '/^Private_Clean:/ {total += $2} END {print total}' /proc/$pid/smaps 2>/dev/null)
      private_dirty=$(awk '/^Private_Dirty:/ {total += $2} END {print total}' /proc/$pid/smaps 2>/dev/null)
      [ -z "$private_clean" ] && private_clean=0
      [ -z "$private_dirty" ] && private_dirty=0
      
      # BSS approximation
      bss=$((VmData - private_clean - private_dirty))
      [ $bss -lt 0 ] && bss=0
      
      # Calculate stack size from smaps
      stack_size=$(awk '/\[stack\]/ && /^Size:/ {print $2}' /proc/$pid/smaps 2>/dev/null)
      [ -z "$stack_size" ] && stack_size=$VmStk
      
      # Calculate data segment size (initialized data)
      data=$((private_clean + private_dirty))
      
      # Default values if not found
      [ -z "$pss" ] && pss=0
      [ -z "$uss" ] && uss=0
      [ -z "$dirty" ] && dirty=0
      [ -z "$bss" ] && bss=0
      [ -z "$data" ] && data=0
      [ -z "$stack_size" ] && stack_size=0
      
      # Calculate shared memory
      shr=$((VmRSS - uss))
      [ $shr -lt 0 ] && shr=0
      
      # Skip processes with zero memory usage
      if [ "$pss" != "0" ]; then
        # Store results
        echo "$pid|$cmd|$VmSize|$VmRSS|$pss|$uss|$shr|$data|$bss|$stack_size|$dirty" >> "$TEMP_FILE"
      fi
    fi
  fi
done

# Check if the temp file exists and has content
if [ -s "$TEMP_FILE" ]; then
  # Sort results
  case $SORT_BY in
    rss) sort_col=4 ;;
    uss) sort_col=6 ;;
    vsz) sort_col=3 ;;
    data) sort_col=8 ;;
    bss) sort_col=9 ;;
    stack) sort_col=10 ;;
    *)   sort_col=5 ;; # Default to PSS
  esac

  # Print sorted results
  sort -t'|' -k${sort_col} -nr "$TEMP_FILE" | while IFS='|' read -r pid cmd vsz rss pss uss shr data bss stack dirty; do
    printf "%-8s %-20s %-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s\n" \
      "$pid" "$cmd" "$(convert_units $vsz)" "$(convert_units $rss)" \
      "$(convert_units $pss)" "$(convert_units $uss)" "$(convert_units $shr)" \
      "$(convert_units $data)" "$(convert_units $bss)" "$(convert_units $stack)" \
      "$(convert_units $dirty)"
  done

  # Calculate totals (always do this for the summary)
  totals=$(awk -F'|' '{vsz+=$3; rss+=$4; pss+=$5; uss+=$6; shr+=$7; data+=$8; bss+=$9; stack+=$10; dirty+=$11} 
           END {print vsz,rss,pss,uss,shr,data,bss,stack,dirty}' "$TEMP_FILE")
  
  # Read the totals
  set -- $totals
  total_vsz=$1
  total_rss=$2
  total_pss=$3
  total_uss=$4
  total_shr=$5
  total_data=$6
  total_bss=$7
  total_stack=$8
  total_dirty=$9

  # Show totals row if requested
  if [ $SHOW_TOTALS -eq 1 ]; then
    echo "----------------------------------------------------------------------------------------"
    printf "%-8s %-20s %-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s %-10s\n" \
      "TOTAL" "" "$(format_with_unit $total_vsz)" "$(format_with_unit $total_rss)" \
      "$(format_with_unit $total_pss)" "$(format_with_unit $total_uss)" "$(format_with_unit $total_shr)" \
      "$(format_with_unit $total_data)" "$(format_with_unit $total_bss)" "$(format_with_unit $total_stack)" \
      "$(format_with_unit $total_dirty)"
  fi

  # Add process count information
  process_count=$(wc -l < "$TEMP_FILE" 2>/dev/null || echo 0)
else
  echo "No processes with memory usage found."
  process_count=0
  # Set default values for totals if no processes found
  total_vsz=0
  total_rss=0
  total_pss=0
  total_uss=0
  total_shr=0
  total_data=0
  total_bss=0
  total_stack=0
  total_dirty=0
fi

# Show system memory information
echo ""
echo "System Memory Information:"
free -k | awk '{
  if (NR==1) {
    printf "%-15s %-10s %-10s %-10s %-10s %-10s %-10s\n", $1, $2, $3, $4, $5, $6, $7
  } else {
    printf "%-15s %-10d %-10d %-10d %-10d %-10d %-10d\n", $1, $2, $3, $4, $5, $6, $7
  }
}'

# Show memory usage summary
echo ""
echo "Memory Usage Summary (${UNIT}):"
echo "Total Physical Memory (PSS): $(format_with_unit $total_pss)"
echo "Total Resident Memory (RSS): $(format_with_unit $total_rss)"
echo "Total Private Memory (USS): $(format_with_unit $total_uss)"
echo "Total Shared Memory: $(format_with_unit $total_shr)"
echo "Total Data Segment: $(format_with_unit $total_data)"
echo "Total BSS Segment: $(format_with_unit $total_bss)"
echo "Total Stack Usage: $(format_with_unit $total_stack)"
echo "Total Dirty Memory: $(format_with_unit $total_dirty)"
echo ""
echo "Total processes analyzed: $process_count"

echo ""
echo "Memory Metrics Explanation:"
echo "VSZ   - Virtual memory size (all allocated memory, may not be in RAM)"
echo "RSS   - Resident Set Size (total memory in RAM, includes shared libraries)"
echo "PSS   - Proportional Set Size (private memory + fair share of shared memory)"
echo "USS   - Unique Set Size (private memory only, excludes shared libraries)"
echo "SHR   - Shared memory (memory shared with other processes)"
echo "DATA  - Initialized data segment size"
echo "BSS   - Uninitialized data segment size (Block Started by Symbol)"
echo "STACK - Stack size used by the process"
echo "DIRTY - Memory that has been modified and needs to be written to storage"
echo ""
echo "NOTE: PSS is the most accurate representation of real memory usage"

