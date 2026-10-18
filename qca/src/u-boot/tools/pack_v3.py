######################################################################
# Copyright (c) 2017, 2019, The Linux Foundation. All rights reserved.
# Copyright (c) 2022 - 2023 Qualcomm Innovation Center, Inc. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 and
# only version 2 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#####################################################################

"""
Script to create a U-Boot flashable multi-image blob.

This script creates a multi-image blob, from a bunch of images, and
adds a U-Boot shell script to the blob, that can flash the images from
within U-Boot. The procedure to use this script is listed below.

  1. Create an images folder. Ex: my-pack

  2. Copy all the images to be flashed into the folder.

  3. Copy the partition MBN file into the folder. The file should be
     named 'partition.mbn'. This is used to determine the offsets for
     each of the named partitions.

  4. Create a flash configuration file, specifying the images be
     flashed, and the partition in which the images is to be
     flashed. The flash configuration file can be specified using the
     -f option, default is flash.conf.

  5. Invoke 'pack' with the folder name as argument, pass flash
     parameters as arguments if required. A single image file will
     be created, out side the images folder, with .img suffix. Ex:
     my-pack.img

  6. Transfer the file into a valid SDRAM address and invoke the
     following U-Boot command to flash the images. Replace 0x41000000,
     with address location where the image has been loaded. The script
     expects the variable 'imgaddr' to be set.

     u-boot> imgaddr=0x88000000 source $imgaddr:script

Host-side Pre-req

  * Python >= 2.6
  * ordereddict >= 1.1 (for Python 2.6)
  * mkimage >= 2012.07
  * dtc >= 1.2.0

Target-side Pre-req

The following U-Boot config macros should be enabled, for the
generated flashing script to work.

  * CONFIG_FIT -- FIT image format support
  * CONFIG_SYS_HUSH_PARSER -- bash style scripting support
  * CONFIG_SYS_NULLDEV -- redirecting command output support
  * CONFIG_CMD_XIMG -- extracting sub-images support
  * CONFIG_CMD_NAND -- NAND Flash commands support
  * CONFIG_CMD_NAND_YAFFS -- NAND YAFFS2 write support
  * CONFIG_CMD_SF -- SPI Flash commands support
  * CONFIG_IPQ_MIBIB_RELOAD -- reloading part info from mibib support
  * CONFIG_IPQ_XTRACT_N_FLASH -- xtract_n_flash cmd support
"""

from os.path import getsize
from getopt import getopt
from getopt import GetoptError
from collections import namedtuple
from string import Template
from shutil import copy
from shutil import rmtree

import os
import sys
import copy
import os.path
import subprocess
import struct
import hashlib
import xml.etree.ElementTree as ET

version = "1.1"
ARCH_NAME = ""
SRC_DIR = ""
MODE = ""
image_type = "all"
memory_size = "default"
flayout = "default"
skip_4k_nand = "false"
atf = "false"
img_suffix = ""
supported_arch = ["ipq5424", "ipq5424_64", "ipq5332", "ipq5332_64"]
supported_flash_type = {}
supported_flash_type["ipq5332"] = { "nand", "nor", "tiny-nor", "emmc", "norplusnand", "norplusemmc", "tiny-nor-debug" };
supported_flash_type["ipq5424"] = { "nor", "nand", "emmc", "norplusnand", "norplusemmc", "norplusnand-gpt", "norplusemmc-gpt" , "tiny-nor", "tiny-nor-debug" };
gpt_flash = ["nor-gpt", "emmc"]
soc_hw_versions = {}
soc_hw_versions["ipq5332"] = { 0x201A0100, 0x201A0101 };
soc_hw_versions["ipq5424"] = { 0xE0010100 };

#
# Python 2.6 and earlier did not have OrderedDict use the backport
# from ordereddict package. If that is not available report error.
#
try:
    from collections import OrderedDict
except ImportError:
    try:
        from ordereddict import OrderedDict
    except ImportError:
        print("error: this script requires the 'ordereddict' class.")
        print("Try 'pip install --user ordereddict'")
        print("Or  'easy_install --user ordereddict'")
        sys.exit(1)

def error(msg, ex=None):
    """Print an error message and exit.

    msg -- string, the message to print
    ex -- exception, the associate exception, if any
    """

    sys.stderr.write("pack: %s" % msg)
    if ex != None: sys.stderr.write(": %s" % str(ex))
    sys.stderr.write("\n")
    sys.exit(1)

FlashInfo = namedtuple("FlashInfo", "type pagesize blocksize chipsize")
ImageInfo = namedtuple("ProgInfo", "name filename type")
PartInfo = namedtuple("PartInfo", "name offset length which_flash")

def hdrobj_byte2str(gpthdr):
    temp_tuple = tuple(gpthdr)
    gpthdr=[]
    for x in temp_tuple:
        if isinstance(x, bytes):
            try:
                x = x.decode("utf-8")
            except:
                x = 0
        gpthdr.append(x)

    gpthdr = tuple(gpthdr)
    return gpthdr

class GPT(object):
    GPTheader = namedtuple("GPTheader", "signature revision header_size"
                            " crc32 current_lba backup_lba first_usable_lba"
                            " last_usable_lba disk_guid start_lba_part_entry"
                            " num_part_entry part_entry_size part_crc32")
    GPT_SIGNATURE = 'EFI PART'
    GPT_REVISION = '\x00\x00\x01\x00'
    GPT_HEADER_SIZE = 0x5C
    GPT_HEADER_FMT = "<8s4sLL4xQQQQ16sQLLL"

    GPTtable = namedtuple("GPTtable", "part_type unique_guid first_lba"
                           " last_lba attribute_flag part_name")
    GPT_TABLE_FMT = "<16s16sQQQ72s"

    def __init__(self, filename, flinfo):
        self.filename = filename
        self.pagesize = flinfo.pagesize
        self.blocksize = flinfo.blocksize
        self.chipsize = flinfo.chipsize
        self.__partitions = OrderedDict()
        self.ftype = flinfo.type

    def __validate_and_read_parts(self, part_fp):
        """Validate the GPT and read the partition"""
        part_fp.seek(self.blocksize, os.SEEK_SET)
        gptheader_str = part_fp.read(struct.calcsize(GPT.GPT_HEADER_FMT))
        gptheader = struct.unpack(GPT.GPT_HEADER_FMT, gptheader_str)
        gptheader = hdrobj_byte2str(gptheader)
        gptheader = GPT.GPTheader._make(gptheader)

        if gptheader.signature != GPT.GPT_SIGNATURE:
            error("Invalid signature")

        if gptheader.revision != GPT.GPT_REVISION:
            error("Unsupported GPT Revision")

        if gptheader.header_size != GPT.GPT_HEADER_SIZE:
            error("Invalid Header size")

        # Adding GPT partition info. This has to be flashed first.
        # GPT Header starts at LBA1 so (current_lba -1) will give the
        # starting of primary GPT.
        # blocksize will equal to gptheader.first_usuable_lba - current_lba + 1
        if self.ftype == "nor-gpt":
            name = "0:NORGPT"
        else:
            name = "0:GPT"
        block_start = gptheader.current_lba - 1
        block_count = gptheader.first_usable_lba - gptheader.current_lba + 1
        which_flash = 0
        part_info = PartInfo(name, block_start, block_count, which_flash)
        self.__partitions[name] = part_info

        part_fp.seek(2 * self.blocksize, os.SEEK_SET)

        for i in range(gptheader.num_part_entry):
            gpt_table_str = part_fp.read(struct.calcsize(GPT.GPT_TABLE_FMT))
            gpt_table = struct.unpack(GPT.GPT_TABLE_FMT, gpt_table_str)
            gpt_table = hdrobj_byte2str(gpt_table)
            gpt_table = GPT.GPTtable._make(gpt_table)

            block_start = gpt_table.first_lba
            block_count = gpt_table.last_lba - gpt_table.first_lba + 1

            if gpt_table.attribute_flag & (1 << 3):
                which_flash = 1
            else:
                which_flash = 0

            part_name = gpt_table.part_name.strip(chr(0))
            name = part_name.replace('\0','')
            part_info = PartInfo(name, block_start, block_count, which_flash)
            self.__partitions[name] = part_info

        # Adding the GPT Backup partition.
        # GPT header backup_lba gives block number where the GPT backup header will be.
        # GPT Backup header will start from offset of 32 blocks before
        # the GPTheader.backup_lba. Backup GPT size is 33 blocks.
        if self.ftype == "nor-gpt":
            name = "0:NORGPTBACKUP"
        else:
            name = "0:GPTBACKUP"
        block_start = gptheader.backup_lba - 32
        block_count = 33
        which_flash = 0
        part_info = PartInfo(name, block_start, block_count, which_flash)
        self.__partitions[name] = part_info

    def get_parts(self):
        """Returns a list of partitions present in the GPT."""

        try:
            with open(self.filename, "rb") as part_fp:
                self.__validate_and_read_parts(part_fp)
        except IOError as e:
            error("error opening %s" % self.filename, e)

        return self.__partitions

class MIBIB(object):
    Header = namedtuple("Header", "magic1 magic2 version age")
    HEADER_FMT = "<LLLL"
    HEADER_MAGIC1 = 0xFE569FAC
    HEADER_MAGIC2 = 0xCD7F127A
    HEADER_VERSION = 4

    Table = namedtuple("Table", "magic1 magic2 version numparts")
    TABLE_FMT = "<LLLL"
    TABLE_MAGIC1 = 0x55EE73AA
    TABLE_MAGIC2 = 0xE35EBDDB
    TABLE_VERSION_OTHERS = 4


    Entry = namedtuple("Entry", "name offset length"
                        " attr1 attr2 attr3 which_flash")
    ENTRY_FMT = "<16sLLBBBB"

    def __init__(self, filename, flinfo, nand_blocksize, nand_chipsize, root_part):
        self.filename = filename
        self.pagesize = flinfo.pagesize
        self.blocksize = flinfo.blocksize
        self.chipsize = flinfo.chipsize
        self.nand_blocksize = nand_blocksize
        self.nand_chipsize = nand_chipsize
        self.__partitions = OrderedDict()

    def __validate(self, part_fp):
        """Validate the MIBIB by checking for magic bytes."""

        mheader_str = part_fp.read(struct.calcsize(MIBIB.HEADER_FMT))
        mheader = struct.unpack(MIBIB.HEADER_FMT, mheader_str)
        mheader = hdrobj_byte2str(mheader)
        mheader = MIBIB.Header._make(mheader)

        if (mheader.magic1 != MIBIB.HEADER_MAGIC1
            or mheader.magic2 != MIBIB.HEADER_MAGIC2):
            """ mheader.magic1 = MIBIB.HEADER_MAGIC1
            mheader.magic2 = MIBIB.HEADER_MAGIC2 """
            error("invalid partition table, magic byte not present")

        if mheader.version != MIBIB.HEADER_VERSION:
            error("unsupport mibib version")

    def __read_parts(self, part_fp):
        """Read the partitions from the MIBIB."""
        global ARCH_NAME
        part_fp.seek(self.pagesize, os.SEEK_SET)
        mtable_str = part_fp.read(struct.calcsize(MIBIB.TABLE_FMT))
        mtable = struct.unpack(MIBIB.TABLE_FMT, mtable_str)
        mtable = hdrobj_byte2str(mtable)
        mtable = MIBIB.Table._make(mtable)

        if (mtable.magic1 != MIBIB.TABLE_MAGIC1
            or mtable.magic2 != MIBIB.TABLE_MAGIC2):
            """ mtable.magic1 = MIBIB.TABLE_MAGIC1
            mtable.magic2 = MIBIB.TABLE_MAGIC2 """
            error("invalid sys part. table, magic byte not present")

        if mtable.version != MIBIB.TABLE_VERSION_OTHERS:
            error("unsupported partition table version")

        for i in range(mtable.numparts):
            mentry_str = part_fp.read(struct.calcsize(MIBIB.ENTRY_FMT))
            mentry = struct.unpack(MIBIB.ENTRY_FMT, mentry_str)
            mentry = hdrobj_byte2str(mentry)
            mentry = MIBIB.Entry._make(mentry)
            self.flash_flag = self.blocksize
            self.chip_flag = self.chipsize

            if mentry.which_flash != 0:
                self.flash_flag = self.nand_blocksize
                self.chip_flag = self.nand_chipsize

            byte_offset = mentry.offset * self.flash_flag

            if mentry.length == 0xFFFFFFFF:
                byte_length = self.chip_flag - byte_offset
            else:
                byte_length = mentry.length * self.flash_flag

            part_name = mentry.name.strip(chr(0))
            part_info = PartInfo(part_name, byte_offset, byte_length, mentry.which_flash)
            self.__partitions[part_name] = part_info

    def get_parts(self):
        """Returns a list of partitions present in the MIBIB. CE """

        try:
            with open(self.filename, "rb") as part_fp:
                self.__validate(part_fp)
                self.__read_parts(part_fp)
        except IOError as e:
            error("error opening %s" % self.filename, e)

        return self.__partitions

class FlashScript(object):
    """Base class for creating flash scripts."""

    def __init__(self, flinfo):
        self.pagesize = flinfo.pagesize
        self.blocksize = flinfo.blocksize
        self.script = []
        self.parts = []
        self.curr_stdout = "serial"
        self.activity = None
        self.flash_type = flinfo.type

    def append(self, cmd, fatal=True):
        """Add a command to the script.

        Add additional code, to terminate on error. This can be
        supressed by passing 'fatal' as False.
        """

        if fatal:
            self.script.append(cmd + ' || exit 1\n')
        else:
            self.script.append(cmd + "\n")

    def dumps(self):
        """Return the created script as a string."""
        return "".join(self.script)

    def redirect(self, dev):
        """Generate code, to redirect command output to a device."""

        if self.curr_stdout == dev:
            return

        self.append("setenv stdout %s" % dev, fatal=False)
        self.curr_stdout = dev

    def imxtract_n_flash(self, part, part_name):
        """Generate code, to extract image location, from a multi-image blob
        and flash it.

        part -- string, name of the sub-image
        part_name -- string, partition name
        """
        self.append("xtract_n_flash $imgaddr %s %s" % (part, part_name))

    def erase_partition(self, part_name):
        """Generate code, to erase a partition.

        part_name -- string, partition name
        """
        self.append("flerase %s" % (part_name))

    def echo(self, msg, nl=True, verbose=False):
        """Generate code, to print a message.

        nl -- bool, indicates whether newline is to be printed
        verbose -- bool, indicates whether printing in verbose mode
        """

        if not verbose:
            self.redirect("serial")

        if nl:
            self.append("echo %s" % msg, fatal=False)
        else:
            self.append("echo %s%s" % (r"\\c", msg), fatal=False)

        if not verbose:
            self.redirect("nulldev")

    def end(self):
        """Generate code, to indicate successful completion of script."""

        self.append("exit 0\n", fatal=False)

    def start_if(self, var, value):
        """Generate code, to check an environment variable.

        var -- string, variable to check
        value -- string, the value to compare with
        """

        self.append('if test "$%s" = "%s"; then\n' % (var, value),
                    fatal=False)

    def start_if_or(self, var, val_list):
        """Generate code, to check an environment variable.

        var -- string, variable to check
        value -- string, the list of values to compare with
        """

        n_val = len(val_list)
        item = 1
        cmd_str = "if "
        for val in val_list:
            cmd_str = cmd_str + str('test "$%s" = "%s"' % (var, val))
            #cmd_str = cmd_str + "\"$" + var + "\"" + "=" + "\"" + val + "\""
            if item <= (n_val - 1):
                cmd_str = cmd_str + " || "
            item = item + 1

        self.append('%s; then\n' % cmd_str, fatal=False)

    def end_if(self):
        """Generate code, to end if statement."""

        self.append('fi\n', fatal=False)

its_tmpl = Template("""
/dts-v1/;

/ {
        description = "${desc}";
        images {
${images}
        };
};
""")

its_image_tmpl = Template("""
                ${name} {
                        description = "${desc}";
                        data = /incbin/("./${fname}");
                        type = "${imtype}";
                        arch = "arm";
                        compression = "none";
                        hash@1 { algo = "crc32"; };
                };
""")

def sha1(message):
    """Returns SHA1 digest in hex format of the message."""

    m = hashlib.sha1()
    m.update(message.encode('utf-8'))
    return m.hexdigest()

class Pack(object):
    """Class to create a flashable, multi-image blob.

    Combine multiple images present in a directory, and generate a
    U-Boot script to flash the images.
    """

    def __init__(self):
        self.flinfo = None
        self.images_dname = None
        self.partitions = {}

        self.fconf_fname = None
        self.scr_fname = None
        self.its_fname = None
        self.img_fname = None
        self.emmc_page_size = 512
        self.emmc_block_size = 512
        self.nor_gpt_page_size = 4096
        self.nor_gpt_block_size = 4096

    def __get_machid(self, section):
        """Get the machid for a section.

        info -- ConfigParser object, containing image flashing info
        section -- section to retreive the machid from
        """
        try:
            machid = int(section.find(".//machid").text, 0)
            machid = "%x" % machid
        except ValueError as e:
            error("invalid value for machid, should be integer")

        return machid

    def __get_img_size(self, filename):
        """Get the size of the image to be flashed

        filaneme -- string, filename of the image to be flashed
        """

        if filename.lower() == "none":
            return 0
        try:
            return getsize(os.path.join(self.images_dname, filename))
        except OSError as e:
            error("error getting image size '%s'" % filename, e)

    def __get_part_info(self, partition):
        """Return partition info for the specified partition.

        partition -- string, partition name
        """
        try:
            return self.partitions[partition]
        except KeyError as e:
            return None

    def mibib_reload(self, filename, partition, flinfo, script):

        img_size = self.__get_img_size(filename)
        part_info = self.__get_part_info(partition)

        section_label = partition.split(":")
        if len(section_label) != 1:
            section_conf = section_label[1]
        else:
            section_conf = section_label[0]

        section_conf = section_conf.lower()

        if img_size > 0:
            script.append("imxtract $imgaddr %s" % (section_conf + "-" + sha1(filename)))

        fl_type = 0 if self.flinfo.type == 'nand' else 1
        script.append("mibib_reload %x %x %x %x" % (fl_type, flinfo.pagesize, flinfo.blocksize,
                          flinfo.chipsize))

        return 1

    def __mkimage(self, images):
        """Create the multi-image blob.

        images -- list of ImageInfo, containing images to be part of the blob
        """
        try:
            its_fp = open(self.its_fname, "wb")
        except IOError as e:
            error("error opening its file '%s'" % self.its_fname, e)

        desc = "Flashing %s %x %x"
        desc = desc % (self.flinfo.type, self.flinfo.pagesize,
                       self.flinfo.blocksize)

        image_data = []
        for (section, fname, imtype) in images:
            fname = fname.replace("\\", "\\\\")
            subs = dict(name=section, desc=fname, fname=fname, imtype=imtype)
            image_data.append(its_image_tmpl.substitute(subs))

        image_data = "".join(image_data)
        its_data = its_tmpl.substitute(desc=desc, images=image_data)

        if sys.version_info.major >= 3:
            its_data = bytes(its_data, 'utf-8')
        its_fp.write(its_data)
        its_fp.close()

        try:
            cmd = [SRC_DIR + "/mkimage", "-f", self.its_fname, self.img_fname]
            ret = subprocess.call(cmd)
            if ret != 0:
                print(ret)
                error("failed to create u-boot image from script")
        except OSError as e:
            error("error executing mkimage", e)

    def __create_fnames(self):
        """Populate the filenames."""

        self.scr_fname = os.path.join(self.images_dname, "flash.scr")
        self.its_fname = os.path.join(self.images_dname, "flash.its")

    def __gen_machid_flash_script(self, machid_map, images):

        for machid, part_img in machid_map.items():
            script_name = os.path.join(self.images_dname, "flash_" + machid + ".scr")
            script_fp = open(script_name, "w")
            flinfo = part_img["flinfo"]
            self.flinfo = flinfo
            script = FlashScript(flinfo)

            print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, part_img["part_info"])
            # For non-apps image, load mibib for mibib based partition configs
            if image_type == "all":
                if flinfo.type != "nor-gpt" and flinfo.type != "emmc":
                    for pinfo in part_img["part_info"]:
                        if pinfo[0] == "0:MIBIB":
                            self.mibib_reload(pinfo[1], pinfo[0], flinfo, script)

            current_pftype = None

            for pinfo in part_img["part_info"]:
                pname = pinfo[0]
                fname = pinfo[1]
                pftype = pinfo[3]
                pre_cmd_list = pinfo[4]
                post_cmd_list = pinfo[5]
                erase_only = pinfo[6]

                if (erase_only == 'true'):
                    script.erase_partition(pname)

                if fname == "":
                    continue
                else:
                    section_conf = pname.lower()
                    section_conf = section_conf.replace("0:","")

                    if ARCH_NAME == "ipq5332":
                        if section_conf == "qsee":
                            section_conf = "tz"
                        elif section_conf == "cdt":
                            section_conf = "ddr" + fname[3:-4]
                        elif section_conf == "bootconfig" or  section_conf == "bootconfig1":
                            section_conf = fname[:-4]
                        elif section_conf == "appsbl":
                            section_conf = "u-boot"
                        elif section_conf == "rootfs" and self.flash_type in ["nand", "nand-4k", "norplusnand", "norplusnand-4k"]:
                            section_conf = "ubi"
                        elif section_conf == "wifi_fw" or section_conf == "wififw":
                            section_conf = fname[:-13]
                    else:
                        if section_conf == "rootfs" and self.flash_type in ["nand", "nand-4k", "norplusnand", "norplusnand-4k", "norplusnand-gpt", "norplusnand-4k-gpt"]:
                            section_conf = "ubi"

                    section_name = section_conf + "-" + sha1(fname)

                    # Identify the change in flashtype and do flash update
                    if current_pftype != pftype:
                        if pftype == "emmc":
                            if ARCH_NAME != "ipq5332" and flayout == "default":
                                script.append("switch_to_user")
                                script.append("mmc partconf 0 0 0 0")
                            script.append("flupdate set mmc")
                        elif pftype == "nor-gpt":
                            script.append("flupdate set nor-gpt")

                        current_pftype = pftype

                    for cmd in pre_cmd_list:
                        script.append(cmd)

                    for cmd in post_cmd_list:
                        script.append(cmd)

                    for img_info in images:
                         if fname == img_info.filename:
                             section_name = img_info.name

                    image_info = ImageInfo(section_name, fname, "firmware")
                    if fname.lower() != "none":
                        if image_info not in images:
                            images.append(image_info)

                    script.imxtract_n_flash(section_name, pname)

                    print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, section_name, pname)

            if current_pftype == "emmc" or current_pftype == "nor-gpt":
                script.append("flupdate clear")

            script.end()

            try:
                script_fp.write(script.dumps())
            except IOError as e:
                error("error writing to script '%s'" % script_fp.name, e)

            script_fp.close()
            print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, script_name + " script generated")

        return 0

    def __gen_main_flash_script(self, machid_map, images):
        script_fp = open(self.scr_fname, "w")
        flinfo = FlashInfo("dummy", 0, 0, 0)
        script = FlashScript(flinfo)

        chip_count = 0
        for soc_hw_version in soc_hw_versions[ARCH_NAME]:
            if skip_test:
                break;
            chip_count = chip_count + 1
            if chip_count == 1:
                script.script.append('if test -n $soc_hw_version')
                script.script.append('; then\n')
                script.script.append('if test "$soc_hw_version" = "%x" ' % soc_hw_version)
            else:
                script.script.append('|| test "$soc_hw_version" = "%x" ' % soc_hw_version)
        if chip_count >= 1:
            script.script.append('; then\n')
            script.script.append('echo \'soc_hw_version : Validation success\'\n')
            script.script.append('else\n')
            script.script.append('echo \'soc_hw_version : did not match, aborting upgrade\'\n')
            script.script.append('exit 1\n')
            script.script.append('fi\n')
            script.script.append('else\n')
            script.script.append('echo \'soc_hw_version : unknown, skipping validation\'\n')
            script.script.append('fi\n')

        if skip_test == False:
            machid_count = 0
            for machid in machid_map:
                machid_count =  machid_count + 1
                if machid_count == 1:
                    script.script.append('if test "$machid" = "%s" ' % machid)
                else:
                    script.script.append('|| test "$machid" = "%s" ' % machid)
            if machid_count >= 1:
                script.script.append('; then\n')
                script.script.append('echo \'machid : Validation success\'\n')
                script.script.append('else\n')
                script.script.append('echo \'machid : unknown, aborting upgrade\'\n')
                script.script.append('exit 1\n')
                script.script.append('fi\n')

        script.script.append('source $imgaddr:script_$machid\n')

        for machid in machid_map:
            fname = "flash_" + machid + ".scr"
            section_name = "script_" + machid

            image_info = ImageInfo(section_name, fname, "script")
            if fname.lower() != "none":
                if image_info not in images:
                    images.insert(0, image_info)

            print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, section_name)

        script.end()

        try:
            script_fp.write(script.dumps())
        except IOError as e:
            error("error writing to script '%s'" % script_fp.name, e)

        script_fp.close()
        print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, "script generated")
        return 1

    def __ubi_cfg_parser(self, ubi_cfg_fname, ubi_vol_info):
        ubi_cfg_file = open(ubi_cfg_fname, 'r')

        vol_found = False
        print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno)
        while True:
            line = ubi_cfg_file.readline()
            if not line:
                break

            tmp = line.strip()
            if tmp == '':
                continue

            if tmp[0] == '[' and tmp[-1] == ']':
                vol_info = {}
                while True:
                    line = ubi_cfg_file.readline()
                    if not line:
                        break

                    tmp = line.strip()
                    if "vol_name=" in tmp:
                        vol_info["vol_name"] = tmp.lstrip("vol_name=")
                    elif "vol_size=" in tmp:
                        vol_info["vol_size"] = tmp.lstrip("vol_size=")
                    elif "vol_type=" in tmp:
                        vol_info["vol_type"] = tmp.lstrip("vol_type=")
                        if vol_info["vol_type"] == "dynamic":
                            vol_info["vol_size"] = "dynamic"

                    if len(vol_info) == 3:
                            if vol_info["vol_type"] == "dynamic":
                                vol_info["vol_size"] = "dynamic"

                            if "iB" in vol_info["vol_size"]:
                                size = int(vol_info["vol_size"][0:-3])
                                if vol_info["vol_size"][-3] == 'M':
                                    size = size * 1024 * 1024
                                elif vol_info["vol_size"][-3] == 'K':
                                    size = size * 1024

                                vol_info["vol_size"] = size

                            ubi_vol_info.append(vol_info)
                            break;

        print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, ubi_vol_info)
        ubi_cfg_file.close()
        return 0

    def __process_board_flash_gpt(self, ftype, images, root):
        """Extract board info from config and generate the flash script.

        ftype -- string, flash type 'emmc'
        board_section -- string, board section in config file
        machid -- string, board machine ID in hex format
        images -- list of ImageInfo, append images used by the board here
        """

        erase_only = "false"
        if "nand" in ftype:
            if "4k" in ftype:
                list_entry = ".//data[@type='NORPLUSNAND-GPT_PARAMETER']/entry[@type='4k']"
            else:
                list_entry = ".//data[@type='NORPLUSNAND-GPT_PARAMETER']/entry[@type='2k']"
        else:
            list_entry = ".//data[@type='" + self.flash_type.upper() + "_PARAMETER']/entry"

        print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, list_entry)
        entries = root.findall(list_entry)
        for layout_entry in entries:
            layout = layout_entry.get('layout')
            if layout == "default":
                layout_name = ""
            else:
                layout_name = "-" + layout

            part_entries = layout_entry.findall(".//entry")
            for part_info in part_entries:
                gpt_type = part_info.get('gpt_type')
                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, layout, gpt_type)
                if gpt_type == None:
                    gpt_type = ""
                    part_ref = ".//physical_partition[@ref='" + ftype + "']/partition"
                else:
                    part_ref = ".//physical_partition[@ref='" + gpt_type + "']/partition"
                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, part_ref)

                if ftype in [ "norplusnand-gpt" , "norplusnand-4k-gpt" , "norplusemmc-gpt" ]:
                    pagesize = self.nor_gpt_page_size
                    blocksize = self.nor_gpt_block_size
                    part_file = SRC_DIR + "/" + ARCH_NAME + "/flash_partition/nor-gpt-partition" + layout_name + ".xml"
                    ftype = "nor-gpt"
                elif ftype == "norplusemmc":
                    pagesize = int(part_info.find(".//page_size_flash").text)
                    blocksize = self.emmc_block_size
                    part_file = SRC_DIR + "/" + ARCH_NAME + "/flash_partition/sec-emmc-partition" + layout_name + ".xml"
                    ftype = "emmc"
                else:
                    pagesize = self.emmc_page_size
                    blocksize = self.emmc_block_size
                    part_file = SRC_DIR + "/" + ARCH_NAME + "/flash_partition/emmc-partition" + layout_name + ".xml"

                chipsize = int(part_info.find(".//total_block").text)

                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, part_file, self.flash_type, part_file)
                part_xml = ET.parse(part_file)

                flinfo = FlashInfo(ftype, pagesize, blocksize, chipsize)

                parts = part_xml.findall(part_ref)
                parts_length = len(parts)
                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, parts_length)

                try:
                    part_img_map = images[layout]["part_info"]
                    print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, images[layout])
                except KeyError as e:
                    images[layout] = dict()
                    part_img_map = []

                part_fname = part_info.find('partition_mbn').text
                psize = str(int(34*flinfo.blocksize))
                pre_cmd_hook = []
                post_cmd_hook = []
                if image_type == "all":
                    if ftype == "emmc":
                        if gpt_type != "":
                            command = {
                                    "user" : "user",
                                    "boot0": "boot 0",
                                    "boot1": "boot 1",
                                    "gpp0" : "user 0",
                                    "gpp1" : "user 1",
                                    "gpp2" : "user 2",
                                    "gpp3" : "user 3"
                                    }

                            pre_cmd_hook = [ "switch_to_" + command[gpt_type] ]
                        pname = "0:GPT"
                    else:
                        pname = "0:NORGPT"
                    part_img_map.append([pname, part_fname, psize, ftype, pre_cmd_hook, post_cmd_hook, erase_only])

                    psize = str(int(33*flinfo.blocksize))
                    pre_cmd_hook = []
                    post_cmd_hook = []
                    if ftype == "emmc":
                        pname = "0:GPTBACKUP"
                    else:
                        pname = "0:NORGPTBACKUP"
                    part_img_map.append([pname, part_info.find('partition_mbn_backup').text, psize, ftype, pre_cmd_hook, post_cmd_hook, erase_only])

                part_fname = os.path.join(self.images_dname, part_fname)
                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, part_fname, flinfo)
                gpt = GPT(part_fname, flinfo)
                self.partitions = gpt.get_parts()

                for index in range(parts_length):
                    pre_cmd_hook = []
                    post_cmd_hook = []
                    partition = parts[index]
                    pname = partition.attrib['label']

                    if (image_type != "all"):
                        try:
                            img_type = partition.attrib['image_type']
                            if (img_type != image_type):
                                continue
                        except KeyError as e:
                            continue

                    try:
                        fname = partition.attrib['filename']
                    except KeyError as e:
                        fname = partition.attrib['filename_' + MODE]

                    if("erase-only" in partition.attrib) :
                        erase_only = partition.attrib['erase-only']
                    else :
                        erase_only = "false"

                    pinfo = self.__get_part_info(pname)
                    psize = pinfo.length * flinfo.blocksize
                    ptype = ftype
                    if ftype == "nor-gpt" and pinfo.which_flash == 1:
                        ptype = "nand"

                    part_img_map.append([pname, fname, psize, ptype, pre_cmd_hook, post_cmd_hook, erase_only])
                    print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, pname, fname, psize, pinfo.which_flash, ptype, erase_only)

                    try:
                        if ptype == "nand" and pname == "rootfs":
                            MODE_APPEND = "_64" if MODE == "64" else ""
                            if memory_size == "default":
                                profile_suffix = ""
                            else:
                                profile_suffix = "-" + memory_size

                            UBINIZE_SRC_CFG_NAME = SRC_DIR + "/" + ARCH_NAME + "/flash_partition/" + ARCH_NAME + "-ubinize" + MODE_APPEND + layout_name + profile_suffix + ".cfg"
                            if (os.path.isfile(UBINIZE_SRC_CFG_NAME) == False):
                                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, UBINIZE_SRC_CFG_NAME, "is not found")
                                UBINIZE_SRC_CFG_NAME = SRC_DIR + "/" + ARCH_NAME + "/flash_partition/" + ARCH_NAME + "-ubinize" + MODE_APPEND + layout_name + ".cfg"

                            if (os.path.isfile(UBINIZE_SRC_CFG_NAME) == False):
                                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, UBINIZE_SRC_CFG_NAME, "is not found", "so skipping ubi cfg parsing")
                            else:
                                ubi_volumes = []
                                self.__ubi_cfg_parser(UBINIZE_SRC_CFG_NAME, ubi_volumes)
                                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, ubi_volumes)

                                for vol_info in ubi_volumes:
                                    if vol_info["vol_type"] == "dynamic":
                                        size = "dynamic"
                                    else:
                                        size = vol_info["vol_size"]
                                    part_img_map.append([vol_info["vol_name"], "", size, ptype, pre_cmd_hook, post_cmd_hook, erase_only])
                    except KeyError as e:
                        pass

                if self.flash_type != "norplusemmc":
                    images[layout] = dict()
                    images[layout]["part_info"] = part_img_map
                    images[layout]["flinfo"] = flinfo
                print(part_img_map)

        return 1

    def __process_board_flash(self, ftype, images, root):
        global SRC_DIR
        global ARCH_NAME
        global MODE

        # pick corresponding flash type node params from config.xml
        if ftype in [ "nand" , "nand-4k" ]:
            list_entry = ".//data[@type='NAND_PARAMETER']/entry"
        elif ftype in [ "norplusnand" , "norplusnand-4k" ]:
            list_entry = ".//data[@type='NORPLUSNAND_PARAMETER']/entry"
        elif ftype in [ "tiny-nor" , "tiny-nor-debug" ]:
            list_entry = ".//data[@type='TINY_NOR_PARAMETER']/entry"
        else:
            list_entry = ".//data[@type='" + self.flash_type.upper() + "_PARAMETER']/entry"

        if ftype in [ "nand-4k" , "norplusnand-4k" ]:
            ntype = "4k"
        elif ftype in [ "nand" , "norplusnand" ]:
            ntype = "2k"

        print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, list_entry)
        entries = root.findall(list_entry)
        for part_info in entries:
            if ftype in [ "nand" , "nand-4k" , "norplusnand" , "norplusnand-4k" ]:
                nand_type = part_info.get('type')
                if nand_type == None:
                    continue
                else:
                    if nand_type != ntype:
                        continue

                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, nand_type)

            layout = part_info.get('layout')
            if layout == "default":
                layout_name = ""
            else:
                layout_name = "-" + layout

            part_file = SRC_DIR + "/" + ARCH_NAME + "/flash_partition/" + ftype + "-partition"+ layout_name +".xml"
            print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, ftype, part_file)
            part_xml = ET.parse(part_file)

            # parse primary flash params from flashtype node
            pagesize = int(part_info.find(".//page_size").text)
            pages_per_block = int(part_info.find(".//pages_per_block").text)
            blocks_per_chip = int(part_info.find(".//total_block").text)

            blocksize = pages_per_block * pagesize
            chipsize = blocks_per_chip * blocksize

            if ftype in ["tiny-nor", "norplusnand", "norplusnand-4k", "norplusemmc", "tiny-nor-debug"]:
                flinfo = FlashInfo("nor", pagesize, blocksize, chipsize)
            else:
                flinfo = FlashInfo("nand", pagesize, blocksize, chipsize)

            print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, flinfo)

            parts = part_xml.findall(".//partitions/partition")
            parts_length = len(parts)

            print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, parts_length)

            # try getting part layout for nor plus comination, if not available create one via exception
            try:
                part_img_map = images[layout]["part_info"]
                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, images[layout]["part_info"])
            except KeyError as e:
                images[layout] = dict()
                part_img_map = []

            # identify the mibib binary for the choosed partition layout
            partition = part_xml.find(".//partitions/partition[name='0:MIBIB']")
            part_fname = partition.findall('img_name')[0].text
            part_fname = os.path.join(self.images_dname, part_fname)
            print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, part_fname, part_file, self.flash_type)

            if self.flash_type in [ "norplusnand", "norplusnand-4k" ]:
                # parse secondary flash params from flashtype node
                nand_pagesize = int(part_info.find(".//nand_page_size").text)
                nand_pages_per_block = int(part_info.find(".//nand_pages_per_block").text)
                nand_blocks_per_chip = int(part_info.find(".//nand_total_block").text)

                nand_blocksize = nand_pages_per_block * nand_pagesize
                nand_chipsize = nand_blocks_per_chip * nand_blocksize

                mibib = MIBIB(part_fname, flinfo, nand_blocksize, nand_chipsize, part_xml)
            else:
                mibib = MIBIB(part_fname, flinfo, blocksize, chipsize, part_xml)

            self.partitions = mibib.get_parts()

            for index in range(parts_length):
                pre_cmd_hook_list = []
                post_cmd_hook_list = []
                partition = parts[index]

                # skip non-hlos partitions incase apps image
                if (image_type != "all"):
                    i_type = partition.findall('image_type')
                    if len(i_type) == 0:
                        continue

                # parse part_name, part_size, part_type, fw_img infos
                pname = partition.findall('name')[0].text
                if ('erase-only' in partition.findall('name')[0].attrib) :
                    erase_only = partition.findall('name')[0].attrib['erase-only']
                else :
                    erase_only = "false"

                pinfo = self.__get_part_info(pname)
                psize = pinfo.length
                if ftype in [ "nand" , "nand-4k" ] or pinfo.which_flash == 1:
                    ptype = "nand"
                else:
                    ptype = "nor"

                fnames = partition.findall('img_name')
                if len(fnames) == 0:
                    fname = ""
                else:
                    try:
                        if fnames[0].attrib['mode'] != MODE:
                            fname = fnames[1].text
                        else:
                            fname = fnames[0].text
                    except KeyError as e:
                        fname = partition.findall('img_name')[0].text
                        pass

                part_img_map.append([pname, fname, psize, ptype, pre_cmd_hook_list, post_cmd_hook_list, erase_only])

                # incase of nand rootfs partition, parse ubi volumes from ubinize config add those as partition
                if ptype == "nand" and pname == "rootfs":
                    MODE_APPEND = "_64" if MODE == "64" else ""
                    if memory_size == "default":
                        profile_suffix = ""
                    else:
                        profile_suffix = "-" + memory_size

                    UBINIZE_SRC_CFG_NAME = SRC_DIR + "/" + ARCH_NAME + "/flash_partition/" + ARCH_NAME + "-ubinize" + MODE_APPEND + layout_name + profile_suffix + ".cfg"
                    if (os.path.isfile(UBINIZE_SRC_CFG_NAME) == False):
                        print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, UBINIZE_SRC_CFG_NAME, "is not found")
                        UBINIZE_SRC_CFG_NAME = SRC_DIR + "/" + ARCH_NAME + "/flash_partition/" + ARCH_NAME + "-ubinize" + MODE_APPEND + layout_name + ".cfg"

                    if (os.path.isfile(UBINIZE_SRC_CFG_NAME) == False):
                        print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, UBINIZE_SRC_CFG_NAME, "is not found", "so skipping ubi cfg parsing")
                    else:
                        ubi_volumes = []
                        self.__ubi_cfg_parser(UBINIZE_SRC_CFG_NAME, ubi_volumes)
                        print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, ubi_volumes)

                        for vol_info in ubi_volumes:
                            pre_cmd_hook_list = []
                            post_cmd_hook_list = []
                            if vol_info["vol_type"] == "dynamic":
                                size = "dynamic"
                            else:
                                size = vol_info["vol_size"]
                            part_img_map.append([vol_info["vol_name"], "", size, ptype, pre_cmd_hook_list, post_cmd_hook_list, erase_only])

            images[layout]["part_info"] = part_img_map
            images[layout]["flinfo"] = flinfo
            print(part_img_map)

        return 1

    def __process_board(self, images, root):
        try:
            print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, self.flash_type)
            if self.flash_type in [ "nand", "nand-4k", "nor", "tiny-nor", "norplusnand", "norplusnand-4k", "tiny-nor-debug" ]:
                ret = self.__process_board_flash(self.flash_type, images, root)
            elif self.flash_type == "emmc":
                ret = self.__process_board_flash_gpt(self.flash_type, images, root)
            elif self.flash_type in [ "norplusnand-gpt", "norplusemmc-gpt", "norplusnand-4k-gpt"]:
                ret = self.__process_board_flash_gpt(self.flash_type, images, root)
                if self.flash_type == "norplusemmc-gpt" and ret:
                    self.flash_type = "norplusemmc"
                    ret = self.__process_board_flash_gpt("norplusemmc", images, root)
            elif self.flash_type == "norplusemmc":
                ret = self.__process_board_flash("norplusemmc", images, root)
                if ret:
                    ret = self.__process_board_flash_gpt("norplusemmc", images, root)
            return ret
        except ValueError as e:
            error("error getting board info in section '%s'" % board_section.find('machid').text, e)

    def __process_machid_board(self, images, id_map, root):
        entries = root.findall(".//data[@type='MACH_ID_BOARD_MAP']/entry")
        for segment in entries:
            override_cfg = None

            # get machid from RDP entry
            machid = int(segment.find(".//machid").text, 0)
            machid = "%x" % machid

            # get support layout list from RDP entry
            # and check whether it supports the requested layout, if not skip this RDP
            supported_layouts = segment.find('.//layouts')
            if (supported_layouts == None):
                continue
            else:
                supported_layouts = supported_layouts.text.split(",")
                if flayout not in supported_layouts:
                    continue

            print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, memory_size, supported_layouts)

            # get fw_override config, if no fw_override, use default fw_imgs
            fw_override = segment.find('.//fw_override')
            if fw_override != None:

                # get fw_override config for the requested memory profile, if no fw_override, use default fw_imgs
                override_list = fw_override.findall('.//profile-'+ memory_size)
                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, memory_size, override_list, len(override_list))

                if len(override_list) != 0:
                    for override in override_list:
                        layouts = override.get('layouts')
                        if layouts == None:
                            continue
                        else:
                            layouts = layouts.split(",")

                        print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, flayout, layouts)
                        if flayout not in layouts:
                            continue

                        override_cfg = override
                        break
                else:
                    print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, "skippping" + machid + "RDP")
                    continue

            try:
                part_img_list = copy.deepcopy(images[flayout]["part_info"])
            except KeyError as e:
                continue

            print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, machid, override_cfg)
            for part in part_img_list:
                pname = part[0]
                fname = part[1]
                psize = part[2]
                ptype = part[3]


                # get fw_override image name
                if override_cfg != None:
                    tag_name = pname.lower()
                    tag_name = tag_name.replace("0:","")

                    if ptype == "nand":
                        ubi_tag_name = {
                                "rootfs" : "ubifs",
                                "kernel" : "hlos",
                                "ubi_rootfs" : "rootfs",
                                "wifi_fw" : "wififw",
                                "rootfs_1" : "ubifs_1",
                                }

                        if tag_name in ubi_tag_name.keys():
                            tag_name = ubi_tag_name[tag_name]

                    tag_list = override_cfg.findall(".//" + tag_name)
                    print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, tag_name, tag_list)

                    if tag_list != None:
                        for tag in tag_list:
                            tag_ftype = tag.get("flash")
                            tag_mode = tag.get("mode")

                            if tag_ftype != None and tag_mode != None:
                                tag_ftype = tag_ftype.split(",")
                                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, tag_ftype, tag_mode, self.flash_type, MODE)
                                if self.flash_type in tag_ftype and tag_mode == MODE:
                                    fname = tag.text
                                    break
                            else:
                                fname = tag.text
                                break

                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, fname)
                if fname == "":
                    continue

                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, os.path.join(self.images_dname, fname))
                if os.path.isfile(os.path.join(self.images_dname, fname)) == False:
                    print("file '%s' is not exist " % fname)
                    return 1

                img_size = self.__get_img_size(fname)
                if psize != "dynamic" and img_size > int(psize):
                    print("img size is larger than part. len in '%s'" % pname)
                    return 1

                part[1] = fname
                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, part)

            id_map[machid] = { "part_info" : part_img_list , "flinfo" : images[flayout]["flinfo"] }

        return 0

    def gen_ubi_root_files(self, ftype, root):
        global SRC_DIR
        global ARCH_NAME
        global MODE

        if self.flash_type in [ "nand" , "norplusnand" , "norplusnand-gpt"]:
            nand_type = "2k"
        else:
            nand_type = "4k"

        ftype = self.flash_type

        print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, nand_type)
        if self.flash_type in ["nand", "nand-4k"]:
            list_entry = ".//data[@type='NAND_PARAMETER']/entry"
        elif self.flash_type in ["norplusnand", "norplusnand-4k"]:
            list_entry = ".//data[@type='NORPLUSNAND_PARAMETER']/entry"
        else:
            ftype = "nor-gpt"
            list_entry = ".//data[@type='NORPLUSNAND-GPT_PARAMETER']/entry"

        entries = root.findall(list_entry)
        for nand_param in entries:
            if (nand_type != nand_param.get('type')):
                continue

            MODE_APPEND = "_64" if MODE == "64" else ""

            nand_layout = nand_param.get('layout')
            if nand_layout == "default":
                layout_name = ""
            else:
                layout_name = "-" + nand_layout

            if memory_size == "default":
                profile_suffix = ""
            else:
                profile_suffix = "-" + memory_size

            UBINIZE_SRC_CFG_NAME = SRC_DIR + "/" + ARCH_NAME + "/flash_partition/" + ARCH_NAME + "-ubinize" + MODE_APPEND + layout_name + profile_suffix + ".cfg"
            if (os.path.isfile(UBINIZE_SRC_CFG_NAME) == False):
                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, UBINIZE_SRC_CFG_NAME, "is not found")
                UBINIZE_SRC_CFG_NAME = SRC_DIR + "/" + ARCH_NAME + "/flash_partition/" + ARCH_NAME + "-ubinize" + MODE_APPEND + layout_name + ".cfg"
                if (os.path.isfile(UBINIZE_SRC_CFG_NAME) == False):
                    print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, UBINIZE_SRC_CFG_NAME, "is not found", "so skipping ubi root generation")
                    continue

            print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, UBINIZE_SRC_CFG_NAME, nand_type)

            f1 = open(UBINIZE_SRC_CFG_NAME, 'r')
            UBINIZE_CFG_NAME = SRC_DIR + "/" + ARCH_NAME + "/flash_partition/tmp-ubinize.cfg"
            f2 = open(UBINIZE_CFG_NAME, 'w')
            for line in f1:
                f2.write(line.replace('image=', "image=" + SRC_DIR + "/"))
            f1.close()
            f2.close()

            part_file = SRC_DIR + "/" + ARCH_NAME + "/flash_partition/" + ftype + "-partition"+ layout_name +".xml"
            print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, part_file)

            if self.flash_type in [ "nand", "nand-4k", "norplusnand", "norplusnand-4k" ]:
                parts = ET.parse(part_file).findall('.//partitions/partition')
                for index in range(len(parts)):
                    section = parts[index]
                    if section[0].text == "rootfs":
                        fnames = section.findall('img_name')
                        rootfs_pos = 9 if MODE == "64" else 8
                        UBI_IMG_NAME = section[rootfs_pos].text
            else:
                part_xml = ET.parse(part_file)
                part_ref = ".//physical_partition[@ref='" + self.flash_type + layout_name + "']/partition[@label='rootfs']"
                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, part_ref)
                partition = part_xml.find(part_ref)
                if partition != None:
                    try:
                        UBI_IMG_NAME = partition.attrib['filename']
                    except KeyError as e:
                        UBI_IMG_NAME = partition.attrib['filename_' + MODE]
                    print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, UBI_IMG_NAME)
                else:
                    return 1

            if self.flash_type in ["nand-4k", "norplusnand-4k", "norplusnand-4k-gpt"]:
                cmd = '%s -m 4096 -p 256KiB -o root.ubi %s' % ((SRC_DIR + "/ubinize") ,UBINIZE_CFG_NAME)
                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, cmd)
                ret = subprocess.call(cmd, shell=True)
                if ret != 0:
                    error("ubinization got failed")
                cmd = 'dd if=root.ubi of=%s bs=4k conv=sync' % (SRC_DIR + "/" + UBI_IMG_NAME)
                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, cmd)
                ret = subprocess.call(cmd, shell=True)
                if ret != 0:
                    error("ubi image copy operation failed")

            elif self.flash_type in ["nand", "norplusnand", "norplusnand-gpt"]:
                cmd = '%s -m 2048 -p 128KiB -o root.ubi %s' % ((SRC_DIR + "/ubinize") ,UBINIZE_CFG_NAME)
                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, cmd)
                ret = subprocess.call(cmd, shell=True)
                if ret != 0:
                    error("ubinization got failed")
                cmd = 'dd if=root.ubi of=%s bs=2k conv=sync' % (SRC_DIR + "/" + UBI_IMG_NAME)
                print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, cmd)
                ret = subprocess.call(cmd, shell=True)
                if ret != 0:
                    error("ubi image copy operation failed")

        return ret

    def main_bconf(self, flash_type, images_dname, out_fname, root):
        """Start the packing process, using board config.

        flash_type -- string, indicates flash type, 'nand' or 'nor' or 'tiny-nor' or 'emmc' or 'norplusnand'
        images_dname -- string, name of images directory
        out_fname -- string, output file path
        """
        self.flash_type = flash_type
        self.images_dname = images_dname
        self.img_fname = out_fname

        self.__create_fnames()
        try:
            os.unlink(self.scr_fname)
        except OSError as e:
            pass

        # generate ubi root images for all the nand included flash builds
        if self.flash_type in [ "nand" , "nand-4k", "norplusnand" , "norplusnand-4k", "norplusnand-gpt", "norplusnand-4k-gpt"]:
            ret = self.gen_ubi_root_files(self.flash_type, root)
            if ret != 0:
                fail_img = out_fname.split("/")
                error("Failed to pack %s" % fail_img[-1])

        # generate partition to fw_img map for all the layouts
        # Eg: {
        #       layout1 : [ part_name, image_name, part_size, part_type, pre_hook_cmd, post_hook_cmd, erase_only ]
        #       layout2 : [ part_name, image_name, part_size, part_type, pre_hook_cmd, post_hook_cmd, erase_only ]
        #     }
        flayout_def_map = {}
        ret = self.__process_board(flayout_def_map, root)
        if ret != 1:
            fail_img = out_fname.split("/")
            error("Failed to pack %s" % fail_img[-1])
        print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, flayout_def_map)

        if not bool(flayout_def_map):
            return 1
        else:
            if flayout not in flayout_def_map.keys():
                return 1

        # generate partition to fw_img map for all the machids after overrides
        # Eg: {
        #       machid1 : [ part_name, image_name, part_size, part_type, pre_hook_cmd, post_hook_cmd, erase_only ]
        #       machid2 : [ part_name, image_name, part_size, part_type, pre_hook_cmd, post_hook_cmd, erase_only ]
        #     }
        machid_map = {}
        ret = self.__process_machid_board(flayout_def_map, machid_map, root)
        if ret != 0:
            fail_img = out_fname.split("/")
            error("Failed to pack %s" % fail_img[-1])
        print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, machid_map)

        if not bool(machid_map):
            return 1

        # generate main RDP specific flash script
        images = []
        ret = self.__gen_machid_flash_script(machid_map, images)
        if ret != 0:
            fail_img = out_fname.split("/")
            error("Failed to pack %s" % fail_img[-1])
        print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, images)

        # generate main flash.scr script
        ret = self.__gen_main_flash_script(machid_map, images)
        print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, images)
        if ret != 0:
            images.insert(0, ImageInfo("script", "flash.scr", "script"))
            self.__mkimage(images)
        else:
            fail_img = out_fname.split("/")
            error("Failed to pack %s" % fail_img[-1])

        return 0

class UsageError(Exception):
    """Indicates error in command arguments."""
    pass

class ArgParser(object):
    """Class to parse command-line arguments."""

    DEFAULT_TYPE = "nor,tiny-nor,nand,norplusnand,emmc,norplusemmc"

    def __init__(self):
        self.flash_type = None
        self.images_dname = None
        self.out_dname = None
        self.scr_fname = None
        self.its_fname = None

    def parse(self, argv):
        global MODE
        global SRC_DIR
        global ARCH_NAME
        global image_type
        global memory_size
        global atf
        global skip_4k_nand
        global img_suffix
        global skip_test
        global flayout
        skip_test = False

        """Start the parsing process, and populate members with parsed value.

        argv -- list of string, the command line arguments
        """

        cdir = os.path.abspath(os.path.dirname(""))
        if len(sys.argv) > 1:
            try:
                opts, args = getopt(sys.argv[1:], "", ["arch=", "fltype=", "srcPath=", "inImage=", "outImage=", "image_type=", "memory=", "img_suffix=", "skip_4k_nand", "atf", "flayout="])
            except GetoptError as e:
                raise UsageError(e.msg)

            for option, value in opts:
                if option == "--arch":
                    ARCH_NAME = value

                elif option == "--fltype":
                    self.flash_type = value

                elif option == "--srcPath":
                    SRC_DIR = os.path.abspath(value)

                elif option == "--inImage":
                    self.images_dname = os.path.join(cdir, value)

                elif option == "--outImage":
                    self.out_dname = os.path.join(cdir, value)

                elif option == "--image_type":
                    image_type = value

                elif option == "--memory":
                    memory_size = value

                elif option == "--img_suffix":
                    img_suffix = "-" + value

                elif option =="--atf":
                    atf = "true"

                elif option =="--skip_4k_nand":
                    skip_4k_nand = "true"

                elif option =="--flayout":
                    flayout = value

            # Verify Arguments passed by user
            # Verify arch type
            if ARCH_NAME not in supported_arch:
                raise UsageError("Invalid arch type '%s'" % arch)

            MODE = "32"
            if ARCH_NAME[-3:] == "_64":
                MODE = "64"
                ARCH_NAME = ARCH_NAME[:-3]

            # Set flash type to default type (nand) if not given by user
            if self.flash_type == None:
                self.flash_type = ArgParser.DEFAULT_TYPE
            for flash_type in self.flash_type.split(","):
                if flash_type not in supported_flash_type[ARCH_NAME]:
                    raise UsageError("invalid flash type '%s'" % flash_type)

            # Verify src Path
            if SRC_DIR == "":
                raise UsageError("Source Path is not provided")

            #Verify input image path
            if self.images_dname == None:
                raise UsageError("input images' Path is not provided")

            #Verify Output image path
            if self.out_dname == None:
                raise UsageError("Output Path is not provided")

    def usage(self, msg):
        """Print error message and command usage information.

        msg -- string, the error message
        """
        print("pack: %s" % msg)
        print()
        print("Usage:")
        print("python pack_v2.py [options] [Value] ...")
        print()
        print("options:")
        print("  --arch \tARCH_TYPE [" + '/'.join(supported_arch) + "]")
        print("  --fltype \tFlash Type [" + '/'.join(supported_flash_type[ARCH_NAME]) + "]")
        print(" \t\tMultiple flashtypes can be passed by a comma separated string")
        print(" \t\tDefault is all. i.e If \"--fltype\" is not passed image for all the flash-type will be created.\n")
        print("  --srcPath \tPath to the directory containg the meta scripts and configs")
        print()
        print("  --inImage \tPath to the direcory containg binaries and images needed for singleimage")
        print()
        print("  --outImage \tPath to the directory where single image will be generated")
        print()
        print("  --memory \tMemory size for low memory profile")
        print(" \t\tIf it is not specified CDTs with default memory size are taken for single-image packing.\n")
        print(" \t\tIf specified, CDTs created with specified memory size will be used for single-image.\n")
        print("  --img_suffix \tSuffix string append to the single image name")
        print()
        print("  --atf \t\tReplace tz with atf for QSEE partition")
        print("  --skip_4k_nand \tskip generating 4k nand images")
        print("  --flayout \tGenerate single image with respect to flash layout")
        print(" \t\tThis Argument does not take any value")
        print("Pack Version: %s" % version)

def main():
    """Main script entry point.

    Created to avoid polluting the global namespace.
    """

    try:
        parser = ArgParser()
        parser.parse(sys.argv)
    except UsageError as e:
        parser.usage(e.args[0])
        sys.exit(1)

    print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno)
    pack = Pack()

    if not os.path.exists(parser.out_dname):
        os.makedirs(parser.out_dname)

    config = SRC_DIR + "/" + ARCH_NAME + "/config.xml"
    root = ET.parse(config)

    print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, config, parser.flash_type)

    if skip_4k_nand != "true":
        # Add nand-4k flash type, if nand flash type is specified
        if "nand" in parser.flash_type.split(","):
            if root.find(".//data[@type='NAND_PARAMETER']/entry") != None:
                parser.flash_type = parser.flash_type + ",nand-4k"

        # Add norplusnand-4k flash type, if norplusnand flash type is specified
        if "norplusnand" in parser.flash_type.split(","):
            if root.find(".//data[@type='NORPLUSNAND_PARAMETER']/entry") != None:
                parser.flash_type = parser.flash_type + ",norplusnand-4k"

        # Add norplusnand-4k-gpt flash type, if norplusnand-gpt flash type is specified
        if "norplusnand-gpt" in parser.flash_type.split(","):
            if root.find(".//data[@type='NORPLUSNAND-GPT_PARAMETER']/entry") != None:
                parser.flash_type = parser.flash_type + ",norplusnand-4k-gpt"

    print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, config, parser.flash_type)

    # Format the output image name from Arch, flash type and mode
    for flash_type in parser.flash_type.split(","):
        MODE_APPEND = "_64" if MODE == "64" else ""
        if image_type == "hlos":
            suffix = "-apps" + img_suffix + ".img"
        else:
            suffix = "-single" + img_suffix + ".img"

        parser.out_fname = flash_type + "-" + ARCH_NAME + MODE_APPEND + suffix

        if ARCH_NAME == "ipq5424":
            if flash_type == "norplusnand-gpt":
                parser.out_fname = "norplusnand-" + ARCH_NAME + MODE_APPEND + suffix
            elif flash_type == "norplusnand-4k-gpt":
                parser.out_fname = "norplusnand-4k-" + ARCH_NAME + MODE_APPEND + suffix
            elif flash_type == "norplusemmc-gpt":
                parser.out_fname = "norplusemmc-" + ARCH_NAME + MODE_APPEND + suffix
            elif flash_type == "norplusnand":
                parser.out_fname = "norplusnand-mibib-" + ARCH_NAME + MODE_APPEND + suffix
            elif flash_type == "norplusnand-4k":
                parser.out_fname = "norplusnand-4k-mibib-" + ARCH_NAME + MODE_APPEND + suffix
            elif flash_type == "norplusemmc":
                parser.out_fname = "norplusemmc-mibib-" + ARCH_NAME + MODE_APPEND + suffix

        parser.out_fname = os.path.join(parser.out_dname, parser.out_fname)
        print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno, config, parser.out_fname)

        pack.main_bconf(flash_type, parser.images_dname, parser.out_fname, root)

    print("#################", sys._getframe(0).f_code.co_name, sys._getframe(0).f_lineno)

if __name__ == "__main__":
    main()
