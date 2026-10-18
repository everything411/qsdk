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


DT_BASE="/proc/device-tree/soc@0/eip*"

# Read reg base (DT order)
EIP_REG_BASE_HEX=$(hexdump -n 4 -e '4/1 "%02x"' \
  $DT_BASE/reg)

# Default offset = 0
EIP_REG_OFFSET_HEX=00000000

# Some boards use reg-offset
if [ -f $DT_BASE/reg-offset ]; then
    EIP_REG_OFFSET_HEX=$(hexdump -n 4 -e '4/1 "%02x"' \
      $DT_BASE/reg-offset)

# Some boards use reg_offset
elif [ -f $DT_BASE/reg_offset ]; then
    EIP_REG_OFFSET_HEX=$(hexdump -n 4 -e '4/1 "%02x"' \
      $DT_BASE/reg_offset)
fi

# Version register address = base + 0xFFFFC (before offset)
EIP_VERSION_ADDR=0x$(printf "%08x" $((0x$EIP_REG_BASE_HEX + 0xFFFFC)))

# Final HW base = base + optional offset
EIP_BASE_HEX=$(printf "%08x" \
  $((0x$EIP_REG_BASE_HEX + 0x$EIP_REG_OFFSET_HEX)))

EIP_BASE=0x$EIP_BASE_HEX

MAX_DESC_MASK=255
EIP_NUMBER=0
RING_COUNT=0
EIP_196=196
EIP_197=197

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
	EIP_NUMBER=$(( $(devmem "$EIP_VERSION_ADDR") & 0xFF ))
	if [ $EIP_NUMBER -eq 197 ]; then
		RING_COUNT=8
	else
		RING_COUNT=4
	fi
}

print_version() {
	local VERSION_REG

	VERSION_REG=$(devmem "$EIP_VERSION_ADDR")

	echo "EIP Number 	: $(( VERSION_REG & 0xFF ))"
	echo "EIP version 	: $(( (VERSION_REG & 0x0F000000) >> 24 )).$(( (VERSION_REG & 0xF00000) >> 20 ))"
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

	# Check if ring is configured; CDR base address register is 0x0 for unconfigured rings
	cdr_base_val=$(($(devmem $(($EIP_BASE + $ring_base + 0x0)))))
	if [ $cdr_base_val -eq 0 ]; then
		echo "====DMA ID: $ring_id is not configured, skipping===="
		return
	fi

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

	# Check if CDR ring is configured; CDR base address register is 0x0 for unconfigured rings
	cdr_base_val=$(($(devmem $ring_fifo_addr)))
	if [ $cdr_base_val -eq 0 ]; then
		echo "==== Command ring id: $ring_id is not configured, skipping===="
		return
	fi

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

	# Check if RDR ring is configured; RDR base address register is 0x0 for unconfigured rings
	rdr_base_val=$(($(devmem $ring_fifo_addr)))
	if [ $rdr_base_val -eq 0 ]; then
		echo "==== Result Ring id : $ring_id  is not configured, skipping===="
		return
	fi

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

get_ctx_name() {
	case "$1" in
		"ahash")      echo "eip_ahash_ctx" ;;
		"aead")       echo "eip_aead_ctx" ;;
		"dtls")       echo "eip_dtls_ctx" ;;
		"ipsec")      echo "eip_ipsec_ctx" ;;
		"ipsec_hy")   echo "eip_hy_ipsec_ctx" ;;
		"skcipher")   echo "eip_skcipher_ctx" ;;
		*)            return 1 ;;
	esac
}

get_tr_size() {
	if [ "$EIP_NUMBER" -eq "$EIP_197" ] && [ $(($1 & 3)) -eq 3 ]; then
		echo $((320 - 0x10))
	elif [ "$EIP_NUMBER" -eq "$EIP_197" ] && [ $(($1 & 3)) -eq 2 ]; then
		echo $((256 - 0x10))
	elif [ "$EIP_NUMBER" -eq "$EIP_196" ] && [ $(($1 & 3)) -eq 3 ]; then
		echo $((256 - 0x10))
	elif [ "$EIP_NUMBER" -eq "$EIP_196" ] && [ $(($1 & 3)) -eq 2 ]; then
		echo $((192 - 0x10))
	else
		echo "Unable to determine tr size"
	fi
}

dump_tr() {
	ctx_name=$(get_ctx_name "$1")
	dir=$(find "/sys/kernel/debug/qca-nss-eip/eip$EIP_NUMBER" -maxdepth 1 -type f -name "${ctx_name}*" | head -n 1)
	if [ -z $dir ]; then
		echo "No context entry found for $1"
		exit 1
	fi

	# 4th field is the tr address, remove the parentheses to get the address value.
	tr_addr=$(find "$dir" -type f -print -exec cat {} \; | grep "Transform" | cut -d " " -f 4 | tr -d "()" | sed -n "$2p")
	if [ -z "$tr_addr" ]; then
		echo "No TR address found for: $1"
		exit 1
	fi

	# tr_addr can have multiple entries, traverse it one by one
	# Determine the large/small transform record size for printing
	for addr in $tr_addr
	do
		addr=$((0x$addr))
		printf 'Transform record addr for %s: 0x%x\n' "$1" "$addr"
		tr_size=$(get_tr_size $addr)
		addr=$(($addr & ~3))

		words=0
		while [ $words -le $tr_size ]
		do
			echo "`devmem $(($addr + $words))` `devmem $(($addr + $words + 4))` `devmem $(($addr + $words + 8))` `devmem $(($addr + $words + 0xC))`"
			words=$(($words + 0x10))
		done
		echo ""
	done
}

dump_eip_hw() {
	start_addr=$EIP_BASE
	size=0x$(hexdump -s 4 -n 4 -e '4/1 "%02x"' /proc/device-tree/soc@0/eip*/reg)

	echo "Starting EIP$EIP_NUMBER HW dump"
	echo "It may take sometime to finish, start logging terminal output in a file"
	echo "Stop any running traffic or keep the device in issue state for accurate register content"
	echo "Press any key to continue..."
	read -n 1 -s

	for i in $(seq 0 $(($size / 4)))
	do
		addr=$(($start_addr + i * 4))
		val=$(devmem $addr)
		printf '0x%x:\t0x%x\n' $addr $val
	done
}

dump_stats() {
	irq_affinity_num=$(grep -E -m1 'eip_irq_ring_0' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' ')
	[ -n "$irq_affinity_num" ] && echo -n "eip_irq_ring_0 affinity- " && cat /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=$(grep -E -m1 'eip_irq_ring_1' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' ')
	[ -n "$irq_affinity_num" ] && echo -n "eip_irq_ring_1 affinity- " && cat /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=$(grep -E -m1 'eip_irq_ring_2' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' ')
	[ -n "$irq_affinity_num" ] && echo -n "eip_irq_ring_2 affinity- " && cat /proc/irq/$irq_affinity_num/smp_affinity
	irq_affinity_num=$(grep -E -m1 'eip_irq_ring_3' /proc/interrupts | cut -d ':' -f 1 | tail -n1 | tr -d ' ')
	[ -n "$irq_affinity_num" ] && echo -n "eip_irq_ring_3 affinity- " && cat /proc/irq/$irq_affinity_num/smp_affinity

	echo "cat /proc/interrupts |grep eip"
	cat /proc/interrupts |grep eip
	find /sys/module/qca_nss_eip/parameters/ -type f -print -exec cat {} \;

	#Dump ipsec client module param if its loaded
	if [ -d /sys/module/qca_nss_eip_ipsec ]; then
		find /sys/module/qca_nss_eip_ipsec/parameters/ -type f -print -exec cat {} \;
	fi

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

	ip xfrm state
	ip xfrm policy

	echo "Total Tunnel: `ls -d /sys/kernel/debug/qca-nss-eip/eip$EIP_NUMBER/ipsectun* | wc -l`"
	find /sys/kernel/debug/qca-nss-eip/eip$EIP_NUMBER/$id -type f -print -exec cat {} \;

	if [ $EIP_NUMBER -eq $EIP_197 ]; then
		find /sys/kernel/debug/qca-nss-eip/eip$EIP_NUMBER/eip_hy_ipsec_ctx* -type f -print -exec cat {} \;
	else
		find /sys/kernel/debug/qca-nss-eip/eip$EIP_NUMBER/eip_ipsec_ctx* -type f -print -exec cat {} \;
	fi
}

dump_flows() {
	if [ $EIP_NUMBER -ne 197 ]; then
		echo "Flow table dump is only supported on EIP197"
		return
	fi
	echo "====Flow Table Dump:===="
	find /sys/kernel/debug/qca-nss-eip/eip$EIP_NUMBER/eip_flow_table -type f -print -exec cat {} \;
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
	dump_flows
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
	echo "	flows: Dumps flow table (EIP197 only)"
	echo "	dump_tr [name] : Dumps transform record for given service name"
	echo "	Supported name for dump_tr: aead/ahash/dtls/ipsec/ipsec_hy/skcipher"
	echo "	hw_dump : Dumps all EIP HW register, avoid using it until its necessary!"
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
	"dump_tr")
		load_version
		dump_tr ${2:-0}
		;;
	"ipsec")
		load_version
		dump_ipsec "ipsectun*"
		;;
	ipsec*)
		load_version
		dump_ipsec $1
		;;
	"flows")
		load_version
		dump_flows
		;;
	"hw_dump")
		load_version
		dump_eip_hw
		;;
	*)
		usage_msg
		;;
esac
