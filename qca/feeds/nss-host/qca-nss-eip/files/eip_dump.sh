#!/bin/sh
#
#
# Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#


# usage: eip_dump.sh [all/reg/stats/desc/help] [ring] [count]
# all - Dumps every information
# reg - Dumps EIP general registers
# stats - Dumps statistics
# desc - Dumps recently filled result and command descriptor equal to [count] for [ring]
#
# with no parameters, eip_dump.sh will print all debug information.

EIP_BASE=$((0x39880000))
MAX_DESC_MASK=255
EIP_NUMBER=0
RING_COUNT=0

get_first_set_bit_lsb() {
	bitmask=$1
	pos=0
	while [ $((bitmask & 0x1)) == 0 ]
	do
		bitmask=$(($bitmask >> 1))
		pos=$(($pos + 1))
	done

	echo $pos
}

print_mem () {
	name=$1
	addr=$2
	mask=${3:-0xFFFFFFFF}
	val=$(devmem $addr)
	bitpos=$(get_first_set_bit_lsb $mask)
	printf '\t%s :\t0x%x\n' "$name" $((($val & $mask) >> $bitpos))
}

print_reg () {
	reg_addr=$(($EIP_BASE + $2))
	print_mem $1 $reg_addr $3
}

load_version() {
	EIP_NUMBER=$((($(devmem 0x398FFFFC) & 0xFF) >> 0))
	if [ $EIP_NUMBER -eq 197 ]; then
		RING_COUNT=8
	else
		RING_COUNT=4
	fi
}

print_version() {
	echo "EIP Number : $EIP_NUMBER"
	echo "Major version : 3.$((($(devmem 0x398FFFFC) & 0xF000000) >> 24))"
}

dump_hia_reg () {
	echo "====HIA Register dump:===="
	print_reg "data_fetch_dma_err" 	0x0C044 0x80000000
	print_reg "data_store_dma_err" 	0x0D044 0x80000000
	print_reg "la_cmd_desc_err"	0x1FF00 0x80000000
	print_reg "la_res_desc_err"	0x1FF04 0x80000000
	print_reg "hia_timeout_err"	0x1FFD0
	print_reg "axi_err"		0x1FFF4 0x00006000
	echo ""
}

dump_pe_reg() {
	echo "====PE Register dump:===="
	print_reg "ipue_ctrl"		0x20C80
	print_reg "ipue_debug"		0x20C84
	print_reg "ifpp_ctrl"		0x20D80
	print_reg "ifpp_debug"		0x20D84
	print_reg "contex_error"	0x21008
	print_reg "opue_ctrl"		0x21880
	print_reg "opue_debug"		0x21884
	print_reg "ofpp_ctrl"		0x21980
	print_reg "ofpp_debug"		0x21984
	print_reg "current_iv0"		0x21210
	print_reg "current_iv1"		0x21214
	print_reg "current_iv2"		0x21218
	print_reg "current_iv3"		0x2121C
	print_reg "trc_dma_read_err"	0x70820 0x40000000
	print_reg "trc_dma_wr_err"	0x70820 0x80000000
	echo ""
}

dump_dma_reg() {
	ring_id=$1
	ring_base=$((0x1000 * $ring_id))
	echo "====DMA ID: $ring_id Register dump:===="
	print_reg "cmd_ring_addr_low"	$(($ring_base + 0x0))
	print_reg "cmd_ring_addr_high"	$(($ring_base + 0x4))
	print_reg "cmd_ring_size"	$(($ring_base + 0x18)) 0xFFFFFC
	print_reg "cmd_config"		$(($ring_base + 0x20))
	print_reg "cmd_dma_config"	$(($ring_base + 0x24))
	print_reg "cmd_thres_pkt_mode"	$(($ring_base + 0x28)) 0x800000
	print_reg "cmd_thres_count"	$(($ring_base + 0x28)) 0x3FFFFF
	print_reg "cmd_prep_words"	$(($ring_base + 0x2C)) 0xFFFFFC
	print_reg "cmd_proc_words"	$(($ring_base + 0x30)) 0xFFFFFC
	print_reg "cmd_prep_ptr_words"	$(($ring_base + 0x34)) 0xFFFFFC
	print_reg "cmd_proc_ptr_words"	$(($ring_base + 0x38)) 0xFFFFFC
	print_reg "cmd_dma_error"	$(($ring_base + 0x3C)) 0x13F

	print_reg "res_ring_addr_low"	$(($ring_base + 0x800))
	print_reg "res_ring_addr_high"	$(($ring_base + 0x804))
	print_reg "res_ring_size"	$(($ring_base + 0x818)) 0xFFFFFC
	print_reg "cmd_config"		$(($ring_base + 0x820))
	print_reg "cmd_dma_config"	$(($ring_base + 0x824))
	print_reg "cmd_thres_pkt_mode"	$(($ring_base + 0x828)) 0x800000
	print_reg "res_thres_count"	$(($ring_base + 0x828)) 0x3FFFFF
	print_reg "res_prep_words"	$(($ring_base + 0x82C)) 0xFFFFFC
	print_reg "res_proc_words"	$(($ring_base + 0x830)) 0xFFFFFC
	print_reg "res_prep_ptr_words"	$(($ring_base + 0x834)) 0xFFFFFC
	print_reg "res_proc_ptr_words"	$(($ring_base + 0x838)) 0xFFFFFC
	print_reg "res_dma_error"	$(($ring_base + 0x83C)) 0x13F
}

dump_cmd_desc() {
	ring_id=$1
	ring_fifo_addr=$((0x1000 * $ring_id + 0x0 + $EIP_BASE))
	ring_prep_ptr_addr=$((0x1000 * $ring_id + 0x34 + $EIP_BASE))
	prep_val=$(($(devmem $ring_prep_ptr_addr) & 0xFFFFFF))
	desc_idx=$(($prep_val / 64))

	i=1
	while [ $i -le $2 ]
	do
		desc_idx=$((($desc_idx - 1) & $MAX_DESC_MASK))
		desc_addr=$(($(devmem $ring_fifo_addr) + (64 * $desc_idx)))

		echo "====Dump for Command Descriptor idx $ring_id:$desc_idx addr($desc_addr):===="
		print_mem "Frag Size"		$desc_addr 0x0000FFFF
		print_mem "Frag header"		$desc_addr 0xFFFF0000
		print_mem "Data pointer"	$(($desc_addr + 0x8))
		print_mem "token pointer"	$(($desc_addr + 0x10))
		print_mem "Data length"		$(($desc_addr + 0x18)) 0x0000FFFF
		print_mem "command header"	$(($desc_addr + 0x18)) 0xFFFF0000
		print_mem "Record pointer"	$(($desc_addr + 0x20))
		print_mem "hw_service_word"	$(($desc_addr + 0x28))
		print_mem "Flags word"		$(($desc_addr + 0x2C))
		print_mem "Meta 0"		$(($desc_addr + 0x30))
		print_mem "Meta 1"		$(($desc_addr + 0x34))

		i=$(($i + 1))
	done
}

dump_res_desc() {
	ring_id=$1
	ring_fifo_addr=$((0x1000 * $ring_id + 0x800 + $EIP_BASE))
	ring_prep_ptr_addr=$((0x1000 * $ring_id + 0x834 + $EIP_BASE))
	prep_val=$(($(devmem $ring_prep_ptr_addr) & 0xFFFFFF))
	desc_idx=$(($prep_val / 64))

	i=1
	while [ $i -le $2 ]
	do
		desc_idx=$((($desc_idx - 1) & $MAX_DESC_MASK))
		desc_addr=$(($(devmem $ring_fifo_addr) + (64 * $desc_idx)))

		echo "====Dump for Result Descriptor idx $ring_id:$desc_idx addr($desc_addr):===="
		print_mem "Frag Size"		$desc_addr 0x0000FFFF
		print_mem "Frag header"		$desc_addr 0xFFFF0000
		print_mem "Data pointer"	$(($desc_addr + 0x8))
		print_mem "Data length"		$(($desc_addr + 0x10)) 0x0000FFFF
		print_mem "Detailed Error enabled"	$(($desc_addr + 0x10)) 0x80000000
		print_mem "Error code"		$(($desc_addr + 0x10)) 0x7FFE0000
		print_mem "CLE code"		$(($desc_addr + 0x14)) 0x001F0000
		print_mem "E14..E15"		$(($desc_addr + 0x14)) 0x000001F0
		print_mem "Record pointer"	$(($desc_addr + 0x20))
		print_mem "hw_service_word"	$(($desc_addr + 0x2C))
		print_mem "Meta 0"		$(($desc_addr + 0x30))
		print_mem "Meta 1"		$(($desc_addr + 0x34))

		i=$(($i + 1))
	done
}

dump_stats() {
	irq_affinity_num=$(grep -E -m1 'eip_ring_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' ')
	[ -n "$irq_affinity_num" ] && echo -n "eip_ring_0 affinity- " && cat /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=$(grep -E -m1 'eip_ring_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' ')
	[ -n "$irq_affinity_num" ] && echo -n "eip_ring_1 affinity- " && cat /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=$(grep -E -m1 'eip_ring_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' ')
	[ -n "$irq_affinity_num" ] && echo -n "eip_ring_2 affinity- " && cat /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=$(grep -E -m1 'eip_ring_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' ')
	[ -n "$irq_affinity_num" ] && echo -n "eip_ring_3 affinity- " && cat /proc/irq/$irq_affinity_num/smp_affinity

	echo "cat /proc/interrupts |grep eip"
	cat /proc/interrupts |grep eip
	find /sys/kernel/debug/qca-nss-eip/eip$EIP_NUMBER/ -type f -print -exec cat {} \;
}

#Invoke default dump function
dump_reg() {
	print_version
	dump_hia_reg
	dump_pe_reg
	r=0
	while [ $r -lt $RING_COUNT ]; do
		dump_dma_reg $r
		r=$(($r + 1))
	done
}

dump_ipsec() {
	id=$1
	for iface in /sys/class/net/$id ; do
		if [ -d "$iface" ]; then
			echo "ifconfig ${iface##*/}"
			ifconfig ${iface##*/}
		fi
	done
	echo "Total Tunnel: `ls -d /sys/kernel/debug/qca-nss-eip/eip$EIP_NUMBER/ipsectun* | wc -l`"
	find /sys/kernel/debug/qca-nss-eip/eip$EIP_NUMBER/$id -type f -print -exec cat {} \;

	if [ $EIP_NUMBER -eq 197 ]; then
		find /sys/kernel/debug/qca-nss-eip/eip$EIP_NUMBER/eip_hy_ipsec_ctx* -type f -print -exec cat {} \;
	else
		find /sys/kernel/debug/qca-nss-eip/eip$EIP_NUMBER/eip_ipsec_ctx* -type f -print -exec cat {} \;
	fi
}

#Invoke default dump function
dump_all() {
	dump_reg

	r=0
	while [ $r -lt $RING_COUNT ]; do
		dump_cmd_desc $r 1
		if [ $r -lt 4 ]; then
			dump_res_desc $r 1
		fi
		r=$(($r + 1))
	done

	dump_stats
	dump_ipsec "ipsectun*"
}

usage_msg () {
	echo "Usage:"
	echo "eip_dump.sh <param>"
	echo "	all: Dumps Everything"
	echo "	ipsec: Dumps statistics for all IPsec tunnel"
	echo "	ipsectun<0|1|..>: Dumps statistics for specified ipsectunX"
	echo "	reg: Dumps general Register configuration"
	echo "	stats: Dumps all statistics"
	echo "	desc [ring] [count] : Dumps recent command & result descriptor equal to count"
	echo "	cmd [ring] [count] : Dumps recent command descriptor equal to count"
	echo "	sh -x <script> : Run script in trace mode (optional)"
	echo ""
}

case "${1:-all}" in

	"all")
		load_version
		dump_all
		;;
	"reg")
		load_version
		dump_reg
		;;
	"stats")
		load_version
		dump_stats
		;;
	"desc")
		load_version
		dump_cmd_desc ${2:-0} ${3:-1}
		dump_res_desc ${2:-0} ${3:-1}
		;;
	"cmd")
		load_version
		dump_cmd_desc ${2:-0} ${3:-1}
		;;
	"ipsec")
		load_version
		dump_ipsec "ipsectun*"
		;;
	ipsec*)
		load_version
		dump_ipsec $1
		;;
	*)
		usage_msg
		;;
esac
