#!/bin/sh
#
# Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
# SPDX-License-Identifier: ISC 
#

#echo "All info:TYPE:$type; IFNAME:$ifname; IPADDR:$ipaddr; LINK:$link" > /dev/console
case $type in
	NEWADDR)	echo "newaddr $ifname $ipaddr"
			exec /etc/omci/script/pots/omci_voip.sh device_change "$type" "$ifname" "$ipaddr"
	;;
	DELADDR)	echo "deladdr $ifname";;
	DELLINK)	echo "dellink $ifname";;
	NEWLINK)	echo "newlink $ifname $link"
			exec /etc/omci/script/pots/omci_voip.sh device_change "$type" "$ifname" "$ipaddr"
	;;
	*)			echo "error type";;
esac
