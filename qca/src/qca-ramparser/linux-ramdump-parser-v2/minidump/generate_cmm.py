#Copyright (c) 2019, The Linux Foundation. All rights reserved.
#
#This program is free software; you can redistribute it and/or modify
#it under the terms of the GNU General Public License version 2 and
#only version 2 as published by the Free Software Foundation.
#
#This program is distributed in the hope that it will be useful,
#but WITHOUT ANY WARRANTY; without even the implied warranty of
#MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#GNU General Public License for more details.
#

import glob
import os
from optparse import OptionParser
import fileinput
import struct
import sys
import subprocess
import re
import random
dmesg_size = 0x20000
def dump2mem_extract(file_name):
    f = open(file_name, mode="rb")

    '''
    struct memdump_hdr {
            uint32_t magic1;
            uint32_t magic2;
            uint32_t nos_dumps;
            uint32_t total_dump_sz;
            uint64_t dumps_list_info_offset;
            uint32_t reserved[2];
    };
    '''
    memdump_hdr_fmt = 'IIIIQII'
    memdump_hdr_struct = struct.Struct(memdump_hdr_fmt)
    memdump_hdr_size = struct.calcsize(memdump_hdr_fmt)

    memdump_hdr_pdata = f.read(memdump_hdr_size)
    memdump_hdr_updata = (memdump_hdr_struct).unpack(memdump_hdr_pdata)

    nos_dumps = memdump_hdr_updata[2]
    dump_list_offset = memdump_hdr_updata[4]

    '''
    struct memdumps_list_info {
            char name[20];
            uint64_t offset;
            uint64_t size;
    };
    '''
    memdump_list_fmt = '20sQQ'
    memdump_list_struct = struct.Struct(memdump_list_fmt)
    memdump_list_size = struct.calcsize(memdump_list_fmt)

    curr_dump_list_offset = dump_list_offset
    for i in range(0, nos_dumps):
        f.seek(curr_dump_list_offset)
        memdump_list_pdata = f.read(memdump_list_size)
        memdump_list_updata = (memdump_list_struct).unpack(memdump_list_pdata)

        f.seek(memdump_list_updata[1])
        dump_content = f.read(memdump_list_updata[2])
        if (sys.version_info.major >= 3):
            dump_name = memdump_list_updata[0].split(b'\x00')[0].decode("utf-8")
        else:
           dump_name = memdump_list_updata[0].split('\x00',1)[0]
        dump = open(dump_name, mode="wb")
        dump.write(dump_content)
        dump.close()
        curr_dump_list_offset = curr_dump_list_offset + memdump_list_size

    f.close()

#def print_mod_info():
#    module_output_cmm.write("TASK.sYmbol.Option AutoLoad Module\n")
#    module_output_cmm.write("TASK.sYmbol.LOADMod ""\n")
#    module_output_cmm.write("d.load.elf "+ mod_name +" /nocode /noclear  /reloc .bss AT 0x" + address + "\n")

def file_base_name(file_name):
	if 'DEBUGFS' in file_name:
		separator_index = file_name.index('_')
		file_name = file_name[separator_index+1:]
	if '.' in file_name:
		separator_index = file_name.index('.')
		base_name = file_name[:separator_index]
		return base_name
	else:
		return file_name

def read_u32(imem_path, offset):
    try:
        with open(imem_path, 'rb') as file:
            file.seek(offset)
            offset = file.read(4)
            little_offset = int.from_bytes(offset, byteorder="little", signed=False)
            little_offset = '{:08x}'.format(little_offset)
            if len(little_offset) < 8:
                little_offset += '00' * (8 - len(little_offset))
            return little_offset
    except IOError as e:
            print("Error: Unable to open file or file not found. {}".format(e))

def read_u64(imem_path, offset):
    try:
        with open(imem_path, 'rb') as file:
            file.seek(offset)
            data = file.read(8)
            little_offset = int.from_bytes(data, byteorder="little", signed=False)
            little_offset = '{:016x}'.format(little_offset)
            return little_offset
    except IOError as e:
        print("Error: Unable to open file or file not found. {}".format(e))

def get_va_to_pa(options, mmu_file, va):
    mmu_file_data = open(mmu_file, 'r')
    for line in reversed(mmu_file_data.readlines()):
        if va in line:
            pa = line[line.index('pa=') +3:line.index('\0')]
            mmu_file_data.close()
            return pa
    return None

def get_file(options, file_address):
    file_buffer = file_address + ".BIN"
    if options.path:
        input_file = os.path.join(options.path,file_buffer)
        if not os.path.isfile(input_file):
            input_file = os.path.join(options.path,file_buffer.upper())
    else:
        input_file = file_buffer
        if not os.path.isfile(input_file):
            input_file = file_buffer.upper()
    return input_file

def extract_file(options, file_address, output_file):
    input_file = get_file(options, file_address)
    if options.path:
        outputfile = os.path.join(options.path, output_file)
    else:
        outputfile = output_file
    subprocess.run(["strings", input_file], stdout=open(outputfile, "w"))

#Add path options for dump binaries, vmlinux file and modules
def parse_args(args=None):
    parser = OptionParser()
    parser.add_option('--config',dest='config',default='32',help='CONFIG is set to 32 or 64. Default is 32 bit')
    parser.add_option('--arch',dest='arch',default='ipq807x',help='arch is set to ipq807x or ipq60xx. Default is ipq807x')
    parser.add_option('--kver',dest='kver',default='4.4',help='kver is set to 4.4 or 5.4. Default is 4.4')
    parser.add_option('--kaslr',dest='kaslr',default='false',help='KASLR enabled is set to FALSE as defualt')
    parser.add_option('--vmlinux',dest='vmlinux',help='vmlinux.elf file along with path.')
    parser.add_option('--path',dest='path',help='Path to dump binaries.')
    parser.add_option('--modpath',dest='mpath',help='Path to load modules.')
    parser.add_option('--dump2mem',dest='dump2mem',default=False, action='store_true', help='dump2mem is set to parse the dumps from minidump2mem.bin')
    (options, _) = parser.parse_args(args)
    return options

def generate_cmm(options):
    if options.path is None:
        print("Dump Location is not provided, Can't proceed\n")
        sys.exit(1)

    if options.path:
        if (options.dump2mem):
            dump2mem_extract(os.path.join(options.path,"minidump2mem.bin"))
        module_input_file=open(os.path.join(options.path,"MOD_INFO.txt"))
    else:
        if (options.dump2mem):
            dump2mem_extract("minidump2mem.bin")
        module_input_file=open("MOD_INFO.txt")

    for line in reversed(module_input_file.readlines()):
        if "PGD" in line:
            PGD = line[line.index('=') +1:line.index('\0')]
        if "DMESG pa" in line:
            dmesg_address = line[line.index('=') + 1 : line.index('\0')]
        if "linux_banner" in line:
            linux_banner_addr = line[line.index('=') + 1 : line.index('\0')]
        if "TZ_DIAG" in line:
            tz_diag_addr = line[line.index('=') + 1 : line.index('\0')]
        if "DMESG_READ" in line:
            dmesg_read = line[line.index('=') + 1 : line.index('\0')]

    if options.path:
        mmu_input_file=open(os.path.join(options.path,"MMU_INFO.txt"))
        mmu_file = os.path.join(options.path,"MMU_INFO.txt")
    else:
        mmu_input_file=open("MMU_INFO.txt","r")
        mmu_file = "MMU_INFO.txt"

    if 'linux_banner_addr' in locals() and linux_banner_addr is not None:
        linux_banner_pa = get_va_to_pa(options, mmu_file, linux_banner_addr)
        extract_file(options, linux_banner_pa, "linux_banner.txt")

        if options.path:
            linux_banner_file = os.path.join(options.path, "linux_banner.txt")
        else:
            linux_banner_file = "linux_banner.txt"

        with open(linux_banner_file, "r") as f:
            linux_data = f.read()

            # Extract kernel version (major.minor), e.g. "6.6" from "Linux version 6.6.116 ..."
            kver_match = re.search(r'Linux version (\d+\.\d+)\.', linux_data)
            if kver_match:
                options.kver = kver_match.group(1)

            # Extract architecture bits, e.g. "64" from "aarch64" or "32" from "armv7l"
            arch_match = re.search(r'\b(aarch64)\b', linux_data)
            if arch_match:
                options.config = "64"
            else:
                options.config = "32"

    if options.kver == "4.4":
        PAGE_OFFSET = "0xffffffc000000000"
        HIGH_MEM = "0xffffffc03f000000"
    else:
        PAGE_OFFSET = "0xffffffc010000000"
        HIGH_MEM = "0xffffffc03f000000"

    if options.config == "64":
        t32commands = ["r.s M 0x05",
        "Data.Set SPR:0x30201 %Quad 0x"+PGD,
        "Data.Set SPR:0x30202 %Quad 0x00000012B5193519",
        "Data.Set SPR:0x30A20 %Quad 0x000000FF440C0400",
        "Data.Set SPR:0x30A30 %Quad 0x0000000000000000",
        "Data.Set SPR:0x30100 %Quad 0x0000000034D5D91D",
        "MMU.SCAN PT 0xFFFFFF8000000000--0xFFFFFFFFFFFFFFFF",
        "mmu.on",
        "mmu.scan",
        "system.Option MMUSPACES ON",
        "task.config c:\\T32\demo\\arm64\kernel\linux\linux-3.x\linux3.t32",
        "menu.reprogram c:\\T32\demo\\arm64\kernel\linux\linux-3.x\linux.men",
        "task.dtask",
        "v.v  %ASCII %STRING linux_banner"]
    else:
        t32commands = ["r.s M 0x13",
            "PER.Set.simple SPR:0x30200 %Quad 0x"+PGD,
            "PER.Set.simple C15:0x1 %Long 0x1025",
            "Data.Set SPR:0x36110 %Quad 0x535",
            "mmu.on",
            "mmu.scan",
            "system.Option MMUSPACES ON",
            "task.config c:\\T32\demo\\arm64\kernel\linux\linux-3.x\linux3.t32",
            "menu.reprogram c:\\T32\demo\\arm64\kernel\linux\linux-3.x\linux.men",
            "task.dtask",
            "v.v  %ASCII %STRING linux_banner"]

    if options.path:
        onlyfiles = (glob.glob(os.path.join(options.path,"*.BIN")))
    else:
        onlyfiles = (glob.glob("*.BIN"))

    if options.path:
        startup_cmm=open(os.path.join(options.path,"startup_t32.cmm"), "w")
    else:
       startup_cmm=open("startup_t32.cmm","w")

    cpu = "CORTEXA53"
    if options.arch == "ipq9574":
        cpu = "CORTEXA73"
    elif options.arch == "ipq5424":
        cpu = "CORTEXA55"

    startup_cmm.write("sys.cpu " + cpu  + "\n")
    startup_cmm.write("sys.up" + "\n");
    startup_cmm.write("&BINDIR=OS.PWD()" + "\n");
    startup_cmm.write("LOCAL &ELFFile" + "\n")

    for i in range(len(onlyfiles)):
        if options.path:
            base_name = file_base_name(os.path.basename(onlyfiles[i]))
        else:
            base_name = file_base_name(onlyfiles[i])
        startup_cmm.write("data.load.binary" + " " + "&BINDIR\\" + base_name + ".BIN "+ " 0x" + base_name + "\n")


    # If KASLR is enabled in dump, KASLR kernel and module offset should be used for parsing.
    # Kernel and module offset details are stored in the below IMEM region
    # Module offset in 0x086006BC - 0x086006C0 (8 bytes)
    # Kernel offset in 0x086006C4 - 0x086006C8 (8 bytes)
    if options.path:
        imemFile = os.path.join(options.path,"8600000.BIN")
    else:
        imemFile = "8600000.BIN"

    if options.vmlinux:
        apss_elf = os.path.basename(options.vmlinux)
        startup_cmm.write("&ELFFile=\"" + apss_elf + "\"\n")
    else:
        # If vmlinux is not provided, then create a dialog box to get the input from user
        if options.path:
            dialog_file=open(os.path.join(options.path,"elf_select.dlg"), "w")
        else:
            dialog_file=open("elf_select.dlg","w")

        dialog_file.write("NAME \"ELF selection\"\n")
        dialog_file.write("HEADER \"ELF File selection\"\n")
        dialog_file.write("\tPOS 2. 0. 35. 1.\n")
        dialog_file.write("\tTEXT \"Select VMLINUX FILE\"\n")
        dialog_file.write("ELFFile:  EDIT \"\" \"\"\n")
        dialog_file.write("\tPOS 38. 1. 10. 1.\n")
        dialog_file.write("\tBUTTON \"Browse\"\n")
        dialog_file.write("\t(\n")
        dialog_file.write("\t\tDIALOG.SetFile ELFFile .\\*\n")
        dialog_file.write("\t)\n\n")
        dialog_file.write("\tPOS 38. 2. 10. 1.\n")
        dialog_file.write("\tDEFBUTTON \"OK\" \"CONTinue\"\n")
        dialog_file.close()

        startup_cmm.write("\nprint \"Choose the vmlinux file in dialog box.\" \n" )
        startup_cmm.write("DIALOG.view elf_select.dlg\n")
        startup_cmm.write("STOP\n")
        startup_cmm.write("&ELFFile=DIALOG.STRing(ELFFile)\n")
        startup_cmm.write("DIALOG.END\n\n")

    if options.kaslr == "true":
        # Choose appropriate read functions based on 32/64-bit configuration
        if options.config == "64":
            read_offset = lambda off: read_u64(imemFile, off)
            magic_read = lambda off: read_u32(imemFile, off)
        else:
            read_offset = lambda off: read_u32(imemFile, off)
            magic_read = lambda off: read_u32(imemFile, off)

        # Read module offset (not currently used but kept for completeness)
        module_offset_hex = read_offset(0x6BC)

        # Check magic code to decide kernel offset location
        magic_code_hex = magic_read(0x6C4)
        if int(magic_code_hex, 16) == 0xCDEFCDEF:
            kernel_offset_addr = 0x6C8
        else:
            kernel_offset_addr = 0x6C4

        # Read kernel offset
        kaslr_kernel_offset_hex = read_offset(kernel_offset_addr)

        # Write ELF load command with the calculated kernel offset
        startup_cmm.write("data.load.elf" + " " + "&ELFFile" + " 0x" + kaslr_kernel_offset_hex + " /nocode\n")
    else:
        startup_cmm.write("data.load.elf" + " " + "&ELFFile" + " /Nocode" + "\n")

    pgd_int = int(PGD, 16)

    if options.path:
        mmu_output_cmm=open(os.path.join(options.path,"Load_mmu.cmm"), "w")
    else:
        mmu_output_cmm=open("Load_mmu.cmm","w")

    # Generate a "Load_mmu.cmm" script that parses through all the
    # VA to PA entries dumped in MMU_INFO.txt and does the following
    # for each dump segment:
    #
    # For 32 bit:
    # "d.s A:< PTE entry address > %LE %Long (physical address & 0x000) | 0x45e"
    #
    #	Calculating PTE entry address from VA:
    #    a)PGD entry address for dump segment = ((( VA / 0x100000) * 4 ) + PGD base address)
    #    b)The content of PGD entry address will be base
    #			address of PTE = PTE base address
    #	 c)Mask attributes & calculate offset  =
    #		 ((PTE base address & (0xFFFFFC00)) &  0xFE000 ) >> 12 = offset
    #	 d)PTE entry address is = Masked PTE base address +  (4 * offset)
    #    e)Set value at PTE entry address as
    #		  physical address | 0x45E to account for page table attributes
    #
    # For 64 bit:
    #	If address range is equal to or above PAGE_OFFSET:
    #		"d.s A:< PMD entry address > %LE %Long (physical address & 0x00000) | 0x001"
    #   else
    #		"d.s A:< PMD entry address > %LE %Long PTE base address"
    #		"d.s A:< PTE entry address > %LE %Long (physical address & 0x000) | 0x00F"
    #
    #	Calculating PMD entry address from VA:
    #	 a)PGD entry address for dump segment
    #		 = (( VA >> 30 & 0x1FF) + PGD base address)
    #	 b)The content of PGD entry address will be
    #		base address of PMD = PMD base address
    #	 c)PMD offset  = (VA >> 21 & 0x1FF ) * 8
    #	 d)PMD entry address = PMD offset + PMD base address
    #
    #	Calculating PTE base address from VA:
    #	 a)PTE base address range is arbitrarily chosen as
    #		0x30000000 - 0x30064000 since these addresses are empty
    #	 b)While trying to fill content at above calculated PMD
    #		  entry address, first check if the entry is empty.
    #	 c)If the above entry is empty proceed to fill content PMD
    #		entry address with current available PTE base address &
    #		increment current available PTE base address by 0x1000
    #
    #	d.s A:< PMD entry address > %LE %Long (PTE base address & 0xFFFFFFF0) + 0x3
    #   to account for page table attributes
    #
    # 	d)If the above entry is NOT do nothing and proceed to the next step.
    #
    #	Calculating PTE entry address from VA:
    #
    #	a)Mask attributes & calculate PTE offset  = (VA >> 12 & 0x1FF ) * 8
    #	b)PTE entry address = Above calculated PTE base address & 0xFFFFFFF0 + PTE offset

    mmu_output_cmm.write("GLOBAL &curr_pte_base\n")
    mmu_output_cmm.write("&curr_pte_base=0x30000000\n")
    next(mmu_input_file)

    for line in mmu_input_file:
        if options.config == "32":
            va =line[line.find('va=') + 3: line.find(' ')]
            pa =line[line.find('pa=') + 3: line.find('\0')]
            pgd_entry_addr = ((int(va, 16) >> 20) * 4) + pgd_int
            pte_off = ((int(va, 16) & 0xFF000) >> 12) * 4
            pgd_entry_addr = hex(pgd_entry_addr).rstrip("L")
            pte_off = hex(pte_off).rstrip("L")
            pa = (int(pa, 16) & 0xFFFFF000) + 0x45E
            pa = hex(pa).rstrip("L")
            mmu_output_cmm.write("GOSUB mmu_translation "+pgd_entry_addr+" "+pte_off+" "+pa+"\n")
        else:
            va =line[line.find('va=') + 3: line.find(' ')]
            pa =line[line.find('pa=') + 3: line.find('\0')]
            mem_check = int(va, 16)
            mem_check = hex(mem_check).rstrip("L")
            if mem_check >= PAGE_OFFSET and mem_check <= HIGH_MEM :
                pgd_entry_addr = (((int(va, 16) >> 30) & 0x1FF) * 8 ) + pgd_int
                pmd_off = ((int(va, 16) >> 21) & 0x1FF) * 8
                pgd_entry_addr = hex(pgd_entry_addr).rstrip("L")
                pmd_off = hex(pmd_off).rstrip("L")
                pa = (int(pa, 16) & 0xFFF00000) + 0x001
                pa =hex(pa).rstrip("L")
                mmu_output_cmm.write("GOSUB mmu_translation_lowmem "+pgd_entry_addr+" "+pmd_off+" "+pa+"\n")
            else:
                pgd_entry_addr = (((int(va, 16) >> 30) & 0x1FF) * 8 ) + pgd_int
                pmd_off = ((int(va, 16) >> 21) & 0x1FF) * 8
                pte_off = ((int(va, 16) >> 12) & 0x1FF) * 8
                pgd_entry_addr = hex(pgd_entry_addr).rstrip("L")
                pmd_off = hex(pmd_off).rstrip("L")
                pte_off = hex(pte_off).rstrip("L")
                pa = (int(pa, 16) & 0xFFFFF000) + 0x00F
                pa =hex(pa).rstrip("L")
                mmu_output_cmm.write("GOSUB mmu_translation_highmem "+pgd_entry_addr+" "+pmd_off+" "+pte_off+" "+pa+"\n")
    mmu_output_cmm.write("ENDDO\n")
    if options.config == "32":
        mmu_output_cmm.write("mmu_translation:\n")
        mmu_output_cmm.write("ENTRY &pgd_entry_addr &pte_off &pa\n")
        mmu_output_cmm.write("LOCAL &pte_base &pte_entry\n")
        mmu_output_cmm.write("&pte_base=(Data.Long(A:&pgd_entry_addr)&0xFFFFFC00)\n")
        mmu_output_cmm.write("&pte_entry=(&pte_base+&pte_off)\n")
        mmu_output_cmm.write("D.S A:&pte_entry %LE %Long &pa \n")
        mmu_output_cmm.write("RETURN \n")
    else:
        mmu_output_cmm.write("mmu_translation_lowmem:\n")
        mmu_output_cmm.write("ENTRY &pgd_entry_addr &pmd_off &pa\n")
        mmu_output_cmm.write("LOCAL &pmd_base &pte_entry &pmd_entry &pte_base\n")
        mmu_output_cmm.write("&pmd_base=(Data.Long(A:&pgd_entry_addr)&0xFFFFFC00)\n")
        mmu_output_cmm.write("&pmd_entry=(&pmd_base+&pmd_off)\n")
        mmu_output_cmm.write("D.S A:&pmd_entry %LE %Long &pa \n")
        mmu_output_cmm.write("RETURN \n")
        mmu_output_cmm.write("mmu_translation_highmem:\n")
        mmu_output_cmm.write("ENTRY &pgd_entry_addr &pmd_off &pte_off &pa\n")
        mmu_output_cmm.write("LOCAL &pmd_base &pte_entry &pmd_entry &pte_base &val_final\n")
        mmu_output_cmm.write("&pmd_base=(Data.Long(A:&pgd_entry_addr)&0xFFFFFC00)\n")
        mmu_output_cmm.write("&pmd_entry=(&pmd_base+&pmd_off)\n")
        mmu_output_cmm.write("&pte_base=Data.Long(A:&pmd_entry)\n")
        mmu_output_cmm.write("IF (Data.Long(A:&pmd_entry))==0x0\n")
        mmu_output_cmm.write("(\n")
        mmu_output_cmm.write("&pte_base=&curr_pte_base\n")
        mmu_output_cmm.write("&pte_entry=(&pte_base+&pte_off)\n")
        mmu_output_cmm.write("D.S A:&pmd_entry %LE %Long ((&pte_base&0xFFFFFFF0)+0x3)\n")
        mmu_output_cmm.write("D.S A:&pte_entry %LE %Long &pa\n")
        mmu_output_cmm.write("&curr_pte_base=(&curr_pte_base+0x1000)\n")
        mmu_output_cmm.write(")\n")
        mmu_output_cmm.write("ELSE\n")
        mmu_output_cmm.write("(\n")
        mmu_output_cmm.write("&pte_entry=((&pte_base&0xFFFFFFF0)+&pte_off)\n")
        mmu_output_cmm.write("D.S A:&pte_entry %LE %Long &pa\n")
        mmu_output_cmm.write(")\n")
        mmu_output_cmm.write("RETURN \n")

    if 'dmesg_read' in locals() and dmesg_read is not None:
        dmesg_read_file = get_file(options, dmesg_read)
        with open(dmesg_read_file, "rb") as f:
            tail_add = f.read()
        read_offset = (int.from_bytes(tail_add, byteorder='little') + dmesg_size) % dmesg_size
        read_offset = min(read_offset, dmesg_size)
        dmesg_file = get_file(options, dmesg_address)
        with open(dmesg_file, "rb") as f:
            dmesg_lines = f.read()
        part1 = dmesg_lines[read_offset:]
        part2 = dmesg_lines[:read_offset]
        if options.path:
            outputfile = os.path.join(options.path, "dmesg.txt")
            dmesg_bin = os.path.join(options.path, "DMESG.BIN")
        else:
            outputfile = "dmesg.txt"
            dmesg_bin = "DMESG.BIN"
        with open(dmesg_bin, "wb") as f:
            f.write(part1)
        with open(dmesg_bin, "ab") as f:
            f.write(part2)
        subprocess.run(["strings", dmesg_bin], stdout=open(outputfile, "w"))
        os.remove(dmesg_bin)
    else:
        extract_file(options, dmesg_address, "dmesg.txt")

    if 'tz_diag_addr' in locals() and tz_diag_addr is not None:
        tz_diga_pa = get_va_to_pa(options, mmu_file, tz_diag_addr)
        extract_file(options, tz_diga_pa, "tz_diag.txt")

    mmu_input_file.close()
    mmu_output_cmm.close()
    module_input_file.close()
    startup_cmm.write("DO Load_mmu.cmm\n")
    for i in range(len(t32commands)):
        startup_cmm.write(t32commands[i] +"\n")
    startup_cmm.write("area.view\n")
    startup_cmm.write("print " + "\"To load modules, please follow below steps\"\n")
    startup_cmm.write("print " + "\"Click on linux from menu -> Module debugging -> Load Sybols -> browse and load module\"\n")
    startup_cmm.close()

    generate_t32_launch(options)

def generate_t32_launch(options):

    if options.path:
        launch_config=open(os.path.join(options.path,"t32_config.t32"), "w")
    else:
        launch_config=open("t32_config.t32","w")

    launch_config.write('OS=\n')
    launch_config.write('ID=T32_1000002\n')

    launch_config.write('TMP=C:\\TEMP\n')
    launch_config.write('HELP=C:\\T32\\pdf\n')
    launch_config.write('\n')
    launch_config.write('PBI=SIM\n')
    launch_config.write('\n')
    launch_config.write('SCREEN=\n')
    launch_config.write('FONT=SMALL\n')
    launch_config.write('HEADER=Trace32-ARM\n')
    launch_config.write('\n')
    launch_config.write('PRINTER=WINDOWS\n')
    launch_config.write('\n')
    launch_config.write('RCL=NETASSIST\n')
    launch_config.write('PACKLEN=1024\n')
    launch_config.write('PORT=%d\n' % random.randint(20000, 30000))
    launch_config.write('\n')

    launch_config.close()

    if options.path:
        t32_bat=open(os.path.join(options.path,"launch_t32.bat"), "w")
    else:
       t32_bat=open("launch_t32.bat","w")

    if options.config == "64":
        t32_binary = 'C:\\T32\\bin\\windows64\\t32MARM64.exe'
    else:
        t32_binary = 'C:\\T32\\bin\\windows64\\t32MARM.exe'

    t32_bat.write("start " + t32_binary + " -c t32_config.t32 -s startup_t32.cmm\n")
    t32_bat.close()

def main():
    options = parse_args()
    generate_cmm(options)

def run_from_ramparser(arg1, arg2, arg3, arg4, kaslr=False, force_hardware=None):
    args = [f"--path={arg1}", f"--vmlinux={arg2}", f"--config={arg3}", f"--kver={arg4}"]
    if kaslr:
        args.append("--kaslr=true")
    if force_hardware is not None:
        args.append(f"--arch=ipq{force_hardware}")
    options = parse_args(args)
    generate_cmm(options)

if __name__ == "__main__":
    main()
