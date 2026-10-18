#!/bin/sh

#
# Copyright (c) 2015-2016, 2019, The Linux Foundation. All rights reserved.
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

[ -e /lib/ipq806x.sh ] && . /lib/ipq806x.sh

type ipq806x_board_name &>/dev/null  || ipq806x_board_name() {
	local board="$(echo $(board_name) | sed 's/^\([^-]*-\)\{1\}//g')"
	if [[ "$board" == *rdp* ]]; then
		board=$(cat /tmp/sysinfo/board_name | awk -F, '{print $2}')
	fi
	echo "$board"
}

. /lib/functions.sh

ipq9574_phy_power_on()
{
	local board=$(ipq806x_board_name)
	case "$board" in
		ap-al01-c1 | ap-al02-c1 | ap-al02-c2 | db-al01-c1 | db-al01-c2 | db-al01-c3 |\
			db-al02-c1 | db-al02-c2)
			ssdk_sh port poweron set 2
			ssdk_sh port poweron set 3
			ssdk_sh port poweron set 4
			ssdk_sh port poweron set 5
			ssdk_sh port poweron set 6
		;;
		db-al02-c3)
			ssdk_sh port poweron set 2
			ssdk_sh port poweron set 3
			ssdk_sh port poweron set 4
			ssdk_sh port poweron set 6
		;;
	esac
}

ipq9574_phy_power_off()
{
	local board=$(ipq806x_board_name)
	case "$board" in
		ap-al01-c1 | ap-al02-c1 | ap-al02-c2 | db-al01-c1 | db-al01-c2 | db-al01-c3 |\
			db-al02-c1 | db-al02-c2)
			ssdk_sh port poweroff set 2
			ssdk_sh port poweroff set 3
			ssdk_sh port poweroff set 4
			ssdk_sh port poweroff set 5
			ssdk_sh port poweroff set 6
		;;
		db-al02-c3)
			ssdk_sh port poweroff set 2
			ssdk_sh port poweroff set 3
			ssdk_sh port poweroff set 4
			ssdk_sh port poweroff set 6
		;;
	esac
}

ipq9574_ac_power()
{
	echo "Entering AC-Power Mode"
# Cortex Power-UP Sequence
	/etc/init.d/powerctl restart

# Enabling Auto scale on NSS cores
	echo 1 > /proc/sys/dev/nss/clock/auto_scale

# Power on PHYs of LAN ports
	ipq9574_phy_power_on
# PCIe Power-UP Sequence
	sleep 1
	if [ -f /sys/bus/pci/rcrescan ]
	then
		echo 1 > /sys/bus/pci/rcrescan
	else
		echo 1 > /sys/bus/pci/rescan
	fi
	sleep 2

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

# LAN interface up
	ifup lan

# Wifi Power-up Sequence
	wifi_load.sh load

# SD/MMC Power-UP sequence
	local emmcblock="$(find_mmc_part "rootfs")"

	if [ -z "$emmcblock" ]; then
		for sd_drvname in $(cat /tmp/sysinfo/sd_drvname)
		do
			echo $sd_drvname > /sys/bus/platform/drivers/sdhci_msm/bind
		done
	fi

	sleep 1

	exit 0
}

ipq9574_battery_power()
{
	echo "Entering Battery Mode..."

# Wifi Power-down Sequence
	wifi_load.sh unload

# PCIe Power-Down Sequence
	if [ -f /sys/bus/pci/rcremove ]
	then
		echo 1 > /sys/bus/pci/rcremove
	else
		for i in `ls /sys/bus/pci/devices/`; do
			echo 1 > /sys/bus/pci/devices/${i}/remove
		done
	fi
	sleep 1

# Find scsi devices and remove it
	partition=`cat /proc/partitions | awk -F " " '{print $4}'`

	for entry in $partition; do
		sd_entry=$(echo $entry | head -c 2)

		if [ "$sd_entry" = "sd" ]; then
			[ -f /sys/block/$entry/device/delete ] && {
				echo 1 > /sys/block/$entry/device/delete
			}
		fi
	done


# Power off PHYs of LAN ports
        ipq9574_phy_power_off
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

#SD/MMC Power-down Sequence
	local emmcblock="$(find_mmc_part "rootfs")"

	if [ -z "$emmcblock" ]; then
		rm /tmp/sysinfo/sd_drvname
		if [ -d /sys/block/mmcblk0 ]; then
			sd_drvname=`readlink /sys/block/mmcblk0 | grep -o "[0-9]*.sdhci[^/]*"`
			echo "$sd_drvname" >> /tmp/sysinfo/sd_drvname
			echo $sd_drvname >> /sys/bus/platform/drivers/sdhci_msm/unbind
		fi
	fi

# LAN interface down
	ifdown lan

# Disabling Auto scale on NSS cores
	echo 0 > /proc/sys/dev/nss/clock/auto_scale

# Scaling Down UBI Cores
	echo 1500000000 > /proc/sys/dev/nss/clock/current_freq;

# Cortex Power-down Sequence
	echo "powersave" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

}

ipq5424_ac_power()
{
	echo "Entering AC-Power Mode"
# Cortex Power-UP Sequence
	/etc/init.d/powerctl restart

# Power on PHYs of LAN ports

# PCIe Power-UP Sequence
	sleep 1
	if [ -f /sys/bus/pci/rcrescan ]
	then
		echo 1 > /sys/bus/pci/rcrescan
	else
		echo 1 > /sys/bus/pci/rescan
	fi
	sleep 2

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

# LAN interface up
	ifup lan

# Wifi Power-up Sequence
	wifi_load.sh load

# SD/MMC Power-UP sequence
	local emmcblock="$(find_mmc_part "rootfs")"

	if [ -z "$emmcblock" ]; then
		for sd_drvname in $(cat /tmp/sysinfo/sd_drvname)
		do
			echo $sd_drvname > /sys/bus/platform/drivers/sdhci_msm/bind
		done
	fi

	sleep 1

	exit 0
}

ipq5424_battery_power()
{
	echo "Entering Battery Mode..."

# Wifi Power-down Sequence
	wifi_load.sh unload

# PCIe Power-Down Sequence
	if [ -f /sys/bus/pci/rcremove ]
	then
		echo 1 > /sys/bus/pci/rcremove
	else
		for i in `ls /sys/bus/pci/devices/`; do
			echo 1 > /sys/bus/pci/devices/${i}/remove
		done
	fi
	sleep 1

# Find scsi devices and remove it
	partition=`cat /proc/partitions | awk -F " " '{print $4}'`

	for entry in $partition; do
		sd_entry=$(echo $entry | head -c 2)

		if [ "$sd_entry" = "sd" ]; then
			[ -f /sys/block/$entry/device/delete ] && {
				echo 1 > /sys/block/$entry/device/delete
			}
		fi
	done


# Power off PHYs of LAN ports

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
		rmmod phy-qcom-qusb2
	fi
	sleep 2

#SD/MMC Power-down Sequence
	local emmcblock="$(find_mmc_part "rootfs")"

	if [ -z "$emmcblock" ]; then
		rm /tmp/sysinfo/sd_drvname
		if [ -d /sys/block/mmcblk0 ]; then
			sd_drvname=`readlink /sys/block/mmcblk0 | grep -o "[0-9]*.sdhci[^/]*"`
			echo "$sd_drvname" >> /tmp/sysinfo/sd_drvname
			echo $sd_drvname >> /sys/bus/platform/drivers/sdhci_msm/unbind
		fi
	fi

# LAN interface down
	ifdown lan

# Cortex Power-down Sequence
	echo "powersave" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

}

ipq5210_ac_power()
{
	echo "Entering AC-Power Mode"
# Cortex Power-UP Sequence
	/etc/init.d/powerctl restart

# Power on PHYs of LAN ports

# PCIe Power-UP Sequence
	sleep 1
	if [ -f /sys/bus/pci/rcrescan ]
	then
		echo 1 > /sys/bus/pci/rcrescan
	else
		echo 1 > /sys/bus/pci/rescan
	fi
	sleep 2

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

# LAN interface up
	ifup lan

# Wifi Power-up Sequence
	wifi_load.sh load

# SD/MMC Power-UP sequence
	local emmcblock="$(find_mmc_part "rootfs")"

	if [ -z "$emmcblock" ]; then
		for sd_drvname in $(cat /tmp/sysinfo/sd_drvname)
		do
			echo $sd_drvname > /sys/bus/platform/drivers/sdhci_msm/bind
		done
	fi

	sleep 1

	exit 0
}

ipq5210_battery_power()
{
	echo "Entering Battery Mode..."

# Wifi Power-down Sequence
	wifi_load.sh unload

# PCIe Power-Down Sequence
	if [ -f /sys/bus/pci/rcremove ]
	then
		echo 1 > /sys/bus/pci/rcremove
	else
		for i in `ls /sys/bus/pci/devices/`; do
			echo 1 > /sys/bus/pci/devices/${i}/remove
		done
	fi
	sleep 1

# Find scsi devices and remove it
	partition=`cat /proc/partitions | awk -F " " '{print $4}'`

	for entry in $partition; do
		sd_entry=$(echo $entry | head -c 2)

		if [ "$sd_entry" = "sd" ]; then
			[ -f /sys/block/$entry/device/delete ] && {
				echo 1 > /sys/block/$entry/device/delete
			}
		fi
	done


# Power off PHYs of LAN ports

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
		rmmod phy-qcom-qusb2
	fi
	sleep 2

#SD/MMC Power-down Sequence
	local emmcblock="$(find_mmc_part "rootfs")"

	if [ -z "$emmcblock" ]; then
		rm /tmp/sysinfo/sd_drvname
		if [ -d /sys/block/mmcblk0 ]; then
			sd_drvname=`readlink /sys/block/mmcblk0 | grep -o "[0-9]*.sdhci[^/]*"`
			echo "$sd_drvname" >> /tmp/sysinfo/sd_drvname
			echo $sd_drvname >> /sys/bus/platform/drivers/sdhci_msm/unbind
		fi
	fi

# LAN interface down
	ifdown lan

# Cortex Power-down Sequence
	echo "powersave" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

}

ipq9650_ac_power()
{
	echo "Entering AC-Power Mode"
# Cortex Power-UP Sequence
	/etc/init.d/powerctl restart

# Power on PHYs of LAN ports

# PCIe Power-UP Sequence
	sleep 1
	if [ -f /sys/bus/pci/rcrescan ]
	then
		echo 1 > /sys/bus/pci/rcrescan
	else
		echo 1 > /sys/bus/pci/rescan
	fi
	sleep 2

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

# LAN interface up
	ifup lan

# Wifi Power-up Sequence
	wifi_load.sh load

# SD/MMC Power-UP sequence
	local emmcblock="$(find_mmc_part "rootfs")"

	if [ -z "$emmcblock" ]; then
		for sd_drvname in $(cat /tmp/sysinfo/sd_drvname)
		do
			echo $sd_drvname > /sys/bus/platform/drivers/sdhci_msm/bind
		done
	fi

	sleep 1

	exit 0
}

ipq9650_battery_power()
{
	echo "Entering Battery Mode..."

# Wifi Power-down Sequence
	wifi_load.sh unload

# PCIe Power-Down Sequence
	if [ -f /sys/bus/pci/rcremove ]
	then
		echo 1 > /sys/bus/pci/rcremove
	else
		for i in `ls /sys/bus/pci/devices/`; do
			echo 1 > /sys/bus/pci/devices/${i}/remove
		done
	fi
	sleep 1

# Find scsi devices and remove it
	partition=`cat /proc/partitions | awk -F " " '{print $4}'`

	for entry in $partition; do
		sd_entry=$(echo $entry | head -c 2)

		if [ "$sd_entry" = "sd" ]; then
			[ -f /sys/block/$entry/device/delete ] && {
				echo 1 > /sys/block/$entry/device/delete
			}
		fi
	done


# Power off PHYs of LAN ports

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
		rmmod phy-qcom-qusb2
	fi
	sleep 2

#SD/MMC Power-down Sequence
	local emmcblock="$(find_mmc_part "rootfs")"

	if [ -z "$emmcblock" ]; then
		rm /tmp/sysinfo/sd_drvname
		if [ -d /sys/block/mmcblk0 ]; then
			sd_drvname=`readlink /sys/block/mmcblk0 | grep -o "[0-9]*.sdhci[^/]*"`
			echo "$sd_drvname" >> /tmp/sysinfo/sd_drvname
			echo $sd_drvname >> /sys/bus/platform/drivers/sdhci_msm/unbind
		fi
	fi

# LAN interface down
	ifdown lan

# Cortex Power-down Sequence
	echo "powersave" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor
	echo "powersave" > /sys/devices/system/cpu/cpu4/cpufreq/scaling_governor
}

ipq5332_phy_power_on()
{
	local board=$(ipq806x_board_name)
	case "$board" in
		ap-mi01.6)
			echo 1 > /sys/ssdk/dev_id
			ssdk_sh port poweron set 1
			ssdk_sh port poweron set 2
			ssdk_sh port poweron set 3
			echo 0 > /sys/ssdk/dev_id
			;;
		ap-mi01.2 | ap-mi01.4 | ap-mi01.3 | ap-mi04.1 |\
		ap-mi01.9 | ap-mi01.2-qcn9160-c1 | ap-mi04.1-c2)
			echo 1 > /sys/ssdk/dev_id
			ssdk_sh port poweron set 1
			ssdk_sh port poweron set 2
			ssdk_sh port poweron set 3
			ssdk_sh port poweron set 4
			echo 0 > /sys/ssdk/dev_id
			;;
		db-mi01.1 | db-mi03.1)
			ssdk_sh port poweron set 1
			;;
		ap-mi03.1)
			echo 1 > /sys/ssdk/dev_id
			ssdk_sh port poweron set 3
			ssdk_sh port poweron set 4
			ssdk_sh port poweron set 5
			echo 0 > /sys/ssdk/dev_id
			;;
	esac
}

ipq5332_phy_power_off()
{
	local board=$(ipq806x_board_name)
	case "$board" in
		ap-mi01.6)
			echo 1 > /sys/ssdk/dev_id
			ssdk_sh port poweroff set 1
			ssdk_sh port poweroff set 2
			ssdk_sh port poweroff set 3
			echo 0 > /sys/ssdk/dev_id
			;;
		ap-mi01.2 | ap-mi01.4 | ap-mi01.3 | ap-mi04.1 |\
		ap-mi01.9 | ap-mi01.2-qcn9160-c1 | ap-mi04.1-c2)
			echo 1 > /sys/ssdk/dev_id
			ssdk_sh port poweroff set 1
			ssdk_sh port poweroff set 2
			ssdk_sh port poweroff set 3
			ssdk_sh port poweroff set 4
			echo 0 > /sys/ssdk/dev_id
			;;
		db-mi01.1 | db-mi03.1)
			ssdk_sh port poweroff set 1
			;;
		ap-mi03.1)
			echo 1 > /sys/ssdk/dev_id
			ssdk_sh port poweroff set 3
			ssdk_sh port poweroff set 4
			ssdk_sh port poweroff set 5
			echo 0 > /sys/ssdk/dev_id
			;;
	esac
}

ipq5332_ac_power()
{
	echo "Entering AC-Power Mode"
# Cortex Power-UP Sequence
	/etc/init.d/powerctl restart

# Enabling Auto scale on NSS cores
	echo 1 > /proc/sys/dev/nss/clock/auto_scale

# Power on PHYs of LAN ports
	ipq5332_phy_power_on
# PCIe Power-UP Sequence
	sleep 1
	if [ -f /sys/bus/pci/rcrescan ]
	then
		echo 1 > /sys/bus/pci/rcrescan
	else
		echo 1 > /sys/bus/pci/rescan
	fi
	sleep 2

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

# LAN interface up
	ifup lan

# Wifi Power-up Sequence
	wifi_load.sh load

# SD/MMC Power-UP sequence
	local emmcblock="$(find_mmc_part "rootfs")"

	if [ -z "$emmcblock" ]; then
		for sd_drvname in $(cat /tmp/sysinfo/sd_drvname)
		do
			echo $sd_drvname > /sys/bus/platform/drivers/sdhci_msm/bind
		done
	fi

	sleep 1

	exit 0
}

ipq5332_battery_power()
{
	echo "Entering Battery Mode..."

# Wifi Power-down Sequence
	wifi_load.sh unload

# PCIe Power-Down Sequence
	if [ -f /sys/bus/pci/rcremove ]
	then
		echo 1 > /sys/bus/pci/rcremove
	else
		for i in `ls /sys/bus/pci/devices/`; do
			echo 1 > /sys/bus/pci/devices/${i}/remove
		done
	fi
	sleep 1

# Find scsi devices and remove it
	partition=`cat /proc/partitions | awk -F " " '{print $4}'`

	for entry in $partition; do
		sd_entry=$(echo $entry | head -c 2)

		if [ "$sd_entry" = "sd" ]; then
			[ -f /sys/block/$entry/device/delete ] && {
				echo 1 > /sys/block/$entry/device/delete
			}
		fi
	done


# Power off PHYs of LAN ports
	ipq5332_phy_power_off
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

#SD/MMC Power-down Sequence
	local emmcblock="$(find_mmc_part "rootfs")"

	if [ -z "$emmcblock" ]; then
		rm /tmp/sysinfo/sd_drvname
		if [ -d /sys/block/mmcblk0 ]; then
			sd_drvname=`readlink /sys/block/mmcblk0 | grep -o "[0-9]*.sdhci[^/]*"`
			echo "$sd_drvname" >> /tmp/sysinfo/sd_drvname
			echo $sd_drvname >> /sys/bus/platform/drivers/sdhci_msm/unbind
		fi
	fi

# LAN interface down
	ifdown lan

# Disabling Auto scale on NSS cores
	echo 0 > /proc/sys/dev/nss/clock/auto_scale

# Cortex Power-down Sequence
	echo "powersave" > /sys/devices/system/cpu/cpu0/cpufreq/scaling_governor

}

board=$(ipq806x_board_name)
case "$1" in
	false)
		case "$board" in
		ap-al* | db-al*)
			ipq9574_ac_power ;;
		ap-emu* | ap-mi* | db-mi*)
			ipq5332_ac_power ;;
		ipq5424*)
			ipq5424_ac_power ;;
		ipq5210*)
			ipq5210_ac_power ;;
		ipq9650*)
			ipq9650_ac_power ;;
		esac ;;
	true)
		case "$board" in
		ap-al* | db-al*)
			ipq9574_battery_power ;;
		ap-emu* | ap-mi* | db-mi*)
			ipq5332_battery_power ;;
		ipq5424*)
			ipq5424_battery_power ;;
		ipq5210*)
			ipq5210_battery_power ;;
		ipq9650*)
			ipq9650_battery_power ;;
		esac ;;
esac
