#!/bin/sh
[ -e /lib/functions.sh ] && . /lib/functions.sh
append DRIVERS "mac80211"

MLD_VAP_DETAILS="/lib/netifd/wireless/wifi_mld_cfg.config"

configure_service_param() {
	enable_service=$2
	phy=$3
	json_load "$1"
	json_get_var svc_id svc_id
	json_get_var disable disable

	[ -z "$disable" ] && disable='0'

	if [ $enable_service -eq 1 ] && [ "$disable" -eq 0 ]; then
		json_get_var app_name app_name
		json_get_var min_thruput_rate min_thruput_rate
		json_get_var max_thruput_rate max_thruput_rate
		json_get_var burst_size burst_size
		json_get_var service_interval service_interval
		json_get_var delay_bound delay_bound
		json_get_var msdu_ttl msdu_ttl
		json_get_var priority priority
		json_get_var tid tid
		json_get_var msdu_rate_loss msdu_rate_loss

		cmd="iw $phy service_class create $svc_id "
		[ ! -z "$app_name" ] && cmd=$cmd"'$app_name' "
		[ ! -z "$min_thruput_rate" ] && cmd=$cmd"min_tput $min_thruput_rate "
		[ ! -z "$max_thruput_rate" ] && cmd=$cmd"max_tput $max_thruput_rate "
		[ ! -z "$burst_size" ] && cmd=$cmd"burst_size $burst_size "
		[ ! -z "$service_interval" ] && cmd=$cmd"service_interval $service_interval "
		[ ! -z "$delay_bound" ] && cmd=$cmd"delay_bound $delay_bound "
		[ ! -z "$msdu_ttl" ] && cmd=$cmd"msdu_ttl $msdu_ttl "
		[ ! -z "$priority" ] && cmd=$cmd"priority $priority "
		[ ! -z "$tid" ] && cmd=$cmd"tid $tid "
		[ ! -z "$msdu_rate_loss" ] && cmd=$cmd"msdu_loss $msdu_rate_loss "

		eval $cmd
	elif [ $enable_service -eq 0 ]; then
		check_svc_id=$(iw $phy service_class view $svc_id | grep "Service ID" | cut -d ":" -f2)
		if [ ! -z "$check_svc_id" ] && [ $check_svc_id -eq $svc_id ]; then
			cmd="iw $phy service_class disable $svc_id"
			eval $cmd
		fi
	fi
}

configure_service_class() {
	PHY_PATH="/sys/kernel/debug/ieee80211"
	phy_present=false
	if [ -d  $PHY_PATH ]
	then
		for phy in $(ls $PHY_PATH 2>/dev/null); do
			dir_name="$PHY_PATH/$phy/ath12k*"
			for dir in $dir_name; do
				[ -d $dir ] && phy_present=true && break
			done
			[ $phy_present = true ] && break
		done
	fi
	[ $phy_present = false ] && return

	json_init
	json_set_namespace default_ns
	json_load_file /lib/wifi/sawf/def_service_classes.json
	json_select service_class
	json_get_keys svc_class_indexes
	svc_class_index=0
	enable_svc=$1

	svc_class_index_count=$(echo "$svc_class_indexes" | wc -w)
	while [ $svc_class_index -lt $svc_class_index_count ]
	do
		svc_class_json=$(jsonfilter -i /lib/wifi/sawf/def_service_classes.json -e "@.service_class[$svc_class_index]")
		configure_service_param "$svc_class_json" "$enable_svc" "$phy"
		svc_class_index=$((svc_class_index+1))
	done

	json_set_namespace default_ns
	json_load_file /lib/wifi/sawf/service_classes.json
	json_select service_class
	json_get_keys svc_class_indexes
	svc_class_index=0

	svc_class_index_count=$(echo "$svc_class_indexes" | wc -w)
	while [ $svc_class_index -lt $svc_class_index_count ]
	do
		svc_class_json=$(jsonfilter -i /lib/wifi/sawf/service_classes.json -e "@.service_class[$svc_class_index]")
		configure_service_param "$svc_class_json" "$enable_svc" "$phy"
		svc_class_index=$((svc_class_index+1))
	done
}

update_mld_vap_details() {
	local _mlds
	local _devices_up
	local _ifaces
	config_load wireless
	mld_vaps_count=0
	radio_up_count=0

	mac80211_get_wifi_mlds() {
		append _mlds $1
	}
	config_foreach mac80211_get_wifi_mlds wifi-mld

	if [ -z "$_mlds" ]; then
		return
	fi

	mac80211_get_wifi_ifaces() {
		config_get iface_mode $1 mode
		if [ -n "$iface_mode" ] && [[ "$iface_mode" == "ap" ]]; then
			append _ifaces $1
		fi
	}
	config_foreach mac80211_get_wifi_ifaces wifi-iface

	for _mld in $_mlds
	do
		for _ifname in $_ifaces
		do
			config_get mld_name $_ifname mld
			config_get mldevice $_ifname device
			config_get mlcaps  $mldevice mlo_capable

			if ! [[ "$mldevices" =~ "$mldevice" ]]; then
				append mldevices $mldevice
			fi

			if [ -n "$mlcaps" ] && [ $mlcaps -eq 1 ] && \
			   [ -n "$mld_name" ] &&  [ "$_mld" = "$mld_name" ]; then
				mld_vaps_count=$((mld_vaps_count+1))
			fi
		done
	done

	for mldev in $mldevices
	do
		# Length of radio name should be 12 in order to ensure only single wiphy wifi-devices are taken into account
		if [ ${#mldev} -ne 12 ]; then
                        continue;
                fi

                config_get disabled "$mldev" disabled

                if [ -z "$disabled" ] || [ "$disabled" -eq 0 ]; then
                        radio_up_count=$((radio_up_count+1))
                fi
	done

	echo "radio_up_count=$radio_up_count mld_vaps_count=$mld_vaps_count" > $MLD_VAP_DETAILS
}

pre_wifi_updown() {
	has_updated_cfg=$(ls /var/run/hostapd-*-updated-cfg 2>/dev/null | wc -l)
	if [ "$has_updated_cfg" -gt 1 ]; then
		rm -rf /var/run/hostapd-*updated-cfg
	fi
	if [ -f "$MLD_VAP_DETAILS" ]; then
		rm -rf $MLD_VAP_DETAILS
	fi

	get_device_config()  {
		if [ ${#1} -eq 12 ]; then
			dev=$1
			drv_mlo_capable=$(cat /sys/module/ath12k/parameters/mlo_capable)
		fi
        }
	config_foreach get_device_config wifi-device

	if ([ -n "$drv_mlo_capable" ] && [ $drv_mlo_capable -eq 0 ]); then
		echo Wireless driver is not in single wiphy architecture. Kindly set mlo_capable module param.
		exit
	fi


	update_mld_vap_details
}

post_wifi_updown() {
	:
}

pre_wifi_reload_legacy() {
	:
}

post_wifi_reload_legacy() {
	:
}

pre_wifi_config() {
	:
}

post_wifi_config() {
	:
}

mac80211_update_config_file() {
cat <<EOF
config wifi-device  $devname
	option type     mac80211
	option channel  ${1}
	option hwmode   11${mode_11n}${mode_band}
$dev_id
$ht_capab
EOF

if [[ -n "$2" ]]; then
cat <<EOF
	option channels ${2}
EOF
fi

cat <<EOF
	# REMOVE THIS LINE TO ENABLE WIFI:
	option disabled 1

config wifi-iface
	option device   $devname
	option network  lan
	option mode     ap
	option ssid     OpenWrt
$security

EOF

}

mac80211_get_channel_list() {
	dev=$1
	n_hw_idx=$2
	chan=$3
	i=0
	match_found=0

	while [ $i -lt $n_hw_idx ]; do
		hw_nchans=$(iw phy ${dev} info | awk -v p1="$i channel list" -v p2="$((i+1)) channel list"  ' $0 ~ p1{f=1;next} $0 ~ p2 {f=0} f')
		first_chan=$(echo $hw_nchans | awk '{print $1}')
		higest_chan=$first_chan
		for chidx in $hw_nchans; do
			if [ $chidx -gt $higest_chan ]; then
				higest_chan=$chidx;
			fi
			if [ "$chidx" == "$chan" ]; then
				match_found=1
			fi
		done
		if [ $match_found -eq 1 ]; then
			break;
		fi
		i=$((i+1))
	done

	if [ $match_found -eq 1 ]; then
		echo "$first_chan-$higest_chan";
	else
		echo ""
	fi
}

mac80211_validate_num_channels() {
	dev=$1
	n_hw_idx=$2
	efreq=$3
	match_found=0
	bandidx=$4
	sub_matched=0
	i=0

	#fetch the band channel list
	band_nchans=$(eval ${3} | awk '{ print $4 }' | sed -e "s/\[//g" | sed -e "s/\]//g")
	band_first_chan=$(echo $band_nchans | awk '{print $1}')

	#entire band channel list without any separator
	band_nchans=$(echo $band_nchans | tr -d ' ')

	while [ $i -lt $n_hw_idx ]; do

		#fetch the hw idx channel list
		hw_nchans=$(iw phy ${dev} info | awk -v p1="$i channel list" -v p2="$((i+1)) channel list"  ' $0 ~ p1{f=1;next} $0 ~ p2 {f=0} f')
		first_chan=$(echo $hw_nchans | awk '{print $1}')
		hw_nchans=$(echo $hw_nchans | tr -d ' ')

		if [ "$band_nchans" = "$hw_nchans" ]; then
			match_found=1
		else
			#check if subchannels matches
			if echo "$band_nchans" | grep -q "${hw_nchans}";
			then
				sub_matched=$((sub_matched+1))
				append chans $first_chan
			fi
		fi
		i=$((i+1))
	done
	if [ $match_found -eq 0 ]; then
		if [ $sub_matched -gt 1 ]; then
                        echo "$chans"
		fi
	else
		echo ""
	fi
}

lookup_phy() {
	[ -n "$phy" ] && {
		[ -d /sys/class/ieee80211/$phy ] && return
	}

	# Incase of multiple radios belonging to the same soc, the device path
	# of these radio's would be same. To find the correct phy, we can
	# get the phy index of the device in soc and use it during searching
	# the global phy list
	local radio_idx=${device:5:1}
	local first_phy_idx=0
	local delta=0
	local devpath
	config_get devpath "$device" path
	while :; do
	if [ ${#device} -eq 12 ]; then
		config_get devicepath "radio$radio_idx\_band$first_phy_idx" path
	else
		config_get devicepath "radio$first_phy_idx" path
	fi
	[ -n "$devicepath" -a -n "$devpath" ] || break
	[ "$devpath" == "$devicepath" ] && break
	first_phy_idx=$(($first_phy_idx + 1))
	done

	delta=$(($radio_idx - $first_phy_idx))

	[ -n "$devpath" ] && {
		for phy in $(ls /sys/class/ieee80211 2>/dev/null); do
			case "$(readlink -f /sys/class/ieee80211/$phy/device)" in
			*$devpath)
				if [ $delta -gt 0 ]; then
					delta=$(($delta - 1))
					continue;
				fi
				return;;
			esac
		done
	}

	local macaddr="$(config_get "$device" macaddr | tr 'A-Z' 'a-z')"
	[ -n "$macaddr" ] && {
		for _phy in /sys/class/ieee80211/*; do
			[ -e "$_phy" ] || continue
			[ "$macaddr" = "$(cat ${_phy}/macaddress)" ] || continue
			phy="${_phy##*/}"
			return
		done
	}
	phy=
	return
}

find_mac80211_phy() {
	local device="$1"

	config_get phy "$device" phy
	lookup_phy
	[ -n "$phy" -a -d "/sys/class/ieee80211/$phy" ] || {
		echo "PHY for wifi device $1 not found"
		return 1
	}
	config_set "$device" phy "$phy"

	config_get macaddr "$device" macaddr
	[ -z "$macaddr" ] && {
		config_set "$device" macaddr "$(cat /sys/class/ieee80211/${phy}/macaddress)"
	}

	[ -z "$macaddr" ] && {
		config_set "$device" macaddr "$(cat /sys/class/ieee80211/${phy}/device/net/wlan${phy#phy}/address)"
	}
	return 0
}

check_mac80211_device() {
	config_get phy "$1" phy
	[ -z "$phy" ] && {
		find_mac80211_phy "$1" >/dev/null || return 0
		config_get phy "$1" phy
	}
	[ "$phy" = "$dev" ] && found=1
}

detect_mac80211() {
	if [ $(cat /sys/bus/coresight/devices/coresight-stm/enable) -eq 0 ]
	then
		chipset=$(grep -o "IPQ.*" /proc/device-tree/model | awk -F/ '{print $1}')
		board=$(grep -o "IPQ.*" /proc/device-tree/model | awk -F/ '{print $2}')
		if [ "$chipset" == "IPQ9574" ] && [ "$board" != "AP-AL02-C4" ] && [ "$board" != "AP-AL02-C9" ]; then
			echo 0 > /sys/bus/coresight/devices/coresight-stm/enable
			echo "q6mem" > /sys/bus/coresight/devices/coresight-tmc-etr/out_mode
			echo 1 > /sys/bus/coresight/devices/coresight-tmc-etr/curr_sink
			echo 1 > /sys/bus/coresight/devices/coresight-stm/enable
		fi
	fi
	devidx=0

	config_load wireless

	if [ ! -f "/etc/config/wireless" ] || ! grep -q "enable_smp_affinity" "/etc/config/wireless"; then
		cat <<EOF
config smp_affinity  mac80211
	option enable_smp_affinity	1
	option enable_color		1

EOF
	fi

	while :; do
		config_get type "radio$devidx" type
		[ -n "$type" ] || break
		devidx=$(($devidx + 1))
	done

	#add this delay for empty wifi script issue
	count=0
	while [ $count -le 10 ]
	do
		sleep  1
		if ([ $(ls /sys/class/ieee80211 | wc -l  | grep -w "0") ])
		then
			count=$(( count+1 ))
		else
			sleep 1
			break
		fi
	done

	for _dev in `ls -dv /sys/class/ieee80211/*`; do
		[ -e "$_dev" ] || continue
		dev="${_dev##*/}"
		found=0
		config_foreach check_mac80211_device wifi-device
		[ "$found" -gt 0 ] && continue

		no_sbands=$(iw phy ${dev} info | grep 'Band ' | wc -l)
		if [ $no_sbands -gt 1 ]; then
			is_swiphy=1
		fi
		no_hw_idx=$(iw phy ${dev} info | grep -e "channel list" | wc -l)

		bandidx=0
		for _band in `iw phy ${dev} info | grep 'Band ' | cut -d' ' -f 2`; do
			[ ! -z $_band ] || continue

			mode_11n=""
			mode_band="a"
			channel="36"
			htmode=""
			ht_capab=""
			encryption="none"
			security=""

			if [ $is_swiphy ]; then
				expr="iw phy ${dev} info | awk  '/Band ${_band}/{ f = 1; next } /Band /{ f = 0 } f'"
			else
				expr="iw phy ${dev} info"
			fi
			expr_freq="$expr | awk '/Frequencies/,/valid /f'"

			if [ $no_hw_idx -gt $no_sbands ]; then
				need_extraconfig=$(mac80211_validate_num_channels $dev $no_hw_idx "$expr_freq")
			fi

			eval $expr_freq | grep -q '5180 MHz' || \
			eval $expr_freq | grep -q '5955 MHz' || { mode_band="g"; channel="11"; }

			(eval $expr_freq | grep -q '5745 MHz' && \
			(eval $expr_freq | grep -q -F '5180 MHz [36] (disabled)')) && { mode_band="a"; channel="149"; }

			eval $expr_freq | grep -q '60480 MHz' && { mode_11n="a"; mode_band="d"; channel="2"; }

			eval $expr | grep -q 'Capabilities:' && htmode=HT20

			eval $expr | grep -q 'Capabilities:' && htmode=HT20
			vht_cap=$(eval $expr | grep -c 'VHT Capabilities')

			[ "$mode_band" = a ] && htmode="VHT80"

			eval $expr_freq | grep -q '5180 MHz' || eval $expr_freq | grep -q '5745 MHz' || {
				eval $expr_freq | grep -q '5955 MHz' && {
					channel="49"; htmode="HE80"; encryption="sae";
					append ht_capab "	option band     3" "$N"
				}
			}

			[ -n $htmode ] && append ht_capab "	option htmode   $htmode" "$N"

			append security "	option encryption  $encryption" "$N"
			if [ $encryption == "sae" ]; then
				append security "	option sae_pwe  1" "$N"
				append security "	option key      0123456789" "$N"
			fi

			if [ -x /usr/bin/readlink -a -h /sys/class/ieee80211/${dev} ]; then
				path="$(readlink -f /sys/class/ieee80211/${dev}/device)"
			else
				path=""
			fi
			if [ -n "$path" ]; then
				path="${path##/sys/devices/}"
				case "$path" in
					platform*/pci*) path="${path##platform/}";;
				esac
				dev_id="        option path     '$path'"
			else
				dev_id="        option macaddr  $(cat /sys/class/ieee80211/${dev}/macaddress)"
			fi
			if [ $is_swiphy ]; then
				devname=radio$devidx\_band$bandidx
			else
				devname=radio$devidx
			fi
			if [ -n "$need_extraconfig" ]; then
				for chan in ${need_extraconfig} ; do
					if [ $chan -eq 100 ]; then
						chan=149
					fi
					chan_list=$(mac80211_get_channel_list $dev $no_hw_idx $chan)
					mac80211_update_config_file $chan $chan_list
					if [ $is_swiphy ]; then
						bandidx=$(($bandidx + 1))
						devname=radio$devidx\_band$bandidx
					fi
				done
			else
				chan_list=$(mac80211_get_channel_list $dev $no_hw_idx $channel)
				mac80211_update_config_file $channel $chan_list
				bandidx=$(($bandidx + 1))
			fi
		done

	devidx=$(($devidx + 1))
	done
}

config_mac80211() {
	detect_mac80211
}

# This start_lbd is to check the dual band availability and
# make sure that dual bands (2.4G and 5G) available before
# starting lbd init script.

start_lbd() {
	local band_24g
	local band_5g
	local i=0

	driver=$(lsmod | cut -d' ' -f 1 | grep ath10k_core)

	if [ "$driver" == "ath10k_core" ]; then
		while [ $i -lt 10 ]
		do
			BANDS=$(/usr/sbin/iw dev 2> /dev/null | grep channel | cut -d' ' -f 2 | cut -d'.' -f 1)
			for channel in $BANDS
			do
				if [ "$channel" -le "14" ]; then
					band_24g=1
				elif [ "$channel" -ge "36" ]; then
					band_5g=1
				fi
			done

			if [ "$band_24g" == "1" ] && [ "$band_5g" == "1" ]; then
				/etc/init.d/lbd start
				return 0
			fi
			sleep 1
			i=$(($i + 1))
		done
	fi
	return 0
}

post_mac80211() {
	local action=${1}

	case "${action}" in
		enable)
			[ -f "/usr/sbin/fst.sh" ] && {
				/usr/sbin/fst.sh start
			}
			if [ -f "/etc/init.d/lbd" ]; then
				start_lbd &
			fi
			sawf_supp="/sys/module/ath12k/parameters/sawf"
			if [ -f $sawf_supp ] && [ $(cat $sawf_supp) == "Y" ]; then
				configure_service_class 1
			fi
		;;
	esac

	chipset=$(grep -o "IPQ.*" /proc/device-tree/model | awk -F/ '{print $1}')
	board=$(grep -o "IPQ.*" /proc/device-tree/model | awk -F/ '{print $2}')
	if [ "$chipset" == "IPQ5018" ]; then
		echo "q6mem" > /sys/bus/coresight/devices/coresight-tmc-etr/out_mode
		echo 1 > /sys/bus/coresight/devices/coresight-tmc-etr/curr_sink
		echo 5 > /sys/bus/coresight/devices/coresight-funnel-mm/funnel_ctrl
		echo 7 >/sys/bus/coresight/devices/coresight-funnel-in0/funnel_ctrl
		echo 1 > /sys/bus/coresight/devices/coresight-stm/enable
	elif [ "$chipset" == "IPQ8074" ] && [ "$board" != "AP-HK10-C2" ]; then
		echo "q6mem" > /sys/bus/coresight/devices/coresight-tmc-etr/out_mode
		echo 1 > /sys/bus/coresight/devices/coresight-tmc-etr/curr_sink
		echo 5 > /sys/bus/coresight/devices/coresight-funnel-mm/funnel_ctrl
		echo 6 > /sys/bus/coresight/devices/coresight-funnel-in0/funnel_ctrl
		echo 1 > /sys/bus/coresight/devices/coresight-stm/enable
	elif [ "$chipset" == "IPQ6018" ] || [ "$chipset" == "IPQ807x" ]; then
		echo "q6mem" > /sys/bus/coresight/devices/coresight-tmc-etr/out_mode
		echo 1 > /sys/bus/coresight/devices/coresight-tmc-etr/curr_sink
		echo 5 > /sys/bus/coresight/devices/coresight-funnel-mm/funnel_ctrl
		echo 6 > /sys/bus/coresight/devices/coresight-funnel-in0/funnel_ctrl
		echo 1 > /sys/bus/coresight/devices/coresight-stm/enable
	elif [ "$chipset" == "IPQ9574" ] && [ "$board" != "AP-AL02-C4" ] && [ "$board" != "AP-AL02-C9" ]; then
                echo 0 > /sys/bus/coresight/devices/coresight-stm/enable
                echo "q6mem" > /sys/bus/coresight/devices/coresight-tmc-etr/out_mode
                echo 1 > /sys/bus/coresight/devices/coresight-tmc-etr/curr_sink
                echo 1 > /sys/bus/coresight/devices/coresight-stm/enable
	fi

	return 0
}

pre_mac80211() {
	local action=${1}

	case "${action}" in
		disable)
			[ -f "/usr/sbin/fst.sh" ] && {
				/usr/sbin/fst.sh set_mac_addr
				/usr/sbin/fst.sh stop
			}
			[ ! -f /etc/init.d/lbd ] || /etc/init.d/lbd stop

			extsta_path=/sys/module/mac80211/parameters/extsta
			[ -e $extsta_path ] && echo 0 > $extsta_path
			sawf_supp="/sys/module/ath12k/parameters/sawf"
                        if [ -f $sawf_supp ] && [ $(cat $sawf_supp) == "Y" ]; then
				configure_service_class 0
			fi
		;;
	esac
	return 0
}
