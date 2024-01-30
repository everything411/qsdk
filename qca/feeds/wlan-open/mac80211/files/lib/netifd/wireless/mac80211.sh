#!/bin/sh
. /lib/netifd/netifd-wireless.sh
[ -e /lib/functions.sh ] && . /lib/functions.sh
[ -e /lib/netifd/hostapd.sh ] && . /lib/netifd/hostapd.sh
[ -e /lib/wifi/hostapd.sh ] && . /lib/wifi/hostapd.sh
[ -e /lib/wifi/wpa_supplicant.sh ] && . /lib/wifi/wpa_supplicant.sh

init_wireless_driver "$@"

MLD_VAP_DETAILS="/lib/netifd/wireless/wifi_mld_cfg.config"

MP_CONFIG_INT="mesh_retry_timeout mesh_confirm_timeout mesh_holding_timeout mesh_max_peer_links
	       mesh_max_retries mesh_ttl mesh_element_ttl mesh_hwmp_max_preq_retries
	       mesh_path_refresh_time mesh_min_discovery_timeout mesh_hwmp_active_path_timeout
	       mesh_hwmp_preq_min_interval mesh_hwmp_net_diameter_traversal_time mesh_hwmp_rootmode
	       mesh_hwmp_rann_interval mesh_gate_announcements mesh_sync_offset_max_neighor
	       mesh_rssi_threshold mesh_hwmp_active_path_to_root_timeout mesh_hwmp_root_interval
	       mesh_hwmp_confirmation_interval mesh_awake_window mesh_plink_timeout"
MP_CONFIG_BOOL="mesh_auto_open_plinks mesh_fwding"
MP_CONFIG_STRING="mesh_power_mode"

HE_MU_EDCA_PARAMS="he_mu_edca_ac_be_aifsn he_mu_edca_ac_be_aci he_mu_edca_ac_be_ecwmin
		   he_mu_edca_ac_be_ecwmax he_mu_edca_ac_be_timer he_mu_edca_ac_bk_aifsn
		   he_mu_edca_ac_bk_aci he_mu_edca_ac_bk_ecwmin he_mu_edca_ac_bk_ecwmax
		   he_mu_edca_ac_bk_timer he_mu_edca_ac_vi_ecwmin he_mu_edca_ac_vi_ecwmax
		   he_mu_edca_ac_vi_aifsn he_mu_edca_ac_vi_aci he_mu_edca_ac_vi_timer
		   he_mu_edca_ac_vo_aifsn he_mu_edca_ac_vo_aci he_mu_edca_ac_vo_ecwmin
		   he_mu_edca_ac_vo_ecwmax he_mu_edca_ac_vo_timer"
HE_MU_EDCA_PARAMS_DEFAULT="8 0 9 10 255 15 1 9 10 255 5 7 5 2 255 5 3 5 7 255"

APLIST=
STALIST=
UMLIST=


set_wifi_up() {
	local vif="$1"
	local ifname="$2"

	uci_set_state wireless $vif up 1
	uci_set_state wireless $vif ifname "$ifname"
}

find_net_config() {(
        local vif="$1"
        local cfg
        local ifname

        config_get cfg "$vif" network

        [ -z "$cfg" ] && {
                include /lib/network
                scan_interfaces

                config_get ifname "$vif" ifname

                cfg="$(find_config "$ifname")"
        }
        [ -z "$cfg" ] && return 0
        echo "$cfg"
)}


bridge_interface() {(
        local cfg="$1"
        [ -z "$cfg" ] && return 0

        include /lib/network
        scan_interfaces

        for cfg in $cfg; do
                config_get iftype "$cfg" type
                [ "$iftype" = bridge ] && config_get "$cfg" ifname
                prepare_interface_bridge "$cfg"
                return $?
        done
)}


mu_edca_init_config() {
	for param in $HE_MU_EDCA_PARAMS; do
		config_add_int $param
	done
}

drv_mac80211_init_device_config() {
	hostapd_common_add_device_config

	config_add_string path phy 'macaddr:macaddr'
	config_add_string hwmode
	config_add_string board_file
	config_add_int beacon_int chanbw frag rts
	config_add_int rxantenna txantenna antenna_gain txpower distance band
	mu_edca_init_config
	config_add_boolean noscan he_mu_edca skip_unii1_dfs_switch
	config_add_int he_spr_sr_control he_spr_non_srg_obss_pd_max_offset
	config_add_array ht_capab
	config_add_array channels
	config_add_boolean \
		rxldpc \
		short_gi_80 \
		short_gi_160 \
		tx_stbc_2by1 \
		su_beamformer \
		su_beamformee \
		mu_beamformer \
		vht_txop_ps \
		htc_vht \
		rx_antenna_pattern \
		tx_antenna_pattern
	config_add_int vht_max_mpdu vht_link_adapt vht160 rx_stbc tx_stbc
	config_add_int max_ampdu_length_exp ru_punct_bitmap ru_punct_acs_threshold \
		ccfs
	config_add_boolean \
               ldpc \
               greenfield \
               short_gi_20 \
               short_gi_40 \
	       max_amsdu \
               dsss_cck_40
	config_add_boolean multiple_bssid ema ru_punct_ofdma disable_csa_dfs use_ru_puncture_dfs
	config_add_boolean disable_eml_cap discard_6g_awgn_event
	config_add_int he_su_beamformer he_su_beamformee he_mu_beamformer
	config_add_int he_ul_mumimo eht_ulmumimo_80mhz eht_ulmumimo_160mhz eht_ulmumimo_320mhz
	config_add_int eht_su_beamformer eht_su_beamformee eht_mu_beamformer
}

drv_mac80211_init_iface_config() {
	hostapd_common_add_bss_config

	config_add_string 'macaddr:macaddr' ifname mld

	config_add_boolean ftm_responder wds extsta powersave
	config_add_int maxassoc
	config_add_int max_listen_int
	config_add_int dtim_period start_disabled

	config_add_int fq_limit
	config_add_int fils_discovery unsol_bcast_presp

	# mesh
	config_add_string mesh_id
	config_add_int $MP_CONFIG_INT
	config_add_boolean $MP_CONFIG_BOOL
	config_add_string $MP_CONFIG_STRING
}

mac80211_add_capabilities() {
	local __var="$1"; shift
	local __mask="$1"; shift
	local __out= oifs

	oifs="$IFS"
	IFS=:
	for capab in "$@"; do
		set -- $capab

		[ "$(($4))" -gt 0 ] || continue
		[ "$(($__mask & $2))" -eq "$((${3:-$2}))" ] || continue
		__out="$__out[$1]"
	done
	IFS="$oifs"

	export -n -- "$__var=$__out"
}

mac80211_add_he_capabilities() {
	local __out= oifs

	oifs="$IFS"
	IFS=:
	for capab in "$@"; do
		set -- $capab
		[ "$(($4))" -gt 0 ] || continue
		[ "$(((0x$2) & $3))" -gt 0 ] || continue
		append base_cfg "$1=1" "$N"
	done
	IFS="$oifs"
}

mac80211_set_he_muedca_params() {
	json_select config

	for param in $HE_MU_EDCA_PARAMS; do
		json_get_vars $param

		eval set_value=\$$param
		if [ -n "$set_value" ]; then
			eval $params=$set_value
		else
			eval $param=$1
		fi

		shift 1
		eval mu_edca_setting=\$$param
		append base_cfg "$param=$mu_edca_setting" "$N"
	done
}

mac80211_get_seg0() {
	local ht_mode="$1"
	local seg0=0

	case "$ht_mode" in
		40)
			if [ $freq -gt 5950 ] && [ $freq -le 7115 ]; then
				case "$(( ($channel / 4) % 2 ))" in
					1) seg0=$(($channel - 2));;
					0) seg0=$(($channel + 2));;
				esac
			elif [ $freq != 5935 ]; then
				case "$(( ($channel / 4) % 2 ))" in
					1) seg0=$(($channel + 2));;
					0) seg0=$(($channel - 2));;
				esac
			fi
		;;
		80)
			if [ $freq -gt 5950 ] && [ $freq -le 7115 ]; then
				case "$(( ($channel / 4) % 4 ))" in
					0) seg0=$(($channel + 6));;
					1) seg0=$(($channel + 2));;
					2) seg0=$(($channel - 2));;
					3) seg0=$(($channel - 6));;
				esac
			elif [ $freq != 5935 ]; then
				case "$(( ($channel / 4) % 4 ))" in
					1) seg0=$(($channel + 6));;
					2) seg0=$(($channel + 2));;
					3) seg0=$(($channel - 2));;
					0) seg0=$(($channel - 6));;
				esac
			fi
		;;
		160)
			if [ $freq -gt 5950 ] && [ $freq -le 7115 ]; then
				case "$channel" in
					1|5|9|13|17|21|25|29) seg0=15;;
					33|37|41|45|49|53|57|61) seg0=47;;
					65|69|73|77|81|85|89|93) seg0=79;;
					97|101|105|109|113|117|121|125) seg0=111;;
					129|133|137|141|145|149|153|157) seg0=143;;
					161|165|169|173|177|181|185|189) seg0=175;;
					193|197|201|205|209|213|217|221) seg0=207;;
				esac
			elif [ $freq != 5935 ]; then
				case "$channel" in
					36|40|44|48|52|56|60|64) seg0=50;;
					100|104|108|112|116|120|124|128) seg0=114;;
					149|153|157|161|165|169|173|177) seg0=163;;
				esac
			fi
		;;
		320)
			if [ $freq -ge 5955 ] && [ $freq -le 7115 ]; then
				case "$channel" in
					1|5|9|13|17|21|25|29|33|37|41|45) seg0=31;;
					49|53|57|61|65|69|73|77) seg0=63;;
					81|85|89|93|97|101|105|109) seg0=95;;
					113|117|121|125|129|133|137|141) seg0=127;;
					145|149|153|157|161|165|169|173) seg0=159;;
					177|181|185|189|193|197|201|205|209|213|217|221) seg0=191;;
				esac
			elif [ $freq -ge 5500 ] && [ $freq -le 5730 ]; then
				seg0=130
			fi
		;;
	esac
	printf "$seg0"
}

mac80211_hostapd_setup_base() {
	local phy="$1"
	local device="$2"
	local sedString="$(get_awk_string "$phy" "$device")"

	json_select config

	[ "$auto_channel" -gt 0 ] && channel=acs_survey
	[ "$auto_channel" -gt 0 ] && json_get_values channel_list channels

	json_get_vars noscan he_mu_edca:-he_mu_edca=0 skip_unii1_dfs_switch
	json_get_vars he_spr_sr_control he_spr_non_srg_obss_pd_max_offset:1 disable_csa_dfs use_ru_puncture_dfs
	json_get_values ht_capab_list ht_capab
	json_get_vars disable_eml_cap discard_6g_awgn_event

	if [ "$band" != 3 ]; then
		ieee80211n=1
		ht_capab=
		case "$htmode" in
			VHT20|HT20|HE20|EHT20) ;;
			HT40*|VHT40|VHT80|VHT160|HE40|HE80|HE160|EHT40|EHT80|EHT160|EHT320)
				case "$hwmode" in
					a)
						case "$(( ($channel / 4) % 2 ))" in
							1) ht_capab="[HT40+]";;
							0) ht_capab="[HT40-]";;
						esac
					;;
					*)
						case "$htmode" in
							HT40+) ht_capab="[HT40+]";;
							HT40-) ht_capab="[HT40-]";;
							*)
								if [ "$channel" -lt 7 ]; then
									ht_capab="[HT40+]"
								else
									ht_capab="[HT40-]"
								fi
							;;
						esac
					;;
				esac
				[ "$auto_channel" -gt 0 ] && ht_capab="[HT40+]"
			;;
			*) ieee80211n= ;;
		esac


		[ -n "$ieee80211n" ] && {
			append base_cfg "ieee80211n=1" "$N"

			json_get_vars \
			ldpc:1 \
			greenfield:0 \
			short_gi_20:1 \
			short_gi_40:1 \
			tx_stbc:1 \
			rx_stbc:3 \
			max_amsdu:1 \
			dsss_cck_40:1 \
			intolerant_40:1

			ht_cap_mask=0
			for cap in $(eval $sedString | grep 'Capabilities:' | cut -d: -f2); do
				ht_cap_mask="$(($ht_cap_mask | $cap))"
			done

			cap_rx_stbc=$((($ht_cap_mask >> 8) & 3))
			[ "$rx_stbc" -lt "$cap_rx_stbc" ] && cap_rx_stbc="$rx_stbc"
			ht_cap_mask="$(( ($ht_cap_mask & ~(0x300)) | ($cap_rx_stbc << 8) ))"

			mac80211_add_capabilities ht_capab_flags $ht_cap_mask \
			LDPC:0x1::$ldpc \
			GF:0x10::$greenfield \
			SHORT-GI-20:0x20::$short_gi_20 \
			SHORT-GI-40:0x40::$short_gi_40 \
			TX-STBC:0x80::$tx_stbc \
			RX-STBC1:0x300:0x100:1 \
			RX-STBC12:0x300:0x200:1 \
			RX-STBC123:0x300:0x300:1 \
			MAX-AMSDU-7935:0x800::$max_amsdu \
			DSSS_CCK-40:0x1000::$dsss_cck_40 \
			40-INTOLERANT:0x4000::$intolerant_40

			ht_capab="$ht_capab$ht_capab_flags"
			[ -n "$ht_capab" ] && append base_cfg "ht_capab=$ht_capab" "$N"
		}

		# 802.11ac
		enable_ac=0
		idx="$channel"
		case "$htmode" in
			VHT20|HE20|EHT20)	enable_ac=1;;
			VHT40|HE40|EHT40)
				case "$(( ($channel / 4) % 2 ))" in
					1) idx=$(($channel + 2));;
					0) idx=$(($channel - 2));;
				esac
				enable_ac=1
				if [ $channel -ge 36 ]; then
					append base_cfg "vht_oper_chwidth=0" "$N"
					append base_cfg "vht_oper_centr_freq_seg0_idx=$idx" "$N"
				fi
				;;
			VHT80|HE80|EHT80)
				case "$(( ($channel / 4) % 4 ))" in
					1) idx=$(($channel + 6));;
					2) idx=$(($channel + 2));;
					3) idx=$(($channel - 2));;
					0) idx=$(($channel - 6));;
				esac
				enable_ac=1
				append base_cfg "vht_oper_chwidth=1" "$N"
				append base_cfg "vht_oper_centr_freq_seg0_idx=$idx" "$N"
				;;
			VHT160|HE160|EHT160|EHT320)
				case "$channel" in
					36|40|44|48|52|56|60|64) idx=50;;
					100|104|108|112|116|120|124|128) idx=114;;
					149|153|157|161|165|169|173|177) idx=163;;
				esac
				enable_ac=1
				append base_cfg "vht_oper_chwidth=2" "$N"
				append base_cfg "vht_oper_centr_freq_seg0_idx=$idx" "$N"
				;;
		esac

		if [ "$enable_ac" != "0" ]; then
			json_get_vars \
			rxldpc:1 \
			short_gi_80:1 \
			short_gi_160:1 \
			tx_stbc_2by1:1 \
			su_beamformer:1 \
			su_beamformee:1 \
			mu_beamformer:1 \
			vht_txop_ps:1 \
			htc_vht:1 \
			max_ampdu_length_exp:7 \
			rx_antenna_pattern:1 \
			tx_antenna_pattern:1 \
			vht_max_mpdu:11454 \
			rx_stbc:4 \
			vht_link_adapt:3 \
			vht160:2

			if [ 1 -eq "$txantenna" ] || [ 2 -eq "$txantenna" ] || [ 4 -eq "$txantenna" ]  || [ 8 -eq "$txantenna"  ]; then
				tx_stbc_2by1=0
				su_beamformer=0
				mu_beamformer=0
			fi

			append base_cfg "ieee80211ac=1" "$N"
			vht_cap=0
			for cap in $(eval $sedString | awk -F "[()]" '/VHT Capabilities/ { print $2 }'); do
				vht_cap="$(($vht_cap | $cap))"
			done

			cap_rx_stbc=$((($vht_cap >> 8) & 7))
			[ "$rx_stbc" -lt "$cap_rx_stbc" ] && cap_rx_stbc="$rx_stbc"
			vht_cap="$(( ($vht_cap & ~(0x700)) | ($cap_rx_stbc << 8) ))"

			mac80211_add_capabilities vht_capab $vht_cap \
			RXLDPC:0x10::$rxldpc \
			SHORT-GI-80:0x20::$short_gi_80 \
			SHORT-GI-160:0x40::$short_gi_160 \
			TX-STBC-2BY1:0x80::$tx_stbc_2by1 \
			SU-BEAMFORMER:0x800::$su_beamformer \
			SU-BEAMFORMEE:0x1000::$su_beamformee \
			MU-BEAMFORMER:0x80000::$mu_beamformer \
			MU-BEAMFORMEE:0x100000::$mu_beamformee \
			VHT-TXOP-PS:0x200000::$vht_txop_ps \
			HTC-VHT:0x400000::$htc_vht \
			RX-ANTENNA-PATTERN:0x10000000::$rx_antenna_pattern \
			TX-ANTENNA-PATTERN:0x20000000::$tx_antenna_pattern \
			RX-STBC-1:0x700:0x100:1 \
			RX-STBC-12:0x700:0x200:1 \
			RX-STBC-123:0x700:0x300:1 \
			RX-STBC-1234:0x700:0x400:1 \

			#beamforming related configurationss

			[ "$(($vht_cap & 57344))" -eq 24576 ] && \
			vht_capab="$vht_capab[BF-ANTENNA-4]"
			[ "$(($vht_cap & 458752))" -eq 196608 ] && \
			[ 15 -eq "$txantenna" ] && \
			vht_capab="$vht_capab[SOUNDING-DIMENSION-4]"
			[ 7 -eq "$txantenna" -o 11 -eq "$txantenna" -o 13 -eq "$txantenna" ] && \
			vht_capab="$vht_capab[SOUNDING-DIMENSION-3]"
			[ 3 -eq "$txantenna" -o 5 -eq "$txantenna" -o 9 -eq "$txantenna" ] && \
			vht_capab="$vht_capab[SOUNDING-DIMENSION-2]"
			[ 1 -eq "$txantenna" ] && \
			vht_capab="$vht_capab[SOUNDING-DIMENSION-1]"

			# supported Channel widths
			vht160_hw=0
			case "$htmode" in
				VHT160|HE160|EHT160|EHT320)
					[ "$(($vht_cap & 12))" -eq 4 -a 1 -le "$vht160" ] && \
					vht160_hw=1
					[ "$vht160_hw" = 1 ] && vht_capab="$vht_capab[VHT160]"
					;;
				VHT80+80|HE80+80)
					[ "$(($vht_cap & 12))" -eq 8 -a 2 -le "$vht160" ] && \
					vht160_hw=2
					[ "$vht160_hw" = 2 ] && vht_capab="$vht_capab[VHT160-80PLUS80]"
					;;
			esac

			# maximum MPDU length
			vht_max_mpdu_hw=3895
			[ "$(($vht_cap & 3))" -ge 1 -a 7991 -le "$vht_max_mpdu" ] && \
			vht_max_mpdu_hw=7991
			[ "$(($vht_cap & 3))" -ge 2 -a 11454 -le "$vht_max_mpdu" ] && \
			vht_max_mpdu_hw=11454
			[ "$vht_max_mpdu_hw" != 3895 ] && \
			vht_capab="$vht_capab[MAX-MPDU-$vht_max_mpdu_hw]"

			# whether or not the STA supports link adaptation using VHT variant
			vht_link_adapt_hw=0
			[ "$(($vht_cap & 201326592))" -ge 134217728 -a 2 -le "$vht_link_adapt" ] && \
				vht_link_adapt_hw=2
			[ "$(($vht_cap & 201326592))" -ge 201326592 -a 3 -le "$vht_link_adapt" ] && \
				vht_link_adapt_hw=3
			[ "$vht_link_adapt_hw" != 0 ] && \
				vht_capab="$vht_capab[VHT-LINK-ADAPT-$vht_link_adapt_hw]"

			# Maximum A-MPDU length exponent
			max_ampdu_length_exp_hw=0
			[ "$(($vht_cap & 58720256))" -ge 8388608 -a 1 -le "$max_ampdu_length_exp" ] && \
				max_ampdu_length_exp_hw=1
			[ "$(($vht_cap & 58720256))" -ge 16777216 -a 2 -le "$max_ampdu_length_exp" ] && \
				max_ampdu_length_exp_hw=2
			[ "$(($vht_cap & 58720256))" -ge 25165824 -a 3 -le "$max_ampdu_length_exp" ] && \
				max_ampdu_length_exp_hw=3
			[ "$(($vht_cap & 58720256))" -ge 33554432 -a 4 -le "$max_ampdu_length_exp" ] && \
				max_ampdu_length_exp_hw=4
			[ "$(($vht_cap & 58720256))" -ge 41943040 -a 5 -le "$max_ampdu_length_exp" ] && \
				max_ampdu_length_exp_hw=5
			[ "$(($vht_cap & 58720256))" -ge 50331648 -a 6 -le "$max_ampdu_length_exp" ] && \
				max_ampdu_length_exp_hw=6
			[ "$(($vht_cap & 58720256))" -ge 58720256 -a 7 -le "$max_ampdu_length_exp" ] && \
				max_ampdu_length_exp_hw=7
			[ "$max_ampdu_length_exp_hw" != 0 ] && \
				vht_capab="$vht_capab[MAX-A-MPDU-LEN-EXP$max_ampdu_length_exp_hw]"

			[ -n "$vht_capab" ] && append base_cfg "vht_capab=$vht_capab" "$N"
		fi
	fi

	# 802.11ax
	enable_ax=0
	enable_be=0
	idx="$channel"
	is_6ghz=0
	if [ -n "$band" ] && [ "$band" -eq 3 ]; then
		is_6ghz=1
	fi

	if [ "$htmode" == "EHT20" ] || [ "$htmode" == "EHT40" ] || [ "$htmode" == "EHT80" ] || [ "$htmode" == "EHT160" ] || [ "$htmode" == "EHT320" ]; then
		enable_be=1;
		drv_mlo_capable=$(cat /sys/module/ath12k/parameters/mlo_capable)
		if [ -n "$drv_mlo_capable" ] && [ $drv_mlo_capable -eq 1 ]; then
			append base_cfg "mlo=1" "$N"
		fi
		config_get bonded $2 bonded
		if [ -n "$bonded" ] && [ $bonded -eq 1 ]; then
			append base_cfg "bonded=1" "$N"
		fi

		[ -n "$disable_eml_cap" ] && append base_cfg "disable_eml_cap=$disable_eml_cap" "$N"
	fi

	case "$htmode" in
		HE20|EHT20)	enable_ax=1
			if [ "$is_6ghz" == "1" ]; then
				if [ $freq == 5935 ]; then
					append base_cfg "op_class=136" "$N"
				else
					append base_cfg "op_class=131" "$N"
				fi
			fi
			;;
		HE40|EHT40)
			enable_ax=1
			idx="$(mac80211_get_seg0 "40")"
			if [ $hwmode == "a" ]; then
				if [ "$is_6ghz" == "1" ]; then
					append base_cfg "op_class=132" "$N"
				fi
				append base_cfg "he_oper_chwidth=0" "$N"
				append base_cfg "he_oper_centr_freq_seg0_idx=$idx" "$N"
				if [ $htmode == "EHT40" ]; then
					append base_cfg "eht_oper_chwidth=0" "$N"
					append base_cfg "eht_oper_centr_freq_seg0_idx=$idx" "$N"
				fi
			fi
			;;
		HE80|EHT80)
			enable_ax=1
			idx="$(mac80211_get_seg0 "80")"
			[ "$is_6ghz" == "1" ] && append base_cfg "op_class=133" "$N"
			append base_cfg "he_oper_chwidth=1" "$N"
			append base_cfg "he_oper_centr_freq_seg0_idx=$idx" "$N"
			if [ $htmode == "EHT80" ]; then
				append base_cfg "eht_oper_chwidth=1" "$N"
				append base_cfg "eht_oper_centr_freq_seg0_idx=$idx" "$N"
			fi
			;;
		HE160|EHT160)
			enable_ax=1
			idx="$(mac80211_get_seg0 "160")"
			[ "$is_6ghz" == "1" ] && append base_cfg "op_class=134" "$N"
			append base_cfg "he_oper_chwidth=2" "$N"
			append base_cfg "he_oper_centr_freq_seg0_idx=$idx" "$N"
			if [ $htmode == "EHT160" ]; then
				append base_cfg "eht_oper_chwidth=2" "$N"
				append base_cfg "eht_oper_centr_freq_seg0_idx=$idx" "$N"
			fi
			;;
		EHT320)
			enable_ax=1
			idx="$(mac80211_get_seg0 "320")"
			[ "$is_6ghz" == "1" ] && append base_cfg "op_class=137" "$N"
			append base_cfg "eht_oper_chwidth=9" "$N"
			if [ -n $ccfs ] && [ $ccfs -gt 0 ]; then
				append base_cfg "eht_oper_centr_freq_seg0_idx=$ccfs" "$N"
			elif [ -z $ccfs ] || [ "$ccfs" -eq "0" ]; then
				append base_cfg "eht_oper_centr_freq_seg0_idx=$idx" "$N"
			fi
			append base_cfg "he_oper_chwidth=2" "$N"
			idx="$(mac80211_get_seg0 "160")"
			append base_cfg "he_oper_centr_freq_seg0_idx=$idx" "$N"
			;;
	esac


	if [ "$enable_ax" != "0" ]; then
		json_get_vars \
		he_su_beamformer:1 \
		he_su_beamformee:0 \
		he_mu_beamformer:1 \
		he_twt_required:0 \
		he_spr_sr_control:0 \
		multiple_bssid ema \
		he_ul_mumimo \
		eht_ulmumimo_80mhz \
		eht_ulmumimo_160mhz \
		eht_ulmumimo_320mhz


		append base_cfg "ieee80211ax=1" "$N"
		he_phy_cap=$(eval $sedString | awk -F "[()]" '/HE PHY Capabilities/ { print $2 }' | head -1)
		he_phy_cap=${he_phy_cap:2}
		
		he_mac_cap=$(eval $sedString | awk -F "[()]" '/HE MAC Capabilities/ { print $2 }' | head -1)
		he_mac_cap=${he_mac_cap:2}
		
		mac80211_add_he_capabilities \
		he_su_beamformer:${he_phy_cap:6:2}:0x80:$he_su_beamformer \
		he_su_beamformee:${he_phy_cap:8:2}:0x1:$he_su_beamformee \
		he_mu_beamformer:${he_phy_cap:8:2}:0x2:$he_mu_beamformer \
		he_spr_sr_control:${he_phy_cap:14:2}:0x1:$he_spr_sr_control \
		he_twt_required:${he_mac_cap:0:2}:0x6:$he_twt_required

		if [ -n "$he_ul_mumimo" ]; then
			if [ $he_ul_mumimo -eq 0 ]; then
				append base_cfg "he_ul_mumimo=0" "$N"
			elif [  $he_ul_mumimo -gt 0 ]; then
				append base_cfg "he_ul_mumimo=1" "$N"
			fi
		else
			append base_cfg "he_ul_mumimo=-1" "$N"
		fi

		append base_cfg "he_default_pe_duration=4" "$N"

		if [ "$enable_be" != "0" ]; then
			json_get_vars ru_punct_bitmap:0 ru_punct_ofdma:0 ru_punct_acs_threshold:0 ccfs:0
			json_get_vars eht_su_beamformee:1 eht_su_beamformer:1 eht_mu_beamformer:1

			append base_cfg "ieee80211be=1" "$N"
			append base_cfg "eht_su_beamformer=$eht_su_beamformer" "$N"
			append base_cfg "eht_mu_beamformer=$eht_mu_beamformer" "$N"
			append base_cfg "eht_su_beamformee=$eht_su_beamformee" "$N"

			if [ -n "$eht_ulmumimo_80mhz" ]; then
				if [ $eht_ulmumimo_80mhz -eq 0 ]; then
					append base_cfg "eht_ulmumimo_80mhz=0" "$N"
				elif [  $eht_ulmumimo_80mhz -gt 0 ]; then
					append base_cfg "eht_ulmumimo_80mhz=1" "$N"
				fi
			else
				append base_cfg "eht_ulmumimo_80mhz=-1" "$N"
			fi

			if [ -n "$eht_ulmumimo_160mhz" ]; then
				if [ $eht_ulmumimo_160mhz -eq 0 ]; then
					append base_cfg "eht_ulmumimo_160mhz=0" "$N"
				elif [  $eht_ulmumimo_160mhz -gt 0 ]; then
					append base_cfg "eht_ulmumimo_160mhz=1" "$N"
				fi
			else
				append base_cfg "eht_ulmumimo_160mhz=-1" "$N"
			fi

			if [ -n "$eht_ulmumimo_320mhz" ]; then
				if [ $eht_ulmumimo_320mhz -eq 0 ]; then
					append base_cfg "eht_ulmumimo_320mhz=0" "$N"
				elif [  $eht_ulmumimo_320mhz -gt 0 ]; then
					append base_cfg "eht_ulmumimo_320mhz=1" "$N"
				fi
			else
				append base_cfg "eht_ulmumimo_320mhz=-1" "$N"
			fi

			if [ -n $ru_punct_bitmap ] && [ $ru_punct_bitmap -gt 0 ]; then
				append base_cfg "ru_punct_bitmap=$ru_punct_bitmap" "$N"
			fi
			if [ -n $ru_punct_ofdma ] && [ $ru_punct_ofdma -gt 0 ]; then
				append base_cfg "ru_punct_ofdma=$ru_punct_ofdma" "$N"
			fi
			if [ -n $ru_punct_acs_threshold ] && [ $ru_punct_acs_threshold -gt 0 ]; then
				append base_cfg "ru_punct_acs_threshold=$ru_punct_acs_threshold" "$N"
			fi
			[ -n "$use_ru_puncture_dfs" ] && append base_cfg "use_ru_puncture_dfs=$use_ru_puncture_dfs" "$N"
		fi

		[ "$he_mu_edca" != "0" ] && {
			json_select ..
			append base_cfg "he_mu_edca_qos_info_param_count=0" "$N"
			append base_cfg "he_mu_edca_qos_info_q_ack=0" "$N"
			append base_cfg "he_mu_edca_qos_info_queue_request=0" "$N"
			append base_cfg "he_mu_edca_qos_info_txop_request=0" "$N"
			mac80211_set_he_muedca_params $HE_MU_EDCA_PARAMS_DEFAULT
		}

		[ "$he_spr_sr_control" != "0" ] && {
			append base_cfg "he_spr_sr_control=$he_spr_sr_control" "$N"
			append base_cfg "he_spr_non_srg_obss_pd_max_offset=$he_spr_non_srg_obss_pd_max_offset" "$N"
		}
		config_get enable_color mac80211 enable_color 1
		if [ $enable_color -eq 1 ]; then
			bsscolor=$(head -1 /dev/urandom | tr -dc '0-9' | head -c2)
			[ -z "$bsscolor" ] && bsscolor=0
			[ "$bsscolor" != "0" ] && bsscolor=${bsscolor#0}
			bsscolor=$(($bsscolor % 63))
			bsscolor=$(($bsscolor + 1))
		fi

		[ -n "$bsscolor" ] && append base_cfg "he_bss_color=$bsscolor" "$N"

		if [ "$is_6ghz" == "1" ]; then
			if [ -z $multiple_bssid ] && [ -z $ema ]; then
				multiple_bssid=1
				ema=1
			fi
		fi

		if [ -z $mlo_capable ] || [ $mlo_capable -eq 0 ] || [ "$is_6ghz" == "1" ]; then
			if [ "$has_ap" -gt 1 ]; then
				[ -n $multiple_bssid ] && [ $multiple_bssid -gt 0 ] && append base_cfg "mbssid=1" "$N"
				[ -n $ema ] && [ $ema -gt 0 ] && append base_cfg "ema=1" "$N"
			fi
		fi
	fi
	[ -n "$disable_csa_dfs" ] && append base_cfg "disable_csa_dfs=$disable_csa_dfs" "$N"
	[ -n "$discard_6g_awgn_event" ] && append base_cfg "discard_6g_awgn_event=$discard_6g_awgn_event" "$N"

	hostapd_prepare_device_config "$hostapd_conf_file" nl80211
	cat >> "$hostapd_conf_file" <<EOF
${channel:+channel=$channel}
${channel_list:+chanlist=$channel_list}
${noscan:+noscan=$noscan}
${skip_unii1_dfs_switch:+skip_unii1_dfs_switch=$skip_unii1_dfs_switch}
$base_cfg

EOF
	json_select ..
}

mac80211_wds_support_check() {
	local phy="$1"

	local platform=$(grep -o "IPQ.*" /proc/device-tree/model | awk -F/ '{print $1}')
	case "$platform" in
		"IPQ8074" | "IPQ6018" | "IPQ5018")
			frame_mode=$(cat /sys/module/ath11k/parameters/frame_mode)
			;;
		"IPQ9574")
			local freq="$(get_freq "$phy" "$channel" "$device")"
			local board_type=$(grep -o "IPQ.*" /proc/device-tree/model | awk -F/ '{print $2}' | awk -F- '{print $3}')

			if [ $board_type == "C6" ] && [ $freq -gt 2000 ] && [ $freq -lt 3000 ]; then
				frame_mode=$(cat /sys/module/ath11k/parameters/frame_mode)
			else
				frame_mode=$(cat /sys/module/ath12k/parameters/frame_mode)
			fi
			;;
		 "IPQ5332")
			frame_mode=$(cat /sys/module/ath12k/parameters/frame_mode)
			;;
	esac

	echo "$frame_mode"
}

mac80211_hostapd_setup_bss() {
	local phy="$1"
	local ifname="$2"
	local macaddr="$3"
	local type="$4"
	local fst_disabled
	local fst_iface1
	local fst_iface2
	local fst_group_id
	local fst_priority1
	local fst_priority2
	local fst_load=0

	hostapd_cfg=
	append hostapd_cfg "$type=$ifname" "$N"

        # 11ad uses cqm notification for packet loss. hostapd requires
        # setting disassoc_low_ack=1 in hostapd config file to disconnect
        # on packet loss indication
        [ $hwmode == "ad" ] && append hostapd_cfg "disassoc_low_ack=1" "$N"

	local net_cfg bridge
	net_cfg="$(find_net_config "$vif")"
	[ -z "$net_cfg" -o "$isolate" = 1 -a "$mode" = "wrap" ] || {
		bridge="$(bridge_interface "$net_cfg")"
		config_set "$vif" bridge "$bridge"
	}

	hostapd_set_bss_options hostapd_cfg "$vif" || return 1
	json_get_vars ftm_responder wds dtim_period max_listen_int start_disabled
	json_get_vars fils_discovery:0 unsol_bcast_presp:0

	set_default wds 0
	set_default ftm_responder 0
	set_default start_disabled 0

	if [ "$wds" -gt 0 ]; then
		frame_mode=$(mac80211_wds_support_check "$phy")

		if [ $frame_mode -ne 1 ]; then
			echo WDS is supported only in native wifi mode. Kindly update the config > /dev/ttyMSM0
			return
		fi
		append hostapd_cfg "wds_sta=1" "$N"
	fi
	[ "$ftm_responder" -gt 0 ] && append hostapd_cfg "ftm_responder=1" "$N"
	[ "$staidx" -gt 0 -o "$start_disabled" -eq 1 ] && append hostapd_cfg "start_disabled=1" "$N"

	config_load fst && {
		fst_load=1
	}

	if [ $fst_load == 1 ] ; then
		config_get fst_disabled config disabled
		config_get fst_iface1 config interface1
		config_get fst_iface2 config interface2
		config_get fst_group_id config mux_interface
		config_get fst_priority1 config interface1_priority
		config_get fst_priority2 config interface2_priority

		if [ $fst_disabled -eq 0 ] ; then
			if [ "$ifname" == $fst_iface1 ] ; then
				append hostapd_cfg "fst_group_id=$fst_group_id" "$N"
				append hostapd_cfg "fst_priority=$fst_priority1" "$N"
			elif [ "$ifname" == $fst_iface2 ] ; then
				append hostapd_cfg "fst_group_id=$fst_group_id" "$N"
				append hostapd_cfg "fst_priority=$fst_priority2" "$N"
			fi
		fi
	fi

	if [ "$is_6ghz" == "1" ]; then
		fils_cfg=
		if [ "$unsol_bcast_presp" -gt 0 ] && [ "$unsol_bcast_presp" -le 20 ]; then
			append fils_cfg "unsol_bcast_probe_resp_interval=$unsol_bcast_presp" "$N"
		elif [ "$fils_discovery" -gt 0 ] && [ "$fils_discovery" -le 20 ]; then
			append fils_cfg "fils_discovery_max_interval=$fils_discovery" "$N"
		else
			append fils_cfg "fils_discovery_max_interval=20" "$N"
		fi

		if [ -n "$multiple_bssid" ] && [ "$multiple_bssid" -eq 1 ] && [ "$type" == "interface" ]; then
			append hostapd_cfg "$fils_cfg" "$N"
		elif [ -z "$multiple_bssid" ] || [ "$multiple_bssid" -eq 0 ]; then
			append hostapd_cfg "$fils_cfg" "$N"
		fi
	fi

	cat >> "$hostapd_conf_file" << EOF
$hostapd_cfg
bssid=$macaddr
${dtim_period:+dtim_period=$dtim_period}
${max_listen_int:+max_listen_interval=$max_listen_int}
EOF
}

mac80211_get_addr() {
       local phy="$1"
       local idx="$(($2 + 1))"

       head -n $(($macidx + 1)) /sys/class/ieee80211/${phy}/addresses | tail -n1
}

mac80211_generate_mac() {
	local phy="$1"
	local id="${macidx:-0}"
	local mode="$2"
	local device="$3"

	local ref="$(cat /sys/class/ieee80211/${phy}/macaddress)"
	local mask="$(cat /sys/class/ieee80211/${phy}/address_mask)"

	[ "$mask" = "00:00:00:00:00:00" ] && {
               mask="ff:ff:ff:ff:ff:ff";

               [ "$(wc -l < /sys/class/ieee80211/${phy}/addresses)" -gt 1 ] && {
                       addr="$(mac80211_get_addr "$phy" "$id")"
                       [ -n "$addr" ] && {
                               echo "$addr"
                               return
			}
		}
	}

	if [ $is_sphy_mband -eq 1 ]; then
		dev_idx=${device:11:1}
		local ref_dec=$( printf '%d\n' $( echo "0x$ref" | tr -d ':' ) )
		local mac_mask=$(($(($(($dev_idx << 8)) | $dev_idx))))
		local genref="$( echo $( printf '%012x\n' $(($(($mac_mask + $ref_dec))))) \
			| sed 's!\(..\)!\1:!g;s!:$!!' )"
                ref=$genref
        fi

	local oIFS="$IFS"; IFS=":"; set -- $mask; IFS="$oIFS"

	local mask1=$1
	local mask6=$6

	local oIFS="$IFS"; IFS=":"; set -- $ref; IFS="$oIFS"

	macidx=$(($id + 1))

	if [ "$mode" == "ap" ] && [ $multiple_bssid -eq 1 ] && [ $id -gt 0 ]; then
		local ref_dec=$( printf '%d\n' $( echo "0x$ref" | tr -d ':' ) )
		local bssid_l_mask=$(((1 << $max_bssid_ind) - 1))
		local bssid_l=$(((($ref_dec & $bssid_l_mask) + $id) % $max_bssid))
		local bssid_h=$((($bssid_l_mask ^ 0xFFFFFFFFFFFF) & $ref_dec))
		printf $( echo $( printf '%012x\n' $((bssid_h | bssid_l))) | sed 's!\(..\)!\1:!g;s!:$!!' )
		return
	fi

	[ "$((0x$mask1))" -gt 0 ] && {
		b1="0x$1"
		[ "$id" -gt 0 ] && \
			b1=$(($b1 ^ ((($id - 1) << 2) | 0x2)))
		printf "%02x:%s:%s:%s:%s:%s" $b1 $2 $3 $4 $5 $6
		return
	}

	[ "$((0x$mask6))" -lt 255 ] && {
		printf "%s:%s:%s:%s:%s:%02x" $1 $2 $3 $4 $5 $(( 0x$6 ^ $id ))
		return
	}

	off2=$(( (0x$6 + $id) / 0x100 ))
	printf "%s:%s:%s:%s:%02x:%02x" \
		$1 $2 $3 $4 \
		$(( (0x$5 + $off2) % 0x100 )) \
		$(( (0x$6 + $id) % 0x100 ))
}

find_phy() {
	[ -n "$phy" -a -d /sys/class/ieee80211/$phy ] && return 0

	# Incase multiple radio's are in the same soc, device path
	# for these radio's will be the same. In such case we can
	# get the phy based on the phy index of the soc
	local radio_idx=${1:5:1}
	local first_phy_idx=0
	local delta=0
	local phy_count=0
	local try=0
	config_load wireless
	while :; do
	if [ $is_sphy_mband -eq 1 ]; then
		devname=radio$radio_idx\_band$first_phy_idx
	else
		devname=radio$first_phy_idx
	fi
	config_get devicepath "$devname" path

	[ -n "$devicepath" -a -n "$path" ] || break
	[ "$path" == "$devicepath" ] && break
	first_phy_idx=$(($first_phy_idx + 1))
	done

	if [ -z "$is_sphy_mband" ]; then
		delta=$(($radio_idx - $first_phy_idx))
	fi

	[ -n "$path" ] && {
		for phy in $(ls /sys/class/ieee80211 2>/dev/null); do
			case "$(readlink -f /sys/class/ieee80211/$phy/device)" in
				*$path)
					if [ $delta -gt 0 ]; then
						delta=$(($delta - 1))
						continue;
					fi
					return 0;;
			esac
		done
	}

	[ -n "$macaddr" ] && {
		for phy in $(ls /sys/class/ieee80211 2>/dev/null); do
			grep -i -q "$macaddr" "/sys/class/ieee80211/${phy}/macaddress" && return 0
		done
	}
	return 1
}

mac80211_check_ap() {
	has_ap=$((has_ap+1))
}

mac80211_iw_interface_add() {
	local phy="$1"
	local ifname="$2"
	local type="$3"
	local wdsflag="$4"
	local rc

	iw phy "$phy" interface add "$ifname" type "$type" $wdsflag
	rc="$?"

	[ "$rc" = 233 ] && {
		# Device might have just been deleted, give the kernel some time to finish cleaning it up
		sleep 1

		iw phy "$phy" interface add "$ifname" type "$type" $wdsflag
		rc="$?"
	}

	[ "$rc" = 233 ] && {
		# Keep matching pre-existing interface
		[ -d "/sys/class/ieee80211/${phy}/device/net/${ifname}" ] && \
		case "$(iw dev $ifname info | grep "^\ttype" | cut -d' ' -f2- 2>/dev/null)" in
			"AP")
				[ "$type" = "__ap" ] && rc=0
				;;
			"IBSS")
				[ "$type" = "adhoc" ] && rc=0
				;;
			"managed")
				[ "$type" = "managed" ] && rc=0
				;;
			"mesh point")
				[ "$type" = "mp" ] && rc=0
				;;
			"monitor")
				[ "$type" = "monitor" ] && rc=0
				;;
		esac
	}

	[ "$rc" = 233 ] && {
		iw dev "$ifname" del
		sleep 1

		iw phy "$phy" interface add "$ifname" type "$type" $wdsflag
		rc="$?"
	}

	[ "$rc" = 233 ] && {
		# Device might not support virtual interfaces, so the interface never got deleted in the first place.
		# Check if the interface already exists, and avoid failing in this case.
		ip link show dev "$ifname" >/dev/null 2>/dev/null && rc=0
	}

	[ "$rc" = 234 ] && [ "$hwmode" = "11ad" ] && {
		# In case of 11ad, interface cannot be removed, so at list one interface will always exist,
		# in such case do not fail.
		return 0;
	}

	[ "$rc" != 0 ] && wireless_setup_failed INTERFACE_CREATION_FAILED

	return $rc
}

mac80211_get_mld_idx() {

	mld_name=$1
	config_load wireless
	mac80211_get_wifi_mlds() {
		append _mlds $1
	}
	config_foreach mac80211_get_wifi_mlds wifi-mld

	if [ -z "$_mlds" ]; then
		return
	fi

	index=$2
	for _mld in $_mlds
	do
		if [ "$mld_name" == "$_mld" ]; then
			echo $index
			return
		else
			index=$((index+1))
		fi
	done
	return
}
mac80211_prepare_vif() {
	json_get_vars vif
	json_select config

	json_get_vars ifname mode ssid wds extsta powersave macaddr mld

	if [ $is_sphy_mband -eq 1 ]; then
		wdev=$((${1:5:1} + ${1:11:1}))
		config_get mlo_caps $device mlo_capable
		if ([ -n "$mlo_caps" ] && [ $mlo_caps -eq 1 ] && [ -n "$mld" ]); then
			config_get mld_ifname "$mld" ifname
			if [ -z "$mld_ifname" ]; then
				ml_idx=$(mac80211_get_mld_idx $mld ${1:5:1})
				[ -z "$ml_idx" ] || ifname="wlan$ml_idx"
			else
				ifname="$mld_ifname"
			fi
			[ -n "$ifname" ] || [ -n "$if_idx" ] || if_idx=1
		else
			if [ $mld_vaps_count -gt 0 ]; then
				[ -n "$if_idx" ] || if_idx=1
			fi
		fi

		[ -n "$ifname" ] || ifname="wlan${wdev#wlan}${if_idx:+-$if_idx}"

		if ([ -n "$mlo_caps" ] && [ $mlo_caps -eq 1 ]) && [ -n "$mld" ]; then
			uci_set wireless "$mld" ifname "$ifname"
			uci commit wireless
		fi
	else
		for wdev in $(list_phy_interfaces "$phy"); do
			phy_name="$(cat /sys/class/ieee80211/${phy}/device/net/${wdev}/phy80211/name)"
			if [ "$phy_name" == "$phy" ]; then
				if_name = $wdev
				break;
			fi
		done
		[ -n "$ifname" ] || ifname="wlan${phy#phy}${if_idx:+-$if_idx}"
	fi

	if_idx=$((${if_idx:-0} + 1))

	set_default wds 0
	set_default extsta 0
	set_default powersave 0

	json_select ..

	[ -n "$macaddr" ] || {
		macaddr="$(mac80211_generate_mac $phy $mode $1)"
		macidx="$(($macidx + 1))"
	}

	json_add_object data
	json_add_string ifname "$ifname"
	json_close_object
	json_select config

	# It is far easier to delete and create the desired interface
	case "$mode" in
		adhoc)
			mac80211_iw_interface_add "$phy" "$ifname" adhoc || return
			UMLIST="${UMLIST}$ifname "
		;;
		ap)
			# Hostapd will handle recreating the interface and
			# subsequent virtual APs belonging to the same PHY
			if [ -n "$hostapd_ctrl" ]; then
				type=bss
			else
				type=interface
			fi

			mac80211_hostapd_setup_bss "$phy" "$ifname" "$macaddr" "$type" || return

			[ -n "$hostapd_ctrl" ] || {
				mac80211_iw_interface_add "$phy" "$ifname" __ap || return
				hostapd_ctrl="${hostapd_ctrl:-/var/run/hostapd/$ifname}"
			}

			[ -n "$ap_ifname" ] || ap_ifname=$ifname

			APLIST="${APLIST}$ifname "

			if [ $hwmode = "11ad" ]; then
				set_wifi_up $vif $ifname
			fi
		;;
		mesh)
			mac80211_iw_interface_add "$phy" "$ifname" mp || return
			STALIST="${STALIST}$ifname "
		;;
		monitor)
			mac80211_iw_interface_add "$phy" "$ifname" monitor || return
			UMLIST="${UMLIST}$ifname "
		;;
		sta)
			extsta_path=/sys/module/mac80211/parameters/extsta
			[ -e $extsta_path ] && echo $extsta > $extsta_path
			local wdsflag=
			staidx="$(($staidx + 1))"
			if [ "$wds" -gt 0 ]; then
				frame_mode=$(mac80211_wds_support_check "$phy")
				if [ $frame_mode -ne 1 ]; then
					echo WDS is supported only in native wifi mode. Kindly update the config > /dev/ttyMSM0
					return
				fi
				wdsflag="4addr on"
			fi
			mac80211_iw_interface_add "$phy" "$ifname" managed "$wdsflag" || return
			if [ "$wds" -gt 0 ]; then
				iw "$ifname" set 4addr on
			else
				iw "$ifname" set 4addr off
			fi
			[ "$powersave" -gt 0 ] && powersave="on" || powersave="off"
			iw "$ifname" set power_save "$powersave"
			sta_ifname=$ifname
			STALIST="${STALIST}$ifname "
		;;
	esac

	case "$mode" in
		monitor)
			if echo "$devname" | grep -q  "band"; then
				[ "$auto_channel" -gt 0 ] || iw dev "$ifname" add channel "$channel" $htmode
			else
				[ "$auto_channel" -gt 0 ] || iw dev "$ifname" set channel "$channel" $htmode
			fi
		;;
		mesh)
			[ "$auto_channel" -gt 0 ] || iw dev "$ifname" set channel "$channel" $htmode
		;;
	esac

	if [ "$mode" != "ap" ]; then
		# ALL ap functionality will be passed to hostapd
		# All interfaces must have unique mac addresses
		# which can either be explicitly set in the device
		# section, or automatically generated
		ifconfig "$ifname" down
		ifconfig "$ifname" hw ether "$macaddr"
		ifconfig "$ifname" up
	fi

	json_select ..
}

mac80211_setup_supplicant() {
	wpa_supplicant_prepare_interface "$ifname" nl80211 || return 1
	wpa_supplicant_add_network "$ifname"
	wpa_supplicant_run "$ifname"
}

mac80211_setup_supplicant_noctl() {
	local centre_freq
	local wpa_state
	local cac_state

	wpa_supplicant_prepare_interface "$ifname" nl80211 || return 1
	wpa_supplicant_add_network "$ifname" "$freq" "$htmode" "$noscan" "$ru_punct_bitmap" "$disable_csa_dfs" "$ccfs"
	wpa_supplicant_run "$ifname"

	if [ ! $channel = "acs_survey" ] && [ ! $channel -eq 0 ];then
		case "$htmode" in
			VHT20|HT20|HE20|EHT20)
				centre_freq="$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 "20")")";;
			HT40*|VHT40|HE40|EHT40)
				centre_freq="$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 "40")")";;
			VHT80|HE80|EHT80)
				centre_freq="$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 "80")")";;
			VHT160|HE160|EHT160)
				centre_freq="$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 "160")")";;
			EHT320)
				centre_freq="$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 "320")")";;
		esac
	fi

	while true;
	do
		if [ $(wpa_cli -i $ifname status 2> /dev/null | grep wpa_state | cut -d'=' -f 2) = "COMPLETED" ]; then
			break;
		fi

		if [ $(wpa_cli -i $ifname status 2> /dev/null | grep wpa_state | cut -d'=' -f 2) = "DISCONNECTED" ]; then
			continue
		fi

		wpa_state="$(wpa_cli -i $ifname status 2> /dev/null | grep wpa_state | cut -d'=' -f 2)"
		if [ $centre_freq -gt 5240 ] && [ $centre_freq -lt 5745 ]; then
			cac_state="$(wpa_cli -i $ifname status 2> /dev/null | grep cac | cut -d'=' -f 2)"
			if [ $wpa_state = "SCANNING" ] && [ $cac_state = "inprogress" ]; then
				break;
			fi
		fi

		if [ $wpa_state = "INACTIVE" ]; then
			break;
		fi
		usleep 100000
	done
}

mac80211_setup_adhoc() {
	json_get_vars bssid ssid key mcast_rate

	keyspec=
	[ "$auth_type" == "wep" ] && {
		set_default key 1
		case "$key" in
			[1234])
				local idx
				for idx in 1 2 3 4; do
					json_get_var ikey "key$idx"

					[ -n "$ikey" ] && {
						ikey="$(($idx - 1)):$(prepare_key_wep "$ikey")"
						[ $idx -eq $key ] && ikey="d:$ikey"
						append keyspec "$ikey"
					}
				done
			;;
			*)
				append keyspec "d:0:$(prepare_key_wep "$key")"
			;;
		esac
	}

	brstr=
	for br in $basic_rate_list; do
		hostapd_add_rate brstr "$br"
	done

	mcval=
	[ -n "$mcast_rate" ] && hostapd_add_rate mcval "$mcast_rate"

	case "$htmode" in
		VHT20|HT20) ibss_htmode=HT20;;
		HT40*|VHT40|VHT160)
			case "$hwmode" in
				a)
					case "$(( ($channel / 4) % 2 ))" in
						1) ibss_htmode="HT40+" ;;
						0) ibss_htmode="HT40-";;
					esac
				;;
				*)
					case "$htmode" in
						HT40+) ibss_htmode="HT40+";;
						HT40-) ibss_htmode="HT40-";;
						*)
							if [ "$channel" -lt 7 ]; then
								ibss_htmode="HT40+"
							else
								ibss_htmode="HT40-"
							fi
						;;
					esac
				;;
			esac
			[ "$auto_channel" -gt 0 ] && ibss_htmode="HT40+"
		;;
		VHT80)
			ibss_htmode="80MHz"
		;;
		NONE|NOHT)
			ibss_htmode="NOHT"
		;;
		*) ibss_htmode="" ;;
	esac

	iw dev "$ifname" ibss join "$ssid" $freq $ibss_htmode fixed-freq $bssid \
		beacon-interval $beacon_int \
		${brstr:+basic-rates $brstr} \
		${mcval:+mcast-rate $mcval} \
		${keyspec:+keys $keyspec}
}

mac80211_set_fq_limit() {
	json_select data
	json_get_vars ifname
	json_select ..

	json_select config
	json_get_vars fq_limit

	if [ $fq_limit -gt 0 ]; then
		tc qdisc add dev $ifname parent :1 fq_codel limit $fq_limit
		tc qdisc add dev $ifname parent :2 fq_codel limit $fq_limit
		tc qdisc add dev $ifname parent :3 fq_codel limit $fq_limit
		tc qdisc add dev $ifname parent :4 fq_codel limit $fq_limit
	fi
	json_select ..
}

mac80211_setup_vif() {
	local name="$1"
	local failed

	json_select data
	json_get_vars ifname
	json_select ..

	json_select config
	json_get_vars mode
	json_get_var vif_txpower txpower

	if [ "$mode" != "ap" ] || \
	   ( [[ "$mode" = "ap" ]] &&  [ $hostapd_started -eq 1 ] ) ; then
		ifconfig "$ifname" up || {
			wireless_setup_vif_failed IFUP_ERROR
			json_select ..
			return
		}
	fi

	set_default vif_txpower "$txpower"
	[ -z "$vif_txpower" ] || iw dev "$ifname" set txpower fixed "${vif_txpower%%.*}00"

	case "$mode" in
		mesh)
			json_get_vars key
			if [ -n "$key" ]; then
				wireless_vif_parse_encryption
				freq="$(get_freq "$phy" "$channel" "$device")"
				freq_list="$(get_freq_list "$phy" "$channel" "$device")"
				mac80211_setup_supplicant_noctl || failed=1
			else
				json_get_vars mesh_id mcast_rate

				mcval=
				[ -n "$mcast_rate" ] && wpa_supplicant_add_rate mcval "$mcast_rate"

				case "$htmode" in
					VHT20|HT20|HE20) mesh_htmode=HT20;;
					HT40*|VHT40|HE40)
						case "$hwmode" in
							a)
								case "$(( ($channel / 4) % 2 ))" in
									1) mesh_htmode="HT40+" ;;
									0) mesh_htmode="HT40-";;
								esac
							;;
						*)
								case "$htmode" in
									HT40+) mesh_htmode="HT40+";;
									HT40-) mesh_htmode="HT40-";;
									*)
										if [ "$channel" -lt 7 ]; then
											mesh_htmode="HT40+"
										else
											mesh_htmode="HT40-"
										fi
									;;
								esac
							;;
						esac
					;;
					VHT80|HE80|EHT80)
						mesh_htmode="80MHz"
					;;
					*) mesh_htmode="NOHT" ;;
				esac

				freq="$(get_freq "$phy" "$channel" "$device")"
				iw dev "$ifname" mesh join "$mesh_id" freq $freq $mesh_htmode \
					${ru_punct_bitmap:+ru-puncturing-bitmap $ru_punct_bitmap} \
					${mcval:+mcast-rate $mcval} \
					beacon-interval $beacon_int

			fi

			for var in $MP_CONFIG_INT $MP_CONFIG_BOOL $MP_CONFIG_STRING; do
				json_get_var mp_val "$var"
				[ -n "$mp_val" ] && iw dev "$ifname" set mesh_param "$var" "$mp_val"
			done
		;;
		adhoc)
			wireless_vif_parse_encryption
			if [ "$wpa" -gt 0 -o "$auto_channel" -gt 0 ]; then
				freq="$(get_freq "$phy" "$channel" "$device")"
				mac80211_setup_supplicant_noctl || failed=1
			else
				mac80211_setup_adhoc
			fi
		;;
		sta)
			if [ "$auto_channel" -gt 0 ]; then
				chan=$(echo ${channel_list} | cut -d '-' -f 1)
				freq="$(get_freq "$phy" "$chan" "$device")"
			else
				freq="$(get_freq "$phy" "$channel" "$device")"
			fi

			freq_list=$(get_sta_freq_list $phy $freq)
			mac80211_setup_supplicant || failed=1
		;;
		monitor)
			case "$htmode" in
				VHT20|HT20|HE20|EHT20)
					if echo "$devname" | grep -q  "band"; then
						iw dev "$ifname" add freq "$freq" "20"
					else
						iw dev "$ifname" set freq "$freq" "20"
					fi
					;;
				HT40*|VHT40|HE40|EHT40)
					if echo "$devname" | grep -q  "band"; then
						iw dev "$ifname" add freq "$freq" "40" "$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 "40")")"
					else
						iw dev "$ifname" set freq "$freq" "40" "$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 "40")")"
					fi
					;;
				VHT80|HE80|EHT80)
					if echo "$devname" | grep -q  "band"; then
						iw dev "$ifname" add freq "$freq" "80" "$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 "80")")"
					else
						iw dev "$ifname" set freq "$freq" "80" "$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 "80")")"
					fi
					;;
				VHT160|HE160|EHT160)
					if echo "$devname" | grep -q  "band"; then
						iw dev "$ifname" add freq "$freq" "160" "$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 "160")")"
					else
						iw dev "$ifname" set freq "$freq" "160" "$(get_seg0_freq "$freq" "$channel" "$(mac80211_get_seg0 "160")")"
					fi
					;;
			esac
		;;
	esac

	json_select ..
	[ -n "$failed" ] || wireless_add_vif "$name" "$ifname"
}

get_seg0_freq() {
	local ctrl_freq="$1"
	local ctrl_chan="$2"
	local seg0_chan="$3"

	if [ $((seg0_chan)) -gt $((ctrl_chan)) ]; then
		printf $(($ctrl_freq + (($seg0_chan - $ctrl_chan) * 5)))
	else
		printf $(($ctrl_freq - (($ctrl_chan - $seg0_chan) * 5)))
	fi
}


get_band_from_device_idx() {
	i=$1
	phy=$2

	#fetch hw idx channels from phy info
	hw_nchans=$(iw phy ${phy} info | awk -v p1="$i channel list" -v p2="$((i+1)) channel list"  ' $0 ~ p1{f=1;next} $0 ~ p2 {f=0} f')

	for _b in `iw phy $phy info | grep 'Band ' | cut -d' ' -f 2`; do
		expr="iw phy ${phy} info | awk  '/Band ${_b}/{ f = 1; next } /Band /{ f = 0 } f'"
		expr_freq="$expr | awk '/Frequencies/,/valid /f'"

		#fetch band channels from phy info
		band_nchans=$(eval ${expr_freq} | awk '{ print $4 }' | sed -e "s/\[//g" | sed -e "s/\]//g")
		band_nchans=$(echo $band_nchans | tr -d ' ')
		hw_nchans=$(echo $hw_nchans | tr -d ' ')

		#check if the list is present in band info
		if echo "$band_nchans" | grep -q "${hw_nchans}";
		then
			echo "$_b"
			return
		fi
	done
	echo ""
}

get_awk_string() {

	local phy="$1"

	if [ $is_sphy_mband -eq 1 ]; then
		local device="$2"
		local idx=${2:11:1}
		local dev=`ls /sys/class/ieee80211/`

		local totalCount=`iw phy $phy info | grep 'Band ' | wc -l`
		local delta=$(($totalCount - $idx))

		no_hw_idx=$(iw phy ${phy} info | grep -e "channel list" | wc -l)
		if [ $no_hw_idx -gt $totalCount ]; then
			_band=$(get_band_from_device_idx $idx $phy)
			if [ -z "$_band" ]; then
				echo "band information not found in $phy info" > /dev/ttyMSM0
				return
			fi
			delta=$(($no_hw_idx - $idx))
		else
			for _band in `iw phy $phy info | grep 'Band ' | cut -d' ' -f 2`; do
				[ $idx -eq 0 ] && break
				idx=$(($idx - 1))
			done
		fi

		if [ $delta -eq 1 ]; then
			sedString="iw phy ${phy} info | awk '/Band ${_band}/,0'"
		else
			sedString="iw phy ${phy} info | awk  '/Band ${_band}/{ f = 1; next } /Band /{ f = 0 } f'"
		fi
	else
		sedString="iw phy ${phy} info"
	fi
	printf "$sedString"
}

get_sta_freq_list() {

	phy=$1
	sta_freq=$2

	hw_indices=$(iw phy ${phy} info | grep -e "channel list" | cut -d' ' -f 2)

	if [ -z $hw_indices ]; then
		#non-single wiphy architecture does not need freq list
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
get_freq() {
	local phy="$1"
	local chan="$2"
	local sedString="$(get_awk_string "$phy" "$3")"

	if [ -n "$band" ] && [ "$band" -eq 3 ]; then
		if [ "$chan" -eq 2 ]; then
			printf 5935
		else
			printf $((5950 + ($chan * 5)))
		fi
	else
		eval $sedString | grep -E -m1 "(\* ${chan:-....} MHz${chan:+|\\[$chan\\]})" | grep MHz | awk '{print $2}'
	fi
}

get_freq_list() {
	local phy="$1"
	local chan="$2"
	local sedString="$(get_awk_string "$phy" "$3")"
	local freq_list=""
	local no_sbands=$(iw phy ${phy} info | grep 'Band ' | wc -l)
	local no_hw_idx=$(iw phy ${phy} info | grep -e "channel list" | wc -l)

	if [ $no_hw_idx -gt $no_sbands ] && [ $is_sphy_mband -eq 1 ]; then
		local device="$3"
		local idx=${3:11:1}
		local chanList=$(iw phy ${phy} info | awk -v p1="$idx channel list" -v p2="$((idx+1)) channel list"  ' $0 ~ p1{f=1;next} $0 ~ p2 {f=0} f')

		for chidx in $chanList; do
			local frqs=$(eval $sedString | grep -E -m1 "(\* ${chidx:-....} MHz${chidx:+|\\[$chidx\\]})" | grep MHz | awk '{print $2}')
			freq_list="${freq_list}${frqs} "
		done
	fi
	echo $freq_list
}

mac80211_vap_cleanup() {
	local service=$1
	local vaps="$2"
	local phy=$3

	for wdev in $vaps; do
		phy_name="$(cat /sys/class/ieee80211/${phy}/device/net/${wdev}/phy80211/name)"
		if [ "$phy_name" != "$phy" ]; then
			continue;
		fi

		case $service in
			"hostapd")
				if ( [ -f "/var/run/hostapd-${wdev}.lock" ] || \
					[ -f "/var/run/hostapd-${4}.lock" ] ); then
					hostapd_cli -iglobal raw REMOVE ${wdev}
					rm /var/run/hostapd-${wdev}.lock
					rm /var/run/hostapd/${wdev}
				fi

				if [ -f "/var/run/wifi-$phy.pid" ]; then
					pid=$(cat /var/run/wifi-$phy.pid)
					kill -9 $pid
					rm -rf  /var/run/wifi-$phy.pid
					rm /var/run/hostapd/w*
				fi
			;;
			"wpa_supplicant")
				[ -f "/var/run/wpa_supplicant-${wdev}.lock" ] && { \
					wpa_cli -g /var/run/wpa_supplicantglobal interface_remove ${wdev}
					rm /var/run/wpa_supplicant-${wdev}.lock
				}

				[ -f "/var/run/wpa_supplicant-${wdev}.pid" ] && { \
					kill -9 $(cat /var/run/wpa_supplicant-${wdev}.pid)
					rm -rf /var/run/wpa_supplicant-${wdev}.pid
				}

			;;
		esac
		ifconfig "$wdev" down 2>/dev/null
		iw dev "$wdev" del
	done
}

mac80211_interface_cleanup() {
	local phy="$1"
	local device="$2"

	if [ ${#device} -eq 12 ]; then
		local dev_wlan=wlan$((${2:5:1} + ${2:11:1}))
		local ap_ifnames="$(uci -q -P /var/state get wireless.${device}.aplist)"
		local sta_ifnames="$(uci -q -P /var/state get wireless.${device}.splist)"
		local adhoc_ifnames="$(uci -q -P /var/state get wireless.${device}.umlist)"
		mac80211_vap_cleanup hostapd "$ap_ifnames" $phy $dev_wlan
		mac80211_vap_cleanup wpa_supplicant "$sta_ifnames" $phy
		mac80211_vap_cleanup none "$adhoc_ifnames" $phy

	else
		for wdev in $(list_phy_interfaces "$phy"); do
			#Ensure the interface belongs to the phy being passed
			phy_name="$(cat /sys/class/ieee80211/${phy}/device/net/${wdev}/phy80211/name)"
			if [ "$phy_name" != "$phy" ]; then
				continue
			fi
			[ -f "/var/run/hostapd-${wdev}.lock" ] && { \
				hostapd_cli -iglobal raw REMOVE ${wdev}
				rm /var/run/hostapd-${wdev}.lock
				rm /var/run/hostapd/${wdev}
			}
			[ -f "/var/run/wpa_supplicant-${wdev}.lock" ] && { \
				wpa_cli -g /var/run/wpa_supplicantglobal interface_remove ${wdev}
				rm /var/run/wpa_supplicant-${wdev}.lock
			}

			ifconfig "$wdev" down 2>/dev/null
			iw dev "$wdev" del
		done
	fi
}

drv_mac80211_cleanup() {
	killall wpa_supplicant
	for phy in $(ls /sys/class/ieee80211 2>/dev/null); do
		for wdev in $(list_phy_interfaces "$phy"); do
			#Ensure the interface belongs to the correct phy
			phy_name="$(cat /sys/class/ieee80211/${phy}/device/net/${wdev}/phy80211/name)"
			if ["$phy_name" != $phy]; then
				continue
			fi
			if [ -f "/var/run/hostapd-${wdev}.lock" ]; then
				hostapd_cli -iglobal raw REMOVE ${wdev}
				rm /var/run/hostapd-${wdev}.lock
				ifconfig "$wdev" down 2>/dev/null
				iw dev "$wdev" del
			fi
			if [ -f "/var/run/wpa_supplicant-${wdev}.lock" ]; then
				wpa_cli -g /var/run/wpa_supplicantglobal interface_remove ${wdev}
				rm /var/run/wpa_supplicant-${wdev}.lock
				ifconfig "$wdev" down 2>/dev/null
				iw dev "$wdev" del
			fi
		done
	done
}

mac80211_map_config_ifaces_to_json() {
	arg_device=$1
	index=1
	json_get_keys ifaces interfaces

	json_select interfaces
	config_cb() {
		local type="$1"
		local section="$2"
		local jiface

		config_get TYPE "$CONFIG_SECTION" TYPE
		case "$TYPE" in
			wifi-iface)
				config_get device "$CONFIG_SECTION" device
				[ "$device" == "$arg_device" ] && {
					jiface=$(echo $ifaces | cut -d' ' -f$index)
					json_select $jiface
					json_add_string "vif" "$CONFIG_SECTION"
					json_select ..
					index=$(( index+1 ))
				}
			;;
		esac
	}

	config_load wireless
	config_set "$arg_device" phy "$phy"
	json_select ..
	reset_cb
}

mac80211_update_mld_iface_config() {
	vif_name=$1
	mld_name=$2
	local _ifaces
	local _iface

	# Get the following from section wifi-mld
	config_get mld_ssid "$mld_name" ssid
	config_get mld_encryption "$mld_name" encryption
	config_get mld_key "$mld_name" key
	config_get mld_sae "$mld_name" sae_pwe

	json_get_keys _ifaces interfaces
	json_select interfaces
	for _iface in $_ifaces; do
		json_select "$_iface"
		json_select config
		json_get_vars mld
		if [[ "$mld" == "$mld_name" ]]; then
			if [ -n "$mld_ssid" ]; then
				json_add_string "ssid" "$mld_ssid"
				uci_set wireless "$vif_name" ssid "$mld_ssid"
			fi

			if [ -n "$mld_encryption" ]; then
				json_add_string "encryption" "$mld_encryption"
				uci_set wireless "$vif_name" encryption "$mld_encryption"
			fi

			if [ -n "$mld_key" ]; then
				json_add_string "key" "$mld_key"
				uci_set wireless "$vif_name" key "$mld_key"
			fi

			if [ -n "$mld_sae" ]; then
				json_add_int "sae_pwe" "$mld_sae"
				uci_set wireless "$vif_name" sae_pwe "$mld_sae"
			fi
		fi
		json_select ..
		json_select ..

	done
	uci commit wireless
	json_select ..
}

mac80211_update_mld_configs() {
	local iflist
	config_load wireless

	mac80211_update_mld_cfg() {
		append iflist $1
	}
	config_foreach mac80211_update_mld_cfg wifi-iface

	for name in $iflist
	do
		config_get mld_name $name mld
		config_get ml_device $name device
		config_get mlcaps $ml_device mlo_capable

		if ([ -n "$mlcaps" ] && [ "$mlcaps" -eq 1 ] && [ -n "$mld_name" ]); then
			mac80211_update_mld_iface_config $name $mld_name
		fi
	done
}

mac80211_derive_ml_info() {
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
}

mac80211_export_mld_info() {
	if [ -f $MLD_VAP_DETAILS ]; then
		source $MLD_VAP_DETAILS
		radio_up_count=$radio_up_count
		mld_vaps_count=$mld_vaps_count
	else
		mac80211_derive_ml_info
	fi
	mac80211_update_mld_configs
}

mac80211_update_bondif() {
	local iflist
	config_load wireless
	mac80211_get_wifi_mlds() {
		append _mlds $1
	}

	config_foreach mac80211_get_wifi_mlds wifi-mld

	if [ -z "$_mlds" ]; then
		return
	fi
	for _mld in $_mlds
	do
		config_get mld_ifname "$_mld" ifname
		config_get is_bonded "$_mld" bonded
		mac80211_export_mld_info
		#Check if ppe_ds_enable is set and then update the bondif
		if ([ $mld_vaps_count -ge 2 ] && [ -n $mld_ifname ]); then
			cmd="ls /sys/class/net"
			iface=$($cmd | grep "$mld_ifname"_b)
			if ([ -d /sys/class/net/"$mld_ifname"_b ] && [ -n $network_bridge ]); then
				bonded_macaddr=$(iw dev $mld_ifname info | grep addr | head -1 | awk '{print $2}')
				brctl delif $network_bridge $mld_ifname
				brctl addif $network_bridge "$mld_ifname"_b
				ifconfig "$mld_ifname"_b down
				ifconfig "$mld_ifname"_b hw ether $bonded_macaddr
				ifconfig "$mld_ifname"_b up
			fi
		fi
	done
	return
}

drv_mac80211_setup() {

	local device=$1
	# Note: In case of single wiphy, the device name would be radio#idx_band#bid
	#       where idx is 0 and bid is [0 - 2] when there is one phy and 3 bands.
	#       similarly it can be extended when there are multiple phy and multiple
	#       bands.
	if [ ${#device} -eq 12 ]; then
		local is_sphy_mband=1
		mac80211_export_mld_info
	fi

	json_select config
	json_get_vars \
		phy macaddr path \
		country chanbw distance \
		board_file \
		txpower antenna_gain \
		rxantenna txantenna \
		frag rts beacon_int:100 \
		htmode band multiple_bssid noscan \
		ru_punct_bitmap \
		disable_csa_dfs \
		he_ul_mumimo \
		eht_ulmumimo_80mhz \
		eht_ulmumimo_160mhz \
		eht_ulmumimo_320mhz \
		ccfs


	json_get_values basic_rate_list basic_rate
	json_get_values channel_list channels
	json_select ..

	find_phy $1 || {
		echo "Could not find PHY for device '$1'"
		sleep 1
		wireless_set_retry 1
		return 1
	}

	# workaround for buggy hostapd.sh in premium profile
	[ -e /lib/wifi/hostapd.sh ] && mac80211_map_config_ifaces_to_json $1

	wireless_set_data phy="$phy"
	mac80211_interface_cleanup "$phy" "$device"

	# convert channel to frequency
	[ "$auto_channel" -gt 0 ] || freq="$(get_freq "$phy" "$channel" "$device")"

	[ -n "$country" ] && {
		if iw reg get | grep -q "^global$"; then
			iw reg get | grep -A 1 "^global$" | grep -q "^country $country:" || {
				iw reg set "$country"
				sleep 1
			}
		else
			iw reg get | grep -q "^country $country:" || {
				iw reg set "$country"
				sleep 1
			}
		fi
	}

	if [ "$is_sphy_mband" -eq 1 ]; then
		hostapd_conf_file="/var/run/hostapd-${phy}_band${device:11:1}.conf"
	else
		hostapd_conf_file="/var/run/hostapd-$phy.conf"
	fi
	no_ap=1
	macidx=0
	staidx=0

	[ -n "$chanbw" ] && {
		for file in /sys/kernel/debug/ieee80211/$phy/ath9k/chanbw /sys/kernel/debug/ieee80211/$phy/ath5k/bwmode; do
			[ -f "$file" ] && echo "$chanbw" > "$file"
		done
	}

	set_default rxantenna all
	set_default txantenna all
	set_default distance 0
	set_default antenna_gain 0

	iw phy "$phy" set antenna $txantenna $rxantenna >/dev/null 2>&1
	iw phy "$phy" set antenna_gain $antenna_gain
	iw phy "$phy" set distance "$distance"

	[ -n "$frag" ] && iw phy "$phy" set frag "${frag%%.*}"
	[ -n "$rts" ] && iw phy "$phy" set rts "${rts%%.*}"

	has_ap=0
	hostapd_ctrl=
	for_each_interface "ap" mac80211_check_ap

	rm -f "$hostapd_conf_file"
	[ "$has_ap" -gt 0 ] && mac80211_hostapd_setup_base "$phy" "$device"

	if [ $multiple_bssid -eq 1 ] && [ "$has_ap" -gt 1 ]; then
		max_bssid_ind=0
		local iter=$((has_ap-1))
		while [ "$iter" -gt 0 ]
		do
			max_bssid_ind=$((max_bssid_ind+1))
			iter=$((iter >> 1))
		done

		max_bssid=$((1 << max_bssid_ind))
	fi

	APLIST=
	for_each_interface "ap" mac80211_prepare_vif ${device} ${multiple_bssid}
	uci -q -P /var/state set wireless.${device}.aplist="${APLIST}"

	STALIST=
	UMLIST=
	for_each_interface "sta adhoc mesh monitor" mac80211_prepare_vif ${device}
	uci -q -P /var/state set wireless.${device}.splist="${STALIST}"
	uci -q -P /var/state set wireless.${device}.umlist="${UMLIST}"

	[ -n "$board_file" ] && {
		file=/sys/class/net/$ifname/device/wil6210/board_file
		[ -f "$file" ] && echo "$board_file" > "$file"
	}

	[ "$is_sphy_mband" -eq 1 ] && {
		if [ "$mld_vaps_count" -gt 1 ] && [ "$radio_up_count" -gt 1 ]; then
			hostapd_add_bss=0
		else
			hostapd_add_bss=1
		fi
	}

	for_each_interface "mesh" mac80211_setup_vif
	[ -n "$hostapd_ctrl" ] && {
		hostapd_started=1

		if [ "$is_sphy_mband" -eq 1 ]; then
			ifname="wlan$((${device:5:1} + ${device:11:1}))"
		else
			ifname="wlan${phy#phy}"
		fi
		if [ -z "$is_sphy_mband" ] || [ "$hostapd_add_bss" -eq 1 ]; then
			[ -f "/var/run/hostapd-$ifname.lock" ] &&
				rm /var/run/hostapd-$ifname.lock
			# let hostapd manage interface $ifname
			hostapd_cli -iglobal raw ADD bss_config=$ifname:$hostapd_conf_file
			touch /var/run/hostapd-$ifname.lock
		else
			if [ -f "/var/run/wifi-$phy.pid" ]; then
				return
			fi
			touch /var/run/hostapd-$device-updated-cfg
			hostapd_cfg_updated=$(ls /var/run/hostapd-*-updated-cfg | wc -l)
			if [ "$hostapd_cfg_updated" = "$radio_up_count" ]; then
				bands_info=$(ls /var/run/hostapd*updated-cfg | grep -o band.)
				for __band in $bands_info
				do
					append  config_files /var/run/hostapd-phy${phy#phy}_${__band}.conf
				done

				/usr/sbin/hostapd -B -P /var/run/wifi-$phy.pid $config_files
				ret="$?"
				wireless_add_process "$(cat /var/run/wifi-$phy.pid)" "/usr/sbin/hostapd" 1
				[ "$ret" != 0 ] && {
					wireless_setup_failed HOSTAPD_START_FAILED
					return
				}
			else
				 hostapd_started=0
			fi
		fi
	}

	if [[ ! -z "$ap_ifname" && ! -z "$sta_ifname" && ! -z "$hostapd_conf_file" ]]; then
		if [ "$channel" -gt 48 ] && [ "$channel" -lt 149 ]; then
			hostapd_cli -i $ap_ifname set ieee80211h 0
			hostapd_cli -i $ap_ifname set ieee80211d 0
			echo "disable dfs support for repeater AP($ifname)" > /dev/ttyMSM0
		fi
	fi

	for_each_interface "ap sta adhoc monitor" mac80211_setup_vif

	wireless_set_up

	config_get enable_smp_affinity mac80211 enable_smp_affinity 0

	if [ "$enable_smp_affinity" -eq 1 ]; then
                [ -f "/lib/smp_affinity_settings.sh" ] && {
                        . /lib/smp_affinity_settings.sh
                        enable_smp_affinity_wifi
                }
                [ -f "/lib/update_smp_affinity.sh" ] && {
                        . /lib/update_smp_affinity.sh
                        enable_smp_affinity_wigig
                }
        fi

	if [[ ! -z "$ap_ifname" && ! -z "$sta_ifname" && ! -z "$hostapd_conf_file" ]]; then
		[ -f "/lib/apsta_mode.sh" ] && {
			. /lib/apsta_mode.sh "$sta_ifname" "$ap_ifname" "$hostapd_conf_file" "$band" "$phy"
			echo "$!" >> /tmp/apsta_mode.pid
		}
	fi

	[ -f "/lib/performance.sh" ] && {
		. /lib/performance.sh
	}
	for_each_interface "ap mesh" mac80211_set_fq_limit
	#Check if ppe_ds_enable is set and then update the bondif
	if [ $(cat /sys/module/ath12k/parameters/ppe_ds_enable) -eq 1 ]; then
		mac80211_update_bondif
	fi


}

list_phy_interfaces() {
	local phy="$1"
	if [ -d "/sys/class/ieee80211/${phy}/device/net" ]; then
		ls "/sys/class/ieee80211/${phy}/device/net" 2>/dev/null;
	else
		ls "/sys/class/ieee80211/${phy}/device" 2>/dev/null | grep net: | sed -e 's,net:,,g'
	fi
}

drv_mac80211_teardown() {
	# kill apsta_mode.sh if running in background
	[ -f "/tmp/apsta_mode.pid" ] && {
		kill -9 $(cat /tmp/apsta_mode.pid)
		rm /tmp/apsta_mode.pid
	}

	wireless_process_kill_all

	json_select data
	json_get_vars phy
	json_select ..

	mac80211_interface_cleanup "$phy" "$1"
}

add_driver mac80211
