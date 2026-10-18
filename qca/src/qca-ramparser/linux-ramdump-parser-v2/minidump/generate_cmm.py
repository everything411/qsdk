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

#Add path options for dump binaries, vmlinux file and modules
parser = parser = OptionParser()
parser.add_option('--config',dest='config',default='32',help='CONFIG is set to 32 or 64. Default is 32 bit')
parser.add_option('--arch',dest='arch',default='ipq807x',help='arch is set to ipq807x or ipq60xx. Default is ipq807x')
parser.add_option('--kver',dest='kver',default='4.4',help='kver is set to 4.4 or 5.4. Default is 4.4')
parser.add_option('--kaslr',dest='kaslr',default='false',help='KASLR enabled is set to FALSE as defualt')
parser.add_option('--vmpath',dest='vmpath',help='Path to vmlinux.elf file.')
parser.add_option('--path',dest='path',help='Path to dump binaries.')
parser.add_option('--modpath',dest='mpath',help='Path to load modules.')
parser.add_option('--dump2mem',dest='dump2mem',default=False, action='store_true', help='dump2mem is set to parse the dumps from minidump2mem.bin')
(options, args) = parser.parse_args()

if options.kver == "4.4":
	PAGE_OFFSET = "0xffffffc000000000"
	HIGH_MEM = "0xffffffc03f000000"
	if options.config == "64":
		elf = "openwrt-ipq-"+ options.arch +"_64-vmlinux.elf"
	else:
		elf = "openwrt-ipq-"+ options.arch +"-vmlinux.elf"
else:
    PAGE_OFFSET = "0xffffffc010000000"
    HIGH_MEM = "0xffffffc03f000000"
    if options.arch == "ipq807x":
        if options.config == "64":
            elf = "openwrt-ipq807x-generic-vmlinux.elf"
        else:
            elf = "openwrt-ipq807x-ipq807x_32-vmlinux.elf"
    else:
        if options.config == "64":
            elf = "openwrt-" + options.arch + "-generic-vmlinux.elf"
        else:
            elf = "openwrt-" + options.arch + "-" + options.arch + "_32-vmlinux.elf"

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

if options.path:
    if (options.dump2mem):
        dump2mem_extract(os.path.join(options.path,"minidump2mem.bin"))
    module_input_file=open(os.path.join(options.path,"MOD_INFO.txt"))
else:
    if (options.dump2mem):
        dump2mem_extract("minidump2mem.bin")
    module_input_file=open("MOD_INFO.txt")

if options.path:
    module_output_cmm=open(os.path.join(options.path,"Load_modules.cmm"), "w")
else:
    module_output_cmm=open("Load_modules.cmm","w")

umac = None
qca_ol = None
wifi_3_0 = None
qdf = None

def print_mod_info(name,line):
    address = line[line.index('=') + 1 : line.index('\0')]
    if options.mpath :
       mod_name = os.path.join(options.mpath,name)
    else :
       mod_name = name
    module_output_cmm.write("d.load.elf "+ mod_name +" /nocode /noclear  /reloc .bss AT 0x" + address + "\n")

for line in reversed(module_input_file.readlines()):
    if "umac" in line and umac != True:
         name = "umac.ko"
         umac = True
         print_mod_info(name,line)
    if "qca_ol" in line and qca_ol != True:
         name = "qca_ol.ko"
         qca_ol = True
         print_mod_info(name,line)
    if "wifi_3_0" in line and wifi_3_0 != True:
         name = "wifi_3_0.ko"
         wifi_3_0 = True
         print_mod_info(name,line)
    if "qdf" in line and qdf != True:
         name = "qdf.ko"
         qdf = True
         print_mod_info(name,line)
    if "PGD" in line:
        PGD = line[line.index('=') +1:line.index('\0')]
    if "DMESG pa" in line:
        dmesg_address = line[line.index('=') + 1 : line.index('\0')]



module_output_cmm.close()

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
    "task.config c:\\T32\demo\\arm64\kernel\linux\linux-3.x\linux3.t32",
    "menu.reprogram c:\\T32\demo\\arm64\kernel\linux\linux-3.x\linux.men",
    "task.dtask",
    "v.v  %ASCII %STRING linux_banner"]
    if options.vmpath:
        vmlinux =os.path.join(options.vmpath,elf)
    else:
        vmlinux = elf
else:
    t32commands = ["r.s M 0x13",
	"PER.Set.simple SPR:0x30200 %Quad 0x"+PGD,
	"PER.Set.simple C15:0x1 %Long 0x1025",
	"Data.Set SPR:0x36110 %Quad 0x535",
	"mmu.on",
	"mmu.scan",
	"task.config c:\\T32\demo\\arm\kernel\linux\linux-3.x\linux3.t32",
	"menu.reprogram c:\\T32\demo\\arm\kernel\linux\linux-3.x\linux.men",
	"task.dtask",
	"v.v  %ASCII %STRING linux_banner"]
    if options.vmpath:
        vmlinux =os.path.join(options.vmpath,elf)
    else:
        vmlinux = elf

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

if options.path:
    onlyfiles = (glob.glob(os.path.join(options.path,"*.BIN")))
else:
    onlyfiles = (glob.glob("*.BIN"))

if options.path:
    startup_cmm=open(os.path.join(options.path,"startup_t32.cmm"), "w")
else:
    startup_cmm=open("startup_t32.cmm","w")
startup_cmm.write("sys.cpu CORTEXA53 " + "\n")
startup_cmm.write("sys.up" + "\n");
startup_cmm.write("&BINDIR=OS.PWD()" + "\n");

for i in range(len(onlyfiles)):
	if options.path:
		base_name = file_base_name(os.path.basename(onlyfiles[i]))
	else:
		base_name = file_base_name(onlyfiles[i])
	startup_cmm.write("data.load.binary" + " " + "&BINDIR\\" + base_name + ".BIN "+ " 0x" + base_name + "\n")

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

# If KASLR is enabled in dump, KASLR kernel and module offset should be used for parsing.
# Kernel and module offset details are stored in the below IMEM region
# Module offset in 0x086006BC - 0x086006C0 (8 bytes)
# Kernel offset in 0x086006C4 - 0x086006C8 (8 bytes)
if options.kaslr == "true":
    if options.path:
        imemFile = os.path.join(options.path,"8600000.BIN")
    else:
        imemFile = "8600000.BIN"

    kernel_offset_former = read_u32(imemFile, 0x6C4)
    kernel_offset_latter = read_u32(imemFile, 0x6C8)
    kaslr_kernel_offset = kernel_offset_latter + kernel_offset_former
    startup_cmm.write("data.load.elf &BINDIR\{0}  0x{1} /nocode\n".format(elf, kaslr_kernel_offset))
else:
    startup_cmm.write("data.load.elf" + " " + "&BINDIR\\" + elf + " "+ "/Nocode" + "\n")

pgd_int = int(PGD, 16)

if options.path:
    mmu_output_cmm=open(os.path.join(options.path,"Load_mmu.cmm"), "w")
else:
    mmu_output_cmm=open("Load_mmu.cmm","w")

if options.path:
	mmu_input_file=open(os.path.join(options.path,"MMU_INFO.txt"))
else:
	mmu_input_file=open("MMU_INFO.txt","r")

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

def extract_Dmesg():
    dmesg_buffer = dmesg_address + ".BIN"
    if options.path:
        dmesg_file = os.path.join(options.path,dmesg_buffer)
        if not os.path.isfile(dmesg_file):
            dmesg_file = os.path.join(options.path,dmesg_buffer.upper())
        outputfile = os.path.join(options.path,"dmesg.txt")
    else:
        dmesg_file = dmesg_buffer
        if not os.path.isfile(dmesg_file):
            dmesg_file = dmesg_buffer.upper()
        outputfile = "dmesg.txt"
    subprocess.run(["strings", dmesg_file], stdout=open(outputfile, "w"))


extract_Dmesg()

mmu_input_file.close()
mmu_output_cmm.close()
module_input_file.close()
startup_cmm.write("DO Load_mmu.cmm\n")
for i in range(len(t32commands)):
    startup_cmm.write(t32commands[i] +"\n")
startup_cmm.write("DO Load_modules.cmm\n")
startup_cmm.close()
