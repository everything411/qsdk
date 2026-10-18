#
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: ISC
#

import os
import sys
import struct
from ramdump import RamDump
from print_out import print_out_str


def read_binary_file(file_path, start_offset=0, count=None):
    stream_low = []
    stream_high = []
    with open(file_path, 'rb') as file:
        file.seek(start_offset)  # Move to the start offset
        index = 0
        while True:
            if count is not None and index >= count:
                break
            bytes_read = file.read(4)
            if len(bytes_read) < 4:
                break
            value = struct.unpack('<I', bytes_read)[0]
            if index % 2 == 0:
                stream_low.append(value)
            else:
                stream_high.append(value)
            index += 1
    return stream_low, stream_high

def extract_gemnoc_poc_dbg_LLCC(file_path, start_offset=0, count=None):
    # Print Stream_low and Stream_high arrays for the first 3 iterations in hexadecimal format
    stream_low, stream_high = read_binary_file(file_path, start_offset, count)

    print_out_str("\n------------GEMNOC LLCC TMO--------------")
    print_out_str(f"Stream_low (first 3 iterations): {[f'0x{val:08x}' for val in stream_low[:3]]}")
    print_out_str(f"Stream_high (first 3 iterations): {[f'0x{val:08x}' for val in stream_high[:3]]}")

    # Calculate Initiator_Type
    initiator_type = (stream_low[0] & 0x100) >> 8
    if initiator_type:
        ext_id = (stream_high[2] >> 9) & 0xFFFF
        bid = (ext_id >> 13) & 0x7
        pid = (ext_id >> 8) & 0x1F
        mid = ext_id & 0xFF
        print_out_str(f"GEMNOC LLCC TMO: ERROR Decode : BID = 0x{bid:x}, PID = 0x{pid:x}, MID = 0x{mid:x}")
    else:
        tgtid = (stream_low[2] & 0xE000000) >> 25
        srcid = (stream_low[2] & 0x70000000) >> 28
        lpid = ((stream_high[2] & 0xE0000000) >> 29) + ((stream_low[3] & 0x3) << 3)
        print_out_str(f"GEMNOC LLCC TMO: ERROR Decode : TGTID = 0x{tgtid:x}, SRCID = 0x{srcid:x}, LPID = 0x{lpid:x}")

    addr_of_violation = ((stream_low[0] + (stream_high[0] << 32)) >> 54) + \
                        ((((stream_low[1] + (stream_high[1] << 32)) & 0x3FFFFFF)) << 10)
    print_out_str(f"GEMNOC LLCC TMO: Address of violation = 0x{addr_of_violation:016x}")

def extract_gemnoc_poc_dbg_PCIE(file_path, start_offset=0, count=None):
    stream_low, stream_high = read_binary_file(file_path, start_offset, count)

    print_out_str("\n------------GEMNOC PCIE TMO--------------")
    # Print Stream_low and Stream_high arrays for the first 3 iterations in hexadecimal format
    print_out_str(f"Stream_low (first 3 iterations): {[f'0x{val:08x}' for val in stream_low[:3]]}")
    print_out_str(f"Stream_high (first 3 iterations): {[f'0x{val:08x}' for val in stream_high[:3]]}")

    # Calculate Initiator_Type
    initiator_type = (stream_low[0] & 0x40) >> 6
    if initiator_type:
        ext_id = ((stream_low[2] & 0x80000000) >> 31) + ((stream_high[2] & 0x7FFF) << 1)
        bid = (ext_id >> 13) & 0x7
        pid = (ext_id >> 8) & 0x1F
        mid = ext_id & 0xFF
        print_out_str(f"GEMNOC QNS PCIE TMO: ERROR Decode : BID = 0x{bid:x}, PID = 0x{pid:x}, MID = 0x{mid:x}")
    else:
        tgtid = (stream_low[2] & 0x38000) >> 15
        srcid = (stream_low[2] & 0x1C0000) >> 18
        lpid = (stream_high[2] & 0xF80000) >> 19
        print_out_str(f"GEMNOC QNS PCIE TMO: ERROR Decode : TGTID = 0x{tgtid:x}, SRCID = 0x{srcid:x}, LPID = 0x{lpid:x}")

    addr_of_violation = ((stream_low[0] + (stream_high[0] << 32)) >> 46) + \
                        ((((stream_low[1] + (stream_high[1] << 32)) & 0x3FFFF)) << 18)
    print_out_str(f"GEMNOC QNS PCIE TMO: Address of violation = 0x{addr_of_violation:016x}")

def extract_gemnoc_poc_dbg_PCNOC(file_path, start_offset=0, count=None):
    stream_low, stream_high = read_binary_file(file_path, start_offset, count)

    print_out_str("\n------------GEMNOC PCNOC TMO--------------")
    # Print Stream_low and Stream_high arrays for the first 3 iterations in hexadecimal format
    print_out_str(f"Stream_low (first 3 iterations): {[f'0x{val:08x}' for val in stream_low[:3]]}")
    print_out_str(f"Stream_high (first 3 iterations): {[f'0x{val:08x}' for val in stream_high[:3]]}")

    # Calculate Initiator_Type
    initiator_type = (stream_low[0] & 0x40) >> 6

    if initiator_type:
        ext_id = (stream_high[2] >> 7) & 0xFFFF
        bid = (ext_id >> 13) & 0x7
        pid = (ext_id >> 8) & 0x1F
        mid = ext_id & 0xFF

        print_out_str(f"GEMNOC QNS PCNOC TMO: ERROR Decode : BID = 0x{bid:x}, PID = 0x{pid:x}, MID = 0x{mid:x}")
    else:
        tgtid = (stream_low[2] & 0x3800000) >> 23
        srcid = (stream_low[2] & 0x1C000000) >> 26
        lpid = (stream_high[2] & 0xF80000000) >> 27

        print_out_str(f"GEMNOC QNS PCNOC TMO: ERROR Decode : TGTID = 0x{tgtid:x}, SRCID = 0x{srcid:x}, LPID = 0x{lpid:x}")

    addr_of_violation = ((stream_low[0] + (stream_high[0] << 32)) >> 52) + \
                        ((((stream_low[1] + (stream_high[1] << 32)) & 0xFFFFFF)) << 12)

    print_out_str(f"GEMNOC QNS PCNOC TMO: Address of violation = 0x{addr_of_violation:016x}")

def get_dcc_dump_addr(ddr_address):
    return (ddr_address & 0xFFFF0000) | 0x09F8
def get_mon_mmu_addr(ddr_address):
    return (ddr_address & 0xFFFF0000) | 0x0A28

# SYDB dump_data_type table entry layout:
#   version(4) @0x00, magic(4) @0x04, name[32] @0x08,
#   start_addr @0x28, len @0x30. Each entry is SYDB_ENTRY_SIZE bytes.
# The number of CPU entries preceding the dcc_sram entry varies per SoC
# (e.g. 4 cores on 5424/Marina vs 5 cores on 96xx/Juhu), so the dcc_sram
# entry is located by NAME rather than a fixed offset from the table base.
SYDB_MAGIC = 0x42445953          # 'SYDB'
SYDB_VALID_VERSIONS = (0x10, 0x14)
SYDB_ENTRY_SIZE = 0x38
SYDB_NAME_OFFSET = 0x8
SYDB_NAME_SIZE = 0x20
SYDB_START_OFFSET = 0x28
SYDB_LEN_OFFSET = 0x30
SYDB_MAX_ENTRIES = 64

def find_sydb_entry(ramdump, table_ptr, entry_name):
    """Walk the SYDB dump_data_type table and return (start_addr, length)
    for the entry whose name matches entry_name, or None if not found.

    Iterates fixed-size (SYDB_ENTRY_SIZE) entries starting at table_ptr,
    validating version/magic on each entry and matching the name field.
    This is core-count independent: the dcc_sram / mon_pt entry is located
    by name regardless of how many CPU entries precede it.
    """
    matched_any = False
    for i in range(SYDB_MAX_ENTRIES):
        entry = table_ptr + i * SYDB_ENTRY_SIZE
        version = ramdump.read_u32(entry, virtual=False)
        magic = ramdump.read_u32(entry + 4, virtual=False)
        if magic != SYDB_MAGIC or version not in SYDB_VALID_VERSIONS:
            if matched_any:
                break
            continue
        matched_any = True
        name_bytes = ramdump.read_physical(
            entry + SYDB_NAME_OFFSET, SYDB_NAME_SIZE, False)
        if name_bytes is None:
            break
        name = name_bytes.split(b'\x00')[0]
        if name == entry_name:
            start_addr = ramdump.read_u32(entry + SYDB_START_OFFSET, virtual=False)
            length = ramdump.read_u32(entry + SYDB_LEN_OFFSET, virtual=False)
            return (start_addr, length)
    return None

# dcc_sram_config structure
dcc_sram_config = {
    5424: {
        "dcc_sram_size": 0x8000,
        "dcc_dump_addr": get_dcc_dump_addr,
        "dumps": [
            {
                "start_offset": 0x554,
                "size_in_bytes": 0x328,
                "function_pointer": extract_gemnoc_poc_dbg_LLCC
            },
            {
                "start_offset": 0x87C,
                "size_in_bytes": 0x328,
                "function_pointer": extract_gemnoc_poc_dbg_PCIE
            },
            {
                "start_offset": 0xBA4,
                "size_in_bytes": 0x328,
                "function_pointer": extract_gemnoc_poc_dbg_PCNOC
            },
        ]
    },

    9650: {
        "dcc_sram_size": 0x8000,
        "dcc_dump_addr": get_dcc_dump_addr,
        "dumps": [
            {
                "start_offset": 0x6B4,
                "size_in_bytes": 0x328,
                "function_pointer": extract_gemnoc_poc_dbg_LLCC
            },
            {
                "start_offset": 0x9E0,
                "size_in_bytes": 0x328,
                "function_pointer": extract_gemnoc_poc_dbg_PCNOC
            },
            {
                "start_offset": 0xD08,
                "size_in_bytes": 0x328,
                "function_pointer": extract_gemnoc_poc_dbg_PCIE
            },
        ]
    },
}

def generate_dcc_sram(ramdump, start_offset, num_elements, output_file_path):
    data = ramdump.read_physical(start_offset, num_elements, False)
    with open(output_file_path, 'wb') as output_file:
        output_file.write(data)

def dcc_sram_parser_func(ramdump):

    if ramdump.hw_id is None:
        print("!!!!! No HW ID !!!!!")
        return
    print_out_str(f"Board : ipq-{ramdump.hw_id}")

    if ramdump.hw_id not in dcc_sram_config:
        print_out_str("!!!!! No DCC SRAM CFG for this board !!!!!")
        return

    imem_location = ramdump.tz_addr
    if imem_location == 0:
        print_out_str("Wrong DDR Location: 0x0")
        return
    # print_out_str(f"DDR Location: 0x{imem_location:08X}")

    ddr_address = ramdump.read_word(imem_location, False)
    if ddr_address == 0:
        print_out_str("Wrong DDR Address: 0x0")
        return
    # print_out_str(f"DDR Address: 0x{ddr_address:08X}")

    dump_addr = dcc_sram_config[ramdump.hw_id]["dcc_dump_addr"](ddr_address)
    if dump_addr == 0:
        print_out_str("Wrong Dump Address: 0x0")
        return
    # print_out_str(f" Calculated address: 0x{dump_addr:08X}")

    table_ptr = ramdump.read_u32(dump_addr, virtual=False)
    if table_ptr == 0:
        print_out_str("Wrong DCC SRAM Struct Info: 0x0")
        return
    # print_out_str(f" SysDbg table base: 0x{table_ptr:08X}")

    # Locate the dcc_sram entry by name. The number of CPU entries before it
    # varies per SoC (4 cores on 5424 vs 5 cores on 96xx), so a fixed offset
    # from the table base is incorrect; search the SYDB table by name instead.
    dcc_region = find_sydb_entry(ramdump, table_ptr, b'dcc_sram')
    if dcc_region is None:
        print_out_str("!!!!! No dcc_sram entry found in SysDbg table !!!!!")
        return

    dcc_sram_buf_start_addr, num_elements = dcc_region
    if num_elements == 0:
        print_out_str("Wrong Number of Elements: 0")
        return
    # print_out_str(f"Number of elements: {num_elements}")

    if dcc_sram_buf_start_addr == 0:
        print_out_str("Wrong DCC SRAM Buffer Start Address: 0x0")
        return
    # print_out_str(f"dcc_sram_buf_start_addr: 0x{dcc_sram_buf_start_addr:08X}")

    output_dir = ramdump.outdir
    output_file_path = os.path.join(output_dir, 'DCC_SRAM.bin')
    generate_dcc_sram(ramdump, dcc_sram_buf_start_addr, num_elements, output_file_path)

    file_path = output_file_path # 'DCC_SRAM.bin'
    for dump in dcc_sram_config[ramdump.hw_id]["dumps"]:
        start_offset = dump["start_offset"]
        count = dump["size_in_bytes"] // 4
        dump["function_pointer"](file_path, start_offset, count)


    # Generate MONITOR.BIN file
    monitor_addr = get_mon_mmu_addr(ddr_address)
    monitor_table_ptr = ramdump.read_u32(monitor_addr, virtual=False)
    # Locate the mon_pt entry by name (core-count independent), matching the
    # same SYDB table-walk logic used for dcc_sram above.
    monitor_region = find_sydb_entry(ramdump, monitor_table_ptr, b'mon_pt')
    if monitor_region is None:
        print_out_str("!!!!! No mon_pt entry found in SysDbg table !!!!!")
    else:
        monitor_start, num_elements_monitor = monitor_region
        if num_elements_monitor == 0:
            print_out_str("Wrong Number of Elements for Monitor: 0")
        elif monitor_start == 0:
            print_out_str("Wrong Monitor Start Address: 0x0")
        else:
            output_file_path = os.path.join(output_dir, 'MONITOR.bin')
            generate_dcc_sram(ramdump, monitor_start, num_elements_monitor, output_file_path)
            # print_out_str(f"MONITOR.BIN file generated successfully at {output_file_path}")
