# Copyright (c) 2014, The Linux Foundation. All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 and
# only version 2 as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

import re
import string

from parser_util import cleanupString
from print_out import print_out_str

class DmesgLib(object):

    def __init__(self, ramdump, outfile):
        self.ramdump = ramdump
        self.wrap_cnt = 0
        self.outfile = outfile

    def log_from_idx(self, idx, logbuf):
        len_offset = self.ramdump.field_offset('struct printk_log', 'len')

        msg = logbuf + idx
        msg_len = self.ramdump.read_u16(msg + len_offset)
        if (msg_len == 0):
            return logbuf
        else:
            return msg

    def log_next(self, idx, logbuf):
        len_offset = self.ramdump.field_offset('struct printk_log', 'len')
        msg = idx

        msg_len = self.ramdump.read_u16(msg + len_offset)
        if (msg_len == 0):
            self.wrap_cnt += 1
            return logbuf
        else:
            return idx + msg_len

    def split_text(self, text_str, timestamp, dmesg_out):
        if self.ramdump.isELF64():
            ptr_re = "[0-9a-f]{16}"
        else:
            ptr_re = "[0-9a-f]{8}"
        for partial in text_str.split('\n'):
            #
            # if CONFIG_KALLSYMS is not enabled, the kernel prints the PC and LR
            # pointers twice without resolving them to symbol names. Hence, print once and
            # parse the pointer strings and resolve them using gdb here
            #
            if ((self.ramdump.kallsyms_offset is None or self.ramdump.kallsyms_offset < 0) and not (self.ramdump.arm64 and (self.ramdump.kernel_version[0], self.ramdump.kernel_version[1]) >= (5, 4)) and (re.search("PC is at", partial) or re.search("LR is at", partial))):
                continue

            if ((self.ramdump.kallsyms_offset is None or self.ramdump.kallsyms_offset < 0) and not (self.ramdump.arm64 and (self.ramdump.kernel_version[0], self.ramdump.kernel_version[1]) >= (5, 4)) and (re.search("pc : \[<" + ptr_re + ">\].* lr : \[<" + ptr_re + ">\].*", partial) or re.search("Function entered at \[<" + ptr_re + ">\].* from \[<" + ptr_re + ">\].*", partial))):
                x = re.findall(ptr_re, partial)
                x0, x1 = x[0], x[1]
                try:
                    s = self.ramdump.gdbmi.get_symbol_info(int(x0, 16))
                except:
                    s = None
                if s is not None:
                    if len(s.mod):
                        s.mod = " " + s.mod
                        x0 = s.symbol + "+" + hex(s.offset) + s.mod
                else:
                    x0 = "0x" + x0

                try:
                    s1 = self.ramdump.gdbmi.get_symbol_info(int(x1, 16))
                except:
                    s1 = None
                if s1 is not None:
                    if len(s1.mod):
                        s1.mod = " " + s1.mod
                        x1 = s1.symbol + "+" + hex(s1.offset) + s1.mod
                else:
                    x1 = "0x" + x1

                if re.search("pc : \[<" + ptr_re + ">\].* lr : \[<" + ptr_re + ">\].*", partial):
                    #
                    # Convert the string from the following format to
                    #	pc : [<813ac97c>]    lr : [<813acc94>]    psr: 60000013
                    #
                    # this format so that ATT is not upset
                    #	PC is at osif_delete_vap_wait_and_free+0x278/0x3a0 [umac]
                    #	LR is at osif_delete_vap_wait_and_free+0x278/0x3a0 [umac]
                    #	pc : [<df50ea24>]    lr : [<df50ea24>]    psr: 60000013
                    f = '[{0:>5}.{1:0>6d}] {2}\n'.format(
                            timestamp // 1000000000, (timestamp % 1000000000) // 1000, "PC is at " + str(x0))
                    self.outfile.write(f.encode('ascii', 'ignore'))
                    dmesg_out.write(f)
                    f = '[{0:>5}.{1:0>6d}] {2}\n'.format(
                        timestamp // 1000000000, (timestamp % 1000000000) // 1000, "LR is at " + str(x1))
                    self.outfile.write(f.encode('ascii', 'ignore'))
                    dmesg_out.write(f)
                    # don't modify 'partial', it has to get printed AS IS
                else:
                    #
                    # Convert the string from the following format to
                    #	Function entered at [<8141039c>] from [<7f0d519c>]
                    #
                    # this format so that ATT is not upset
                    #	[<df512690>] (osif_ioctl_delete_vap [umac]) from [<df4fc9ac>] (ieee80211_ioctl+0x140/0x1c00 [umac])
                    if s is not None and s1 is not None:
                        partial = "[<" + x[0] + ">] (" + x0 + ") from [<" + x[1] + ">] (" + x1 + ")"

            f = '[{0:>5}.{1:0>6d}] {2}\n'.format(
                timestamp // 1000000000, (timestamp % 1000000000) // 1000, partial)

            self.outfile.write(f.encode('ascii', 'ignore'))
            dmesg_out.write(f)

    def extract_dmesg_flat(self):
        log_buf_addr = 0
        log_buf_size = 0
        log_buf_end = 0
        log_ind = 0
        i = 1
        j = 0
        dmesg_buf = ""
        dmesg_out = self.ramdump.open_file('dmesg.txt')
        log_buf_addr = self.ramdump.read_word(self.ramdump.addr_lookup('log_buf'))
        log_buf_size = self.ramdump.read_word(self.ramdump.addr_lookup('log_buf_len'))
        log_buf_end = self.ramdump.read_word(self.ramdump.addr_lookup('log_end'))
        log_buf_mask = log_buf_size - 1

        j = log_buf_end - 1
        log_ind = j & log_buf_mask
        dmesg_buf = '{0:c}'.format(self.ramdump.read_byte(log_buf_addr + log_ind))

        while (1):
           j = log_buf_end - 1 - i
           if (j + log_buf_size  < log_buf_end):
              break
           log_ind = j & log_buf_mask
           dmesg_buf = '{0:c}{1}'.format(self.ramdump.read_byte(log_buf_addr + log_ind), dmesg_buf)
           i = i + 1

        self.outfile.write('\n' + dmesg_buf + '\n')
        dmesg_out.write('{0}\n'.format(dmesg_buf))
        dmesg_out.close()

    def extract_dmesg_6_1(self, write_to_file=True):
        dmesg_out = self.ramdump.open_file('dmesg.txt')
        prb_addr = self.ramdump.read_word('prb')
        desc_ring_offset = self.ramdump.field_offset(
                                'struct printk_ringbuffer', 'desc_ring')
        desc_ring_addr = prb_addr + desc_ring_offset

        count_bits_off = self.ramdump.field_offset(
                                'struct prb_desc_ring', 'count_bits')
        desc_ring_count = 1 << self.ramdump.read_u32(
                                desc_ring_addr + count_bits_off)

        desc_size = self.ramdump.sizeof('struct prb_desc')
        descs_offset = self.ramdump.field_offset('struct prb_desc_ring', 'descs')
        descs_addr = self.ramdump.read_word(desc_ring_addr + descs_offset)

        info_size = self.ramdump.sizeof('struct printk_info')
        infos_offset = self.ramdump.field_offset('struct prb_desc_ring', 'infos')
        infos_addr = self.ramdump.read_word(desc_ring_addr + infos_offset)

        data_ring_offset = self.ramdump.field_offset(
                                'struct printk_ringbuffer', 'text_data_ring')
        text_data_ring_addr = prb_addr + data_ring_offset

        size_bits_offset = self.ramdump.field_offset(
                                'struct prb_data_ring', 'size_bits')
        text_data_size = 1 << self.ramdump.read_u32(
                                text_data_ring_addr + size_bits_offset)
        data_ring_offset = self.ramdump.field_offset('struct prb_data_ring', 'data')
        data_addr = self.ramdump.read_word(text_data_ring_addr + data_ring_offset)

        state_var_offset = self.ramdump.field_offset(
                                'struct prb_desc', 'state_var')
        blk_lpos_offset = self.ramdump.field_offset(
                                'struct prb_desc','text_blk_lpos')
        begin_off = blk_lpos_offset + self.ramdump.field_offset(
                                        'struct prb_data_blk_lpos', 'begin')
        next_off = blk_lpos_offset + self.ramdump.field_offset(
                                        'struct prb_data_blk_lpos', 'next')
        ts_offset = self.ramdump.field_offset('struct printk_info', 'ts_nsec')
        len_off = self.ramdump.field_offset('struct printk_info', 'text_len')

        desc_sv_bits = self.ramdump.sizeof('long') * 8
        desc_flags_shift = desc_sv_bits - 2
        desc_flags_mask = 3 << desc_flags_shift
        desc_id_mask = ~desc_flags_mask

        tail_id_offset = self.ramdump.field_offset('struct prb_desc_ring','tail_id')
        tail_id = self.ramdump.read_word(desc_ring_addr + tail_id_offset)
        head_id_offset = self.ramdump.field_offset('struct prb_desc_ring','head_id')
        head_id = self.ramdump.read_word(desc_ring_addr + head_id_offset)

        last = tail_id

        dmesg_list={}
        while True:
            index = last % desc_ring_count
            desc_off = desc_size * index
            info_off = info_size * index

            # skip non-committed record
            state = 3 & (self.ramdump.read_word(descs_addr + desc_off +
                                        state_var_offset) >> desc_flags_shift)
            if state != 1 and state != 2:
                if last == head_id:
                    break
                last = (last + 1) & desc_id_mask
                continue

            begin = self.ramdump.read_word(descs_addr + desc_off +
                                                begin_off) % text_data_size
            end = self.ramdump.read_word(descs_addr + desc_off +
                                                next_off) % text_data_size
            if begin & 1 == 1:
                text = ""
            else:
                if begin > end:
                    begin = 0

                text_start = begin + self.ramdump.sizeof('long')
                text_len = self.ramdump.read_u16(infos_addr +
                                                    info_off + len_off)

                if end - text_start < text_len:
                    text_len = end - text_start
                if text_len < 0:
                    text_len = 0

                text = self.ramdump.read_cstring(data_addr +
                                                    text_start, text_len)

            timestamp = self.ramdump.read_u64(infos_addr +
                                                    info_off + ts_offset)
            self.split_text(text, timestamp, dmesg_out)

            if last == head_id:
                break
            last = (last + 1) & desc_id_mask
        dmesg_out.close()

    def extract_dmesg_binary(self):
        dmesg_out = self.ramdump.open_file('dmesg.txt')
        first_idx_addr = self.ramdump.addr_lookup('log_first_idx')
        last_idx_addr = self.ramdump.addr_lookup('log_next_idx')
        logbuf_addr = self.ramdump.read_word(self.ramdump.addr_lookup('log_buf'))
        time_offset = self.ramdump.field_offset('struct printk_log', 'ts_nsec')
        len_offset = self.ramdump.field_offset('struct printk_log', 'len')
        text_len_offset = self.ramdump.field_offset('struct printk_log', 'text_len')
        log_size = self.ramdump.sizeof('struct printk_log')

        first_idx = self.ramdump.read_u32(first_idx_addr)
        last_idx = self.ramdump.read_u32(last_idx_addr)

        curr_idx = logbuf_addr + first_idx

        if self.ramdump.isELF64():
            ptr_re = "[0-9a-f]{16}"
        else:
            ptr_re = "[0-9a-f]{8}"

        while curr_idx != logbuf_addr + last_idx and self.wrap_cnt < 2:
            timestamp = self.ramdump.read_dword(curr_idx + time_offset)
            text_len = self.ramdump.read_u16(curr_idx + text_len_offset)
            text_str = self.ramdump.read_cstring(curr_idx + log_size, text_len)
            self.split_text(text_str, timestamp, dmesg_out)
            curr_idx = self.log_next(curr_idx, logbuf_addr)
        dmesg_out.close()

    def extract_dmesg(self):
        major, minor, patch = self.ramdump.kernel_version
        if re.search('3.7.\d', self.ramdump.version) is not None:
            self.extract_dmesg_binary()
        elif re.search('3\.10\.\d', self.ramdump.version) is not None:
            self.extract_dmesg_binary()
        elif re.search('3\.14\.\d', self.ramdump.version) is not None:
            self.extract_dmesg_binary()
        elif re.search('3\.18\.\d', self.ramdump.version) is not None:
            self.extract_dmesg_binary()
        elif (major, minor) >= (6, 1):
            self.extract_dmesg_6_1()
        elif (major, minor) >= (4, 4):
            self.extract_dmesg_binary()
        else:
            self.extract_dmesg_flat()
