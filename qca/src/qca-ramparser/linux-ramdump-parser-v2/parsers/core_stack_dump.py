# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: GPL-2.0-only

import glob
import os
import re
import struct
import tempfile

from print_out import print_out_str
from parser_util import register_parser, RamParser

CORE_REGS_PATTERN = re.compile(r'r\.s\s+(\w+)\s+0x([0-9a-fA-F]+)')
GDB_HEX_WORD_PATTERN = re.compile(r'0x[0-9a-fA-F]+:\\t(0x[0-9a-fA-F]+)')

# ARM EXIDX Unwinder register-slot indices, matching ramdump.py's Unwinder.
FP = 11
SP = 13
LR = 14
PC = 15


@register_parser('--print-core-stack', 'Symbolicate raw core stack-pointer dumps (CPU<N>_STACK.BIN) captured in minidumps')
class CoreStackDump(RamParser):

    def find_core_file(self, core_num):
        candidates = glob.glob(os.path.join(
            self.ramdump.minidump_path, 'CPU{0}_STACK.[Bb][Ii][Nn]'.format(core_num)))
        if not candidates:
            return None
        return candidates[0]

    def resolve_kernel_symbol(self, addr):
        wname = self.ramdump.unwind_lookup(addr, check_modules=0)
        if wname is None or len(wname) <= 3:
            return None
        symname, offset, modname, symtab_st_size = wname
        if symname is None or symtab_st_size is None:
            return None
        return '[<0x{0:x}>] {1}+0x{2:x}/0x{3:x}'.format(
            addr, symname, offset, symtab_st_size)

    def resolve_module_symbol(self, addr):
        # unwind_lookup() forces check_modules=0 for IsMinidump (the
        # modules list is normally unwalkable in a minidump), so it can
        # never reach get_module_name_from_addr(). The .ko files found by
        # find_minidump_modules()/add_minidump_sym_files() are already
        # loaded into the live GDB session via add-symbol-file though, so
        # query GDB directly for non-kernel-text addresses instead. GDB
        # itself is the authority on whether addr falls inside a loaded
        # .ko (returns None/no .mod otherwise), so no fixed module VA
        # window is needed here.
        gs = self.ramdump.gdbmi.get_symbol_info(addr)
        if gs is None or gs.symbol is None or not gs.mod:
            return None
        modname = gs.mod.strip('[]')
        if not modname or modname.startswith('.'):
            # get_symbol_info() derives .mod from the last token of GDB's
            # "info symbol" output. That token is a file path only when
            # another objfile (a loaded .ko) is present in the session
            # ("... of /path/to/mod.ko"); with no .ko ever added, GDB's
            # output has no "of <file>" suffix and the token is just the
            # ELF section name (e.g. ".rodata"), which slips past
            # gdbmi.py's own "vmlinux" filter and gets misread as a
            # bogus module name. Real module names never start with
            # ".", so this rejects that case without touching gdbmi.py.
            return None
        return '[<0x{0:x}>] {1}+0x{2:x} [{3}.ko]'.format(
            addr, gs.symbol, gs.offset, modname)

    def parse_regs(self, core_num):
        regs_file = os.path.join(self.ramdump.outdir, 'core{0}_regs.cmm'.format(core_num))
        if not os.path.exists(regs_file):
            return None
        regs = {}
        with open(regs_file, 'r') as f:
            for line in f:
                m = CORE_REGS_PATTERN.match(line.strip())
                if m:
                    regs[m.group(1)] = int(m.group(2), 16)
        return regs

    def resolve_addr(self, addr, stext, etext):
        if stext <= addr < etext:
            return self.resolve_kernel_symbol(addr)
        return self.resolve_module_symbol(addr)

    def _current_el_sp(self, regs):
        # On kernels built with VHE (e.g. IPQ9650/juhu), the kernel runs
        # at EL2 rather than EL1, so sp_el1 is always 0 in the TZ-dumped
        # regs and the live SP is in sp_el2 instead. spsr_el3's mode
        # field (bits[3:2] = EL, bit[0] = SP selector) tells us which EL
        # was actually current at capture time, so read the matching
        # sp_el<N> register rather than assuming EL1.
        spsr_el3 = regs.get('spsr_el3')
        if spsr_el3 is not None:
            el = (spsr_el3 >> 2) & 0x3
            sp = regs.get('sp_el{0}'.format(el))
            if sp:
                return sp
        return regs.get('sp_el1') or regs.get('sp_el2')

    def fp_walk_core_file(self, core_file, regs, stext, etext):
        with open(core_file, 'rb') as f:
            data = f.read()

        sp_el1 = self._current_el_sp(regs)
        fp = regs.get('x29')
        pc = regs.get('pc')
        if not sp_el1 or fp is None or pc is None:
            return False

        capture_size = len(data)
        low = sp_el1

        line = self.resolve_addr(pc, stext, etext) or '[<0x{0:x}>] 0x{0:x}'.format(pc)
        print_out_str(line)

        while True:
            # Mirrors ramdump.py's Unwinder.unwind_frame_generic64(): fp
            # must land at or after the current frame's sp (stack grows
            # down, so caller frames sit at strictly higher addresses)
            # and within the captured blob, otherwise treat it as the
            # end of the chain (e.g. execution moved to the separate,
            # uncaptured per-CPU IRQ stack) rather than stale garbage.
            if fp < low or fp & 0xf:
                break
            offset = fp - sp_el1
            if offset < 0 or offset + 16 > capture_size:
                break

            saved_fp, saved_lr = struct.unpack_from('<QQ', data, offset)
            if saved_fp == 0 or saved_lr == 0:
                break

            line = self.resolve_addr(saved_lr, stext, etext) or '[<0x{0:x}>] 0x{0:x}'.format(saved_lr)
            print_out_str(line)

            low = fp + 0x10
            fp = saved_fp

        return True

    def _vmlinux_word(self, addr):
        # This capture has no EBI/DDR file, only CPU<N>_STACK.BIN blobs, so
        # read_word() can't resolve VAs backed by static vmlinux data
        # (e.g. the .ARM.unwind_idx/.ARM.unwind_tab EXIDX tables) --
        # virt_to_phys() has nothing to map them against. That data is
        # static ELF content though, already loaded into the live GDB
        # session, so fetch it straight from there instead.
        result = self.ramdump.gdbmi._run(
            'x/1xw 0x{0:x}'.format(addr), skip_cache=False, save_in_cache=True)
        for line in result.lines:
            m = GDB_HEX_WORD_PATTERN.search(line)
            if m:
                return int(m.group(1), 16)
        return None

    def _prel31_to_addr(self, addr):
        value = self._vmlinux_word(addr)
        if value is None:
            return None
        # Full sign-extension of the 31-bit prel31 field (bits[30:0]),
        # unlike ramdump.py's Unwinder.prel31_to_addr() which only sets
        # bit 31 and then re-adds the resulting overflow carry -- that
        # combination is numerically wrong for negative displacements
        # (verified empirically: off-by-one whenever addr+offset carries
        # past bit 31). Python's arbitrary-precision ints make a plain
        # sign-extend-then-mask correct with no carry trick needed.
        if (value & 0x40000000):
            offset = value | ~0x7fffffff
        else:
            offset = value
        return (addr + offset) & 0xffffffff

    def _build_unwind_table(self):
        unwind = self.ramdump.unwind
        start_idx = unwind.start_idx
        stop_idx = unwind.stop_idx

        with tempfile.NamedTemporaryFile(delete=False) as tf:
            tmp_path = tf.name
        try:
            self.ramdump.gdbmi._run(
                'dump binary memory {0} 0x{1:x} 0x{2:x}'.format(
                    tmp_path, start_idx, stop_idx),
                skip_cache=True, save_in_cache=False)
            with open(tmp_path, 'rb') as f:
                data = f.read()
        finally:
            os.remove(tmp_path)

        table = []
        n = len(data) // 8
        for i in range(n):
            a, b = struct.unpack_from('<II', data, i * 8)
            table.append((a, b, start_idx + i * 8))
        return table

    def _unwind_find_origin(self, table):
        start = 0
        stop = len(table)
        while start < stop:
            mid = start + ((stop - start) >> 1)
            if table[mid][0] >= 0x40000000:
                start = mid + 1
            else:
                stop = mid
        return stop

    def _search_idx(self, table, origin, addr):
        # Direct port of ramdump.py's Unwinder.search_idx_3_4.
        start = 0
        stop = len(table)
        if addr < self.ramdump.unwind.start_idx:
            stop = origin
        else:
            start = origin

        if start >= stop:
            return None

        addr = (addr - table[start][2]) & 0x7fffffff

        while start < (stop - 1):
            mid = start + ((stop - start) >> 1)
            dif = table[mid][2] - table[start][2]
            if (addr - dif) < table[mid][0]:
                stop = mid
            else:
                addr = addr - dif
                start = mid

        if table[start][0] <= addr:
            return table[start]
        return None

    def _unwind_get_byte(self, ctrl):
        if ctrl['entries'] <= 0:
            print_out_str('unwind: Corrupt unwind table')
            return 0

        val = self._vmlinux_word(ctrl['insn'])
        if val is None:
            ctrl['entries'] = 0
            return 0

        ret = (val >> (ctrl['byte'] * 8)) & 0xff

        if ctrl['byte'] == 0:
            ctrl['insn'] += 4
            ctrl['entries'] -= 1
            ctrl['byte'] = 3
        else:
            ctrl['byte'] -= 1
        return ret

    def _read_core_word(self, vsp, sp0, core_data, capture_size):
        offset = vsp - sp0
        if offset < 0 or offset + 4 > capture_size:
            return None
        return struct.unpack_from('<I', core_data, offset)[0]

    def _unwind_exec_insn(self, ctrl, sp0, core_data, capture_size):
        # Direct port of ramdump.py's Unwinder.unwind_exec_insn, with
        # every stack-relative read (register pops) sourced from the raw
        # CPU<N>_STACK.BIN capture instead of self.ramdump.read_word(), since
        # that capture -- not general RAM -- is the only place the
        # thread's live stack contents actually exist here.
        insn = self._unwind_get_byte(ctrl)

        if (insn & 0xc0) == 0x00:
            ctrl['vrs'][SP] += ((insn & 0x3f) << 2) + 4
        elif (insn & 0xc0) == 0x40:
            ctrl['vrs'][SP] -= ((insn & 0x3f) << 2) + 4
        elif (insn & 0xf0) == 0x80:
            vsp = ctrl['vrs'][SP]
            reg = 4

            insn = (insn << 8) | self._unwind_get_byte(ctrl)
            mask = insn & 0x0fff
            if mask == 0:
                print_out_str("unwind: 'Refuse to unwind' instruction")
                return -1

            load_sp = mask & (1 << (13 - 4))
            while mask:
                if mask & 1:
                    v = self._read_core_word(vsp, sp0, core_data, capture_size)
                    ctrl['vrs'][reg] = v
                    if v is None:
                        return -1
                    vsp += 4
                mask >>= 1
                reg += 1
            if not load_sp:
                ctrl['vrs'][SP] = vsp
        elif (insn & 0xf0) == 0x90 and (insn & 0x0d) != 0x0d:
            ctrl['vrs'][SP] = ctrl['vrs'][insn & 0x0f]
        elif (insn & 0xf0) == 0xa0:
            vsp = ctrl['vrs'][SP]
            # Pop r4-r[4+nnn] (and r14 below, if insn&0x80) per EHABI 10100nnn.
            regs = list(range(4, 5 + (insn & 7)))
            for reg in regs:
                v = self._read_core_word(vsp, sp0, core_data, capture_size)
                ctrl['vrs'][reg] = v
                if v is None:
                    return -1
                vsp += 4
            if insn & 0x80:
                v = self._read_core_word(vsp, sp0, core_data, capture_size)
                ctrl['vrs'][14] = v
                if v is None:
                    return -1
                vsp += 4
            ctrl['vrs'][SP] = vsp
        elif insn == 0xb0:
            if ctrl['vrs'][PC] == 0:
                ctrl['vrs'][PC] = ctrl['vrs'][LR]
            ctrl['entries'] = 0
        elif insn == 0xb1:
            mask = self._unwind_get_byte(ctrl)
            vsp = ctrl['vrs'][SP]
            reg = 0

            if mask == 0 or mask & 0xf0:
                print_out_str('unwind: Spare encoding')
                return -1

            while mask:
                if mask & 1:
                    v = self._read_core_word(vsp, sp0, core_data, capture_size)
                    ctrl['vrs'][reg] = v
                    if v is None:
                        return -1
                    vsp += 4
                mask >>= 1
                reg += 1
            ctrl['vrs'][SP] = vsp
        elif insn == 0xb2:
            uleb128 = self._unwind_get_byte(ctrl)
            ctrl['vrs'][SP] += 0x204 + (uleb128 << 2)
        else:
            print_out_str('unwind: Unhandled instruction')
            return -1

        return 0

    def _unwind_frame_tables(self, table, origin, fp, sp, lr, pc, sp0,
                              core_data, capture_size, thread_size):
        # Direct port of ramdump.py's Unwinder.unwind_frame_tables.
        low = sp
        high = ((low + (thread_size - 1)) & ~(thread_size - 1)) + thread_size
        idx = self._search_idx(table, origin, pc)
        if idx is None:
            return None

        ctrl = {'vrs': 16 * [0], 'insn': 0, 'entries': -1, 'byte': -1}
        ctrl['vrs'][FP] = fp
        ctrl['vrs'][SP] = sp
        ctrl['vrs'][LR] = lr
        ctrl['vrs'][PC] = 0

        if idx[1] == 1:
            return None
        elif (idx[1] & 0x80000000) == 0:
            insn_addr = self._prel31_to_addr(idx[2] + 4)
            if insn_addr is None:
                return None
            ctrl['insn'] = insn_addr
        elif (idx[1] & 0xff000000) == 0x80000000:
            ctrl['insn'] = idx[2] + 4
        else:
            return None

        val = self._vmlinux_word(ctrl['insn'])
        if val is None:
            return None

        if (val & 0xff000000) == 0x80000000:
            ctrl['byte'] = 2
            ctrl['entries'] = 1
        elif (val & 0xff000000) == 0x81000000:
            ctrl['byte'] = 1
            ctrl['entries'] = 1 + ((val & 0x00ff0000) >> 16)
        else:
            return None

        while ctrl['entries'] > 0:
            urc = self._unwind_exec_insn(ctrl, sp0, core_data, capture_size)
            if urc < 0:
                return None
            if ctrl['vrs'][SP] < low or ctrl['vrs'][SP] >= high:
                return None

        if ctrl['vrs'][PC] == 0:
            ctrl['vrs'][PC] = ctrl['vrs'][LR]

        if pc == ctrl['vrs'][PC]:
            return None

        return (ctrl['vrs'][FP], ctrl['vrs'][SP], ctrl['vrs'][LR], ctrl['vrs'][PC])

    def exidx_walk_core_file(self, core_file, regs, stext, etext):
        # ARM32 register-role mapping (lr=x18, sp/bt=x19, fp=x11) matches
        # watchdog_v3.py's dump_core_pc(), the existing authority for this
        # naming convention in this codebase. Note fp/x11 is only carried
        # through the walk for bookkeeping -- kernels built with
        # CONFIG_ARM_UNWIND=y (as confirmed for this dataset via kconfig)
        # don't maintain it as an APCS frame-pointer chain, so it is never
        # used to compute the next frame; only the EXIDX tables are.
        sp = regs.get('x19')
        fp = regs.get('x11')
        lr = regs.get('x18')
        pc = regs.get('pc')
        if sp is None or fp is None or lr is None or pc is None:
            return False

        unwind = self.ramdump.unwind
        if not hasattr(unwind, 'start_idx'):
            return False

        with open(core_file, 'rb') as f:
            core_data = f.read()
        capture_size = len(core_data)
        thread_size = self.ramdump.thread_size

        # capture_base is the fixed anchor: offset 0 in CPU<N>_STACK.BIN
        # corresponds to this initial sp. Every later frame's sp is
        # translated back to a CPU<N>_STACK.BIN offset relative to this same
        # anchor, since the capture is a single contiguous stack dump
        # starting at the register-dump's sp, not a per-frame snapshot.
        capture_base = sp

        table = self._build_unwind_table()
        origin = self._unwind_find_origin(table)

        line = self.resolve_addr(pc, stext, etext) or '[<0x{0:x}>] 0x{0:x}'.format(pc)
        print_out_str(line)

        while True:
            res = self._unwind_frame_tables(
                table, origin, fp, sp, lr, pc, capture_base, core_data,
                capture_size, thread_size)
            if res is None:
                break
            fp, sp, lr, pc = res

            line = self.resolve_addr(pc, stext, etext) or '[<0x{0:x}>] 0x{0:x}'.format(pc)
            print_out_str(line)

        return True

    def scan_core_file(self, core_file, stext, etext):
        with open(core_file, 'rb') as f:
            data = f.read()

        isElf64 = self.ramdump.isELF64()
        if isElf64:
            wordsize, fmt = 8, 'Q'
            b = 0xffffffb000000000
            c = 0xffffffc000000000
            d = 0xffffffd000000000
        else:
            wordsize, fmt = 4, 'I'
            b = 0xb0000000
            c = 0xc0000000
            d = 0xd0000000

        nwords = len(data) // wordsize
        words = struct.unpack('<{0}{1}'.format(nwords, fmt), data[:nwords * wordsize])

        for addr in words:
            if not ((addr & b) == b or (addr & c) == c):
                continue
            if (addr & d) == d:
                continue

            # Real devices have been observed placing module base
            # addresses inside the same 0xffffffc0... segment as kernel
            # text (not the separate 0xffffffb0... window a fixed-range
            # heuristic would assume), so stext/etext only tells us it's
            # a *kernel-text* hit -- anything else plausible is handed to
            # GDB, which is the authority on whether it lands inside a
            # loaded .ko.
            if (addr & c) == c and stext <= addr < etext:
                line = self.resolve_kernel_symbol(addr)
            else:
                line = self.resolve_module_symbol(addr)
            line = line or '[<0x{0:x}>] 0x{0:x}'.format(addr)
            print_out_str(line)

    def parse(self):
        if not self.ramdump.IsMinidump or not self.ramdump.minidump_path:
            return

        stext = self.ramdump.addr_lookup('_stext')
        etext = self.ramdump.addr_lookup('_etext')
        if stext is None or etext is None:
            print_out_str('!!! Could not resolve _stext/_etext, skipping core stack dump')
            return

        # get_num_cpus() relies on reading cpu_present_bits from mapped
        # memory, which minidump captures don't reliably have, so instead
        # bound the loop by how many CPU<N>_STACK.BIN files actually exist
        # rather than the target's core count (e.g. quad-core targets have
        # no CPU4..7_STACK.BIN, which isn't an error worth logging).
        found_any = False
        core_num = 0
        while True:
            core_file = self.find_core_file(core_num)
            if core_file is None:
                break
            found_any = True
            print_out_str('\n======== Core {0} stack dump ========'.format(core_num))

            walked = False
            regs = self.parse_regs(core_num)
            if self.ramdump.arm64:
                if regs is not None:
                    walked = self.fp_walk_core_file(core_file, regs, stext, etext)
            elif regs is not None:
                # Only attempt the EXIDX walk when the vmlinux actually has
                # unwind tables (Unwinder.__init__ only sets start_idx when
                # __start_unwind_idx/__stop_unwind_idx resolved, i.e.
                # CONFIG_ARM_UNWIND=y) -- otherwise fall straight through to
                # the heuristic scan below, same as today.
                if hasattr(self.ramdump.unwind, 'start_idx'):
                    walked = self.exidx_walk_core_file(core_file, regs, stext, etext)

            if not walked:
                self.scan_core_file(core_file, stext, etext)

            core_num += 1

        if not found_any:
            print_out_str('!!! No CPU<N>_STACK.BIN found in minidump path')
