#!/bin/sh
#
# Copyright (c) 2016, The Linux Foundation. All rights reserved.

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

ubootenv_add_uci_config() {
	local dev=$1
	local offset=$2
	local envsize=$3
	local secsize=$4
	local numsec=$5
	uci batch <<EOF
add ubootenv ubootenv
set ubootenv.@ubootenv[-1].dev='$dev'
set ubootenv.@ubootenv[-1].offset='$offset'
set ubootenv.@ubootenv[-1].envsize='$envsize'
set ubootenv.@ubootenv[-1].secsize='$secsize'
set ubootenv.@ubootenv[-1].numsec='$numsec'
EOF
	uci commit ubootenv
}

ubootenv_add_app_config() {
	local dev
	local offset
	local envsize
	local secsize
	local numsec
	config_get dev "$1" dev
	config_get offset "$1" offset
	config_get envsize "$1" envsize
	config_get secsize "$1" secsize
	config_get numsec "$1" numsec
	echo "$dev $offset $envsize $secsize $numsec" >>/etc/fw_env.config
}

