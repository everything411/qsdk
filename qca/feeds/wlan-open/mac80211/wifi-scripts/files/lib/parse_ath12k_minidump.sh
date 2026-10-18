#!/bin/sh
#Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#All rights reserved.
#Confidential and Proprietary - Qualcomm Technologies, Inc.

# Script to collect minidump and parse structure
# Usage: ./collect_and_parse_minidump.sh <structure_name> [parser_args...] [-v] [--local] [--minidump-file <filename>]

# Color codes for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
ATH_DEBUG_BASE="/sys/kernel/debug/ath12k"
# Get serverip from fw_printenv if available, otherwise use default
if command -v fw_printenv >/dev/null 2>&1; then
    TFTP_SERVER_IP=$(fw_printenv serverip 2>/dev/null | cut -d'=' -f2)
fi
TFTP_SERVER_IP="${TFTP_SERVER_IP:-${serverip:-192.168.1.100}}"  # Use fw_printenv value, or serverip variable, or default
LOCAL_TMP="/tmp"
PARSER_EXEC="athstruct-parser"
COLLECTION_TIMEOUT=30  # Timeout for collection confirmation
TFTP_TIMEOUT=3  # Timeout for TFTP operations
TIMESTAMP_SEARCH_RANGE=5  # Search ±5 seconds around collection timestamp

# Function to print colored messages
print_info() {
    if [ "$VERBOSE_MODE" = "true" ]; then
        printf "${GREEN}[INFO]${NC} %s\n" "$1" >&2
    fi
}

print_error() {
    printf "${RED}[ERROR]${NC} %s\n" "$1" >&2
}

print_warning() {
    if [ "$VERBOSE_MODE" = "true" ]; then
        printf "${YELLOW}[WARNING]${NC} %s\n" "$1" >&2
    fi
}

print_debug() {
    if [ "$VERBOSE_MODE" = "true" ]; then
        printf "${BLUE}[DEBUG]${NC} %s\n" "$1" >&2
    fi
}

# Function to display usage
usage() {
    echo "Usage: $0 <structure_name> [parser_args...] [-v] [--local] [--minidump-file <filename>]"
    echo "  structure_name: Name of the structure to parse (e.g., ppe_drv_iface)"
    echo "  parser_args: Arguments to pass to athstruct-parser (e.g., verbose level)"
    echo "  -v: Enable verbose mode for this script (shows info, warning, debug logs)"
    echo "  --local: Skip minidump collection and use existing extracted dump"
    echo "  --minidump-file <filename>: Specify minidump filename to fetch from TFTP server"
    echo ""
    echo "Environment Variables:"
    echo "  serverip: TFTP server IP address (default: 192.168.1.100)"
    echo ""
    echo "Example: $0 ath12k_hw 1"
    echo "Example: $0 ath12k_hw 2 -v --local"
    echo "Example: serverip=192.168.1.50 $0 ppe_drv_iface 2 -v"
    echo "Example: $0 ath12k_hw 2 --minidump-file /tmp/minidump_1766470419.tar"
    exit 1
}

# Function to cleanup old minidump files
cleanup_old_minidumps() {
    print_info "Cleaning up old minidump files..."

    # Remove old extracted directories (timestamp directories)
    print_debug "Removing old extracted directories..."
    find "$LOCAL_TMP" -maxdepth 1 -type d -name '[0-9]*' -exec rm -rf {} \; 2>/dev/null

    # Remove old tar files
    print_debug "Removing old minidump archives..."
    rm -f "$LOCAL_TMP"/minidump_*.tar* 2>/dev/null

    # Remove old structure BIN files created by previous runs
    print_debug "Removing old structure files..."
    rm -f "$LOCAL_TMP"/ath12k_* 2>/dev/null
    rm -f "$LOCAL_TMP"/ppe_drv_* 2>/dev/null
    rm -f "$LOCAL_TMP"/linux_banner 2>/dev/null
    rm -f "$LOCAL_TMP"/mod_info 2>/dev/null
    rm -f "$LOCAL_TMP"/mmu_info 2>/dev/null

    print_debug "Cleanup completed"
}

# Function to fetch minidump based on collection timestamp
fetch_minidump_by_timestamp() {
    local server_ip="$1"
    local collection_timestamp="$2"

    print_info "Searching for minidump around collection timestamp: $collection_timestamp"
    print_debug "Will search from $((collection_timestamp - TIMESTAMP_SEARCH_RANGE)) to $((collection_timestamp + TIMESTAMP_SEARCH_RANGE))"

    cd "$LOCAL_TMP" || return 1

    local LATEST_FILE=""
    local LATEST_TIMESTAMP=0
    local found_count=0

    # Search ±5 seconds around the collection timestamp
    local offset=-$TIMESTAMP_SEARCH_RANGE
    while [ $offset -le $TIMESTAMP_SEARCH_RANGE ]; do
        local TIMESTAMP=$((collection_timestamp + offset))

        #print_debug "Trying timestamp: $TIMESTAMP (offset: ${offset}s)"

        # Try .tar extension
        local MINIDUMP_FILE="minidump_${TIMESTAMP}.tar"
        if timeout $TFTP_TIMEOUT tftp -g -r "$MINIDUMP_FILE" "$server_ip" 2>/dev/null; then
            if [ -f "$MINIDUMP_FILE" ]; then
                print_info "Found: $MINIDUMP_FILE"
                found_count=$((found_count + 1))
                if [ $TIMESTAMP -gt $LATEST_TIMESTAMP ]; then
                    LATEST_TIMESTAMP=$TIMESTAMP
                    LATEST_FILE="$MINIDUMP_FILE"
                fi
            fi
        fi

        # Try .tar.gz extension
        MINIDUMP_FILE="minidump_${TIMESTAMP}.tar.gz"
        if timeout $TFTP_TIMEOUT tftp -g -r "$MINIDUMP_FILE" "$server_ip" 2>/dev/null; then
            if [ -f "$MINIDUMP_FILE" ]; then
                print_info "Found: $MINIDUMP_FILE"
                found_count=$((found_count + 1))
                if [ $TIMESTAMP -gt $LATEST_TIMESTAMP ]; then
                    LATEST_TIMESTAMP=$TIMESTAMP
                    LATEST_FILE="$MINIDUMP_FILE"
                fi
            fi
        fi

        offset=$((offset + 1))
    done

    print_info "Search complete. Found $found_count files in ±${TIMESTAMP_SEARCH_RANGE}s range"

    if [ -n "$LATEST_FILE" ]; then
        print_info "Selected minidump: $LATEST_FILE (timestamp: $LATEST_TIMESTAMP)"
        echo "$LATEST_FILE"
        return 0
    else
        print_warning "No minidump files found in timestamp range"
        return 1
    fi
}

# Function to fetch latest minidump (fallback method)
fetch_latest_minidump() {
    local server_ip="$1"

    print_info "Using fallback method: searching recent minidumps..."

    cd "$LOCAL_TMP" || return 1

    local LATEST_FILE=""
    local LATEST_TIMESTAMP=0
    local CURRENT_TIME=$(date +%s)

    print_info "Searching last 48 hours (every 10 minutes)..."

    local minutes_ago=0
    local found_count=0
    local max_minutes=$((48 * 60))

    while [ $minutes_ago -le $max_minutes ]; do
        local TIMESTAMP=$((CURRENT_TIME - minutes_ago * 60))

        local MINIDUMP_FILE="minidump_${TIMESTAMP}.tar"
        if timeout $TFTP_TIMEOUT tftp -g -r "$MINIDUMP_FILE" "$server_ip" 2>/dev/null; then
            if [ -f "$MINIDUMP_FILE" ]; then
                print_info "Found: $MINIDUMP_FILE"
                found_count=$((found_count + 1))
                if [ $TIMESTAMP -gt $LATEST_TIMESTAMP ]; then
                    LATEST_TIMESTAMP=$TIMESTAMP
                    LATEST_FILE="$MINIDUMP_FILE"
                fi
            fi
        fi

        MINIDUMP_FILE="minidump_${TIMESTAMP}.tar.gz"
        if timeout $TFTP_TIMEOUT tftp -g -r "$MINIDUMP_FILE" "$server_ip" 2>/dev/null; then
            if [ -f "$MINIDUMP_FILE" ]; then
                print_info "Found: $MINIDUMP_FILE"
                found_count=$((found_count + 1))
                if [ $TIMESTAMP -gt $LATEST_TIMESTAMP ]; then
                    LATEST_TIMESTAMP=$TIMESTAMP
                    LATEST_FILE="$MINIDUMP_FILE"
                fi
            fi
        fi

        minutes_ago=$((minutes_ago + 10))

        if [ $((minutes_ago % 360)) -eq 0 ] && [ $minutes_ago -gt 0 ]; then
            local hours_checked=$((minutes_ago / 60))
            print_info "Checked last $hours_checked hours... (found $found_count files so far)"
        fi
    done

    if [ -n "$LATEST_FILE" ]; then
        print_info "Latest minidump selected: $LATEST_FILE (timestamp: $LATEST_TIMESTAMP)"
        echo "$LATEST_FILE"
        return 0
    else
        print_warning "No minidump files found"
        return 1
    fi
}

# Parse arguments
if [ $# -lt 1 ]; then
    print_error "Invalid number of arguments"
    usage
fi

STRUCT_NAME="$1"
VERBOSE_MODE="false"  # Default to false (errors only)
SKIP_COLLECTION=false
MINIDUMP_FILE=""
PARSER_ARGS=""  # Arguments to pass to parser

shift 1

while [ $# -gt 0 ]; do
    case "$1" in
        -v)
            VERBOSE_MODE="true"
            shift
            ;;
        --local)
            SKIP_COLLECTION=true
            shift
            ;;
        --minidump-file)
            if [ -z "$2" ]; then
                print_error "--minidump-file requires a filename argument"
                usage
            fi
            MINIDUMP_FILE="$2"
            shift 2
            ;;
        *)
            # Collect all other arguments for parser
            PARSER_ARGS="$PARSER_ARGS $1"
            shift
            ;;
    esac
done

print_info "Starting minidump processing for structure: $STRUCT_NAME"
print_debug "Parser arguments:$PARSER_ARGS"
print_info "TFTP Server IP: $TFTP_SERVER_IP"

# Variables for collection tracking
COLLECTION_TIMESTAMP=""
COLLECTION_SUCCESS=false
MINIDUMP_DIR=""

# Step 1: Check if --local is set and use existing extracted dump
if [ "$SKIP_COLLECTION" = true ]; then
    print_info "Skip-collection flag set, looking for existing extracted minidump..."

    # Find existing extracted minidump directory (most recent)
    EXISTING_DIR=$(find "$LOCAL_TMP" -maxdepth 1 -type d -name '[0-9]*' 2>/dev/null | sort -r | head -1)

    if [ -n "$EXISTING_DIR" ] && [ -f "$EXISTING_DIR/MOD_INFO.txt" ]; then
        print_info "Found existing extracted minidump: $EXISTING_DIR"
        MINIDUMP_DIR="$EXISTING_DIR"
        print_info "Using existing minidump, skipping collection and fetch steps"
    else
        print_warning "No existing extracted minidump found in /tmp"
        print_info "Will proceed with normal fetch process..."
    fi
else
    # Cleanup old minidump files before new collection
    print_info "Cleaning up old minidump files before new collection..."
    cleanup_old_minidumps
fi

# Step 2: Trigger minidump collection (if not skipped and no existing dump found)
if [ "$SKIP_COLLECTION" = false ] && [ -z "$MINIDUMP_DIR" ]; then
    print_info "Triggering minidump collection via ath_debug..."
    # Discover the first available ahb-* or pci-* interface under ATH_DEBUG_BASE
    ATH_COLLECT_PATH=""
    for iface_dir in "${ATH_DEBUG_BASE}"/ahb-* "${ATH_DEBUG_BASE}"/pci-*; do
        collect_path="${iface_dir}/ath_debug/minidump/collect"
        if [ -f "$collect_path" ]; then
            ATH_COLLECT_PATH="$collect_path"
            break
        fi
    done

    if [ -n "$ATH_COLLECT_PATH" ]; then
        print_info "Monitoring console output for collection message..."

        # Capture the timestamp before triggering
        TRIGGER_TIME=$(date +%s)
        print_debug "Trigger time: $TRIGGER_TIME"

        # Trigger collection on the first discovered interface
        print_info "Triggering minidump collection: $ATH_COLLECT_PATH"
        echo 1 > "$ATH_COLLECT_PATH" 2>&1 || {
            print_error "Failed to trigger minidump collection on $ATH_COLLECT_PATH. Check permissions."
            exit 1
        }

        # Wait for the collection process to complete
        print_info "Waiting for collection to complete..."
        sleep 3

        print_info "Collection triggered successfully"
        COLLECTION_SUCCESS=true
        COLLECTION_TIMESTAMP=$TRIGGER_TIME
        print_info "Using trigger timestamp for search: $COLLECTION_TIMESTAMP"
        print_debug "Will search for minidump files around this timestamp (±${TIMESTAMP_SEARCH_RANGE}s)"
    else
        print_warning "No ath12k interfaces (ahb-* or pci-*) found under: $ATH_DEBUG_BASE"
        print_warning "Skipping minidump collection trigger"
    fi
fi

# Step 3: Fetch minidump from TFTP server (if not using existing extracted dump)
if [ -z "$MINIDUMP_DIR" ]; then
    print_info "Fetching minidump from TFTP server..."

    cd "$LOCAL_TMP"

    FOUND_MINIDUMP=false
    MINIDUMP_BASENAME=""

    if [ -n "$MINIDUMP_FILE" ]; then
        # User specified a minidump file
        #print_info "Attempting to fetch specified file: $MINIDUMP_FILE"

        if timeout 10 tftp -g -r "$MINIDUMP_FILE" "$TFTP_SERVER_IP" 2>/dev/null; then
            if [ -f "$MINIDUMP_FILE" ]; then
                print_info "Successfully fetched: $MINIDUMP_FILE"
                MINIDUMP_BASENAME="$MINIDUMP_FILE"
                FOUND_MINIDUMP=true
            fi
        else
            print_error "Failed to fetch $MINIDUMP_FILE from TFTP server $TFTP_SERVER_IP"
            exit 1
        fi
    else
        # Automatically find and fetch the minidump
        if [ "$COLLECTION_SUCCESS" = true ] && [ -n "$COLLECTION_TIMESTAMP" ]; then
            # Use timestamp-based search (more accurate)
            print_info "Using timestamp-based search (collection time: $COLLECTION_TIMESTAMP)"

            LATEST=$(fetch_minidump_by_timestamp "$TFTP_SERVER_IP" "$COLLECTION_TIMESTAMP")
            FETCH_RESULT=$?

            if [ $FETCH_RESULT -eq 0 ] && [ -n "$LATEST" ] && [ -f "$LATEST" ]; then
                print_info "Using fetched minidump: $LATEST"
                MINIDUMP_BASENAME="$LATEST"
                FOUND_MINIDUMP=true
            else
                print_warning "Timestamp-based search failed, trying fallback method..."
            fi
        fi

        # Fallback: search recent minidumps
        if [ "$FOUND_MINIDUMP" = false ]; then
            print_info "Automatically searching for latest minidump..."

            LATEST=$(fetch_latest_minidump "$TFTP_SERVER_IP")
            FETCH_RESULT=$?

            if [ $FETCH_RESULT -eq 0 ] && [ -n "$LATEST" ] && [ -f "$LATEST" ]; then
                print_info "Using fetched minidump: $LATEST"
                MINIDUMP_BASENAME="$LATEST"
                FOUND_MINIDUMP=true
            else
                print_error "Could not fetch minidump from TFTP server"
                exit 1
            fi
        fi
    fi

    MINIDUMP_LOCAL="$LOCAL_TMP/$MINIDUMP_BASENAME"
    print_info "Using minidump: $MINIDUMP_LOCAL"

    # Step 4: Extract the tar file
    print_info "Extracting minidump archive..."
    print_debug "Archive: $MINIDUMP_BASENAME"
    print_debug "Current directory: $(pwd)"

    # Extract based on file extension
    case "$MINIDUMP_BASENAME" in
        *.tar.gz)
            print_debug "Extracting tar.gz file..."
            tar -xzf "$MINIDUMP_BASENAME" 2>&1 || {
                print_error "Failed to extract tar.gz file"
                exit 1
            }
            ;;
        *.tar)
            print_debug "Extracting tar file..."
            tar -xf "$MINIDUMP_BASENAME" 2>&1 || {
                print_error "Failed to extract tar file"
                exit 1
            }
            ;;
        *)
            print_error "Unknown archive format: $MINIDUMP_BASENAME"
            exit 1
            ;;
    esac

    print_debug "Extraction completed"

    # Remove the tar file after successful extraction
    print_info "Removing archive file: $MINIDUMP_BASENAME"
    rm -f "$MINIDUMP_BASENAME"

    # Find the extracted directory - try multiple methods
    print_debug "Searching for extracted directory..."

    # Method 1: Using find with -printf (GNU find)
    MINIDUMP_DIR=$(find "$LOCAL_TMP" -maxdepth 2 -name "MOD_INFO.txt" -type f -printf '%h\n' 2>/dev/null | head -1)
    print_debug "Method 1 result: '$MINIDUMP_DIR'"

    # Method 2: Using find without -printf (BusyBox compatible)
    if [ -z "$MINIDUMP_DIR" ]; then
        print_debug "Trying BusyBox compatible method..."
        MOD_INFO_PATH=$(find "$LOCAL_TMP" -maxdepth 2 -name "MOD_INFO.txt" -type f 2>/dev/null | head -1)
        if [ -n "$MOD_INFO_PATH" ]; then
            MINIDUMP_DIR=$(dirname "$MOD_INFO_PATH")
            print_debug "Method 2 result: '$MINIDUMP_DIR'"
        fi
    fi

    # Method 3: Extract timestamp from filename
    if [ -z "$MINIDUMP_DIR" ]; then
        print_debug "Trying timestamp extraction method..."
        TIMESTAMP=$(echo "$MINIDUMP_BASENAME" | sed -n 's/minidump_\([0-9]*\).*/\1/p')
        print_debug "Extracted timestamp: '$TIMESTAMP'"

        if [ -n "$TIMESTAMP" ] && [ -d "$LOCAL_TMP/$TIMESTAMP" ]; then
            MINIDUMP_DIR="$LOCAL_TMP/$TIMESTAMP"
            print_debug "Method 3 result: '$MINIDUMP_DIR'"
        fi
    fi

    if [ -z "$MINIDUMP_DIR" ]; then
        print_error "Could not find extracted minidump directory with MOD_INFO.txt"
        print_info "Listing directories in /tmp:"
        ls -ld "$LOCAL_TMP"/[0-9]* 2>/dev/null >&2
        exit 1
    fi

    print_info "Minidump extracted to: $MINIDUMP_DIR"

    # Verify MOD_INFO.txt exists
    if [ ! -f "$MINIDUMP_DIR/MOD_INFO.txt" ]; then
        print_error "MOD_INFO.txt not found in $MINIDUMP_DIR"
        print_info "Directory contents:"
        ls -la "$MINIDUMP_DIR" 2>/dev/null | head -10 >&2
        exit 1
    fi
fi
# Smart cleanup logic for structure binaries
# This section goes after argument parsing and before Step 5

# Parse structure name to check if it has an instance number
ORIGINAL_STRUCT_NAME="$STRUCT_NAME"
REQUESTED_INSTANCE=""

# Check if structure name ends with a number (e.g., ath12k_ce_stats3)
if echo "$STRUCT_NAME" | grep -qE '[0-9]+$'; then
    REQUESTED_INSTANCE=$(echo "$STRUCT_NAME" | sed 's/.*[^0-9]\([0-9]\+\)$/\1/')
    STRUCT_NAME=$(echo "$STRUCT_NAME" | sed 's/[0-9]\+$//')
    print_info "Detected instance-based request: structure='$STRUCT_NAME', instance=$REQUESTED_INSTANCE"
fi

# Smart cleanup of old structure binaries
print_debug "Performing smart cleanup of old structure binaries..."

# If this is a completely different structure (not instance-based request)
if [ -z "$REQUESTED_INSTANCE" ]; then
    # Remove all structure BIN files except the current one being processed
    print_debug "Full structure request - cleaning up old structure binaries"

    # Get list of all structure BIN files in /tmp
    find "$LOCAL_TMP" -maxdepth 1 -name "*.BIN" -type f 2>/dev/null | while read bin_file; do
        if [ -f "$bin_file" ]; then
            bin_basename=$(basename "$bin_file" .BIN)

            # Check if this file belongs to a different structure
            # Extract base structure name (remove trailing numbers if any)
            bin_struct_base=$(echo "$bin_basename" | sed 's/[0-9]\+$//')

            # If the base structure name is different from current structure, remove it
            if [ "$bin_struct_base" != "$STRUCT_NAME" ]; then
                print_debug "Removing old structure binary: $bin_file"
                rm -f "$bin_file"
            fi
        fi
    done
else
    # Instance-based request - only remove files from completely different structures
    print_debug "Instance-based request - preserving other instances of same structure"

    find "$LOCAL_TMP" -maxdepth 1 -name "*.BIN" -type f 2>/dev/null | while read bin_file; do
        if [ -f "$bin_file" ]; then
            bin_basename=$(basename "$bin_file" .BIN)

            # Extract base structure name (remove trailing numbers if any)
            bin_struct_base=$(echo "$bin_basename" | sed 's/[0-9]\+$//')

            # Only remove if it's a completely different structure
            if [ "$bin_struct_base" != "$STRUCT_NAME" ]; then
                print_debug "Removing different structure binary: $bin_file"
                rm -f "$bin_file"
            fi
        fi
    done
fi

print_debug "Cleanup completed"

# Step 5: Map structure name to VA address from MOD_INFO.txt
print_info "Searching for structure '$STRUCT_NAME' in MOD_INFO.txt..."
MOD_INFO_FILE="$MINIDUMP_DIR/MOD_INFO.txt"

if [ ! -f "$MOD_INFO_FILE" ]; then
    print_error "MOD_INFO.txt not found in $MINIDUMP_DIR"
    exit 1
fi

# Find all VA addresses for the structure (handle duplicates)
VA_ADDRESSES=$(strings "$MOD_INFO_FILE" | grep "^${STRUCT_NAME} va=" | sed -n 's/.*va=\([0-9a-fA-F]*\).*/\1/p')

if [ -z "$VA_ADDRESSES" ]; then
    print_error "Structure '$STRUCT_NAME' not found in MOD_INFO.txt"
    print_info "Available structures:"
    strings "$MOD_INFO_FILE" | grep " va=" | sed 's/ va=.*//' | head -20
    exit 1
fi

# Count how many instances found
INSTANCE_COUNT=$(echo "$VA_ADDRESSES" | wc -l)

# Print count based on verbose mode
if [ "$VERBOSE_MODE" = "true" ]; then
    print_info "Found $INSTANCE_COUNT instance(s) of structure '$STRUCT_NAME'"
else
    printf "Found %d instance(s) of structure '%s'\n" "$INSTANCE_COUNT" "$STRUCT_NAME" >&2
fi

# If specific instance requested, validate it
if [ -n "$REQUESTED_INSTANCE" ]; then
    if [ "$REQUESTED_INSTANCE" -gt "$INSTANCE_COUNT" ] || [ "$REQUESTED_INSTANCE" -lt 1 ]; then
        print_error "Requested instance $REQUESTED_INSTANCE is out of range (1-$INSTANCE_COUNT)"
        exit 1
    fi
    print_info "Processing only instance $REQUESTED_INSTANCE"
fi
# Step 6 & 7: Process each VA address and create numbered binary files
MMU_INFO_FILE="$MINIDUMP_DIR/MMU_INFO.txt"

if [ ! -f "$MMU_INFO_FILE" ]; then
    print_error "MMU_INFO.txt not found in $MINIDUMP_DIR"
    exit 1
fi

INSTANCE_NUM=1
SUCCESS_COUNT=0

# Convert VA_ADDRESSES to a temporary file to avoid subshell issues
TMP_VA_FILE="$LOCAL_TMP/.va_addresses_$$"
echo "$VA_ADDRESSES" > "$TMP_VA_FILE"

while IFS= read -r VA_ADDRESS; do
    if [ -z "$VA_ADDRESS" ]; then
        continue
    fi

    # Skip if specific instance requested and this is not it
    if [ -n "$REQUESTED_INSTANCE" ] && [ "$INSTANCE_NUM" -ne "$REQUESTED_INSTANCE" ]; then
        INSTANCE_NUM=$((INSTANCE_NUM + 1))
        continue
    fi

    print_info "Processing instance $INSTANCE_NUM: VA address $VA_ADDRESS"

    # Map VA address to PA address using MMU_INFO.txt
    PA_ADDRESS=$(strings "$MMU_INFO_FILE" | grep "^va=${VA_ADDRESS} " | head -1 | sed -n 's/.*pa=\([0-9a-fA-F]*\).*/\1/p')

    if [ -z "$PA_ADDRESS" ]; then
        print_warning "PA address not found for VA: $VA_ADDRESS (instance $INSTANCE_NUM)"
        INSTANCE_NUM=$((INSTANCE_NUM + 1))
        continue
    fi

    print_info "Found PA address: $PA_ADDRESS"

    # Find and copy the PA address file
    PA_FILE="$MINIDUMP_DIR/${PA_ADDRESS}.BIN"

    if [ ! -f "$PA_FILE" ]; then
        print_warning "PA address file not found: $PA_FILE (instance $INSTANCE_NUM)"
        INSTANCE_NUM=$((INSTANCE_NUM + 1))
        continue
    fi

    # Create numbered filename for multiple instances
    if [ $INSTANCE_COUNT -gt 1 ]; then
        STRUCT_FILE="$LOCAL_TMP/${STRUCT_NAME}${INSTANCE_NUM}"
    else
        STRUCT_FILE="$LOCAL_TMP/$STRUCT_NAME"
    fi

    print_info "Copying $PA_FILE to ${STRUCT_FILE}.BIN..."
    cp "$PA_FILE" "${STRUCT_FILE}.BIN" || {
        print_warning "Failed to copy PA file for instance $INSTANCE_NUM"
        INSTANCE_NUM=$((INSTANCE_NUM + 1))
        continue
    }

    print_info "Created: ${STRUCT_FILE}.BIN"
    SUCCESS_COUNT=$((SUCCESS_COUNT + 1))

    INSTANCE_NUM=$((INSTANCE_NUM + 1))

    # If specific instance requested, we're done after processing it
    if [ -n "$REQUESTED_INSTANCE" ]; then
        break
    fi
done < "$TMP_VA_FILE"

# Clean up temporary file
rm -f "$TMP_VA_FILE"

# Summary of created files
if [ -n "$REQUESTED_INSTANCE" ]; then
    # Single instance requested
    if [ $INSTANCE_COUNT -gt 1 ]; then
        STRUCT_FILE="$LOCAL_TMP/${STRUCT_NAME}${REQUESTED_INSTANCE}.BIN"
    else
        STRUCT_FILE="$LOCAL_TMP/${STRUCT_NAME}.BIN"
    fi
    if [ -f "$STRUCT_FILE" ]; then
        printf "Created: %s\n" "$STRUCT_FILE" >&2
    fi
elif [ $INSTANCE_COUNT -gt 1 ]; then
    # Multiple instances - only show count in non-verbose mode
    if [ "$VERBOSE_MODE" = "true" ]; then
        printf "\n${GREEN}[SUMMARY]${NC} Created binary files for structure '%s':\n" "$STRUCT_NAME" >&2
        INSTANCE_NUM=1
        while [ $INSTANCE_NUM -le $INSTANCE_COUNT ]; do
            STRUCT_FILE="$LOCAL_TMP/${STRUCT_NAME}${INSTANCE_NUM}.BIN"
            if [ -f "$STRUCT_FILE" ]; then
                printf "  %d. %s\n" "$INSTANCE_NUM" "$STRUCT_FILE" >&2
            fi
            INSTANCE_NUM=$((INSTANCE_NUM + 1))
        done
        printf "\n" >&2
    else
        # Count actual created files
        CREATED_COUNT=$(ls -1 "$LOCAL_TMP/${STRUCT_NAME}"*.BIN 2>/dev/null | wc -l)
        printf "Created %d binary file(s) in %s\n" "$CREATED_COUNT" "$LOCAL_TMP" >&2
    fi
else
    STRUCT_FILE="$LOCAL_TMP/$STRUCT_NAME"
    if [ "$VERBOSE_MODE" = "true" ]; then
        print_info "Structure data file created: ${STRUCT_FILE}.BIN"
    else
        printf "Created: %s.BIN\n" "$STRUCT_FILE" >&2
    fi
fi

# Step 8: Execute the parser
print_info "Executing parser: $PARSER_EXEC $STRUCT_NAME$PARSER_ARGS"

cd - > /dev/null

# Check if parser is available
if ! command -v "$PARSER_EXEC" >/dev/null 2>&1; then
    print_error "Parser executable not found: $PARSER_EXEC"
    print_info "Please ensure athstruct-parser is in PATH or current directory"
    print_debug "Tried: command -v $PARSER_EXEC"
    exit 1
fi

$PARSER_EXEC "$STRUCT_NAME" $PARSER_ARGS || {
    print_error "Parser execution failed"
    exit 1
}

print_info "Parsing completed successfully!"
print_info "Structure file location: $STRUCT_FILE"
