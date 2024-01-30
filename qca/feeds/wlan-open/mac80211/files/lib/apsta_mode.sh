#!bin/sh
#
# Copyright (c) 2018, 2020 The Linux Foundation. All rights reserved.
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


sta_intf="$1"
ap_intf="$2"
hostapd_conf="$3"
oper_band="$4"
dfs_log=1
phy="$5"
ml_link=""
ap_ht_capab=$(cat $hostapd_conf 2> /dev/null | grep ht_capab | grep -v vht | cut -d'=' -f 2)

get_sta_freq_list() {
	phy=$1
	sta_freq=$2

	hw_indices=$(iw phy ${phy} info | grep -e "channel list" | cut -d' ' -f 2)

	if [ -z "$hw_indices" ]; then
		return
	fi

	for i in $hw_indices
	do
		#fetch hw idx channels from phy info
		hw_nchans=$(iw phy ${phy} info | awk -v p1="$i channel list" -v p2="$((i+1)) channel list"  ' $0 ~ p1{f=1;next} $0 ~ p2 {f=0} f')

		for _b in `iw phy $phy info | grep 'Band ' | cut -d' ' -f 2`; do
			expr="iw phy ${phy} info | awk  '/Band ${_b}/{ f = 1; next } /Band /{ f = 0 } f'"
			expr_freq="$expr | awk '/Frequencies/,/valid /f'"
			band_freq=$(eval ${expr_freq} | awk '{ print $2 }' | sed -e "s/\[//g" | sed -e "s/\]//g")

			# band_freq list has the sta freq in it
			if [[ "$band_freq" =~ "${sta_freq}" ]]; then
				sta_chan=$(eval $expr_freq | grep -E -m1 "(\* ${sta_freq:-....} MHz${sta_freq:+|\\[$sta_freq\\]})" | grep MHz | awk '{print $4}' | sed -e "s/\[//g" | sed -e "s/\]//g")

				#fetch band channels from phy info
				band_nchans=$(echo $(eval ${expr_freq} | awk '{ print $4 }' | sed -e "s/\[//g" | sed -e "s/\]//g") | tr -d ' ')
				hw_chans=$(echo $hw_nchans | tr -d ' ')

				#check if the list is present in band info
				if echo "$band_nchans" | grep -q "${hw_chans}";
				then
					found=false
					for chan in $hw_nchans
					do
						if [[ "$chan" == "$sta_chan" ]]; then
							found=true
						fi
					done
					if [[ "$found" == "true" ]]; then
						sta_freq_list=""
						for chidx in ${hw_nchans}; do
							frqs=$(eval $expr_freq | grep -E -m1 "(\* ${chidx:-....} MHz${chidx:+|\\[$chidx\\]})" | grep MHz | awk '{print $2}')
							sta_freq_list="${sta_freq_list}${frqs} "
						done
						echo $sta_freq_list
					fi
				fi
			else
				continue;
			fi
		done
	done
}

# Hostapd VHT and HE calculations
hostapd_vht_he_eht_oper_chwidth() {
	local sta_width="$1"
	case $sta_width in
		"80")
			ap_vht_he_eht_oper_chwidth=1;;
		"160")
			ap_vht_he_eht_oper_chwidth=2;;
		"80+80")
			ap_vht_he_eht_oper_chwidth=3;;
		"320" )
			ap_vht_he_eht_oper_chwidth=9;;
		"20"|"40"|*)
			ap_vht_he_eht_oper_chwidth=0;;
	esac
}


hostapd_vht_he_eht_oper_centr_freq_seg0_idx() {
	local sta_width="$1"
	local sta_channel="$2"
	# Frequency is needed for specially handling 6 GHz channels
	local freq="$3"

	case $ap_vht_he_eht_oper_chwidth in
		# 20/40 MHz chan width
		"0")
			case $sta_width in
			"20")
				ap_vht_he_eht_oper_centr_freq_seg0_idx=$sta_channel;;
			"40")
				if [ $freq -gt 5950 ] && [ $freq -le 7115 ]; then
					case "$(( ($sta_channel / 4) % 2 ))" in
						1) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel - 2));;
						0) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel + 2));;
					esac
				elif [ $sta_channel -lt 7 ]; then
					ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel + 2))
				elif [ $sta_channel -lt 36 ]; then
					ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel - 2))
				else
					case "$(( ($sta_channel / 4) % 2 ))" in
						1) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel + 2));;
						0) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel - 2));;
					esac
				fi
			;;
			esac
		;;
		# 80 MHz chan width
		"1")
			if [ $freq -gt 5950 ] && [ $freq -le 7115 ]; then
				case "$(( ($sta_channel / 4) % 4 ))" in
					0) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel + 6));;
					1) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel + 2));;
					2) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel - 2));;
					3) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel - 6));;
				esac
			elif [ $freq != 5935 ]; then
				case "$(( ($sta_channel / 4) % 4 ))" in
					1) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel + 6));;
					2) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel + 2));;
					3) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel - 2));;
					0) ap_vht_he_eht_oper_centr_freq_seg0_idx=$(($sta_channel - 6));;
				esac
			fi
		;;
		# 160 MHz chan width
		"2")
			# Freq based division is required since for 5 GHz, no need to have seg0 for DFS channels
			if [ $freq -gt 5950 ] && [ $freq -le 7115 ]; then
				case "$sta_channel" in
					1|5|9|13|17|21|25|29) ap_vht_he_eht_oper_centr_freq_seg0_idx=15;;
					33|37|41|45|49|53|57|61) ap_vht_he_eht_oper_centr_freq_seg0_idx=47;;
					65|69|73|77|81|85|89|93) ap_vht_he_eht_oper_centr_freq_seg0_idx=79;;
					97|101|105|109|113|117|121|125) ap_vht_he_eht_oper_centr_freq_seg0_idx=111;;
					129|133|137|141|145|149|153|157) ap_vht_he_eht_oper_centr_freq_seg0_idx=143;;
					161|165|169|173|177|181|185|189) ap_vht_he_eht_oper_centr_freq_seg0_idx=175;;
					193|197|201|205|209|213|217|221) ap_vht_he_eht_oper_centr_freq_seg0_idx=207;;
				esac
			elif [ $freq != 5935 ]; then
				case "$sta_channel" in
					36|40|44|48|52|56|60|64) ap_vht_he_eht_oper_centr_freq_seg0_idx=50;;
					100|104|108|112|116|120|124|128) ap_vht_he_eht_oper_centr_freq_seg0_idx=114;;
				esac
			fi
		;;
                "9" )
			if [ $freq -ge 5955 ] && [ $freq -le 7115 ]; then
				case "$sta_channel" in
					1|5|9|13|17|21|25|29|33|37|41|45) ap_vht_he_eht_oper_centr_freq_seg0_idx=31;;
					49|53|57|61|65|69|73|77) ap_vht_he_eht_oper_centr_freq_seg0_idx=63;;
					81|85|89|93|97|101|105|109) ap_vht_he_eht_oper_centr_freq_seg0_idx=95;;
					113|117|121|125|129|133|137|141) ap_vht_he_eht_oper_centr_freq_seg0_idx=127;;
					145|149|153|157|161|165|169|173) ap_vht_he_eht_oper_centr_freq_seg0_idx=159;;
					177|181|185|189|193|197|201|205|209|213|217|221) ap_vht_he_eht_oper_centr_freq_seg0_idx=191;;
				esac
			elif [ $freq -ge 5500 ] && [ $freq -le 5730 ]; then
				ap_vht_he_eht_oper_centr_freq_seg0_idx=130
			fi
	esac
}

# Hostapd HT40 secondary channel offset calculations
hostapd_ht40_mode() {
	local sta_channel="$1"

	# 5Ghz channels
	if [ $sta_channel -ge 36 ]; then
		case "$(( ($sta_channel / 4) % 2 ))" in
			1) if [ ! -z $ap_ht_mode ]; then
				ap_ht_mode="$(echo $ap_ht_mode | sed -e "s/HT40-/HT40+/g")"
			   else
				ap_ht_mode="[HT40+]"
			   fi
			;;
			0) if [ ! -z $ap_ht_mode ]; then
				ap_ht_mode="$(echo $ap_ht_mode | sed -e "s/HT40+/HT40-/g")"
			   else
				ap_ht_mode="[HT40-]"
			   fi
			;;
		esac
	else
		# 2.4Ghz channels
		if [ "$sta_channel" -lt 7 ]; then
			if [ ! -z $ap_ht_mode ]; then
				ap_ht_mode="$(echo $ap_ht_mode | sed -e "s/HT40-/HT40+/g")"
                        else
                                ap_ht_mode="[HT40+]"
                        fi
		else
			if [ ! -z $ap_ht_mode ]; then
				ap_ht_mode="$(echo $ap_ht_mode | sed -e "s/HT40+/HT40-/g")"
                        else
                                ap_ht_mode="[HT40-]"
                        fi
		fi
	fi
	#echo "Secondary offset is $ap_ht_mode" > /dev/ttyMSM0
	hostapd_cli -i $ap_intf $ml_link set ht_capab $ap_ht_mode 2> /dev/null
}

# Hostapd HT20 mode
hostapd_ht20_mode() {
        local ht_capab_20=$(echo $ap_ht_capab | sed -e 's/\(\[HT40*+*-*]\)//g')
        #echo "Setting HT capab $ht_capab_20" > /dev/ttyMSM0
        hostapd_cli -i $ap_intf $ml_link set ht_capab $ht_capab_20 2> /dev/null
}

# STA association is completed, hence adjusting hostapd running config
hostapd_adjust_config() {
	sta_channel=$(iw $sta_intf info 2> /dev/null | grep channel | cut -d' ' -f 2)
	sta_width=$(wpa_cli -i $sta_intf signal_poll 2> /dev/null | grep WIDTH | cut -d'=' -f 2 | grep MHz | cut -d' ' -f 1)
	sta_freq=$(wpa_cli -i $sta_intf status 2> /dev/null | grep freq | cut -d'=' -f 2)
	wifi_gen=$(wpa_cli -i $sta_intf status 2> /dev/null | grep wifi_generation | cut -d'=' -f 2)
	ieee80211ac=$(wpa_cli -i $sta_intf status 2> /dev/null | grep ieee80211ac | cut -d'=' -f 2)

	if [ -z $ieee80211ac ]; then
		ieee80211ac=0
	fi
	#echo "STA associated in Channel $sta_channel, Width $sta_width MHz, Wifi Gen $wifi_gen, ieee80211ac $ieee80211ac $ml_link" > /dev/ttyMSM0
	hostapd_cli -i $ap_intf $ml_link set channel $sta_channel 2> /dev/null
	if [ $sta_channel -ge 36 ] || [ $oper_band == 3 ]; then
		hostapd_cli -i $ap_intf $ml_link set hw_mode a 2> /dev/null
	else
		hostapd_cli -i $ap_intf $ml_link set hw_mode g 2> /dev/null
	fi

	ap_ht_mode=$(echo $ap_ht_capab | sed -n 's/.*\(\[HT40*+*-*]\).*/\1/p')
	#echo "Current AP HT capab $ap_ht_capab" > /dev/ttyMSM0

	if [ "$(wpa_cli -i $sta_intf signal_poll 2> /dev/null | grep WIDTH | cut -d'(' -f 2 | cut -d')' -f 1)" = "no HT" ]; then
		#echo "STA associated in No HT mode, downgrading AP as well" > /dev/ttyMSM0
		hostapd_cli -i $ap_intf $ml_link set ieee80211ac 0 2> /dev/null
		hostapd_cli -i $ap_intf $ml_link set ieee80211n 0 2> /dev/null
		# Set HT capab without HT40+/- config to set secondary channel 0
		hostapd_ht20_mode
		hostapd_cli -i $ap_intf $ml_link set ieee80211ax 0 2> /dev/null
		hostapd_cli -i $ap_intf $ml_link set ieee80211be 0 2> /dev/null

	 elif [ $wifi_gen == 6 ] || [ $wifi_gen == 7 ]; then
		#echo "STA associated in HE$sta_width mode, applying same config to AP" > /dev/ttyMSM0
                local ap_vht_he_eht_oper_chwidth
                local ap_vht_he_eht_oper_centr_freq_seg0_idx

                hostapd_vht_he_eht_oper_chwidth "$sta_width"
                hostapd_vht_he_eht_oper_centr_freq_seg0_idx "$sta_width" "$sta_channel" "$sta_freq"

				if [ $wifi_gen == 7 ]; then
					hostapd_cli -i $ap_intf $ml_link set ieee80211be 1 2> /dev/null
					hostapd_cli -i $ap_intf $ml_link set eht_oper_chwidth $ap_vht_he_eht_oper_chwidth
					hostapd_cli -i $ap_intf $ml_link set eht_oper_centr_freq_seg0_idx $ap_vht_he_eht_oper_centr_freq_seg0_idx
				fi
				hostapd_cli -i $ap_intf $ml_link set ieee80211ax 1 2> /dev/null
				hostapd_cli -i $ap_intf $ml_link set he_oper_chwidth $ap_vht_he_eht_oper_chwidth
				hostapd_cli -i $ap_intf $ml_link set he_oper_centr_freq_seg0_idx $ap_vht_he_eht_oper_centr_freq_seg0_idx

		# VHT Operations is not applicable for 6GHz interface
		if [ $wifi_6gband == false ]; then
			#echo "Interface in Wifi 6 gen, but its not a 6GHz interface" > /dev/ttyMSM0
			#echo "Set 802.11n, ac to 1" > /dev/ttyMSM0
			hostapd_cli -i $ap_intf $ml_link set ieee80211ac 1 2> /dev/nul
			hostapd_cli -i $ap_intf $ml_link set ieee80211n 1 2> /dev/null
			hostapd_cli -i $ap_intf $ml_link set vht_oper_chwidth $ap_vht_he_eht_oper_chwidth

			#echo "New vht_oper_chwidth is $ap_vht_he_eht_oper_chwidth" > /dev/ttyMSM0
			#echo "vht_oper_centr_freq_seg0_idx is $ap_vht_he_eht_oper_centr_freq_seg0_idx" > /dev/ttyMSM0
			hostapd_cli -i $ap_intf set vht_oper_centr_freq_seg0_idx $ap_vht_he_eht_oper_centr_freq_seg0_idx

			if [ $sta_width = "20" ]; then
				#echo "Setting HE20 mode to AP" > /dev/ttyMSM0
				hostapd_ht20_mode
			else
				hostapd_ht40_mode "$sta_channel"
			fi
		fi
	elif [ $wifi_gen == 5 -o $ieee80211ac == 1 ]; then
		#echo "STA associated in VHT$sta_width mode, applying same config to AP" > /dev/ttyMSM0
		local ap_vht_he_eht_oper_chwidth
		local ap_vht_he_eht_oper_centr_freq_seg0_idx

		hostapd_vht_he_eht_oper_chwidth "$sta_width"

		#echo "Set 802.11n & ac to 1" > /dev/ttyMSM0
		hostapd_cli -i $ap_intf $ml_link set ieee80211ac 1 2> /dev/null
		hostapd_cli -i $ap_intf $ml_link set ieee80211n 1 2> /dev/null
		hostapd_cli -i $ap_intf $ml_link set ieee80211ax 0 2> /dev/null
		hostapd_cli -i $ap_intf $ml_link set ieee80211be 0 2> /dev/null

		#echo "vht_oper_chwidth is $ap_vht_he_eht_oper_chwidth for VHT$sta_width mode" > /dev/ttyMSM0

		hostapd_vht_he_eht_oper_centr_freq_seg0_idx "$sta_width" "$sta_channel" "$sta_freq"

		#echo "New vht_oper_chwidth is $ap_vht_he_eht_oper_chwidth" > /dev/ttyMSM0
		hostapd_cli -i $ap_intf $ml_link set vht_oper_chwidth $ap_vht_he_eht_oper_chwidth

		#echo "vht_oper_centr_freq_seg0_idx is $ap_vht_he_eht_oper_centr_freq_seg0_idx" > /dev/ttyMSM0
		hostapd_cli -i $ap_intf $ml_link set vht_oper_centr_freq_seg0_idx $ap_vht_he_eht_oper_centr_freq_seg0_idx

		if [ $sta_width = "20" ]; then
                        #echo "Setting VHT20 mode to AP" > /dev/ttyMSM0
                        hostapd_ht20_mode
                else
                        hostapd_ht40_mode "$sta_channel"
                fi
	else
		#echo "STA associated in HT$sta_width mode, applying same config to AP" > /dev/ttyMSM0
		hostapd_cli -i $ap_intf $ml_link set ieee80211n 1
		hostapd_cli -i $ap_intf $ml_link set ieee80211ac 0
		hostapd_cli -i $ap_intf $ml_link set ieee80211ax 0
		hostapd_cli -i $ap_intf $ml_link set ieee80211be 0

		if [ $sta_width = "20" ]; then
                        #echo "Setting HT20 mode to AP" > /dev/ttyMSM0
                        hostapd_ht20_mode
                else
                        hostapd_ht40_mode "$sta_channel"
                fi
	fi
}

hostapd_is_6ghz_band() {
	local freq=$1
	if [ $freq -gt 5950 ] && [ $freq -le 7115 ]; then
		echo true
	else
		echo false
	fi
}

is_apfreq_in_sta_freq_list() {
	ap_freq=$1
	found=0
	sta_freq_list="$2"

	for f in $sta_freq_list
	do
		if [[ "$ap_freq" == "$f" ]]; then
			found=1
		fi
	done
	echo $found
}


get_link_info() {
	ifname=$1
	freq=$2
	ap_freq=
	num_links=$(hostapd_cli -i $ifname status | grep num_links | cut -d'=' -f 2)
	if [ $num_links -eq 0 ]; then
		echo ""
		return
	fi

	links=$(hostapd_cli -i $ifname status | grep link_id | cut -d'=' -f 2)
	for i in $links
	do
		ap_freq=$(hostapd_cli -i $ifname -l $i status | grep -w freq | cut -d'=' -f 2)
		sta_freq_list="$(get_sta_freq_list $phy $freq)"
		is_freq_present=$(is_apfreq_in_sta_freq_list $ap_freq "$sta_freq_list")
		if [ $is_freq_present -eq 1 ]; then
			ml_link="-l $i"
			echo "$ml_link"
			return
		fi
	done
	echo "$ml_link"
}

ap_freq=$(iw $ap_intf info 2> /dev/null | grep channel | cut -d' ' -f 3 | cut -b 2-5)
wifi_6gband=$(hostapd_is_6ghz_band $ap_freq)
if [ $wifi_6gband == true ] || [ $oper_band == 3 ]; then
cat > /lib/repeater_6g_ch_sw.sh << EOF
#!/bin/sh

if [ "`echo $\2`" = "CTRL-EVENT-STARTED-CHANNEL-SWITCH" ] && [ "`echo $\1`" = $sta_intf ]; then
        wpa_cli -i $sta_intf disable 0
        sleep 5
        wpa_cli -i $sta_intf enable 0
fi
EOF

        chmod 777 /lib/repeater_6g_ch_sw.sh
        wpa_cli -i $sta_intf -a /lib/repeater_6g_ch_sw.sh &
fi

#echo "Checking wpa_state $(wpa_cli -i $sta_intf status 2> /dev/null | grep wpa_state | cut -d'=' -f 2)" > /dev/ttyMSM0

while true;
do
	if [[ ! -e "/var/run/wpa_supplicant/$sta_intf" && ! -e "/var/run/hostapd/$ap_intf*" ]]; then
		echo "AP+STA mode not running, exiting script" >> /tmp/apsta_debug.log
		exit
	fi

	res=$(hostapd_cli -i $ap_intf status 2> /dev/null | grep state | cut -d'=' -f 2)
	if [ -z $res ]; then
		ap_link=$(iw dev $ap_intf info | grep link | head -n 1 | cut -d':' -f 1 2> /dev/null  | cut -d ' ' -f 2)
		if [ -n "$ap_link" ]; then
			ml_link="-l $ap_link"
		fi
	fi

	if [ $(wpa_cli -i $sta_intf status 2> /dev/null | grep wpa_state | cut -d'=' -f 2) = "DISCONNECTED"  -o \
			 $(wpa_cli -i $sta_intf status 2> /dev/null | grep wpa_state | cut -d'=' -f 2) = "SCANNING" ] &&
				[ $(hostapd_cli -i $ap_intf $ml_link status 2> /dev/null | grep state | cut -d'=' -f 2) = "ENABLED" ]; then
		#echo "wpa_s state: $(wpa_cli -i $sta_intf status 2> /dev/null | grep wpa_state | cut -d'=' -f 2), stopping AP" > /dev/ttyMSM0
		hostapd_cli -i $ap_intf $ml_link disable
	fi

	if [ $(wpa_cli -i $sta_intf status 2> /dev/null | grep wpa_state | cut -d'=' -f 2) = "COMPLETED" ] &&
		[ $(hostapd_cli -i $ap_intf $ml_link status 2> /dev/null | grep state | cut -d'=' -f 2) = "DISABLED" ]; then
		#echo "wpa_s state: $(wpa_cli -i $sta_intf status 2> /dev/null | grep wpa_state | cut -d'=' -f 2), starting AP" > /dev/ttyMSM0
		wpa_cli -i $sta_intf signal_poll
		sta_chan=$(iw $sta_intf info 2> /dev/null | grep channel | cut -d' ' -f 2)

		# workaround for sending 4addr packet to AP from repeater after association
		ip_addr="$(ifconfig | grep -A 1 'br-lan' | tail -1 | cut -d ':' -f 2 | cut -d ' ' -f 1)"
		arping "$ip_addr" -U -I br-lan -D -c 5

		# DFS Channel check is applicable only for 5 GHz interface
		local sta_freq=$(wpa_cli -i $sta_intf status 2> /dev/null | grep freq | cut -d'=' -f 2)
		wifi_6gband=$(hostapd_is_6ghz_band $sta_freq)
		#echo "Is 6GHz interface: $wifi_6gband" > /dev/ttyMSM0

		ml_link=$(get_link_info $ap_intf $sta_freq)
		#echo link config command is $ml_link > /dev/console
		if [ $sta_chan -le 48 -o $sta_chan -ge 149 ] || [ $wifi_6gband == true ]; then
			hostapd_adjust_config
			#echo "Enabling below hostapd config:" > /dev/ttyMSM0
			hostapd_cli -i $ap_intf $ml_link status 2> /dev/null

			[ $(hostapd_cli -i $ap_intf $ml_link status 2> /dev/null | grep state | cut -d'=' -f 2) = "DISABLED" ] && hostapd_cli -i $ap_intf enable
			sleep 4
			#echo "Hostapd state: $(hostapd_cli status 2> /dev/null | grep state | cut -d'=' -f 2)" > /dev/ttyMSM0

			# workaround for single instance hostapd not doing "enable" without "disable" call to deinit hapd driver
			if [ $(hostapd_cli -i $ap_intf $ml_link status 2> /dev/null | grep state | cut -d'=' -f 2) = "DISABLED" ]; then
				hostapd_cli -i $ap_intf $ml_link disable
				sleep 1
				hostapd_cli -i $ap_intf $ml_link enable
				sleep 4
			fi

			if [ $(hostapd_cli -i $ap_intf $ml_link  status 2> /dev/null | grep state | cut -d'=' -f 2) = "DISABLED" ]; then
				echo "REPEATER AP failed bring-up, exiting" > /dev/ttyMSM0
				echo "Please check logs at /tmp/apsta_debug.log"  > /dev/ttyMSM0
				echo "Hostapd enable failed, exiting" >> /tmp/apsta_debug.log
				echo "link config command is $ml_link" >> /tmp/apsta_debug.log
				date >> /tmp/apsta_debug.log
				hostapd_cli -i $ap_intf $ml_link status >> /tmp/apsta_debug.log
				wpa_cli -i $sta_intf signal_poll >> /tmp/apsta_debug.log
				wpa_cli -i $sta_intf status >> /tmp/apsta_debug.log
				wpa_cli -i $sta_intf list_n >> /tmp/apsta_debug.log
				wpa_cli -i $sta_intf all_bss >> /tmp/apsta_debug.log
				date >> /tmp/apsta_debug.log
				wifi down
				exit
			fi
		else
			if [ $dfs_log == 1 ]; then
				echo "DFS channel($sta_chan) is not supported for repeater, so bringdown AP" > /dev/ttyMSM0
				dfs_log=0
			fi
		fi
	fi
	usleep 100000
done &
