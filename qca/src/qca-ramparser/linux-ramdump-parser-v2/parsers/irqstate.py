# Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
from print_out import print_out_str
from parser_util import register_parser, RamParser
import maple_tree

@register_parser('--print-irqs', 'Print all the irq information', shortopt='-i')
class IrqParse(RamParser):

    def print_irq_state_3_0(self, ram_dump):
        print_out_str(
            '=========================== IRQ STATE ===============================')
        per_cpu_offset_addr = ram_dump.addr_lookup('__per_cpu_offset')
        cpus = ram_dump.get_num_cpus()
        irq_desc = ram_dump.addr_lookup('irq_desc')
        foo, irq_desc_size, dummy, symtab_st_size = ram_dump.unwind_lookup(irq_desc, 1)
        h_irq_offset = ram_dump.field_offset('struct irq_desc', 'handle_irq')
        irq_num_offset = ram_dump.field_offset('struct irq_data', 'irq')
        irq_data_offset = ram_dump.field_offset('struct irq_desc', 'irq_data')
        irq_count_offset = ram_dump.field_offset(
            'struct irq_desc', 'irq_count')
        irq_chip_offset = ram_dump.field_offset('struct irq_data', 'chip')
        irq_action_offset = ram_dump.field_offset('struct irq_desc', 'action')
        action_name_offset = ram_dump.field_offset('struct irqaction', 'name')
        kstat_irqs_offset = ram_dump.field_offset(
            'struct irq_desc', 'kstat_irqs')
        chip_name_offset = ram_dump.field_offset('struct irq_chip', 'name')
        irq_desc_entry_size = ram_dump.sizeof('irq_desc[0]')
        cpu_str = ''

        for i in range(0, cpus):
            cpu_str = cpu_str + '{0:10} '.format('CPU{0}'.format(i))

        print_out_str(
            '{0:4} {1} {2:30} {3:10}'.format('IRQ', cpu_str, 'Name', 'Chip'))
        for i in range(0, irq_desc_size, irq_desc_entry_size):
            irqnum = ram_dump.read_word(irq_desc + i + irq_num_offset)
            irqcount = ram_dump.read_word(irq_desc + i + irq_count_offset)
            action = ram_dump.read_word(irq_desc + i + irq_action_offset)
            kstat_irqs_addr = ram_dump.read_word(
                irq_desc + i + kstat_irqs_offset)
            irq_stats_str = ''

            for j in range(0, cpus):
                if per_cpu_offset_addr is None:
                    offset = 0
                else:
                    offset = ram_dump.read_word(per_cpu_offset_addr + 4 * j)
                irq_statsn = ram_dump.read_word(kstat_irqs_addr + offset)
                irq_stats_str = irq_stats_str + \
                    '{0:10} '.format('{0}'.format(irq_statsn))

            chip = ram_dump.read_word(
                irq_desc + i + irq_data_offset + irq_chip_offset)
            chip_name_addr = ram_dump.read_word(chip + chip_name_offset)
            chip_name = ram_dump.read_cstring(chip_name_addr, 48)

            if action != 0:
                name_addr = ram_dump.read_word(action + action_name_offset)
                name = ram_dump.read_cstring(name_addr, 48)
                print_out_str(
                    '{0:4} {1} {2:30} {3:10}'.format(irqnum, irq_stats_str, name, chip_name))

    def radix_tree_lookup_element(self, ram_dump, root_addr, index):
        rnode_offset = ram_dump.field_offset('struct radix_tree_root', 'rnode')
        if re.search('3\.18\.\d', self.ramdump.version) is not None or (ram_dump.kernel_version[0], ram_dump.kernel_version[1]) >= (4, 4):
            rnode_height_offset = ram_dump.field_offset(
                 'struct radix_tree_node', 'path')
        else:
            rnode_height_offset = ram_dump.field_offset(
                 'struct radix_tree_node', 'height')
        slots_offset = ram_dump.field_offset('struct radix_tree_node', 'slots')
        pointer_size = ram_dump.sizeof('struct radix_tree_node *')

        # if CONFIG_BASE_SMALL=0: radix_tree_map_shift = 6
        radix_tree_map_shift = 6
        radix_tree_map_mask = 0x3f
        height_to_maxindex = [0x0, 0x3F, 0x0FFF,
                              0x0003FFFF, 0x00FFFFFF, 0x3FFFFFFF, 0xFFFFFFFF]

        if ram_dump.read_word(root_addr + rnode_offset) & 1 == 0:
            if index > 0:
                return None
            return (ram_dump.read_word(root_addr + rnode_offset) & 0xfffffffffffffffe)

        node_addr = ram_dump.read_word(root_addr + rnode_offset) & 0xfffffffffffffffe
        height = ram_dump.read_int(node_addr + rnode_height_offset)

        if height > len(height_to_maxindex):
            return None

        if height is None or index > height_to_maxindex[height]:
            return None

        shift = (height - 1) * radix_tree_map_shift
        for h in range(height, 0, -1):
            node_addr = ram_dump.read_word(
                node_addr + slots_offset + ((index >> shift) & radix_tree_map_mask) * pointer_size)
            if node_addr == 0:
                return None
            shift -= radix_tree_map_shift
        return (node_addr & 0xfffffffffffffffe)

    def shift_to_maxindex(self, shift):
        radix_tree_map_shift = 6
        radix_tree_map_size = 1 << radix_tree_map_shift
        return (radix_tree_map_size << shift) - 1

    def is_internal_node(self, addr):
        radix_tree_entry_mask = 0x3
        radix_tree_internal_node = 0x2
        return (addr & radix_tree_entry_mask) == radix_tree_internal_node

    def entry_to_node(self, addr):
        return addr & 0xfffffffffffffffd

    def save_irq_desc(self, node, irq_desc):
        if node:
            irq_desc.append(node)

    def xarray_lookup_element(self, ram_dump, root_addr, index):
        rnode_offset = ram_dump.field_offset('struct xarray', 'xa_head')
        rnode_shift_offset = ram_dump.field_offset('struct xa_node', 'shift')
        slots_offset = ram_dump.field_offset('struct xa_node', 'slots')
        pointer_size = ram_dump.sizeof('struct xa_node *')

        # if CONFIG_BASE_SMALL=0: radix_tree_map_shift = 6
        maxindex = 0
        radix_tree_map_shift = 6
        radix_tree_map_mask = 0x3f

        rnode_addr = ram_dump.read_word(root_addr + rnode_offset)
        if self.is_internal_node(rnode_addr):
            node_addr = self.entry_to_node(rnode_addr)
            shift = ram_dump.read_byte(node_addr + rnode_shift_offset)
            maxindex = self.shift_to_maxindex(shift)

        if index > maxindex:
            return None

        while self.is_internal_node(rnode_addr):
            parent_addr = self.entry_to_node(rnode_addr)
            parent_shift = ram_dump.read_byte(parent_addr + rnode_shift_offset)
            offset = (index >> parent_shift) & radix_tree_map_mask
            rnode_addr = ram_dump.read_word(parent_addr + slots_offset +
                (offset * pointer_size))

        if rnode_addr == 0:
            return None

        return rnode_addr

    def dump_sparse_irq_state(self, ram_dump, irq_desc_addr):
        if irq_desc_addr is None:
            return
        irq_num_offset = ram_dump.field_offset('struct irq_data', 'irq')
        irq_data_offset = ram_dump.field_offset('struct irq_desc', 'irq_data')
        irq_count_offset = ram_dump.field_offset(
            'struct irq_desc', 'irq_count')
        irq_chip_offset = ram_dump.field_offset('struct irq_data', 'chip')
        irq_action_offset = ram_dump.field_offset('struct irq_desc', 'action')
        action_name_offset = ram_dump.field_offset('struct irqaction', 'name')
        kstat_irqs_offset = ram_dump.field_offset(
            'struct irq_desc', 'kstat_irqs')
        chip_name_offset = ram_dump.field_offset('struct irq_chip', 'name')
        irq_chip_data_offset = ram_dump.field_offset('struct irq_data',
                                                     'chip_data')
        gic_domain_offset = ram_dump.field_offset('struct gic_chip_data',
                                                     'domain')
        irq_domain_pmdev_offset = ram_dump.field_offset('struct irq_domain',
                                                      'pm_dev')
        node_offset = ram_dump.field_offset('struct device', 'of_node')

        chip_data =  ram_dump.read_word(irq_desc_addr +
		                      irq_data_offset + irq_chip_data_offset)
        domain = chip_data + gic_domain_offset

        irqnum = ram_dump.read_u32(irq_desc_addr +
		              irq_data_offset + irq_num_offset)
        irqcount = ram_dump.read_u32(irq_desc_addr + irq_count_offset)
        action = ram_dump.read_word(irq_desc_addr + irq_action_offset)
        kstat_irqs_addr = ram_dump.read_word(irq_desc_addr + kstat_irqs_offset)
        irq_stats_str = ''

        if kstat_irqs_addr is None:
            return

        for j in ram_dump.iter_cpus():
            irq_statsn = ram_dump.read_u32(kstat_irqs_addr, cpu=j)
            irq_stats_str = irq_stats_str + \
                '{0:10} '.format('{0}'.format(irq_statsn))

        if (ram_dump.kernel_version[0], ram_dump.kernel_version[1]) >= (6, 4):
            pm_dev =  ram_dump.read_structure_field(domain,
                                          'struct domain', 'pm_dev')
            if pm_dev is None:
                chip_name = "GIC-0"
            else:
                chip_name = ram_dump.read_structure_field(pm_dev + node_offset,
                                              'struct device_node', 'name')
        else:
            chip = ram_dump.read_word(
                    irq_desc_addr + irq_data_offset + irq_chip_offset)
            chip_name_addr = ram_dump.read_word(chip + chip_name_offset)
            chip_name = ram_dump.read_cstring(chip_name_addr, 48)

        if chip_name is None:
            chip_name = ""

        if action != 0:
            name_addr = ram_dump.read_word(action + action_name_offset)
            if not name_addr:
               return
            else:
               name = ram_dump.read_cstring(name_addr, 48)
               if name is None:
                   return
            print_out_str(
                '{0:4} {1} {2:30} {3:10}'.format(irqnum,
				                     irq_stats_str, name, chip_name))

    def print_irq_state_sparse_irq(self, ram_dump):
        h_irq_offset = ram_dump.field_offset('struct irq_desc', 'handle_irq')
        cpu_str = ''

        nr_irqs = ram_dump.read_int(ram_dump.addr_lookup('nr_irqs'))
        irq_descs = []

        for i in ram_dump.iter_cpus():
            cpu_str = cpu_str + '{0:10} '.format('CPU{0}'.format(i))

        print_out_str(
            '{0:4} {1} {2:30} {3:10}'.format('IRQ', cpu_str, 'Name', 'Chip'))

        if nr_irqs > 50000:
            return

        if (ram_dump.kernel_version[0], ram_dump.kernel_version[1]) >= (6, 4):
            mt_walk = maple_tree.MapleTreeWalker(ram_dump)
            irq_desc_tree = ram_dump.addr_lookup('sparse_irqs')
            mt_walk.walk(irq_desc_tree, self.save_irq_desc, irq_descs)
            for i in range(len(irq_descs)):
                self.dump_sparse_irq_state(ram_dump, irq_descs[i])
        else:
            irq_desc_tree = ram_dump.addr_lookup('irq_desc_tree')
            for i in range(0, nr_irqs):
                if (ram_dump.kernel_version[0],
                                     ram_dump.kernel_version[1]) >= (5, 4):
                    irq_desc = self.xarray_lookup_element(ram_dump,
                                           irq_desc_tree, i)
                else:
                    irq_desc = self.radix_tree_lookup_element(ram_dump,
                                           irq_desc_tree, i)
                self.dump_sparse_irq_state(ram_dump, irq_desc)

    def parse(self):
        irq_desc = self.ramdump.addr_lookup('irq_desc')
        if self.ramdump.is_config_defined('CONFIG_SPARSE_IRQ'):
            self.print_irq_state_sparse_irq(self.ramdump)

        if irq_desc is None:
            return

        self.print_irq_state_3_0(self.ramdump)
