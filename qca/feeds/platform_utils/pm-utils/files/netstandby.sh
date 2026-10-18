#!/bin/sh

#
# Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
#
# Permission to use, copy, modify, and/or distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL  WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER
# RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
# CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
# CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
#

type ipq_board_name &>/dev/null  || ipq_board_name() {
	local board="$(echo $(board_name) | sed 's/^\([^-]*-\)\{1\}//g')"
	if [[ "$board" == *rdp* ]]; then
		board=$(cat /tmp/sysinfo/board_name | awk -F, '{print$2}')
	fi
	echo "$board"
}

. /lib/functions.sh

ipq9574_enter_standby()
{
# USB Power-down Sequence
	if [ -d config/usb_gadget/g1 ]
	then
		echo "" > /config/usb_gadget/g1/UDC
	fi

	if [ -d /sys/module/dwc3_qcom ]
	then
		rmmod usb_f_qdss
		rmmod dwc3
		rmmod dwc3_qcom
		rmmod phy_qcom_qusb2
	fi
	sleep 2
}

ipq9574_exit_standby()
{
# USB Power-UP Sequence
	if [ -e /lib/modules/$(uname -r)/dwc3-qcom.ko ]
	then
		insmod phy-qcom-qusb2.ko
		insmod dwc3-qcom.ko
		insmod dwc3.ko
		insmod usb_f_qdss.ko
	fi

	if [ -d config/usb_gadget/g1 ]
	then
		echo "8a00000.dwc3" > /config/usb_gadget/g1/UDC
	fi
}

ipq5332_enter_standby()
{
# USB Power-down Sequence
	if [ -d config/usb_gadget/g1 ]
	then
		echo "" > /config/usb_gadget/g1/UDC
	fi

	if [ -d /sys/module/dwc3_qcom ]
	then
		rmmod usb_f_qdss
		rmmod dwc3
		rmmod dwc3_qcom
		rmmod phy-qca-uniphy.ko
		rmmod phy-qca-m31.ko
	fi
	sleep 2
}

ipq5332_exit_standby()
{
# USB Power-UP Sequence
	if [ -e /lib/modules/$(uname -r)/dwc3-qcom.ko ]
	then
		insmod phy-qca-uniphy.ko
		insmod phy-qca-m31.ko
		insmod dwc3-qcom.ko
		insmod dwc3.ko
		insmod usb_f_qdss.ko
	fi

	if [ -d config/usb_gadget/g1 ]
	then
		echo "8a00000.dwc3" > /config/usb_gadget/g1/UDC
	fi
}

ipq5424_enter_standby()
{
# USB Power-down Sequence
	if [ -d config/usb_gadget/g1 ]
	then
		echo "" > /config/usb_gadget/g1/UDC
	fi

	if [ -d /sys/module/dwc3_qcom ]
	then
		rmmod usb_f_qdss
		rmmod dwc3
		rmmod dwc3_qcom
		rmmod phy_qcom_qusb2
	fi
	sleep 2
}

ipq5424_exit_standby()
{
# USB Power-UP Sequence
	if [ -e /lib/modules/$(uname -r)/dwc3-qcom.ko ]
	then
		insmod phy-qcom-qusb2.ko
		insmod dwc3-qcom.ko
		insmod dwc3.ko
		insmod usb_f_qdss.ko
	fi

	if [ -d config/usb_gadget/g1 ]
	then
		echo "8a00000.dwc3" > /config/usb_gadget/g1/UDC
	fi
}

board=$(ipq_board_name)
case "$1" in
	enter)
		case "$board" in
		ap-al* | db-al*)
			ipq9574_enter_standby ;;
		ap-mi* | db-mi*)
			ipq5332_enter_standby ;;
		ipq5424*)
			ipq5424_enter_standby ;;
		esac ;;
	exit)
		case "$board" in
		ap-al* | db-al*)
			ipq9574_exit_standby ;;
		ap-mi* | db-mi*)
			ipq5332_exit_standby ;;
		ipq5424*)
			ipq5424_exit_standby ;;
		esac ;;
esac
