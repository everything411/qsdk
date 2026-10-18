#!/usr/bin/env ucode

/*
 * This has been copied from https://github.com/openwrt/openwrt which is under GPL-2.0-only license
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: GPL-2.0-only
 */

import { readfile } from "fs";
import * as uci from 'uci';

const bands_order = [ "6G", "5G", "2G" ];
const htmode_order = [ "UHR", "EHT", "HE", "VHT", "HT" ];

let board = json(readfile("/etc/board.json"));
if (!board.wlan)
	exit(0);

let idx = 0;
let commit;
let sysupgrade = false;

let config = uci.cursor().get_all("wireless") ?? {};

print(`set wireless.mac80211=smp_affinity
set wireless.mac80211.enable_smp_affinity='1'
set wireless.mac80211.enable_color='1'
`);

function freq_to_channel(freq) {
	if (freq < 1000)
		return 0;
	if (freq == 2484)
		return 14;
	if (freq == 5935)
		return 2;
	if (freq < 2484)
		return (freq - 2407) / 5;
	if (freq >= 4910 && freq <= 4980)
		return (freq - 4000) / 5;
	if (freq < 5950)
		return (freq - 5000) / 5;
	if (freq <= 45000)
		return (freq - 5950) / 5;
	if (freq >= 58320 && freq <= 70200)
		return (freq - 56160) / 2160;
	return 0;
}

function is_scan_radio(phy_name) {
	/* Scan Radios have a phy-scan-XX or phy_scan_XX format */
	if (match(phy_name, /^phy-scan-[0-9]+$/) || match(phy_name, /^phy_scan_[0-9]+$/))
		return true;
	return false;
}

function radio_exists(path, macaddr, phy) {
	/* For scan radios: check if a wifi-iface with vap_submode=scan already exists */
	if (is_scan_radio(phy)) {
		for (let name, s in config) {
			if (s[".type"] == "wifi-iface" && s.vap_submode == "scan")
				return true;
		}
		return false;
	}

	for (let name, s in config) {
		if (s[".type"] != "wifi-device")
			continue;

		/* For multi-radio case: if we find any radio0_bandX device, */
		/* it means this phy has already been processed */
		if (match(name, /^radio[0-9]+_band[0-9]+$/)) {
			return true;
		}

		if (s.macaddr & lc(s.macaddr) == lc(macaddr))
			return true;
		if (s.phy == phy)
			return true;
		if (!s.path || !path)
			continue;
		if (substr(s.path, -length(path)) == path)
			return true;
	}
}

function get_band(freq) {
	if (freq > 50000)
		band_name = "60G";
	else if (freq > 5900)
		band_name = "6G";
	else if (freq > 4000)
		band_name = "5G";
	else if (freq > 2000)
		band_name = "2G";
	return band_name;
}

function get_channel_list_for_range(band, start_freq, end_freq) {
	let first_channel = 0;
	let last_channel = 0;

	for (let freq in band.freqs) {
		if (freq.freq < start_freq || freq.freq > end_freq)
			continue;

		if (freq.disabled)
			continue;

		let channel = freq_to_channel(freq.freq);
		if (!channel)
			continue;

		if (!first_channel)
			first_channel = channel;
		last_channel = channel;
	}

	if (!first_channel || !last_channel)
		return "";

	return first_channel + "-" + last_channel;
}

/* ADDED: helpers for proprietary→ath-ud translator */
function parse_ht_width(val) {
	if (!val)
		return null;

	let m = match(val, /([0-9]+)([+-]?)/, "s");
	if (!m) return null;
	let width = m[1];
	if (m[2] == "+") width += "PLUS";
	else if (m[2] == "-") width += "MINUS";
	return width;
}

function map_hwmode_to_htmode(hwmode, curr_htmode) {
	if (!hwmode)
		return null;

	let width = parse_ht_width(curr_htmode);

	/* 11be (EHT): ensure 11bea+HT80->EHT80; 11beg+HT40->EHT40 else EHT20 */
	if (match(hwmode, /^11be/, "s")) {
		if (width)
			return "EHT" + width;
		return (hwmode == "11beg") ? "EHT20" : "EHT80";
	}

	/* 11ax (HE): ensure 11axa+HT40->HE40 else HE20 */
	if (match(hwmode, /^11ax/, "s")) {
		if (width)
			return "HE" + width;
		return "HE20";
	}

	/* 11ac (VHT): map width generically (e.g., HT80 -> VHT80) */
	if (match(hwmode, /^11ac/, "s")) {
		if (width)
			return "VHT" + width;
		/* default to VHT80 if not specified */
		return "VHT80";
	}

	/* 11na (HT on 5GHz): map width generically (e.g., HT40 -> HT40) */
	if (match(hwmode, /^11na/, "s")) {
		if (width)
			return "HT" + width;
		/* default similar to legacy: HT40 for 11na */
		return "HT40";
	}

	/* 11ng (HT on 2.4GHz): map width generically (e.g., HT20/HT40) */
	if (match(hwmode, /^11ng/, "s")) {
		if (width)
			return "HT" + width;
		/* default: HT20 for 11ng */
		return "HT20";
	}

	return null;
}

/* ADDED: convert proprietary encryption formats to mac80211 format */
function map_encryption(enc, sae) {
	if (!enc)
		return null;

	let enc_lower = lc(enc);

	/* Single SAE gate: psk* with sae=1 should become sae-mixed */
	if (sae == "1") {
		if (match(enc_lower, /^sae-mixed/, "s"))
			return "sae-mixed";
		if (match(enc_lower, /^ccmp/, "s"))
			return "sae";
		if (match(enc_lower, /^psk/, "s"))
			return "sae-mixed";
	}

	/* WPA3 SAE variants */
	if (match(enc_lower, /^sae-mixed/, "s"))
		return "sae-mixed";  /* WPA2-PSK + WPA3-SAE */
	if (match(enc_lower, /^sae/, "s"))
		return "sae";  /* WPA3-SAE only */

	/* WPA2-PSK variants -> psk+wpa2 */
	if (match(enc_lower, /^psk2\+ccmp/, "s") || match(enc_lower, /^psk2\+aes/, "s"))
		return "psk+wpa2";  /* WPA2-PSK with CCMP */
	if (match(enc_lower, /^psk2\+tkip\+ccmp/, "s") || match(enc_lower, /^psk2\+tkip\+aes/, "s"))
		return "psk+wpa2";  /* WPA2-PSK with TKIP+CCMP */
	if (match(enc_lower, /^psk2/, "s"))
		return "psk+wpa2";  /* WPA2-PSK (generic) */

	/* WPA-PSK variants -> psk+wpa1 */
	if (match(enc_lower, /^psk\+ccmp/, "s") || match(enc_lower, /^psk\+aes/, "s"))
		return "psk+wpa1";  /* WPA-PSK with CCMP */
	if (match(enc_lower, /^psk\+tkip/, "s"))
		return "psk+wpa1";  /* WPA-PSK with TKIP */
	if (match(enc_lower, /^psk-mixed/, "s"))
		return "psk-mixed";  /* WPA/WPA2-PSK mixed */
	if (match(enc_lower, /^psk/, "s"))
		return "psk+wpa1";  /* WPA-PSK (generic) */

	/* WPA Enterprise (802.1X) variants */
	if (match(enc_lower, /^wpa2\+ccmp/, "s") || match(enc_lower, /^wpa2\+aes/, "s"))
		return "wpa2+ccmp";  /* WPA2-Enterprise with CCMP */
	if (match(enc_lower, /^wpa2\+tkip\+ccmp/, "s"))
		return "wpa2+ccmp";  /* WPA2-Enterprise with TKIP+CCMP */
	if (match(enc_lower, /^wpa2/, "s"))
		return "wpa2";  /* WPA2-Enterprise (generic) */

	if (match(enc_lower, /^wpa\+ccmp/, "s") || match(enc_lower, /^wpa\+aes/, "s"))
		return "wpa+ccmp";  /* WPA-Enterprise with CCMP */
	if (match(enc_lower, /^wpa\+tkip/, "s"))
		return "wpa+tkip";  /* WPA-Enterprise with TKIP */
	if (match(enc_lower, /^wpa-mixed/, "s"))
		return "wpa-mixed";  /* WPA/WPA2-Enterprise mixed */
	if (match(enc_lower, /^wpa/, "s"))
		return "wpa";  /* WPA-Enterprise (generic) */

	/* WEP variants (legacy) */
	if (match(enc_lower, /^wep-open/, "s"))
		return "wep-open";  /* WEP with open authentication */
	if (match(enc_lower, /^wep-shared/, "s"))
		return "wep-shared";  /* WEP with shared key authentication */
	if (match(enc_lower, /^wep/, "s"))
		return "wep";  /* WEP (generic) */

	/* OWE (Opportunistic Wireless Encryption) */
	if (match(enc_lower, /^owe/, "s"))
		return "owe";  /* Enhanced Open / OWE */

	/* No encryption */
	if (match(enc_lower, /^none/, "s") || match(enc_lower, /^open/, "s"))
		return "none";  /* Open network */

	/* Return original if no mapping needed */
	return enc;
}

/* Helper: determine if iface is station mode */
function is_sta_iface(s) {
	return (s && s.mode && lc(s.mode) == 'sta');
}

/* Helper: normalize station interface options */
function normalize_sta_iface(secname, s) {
	let changed = false;
	/* Switch LAN->WAN only when extap is enabled and WDS is NOT enabled */
	let extap_enabled = (s.extap && s.extap == '1');
	let wds_enabled = (s.wds && s.wds == '1');
	if (extap_enabled && !wds_enabled) {
		if (!s.network || lc(s.network) == 'lan') {
			print(`set wireless.${secname}.network='wan'\n`);
			changed = true;
		}
	}
	return changed;
}

/* ADDED: run translator only if proprietary QCA type present */
function has_qca_cfg80211() {
	for (let secname, s in config) {
		if (s[".type"] != "wifi-device")
			continue;
		if (lc(s.type ?? "") == "qcawificfg80211")
			return true;
	}
	return false;
}

/* ADDED: proprietary → ath-ud translator (UCI batch emitter) */
function translate_proprietary_to_ath_ud() {
	let changed = false;

	/* Build a map of radio configs from board.json */
	let radio_map = {};
	if (board && board.wlan) {
		for (let phy_name, phy in board.wlan) {
			/* Currently supports single phy */
			if (!phy.multi_radio)
				continue;

			let multi_radio = phy.multi_radio;
			let hw_idx;
			for (let radio_name in multi_radio) {
				let radio_idx = multi_radio[radio_name];
				if (radio_idx && radio_idx.idx != null) {
					/* Derive band from frequency (same as generate_config) */
					let freq = radio_idx.first_freq;
					let band_name = get_band(freq);

					/* Map radio names to expected band numbers and radio indices */
					let band_num, mapped_radio_idx;
					if (radio_name == "radio0") {
						/* radio0_band0: 2G, radio=3 */
						band_num = "0";
						mapped_radio_idx = radio_idx.idx ;
					} else if (radio_name == "radio1") {
						/* radio0_band1: 5G, radio=0 */
						band_num = "1";
						mapped_radio_idx = radio_idx.idx ;
					} else if (radio_name == "radio2") {
						/* radio0_band2: 5G, radio=1 */
						band_num = "2";
						mapped_radio_idx = radio_idx.idx ;
					} else if (radio_name == "radio3") {
						/* radio0_band3: 6G, radio=2 */
						band_num = "3";
						mapped_radio_idx = radio_idx.idx ;
					} else {
						/* Skip unknown radios */
						continue;
					}

					/* Calculate channels from enabled frequencies in this radio's range */
					let band = phy.info.bands[band_name];
					let channels = get_channel_list_for_range(band,
						radio_idx.first_freq, radio_idx.last_freq);

					radio_map[band_num] = {
						path: phy.path,
						band: lc(band_name),
						radio: mapped_radio_idx,
						channels: channels
					};
				}
			}
		}
	}

	for (let secname, s in config) {
		if (s[".type"] == "wifi-device") {
			if (lc(s.type ?? "") != "mac80211" || s.hwmode || !s.band || !s.radio || !s.channels) {
				/* Extract band number from section name (e.g., radio0_band2 -> 2) */
				let m = match(secname ?? "", /band([0-9]+)/, "s");
				let band_num = m ? m[1] : null;

				/* Look up radio config from board.json */
				let radio_cfg = band_num ? radio_map[band_num] : null;

				if (!radio_cfg) {
					/* Skip if no board.json data available */
					continue;
				}

				print(`set wireless.${secname}.type='mac80211'\n`);
				print(`set wireless.${secname}.path='${radio_cfg.path}'\n`);
				print(`set wireless.${secname}.band='${radio_cfg.band}'\n`);
				print(`set wireless.${secname}.radio='${radio_cfg.radio}'\n`);
				print(`set wireless.${secname}.channels='${radio_cfg.channels}'\n`);

				/* Preserve channel if set (including 'auto' and 0) */
				if (s.channel != null && s.channel != '') {
					print(`set wireless.${secname}.channel='${s.channel}'\n`);
				}

				/* Preserve disabled state */
				if (s.disabled != null) {
					print(`set wireless.${secname}.disabled='${s.disabled}'\n`);
				}

				/* Convert chainmasks -> antenna masks during sysupgrade migration */
				if (s.txchainmask != null && s.txchainmask != '') {
					print(`set wireless.${secname}.txantenna='${int(s.txchainmask, 0)}'\n`);
					print(`delete wireless.${secname}.txchainmask\n`);
				}
				if (s.rxchainmask != null && s.rxchainmask != '') {
					print(`set wireless.${secname}.rxantenna='${int(s.rxchainmask, 0)}'\n`);
					print(`delete wireless.${secname}.rxchainmask\n`);
				}

				if (s.hwmode) {
					let ht = map_hwmode_to_htmode(s.hwmode, s.htmode);
					if (ht) print(`set wireless.${secname}.htmode='${ht}'\n`);
					print(`delete wireless.${secname}.hwmode\n`);
					/* Check if any iface on this device has disablecoext */
					let has_disablecoext = false;
					for (let iface_name, iface in config) {
						if (iface[".type"] == "wifi-iface" && iface.device == secname && iface.disablecoext == "1") {
							has_disablecoext = true;
							break;
						}
					}
					if (has_disablecoext && radio_cfg.band == '2g') {
						print(`set wireless.${secname}.noscan='1'\n`);
					}
				}

                		/* remove device-level MAC address */
				if (s.macaddr)
					print(`delete wireless.${secname}.macaddr\n`);

				changed = true;
			}
			continue;
		}

		if (s[".type"] == "wifi-iface") {
			let key = s.key;
			if (!key && s.sae_password) {
				if (type(s.sae_password) == "array")
					key = s.sae_password[0];
				else
					key = s.sae_password;
			}

			/* Normalize station (STA) interface defaults */
			if (is_sta_iface(s))
				changed = normalize_sta_iface(secname, s) || changed;

			let mode = lc(s.mode ?? "");
			switch (mode) {
			case 'lite_monitor':
			case 'ap_smart_monitor':
				print(`set wireless.${secname}.mode='monitor'\n`);
				changed = true;
				break;
			case 'ap_monitor':
				print(`set wireless.${secname}.mode='ap'\n`);
				print(`set wireless.${secname}.vap_submode='scan'\n`);
				changed = true;
				break;
			case 'wrap':
			case 'ap_lp_iot':
				print(`set wireless.${secname}.mode='ap'\n`);
				changed = true;
				break;
			}

			if (s.cactimeout != null && s.cactimeout != '' && s.device) {
				print(`set wireless.${s.device}.cac_timeout='${s.cactimeout}'\n`);
				changed = true;
			}

			/* Handle encryption conversion */
			if (s.encryption) {
				let new_enc = map_encryption(s.encryption, s.sae);
				if (new_enc && new_enc != s.encryption) {
					print(`set wireless.${secname}.encryption='${new_enc}'\n`);
					changed = true;
				}
				if (new_enc && match(lc(new_enc), /^sae/, "s") && s.sae_pwe != "1") {
					print(`set wireless.${secname}.sae_pwe='1'\n`);
					changed = true;
				}
			}

			if (s.sae && s.sae == '1') {
				if (key) print(`set wireless.${secname}.key='${key}'\n`);

				if (s.sae_groups && length(s.sae_groups)) {
					print(`delete wireless.${secname}.sae_groups\n`);
					for (let g in s.sae_groups)
						print(`add_list wireless.${secname}.sae_groups='${g}'\n`);
				}
				if (s.sae_password && length(s.sae_password))
					print(`delete wireless.${secname}.sae_password\n`);

				changed = true;
			}
		}
	}

	for (let secname, s in config) {
		if (s[".type"] != "wifi-mld")
			continue;

		if (s.mld_ssid) {
			print(`set wireless.${secname}.ssid='${s.mld_ssid}'\n`);
			print(`delete wireless.${secname}.mld_ssid\n`);
			changed = true;
		}
	}

	if (changed)
		commit = true;
}

/* ADDED: rename wifi0/1/2/3 → radio0_band0/1/2/3 and rebind iface.device */
function rename_devices_and_rebind_ifaces() {
	let map = {
		wifi0: "radio0_band0",
		wifi1: "radio0_band2",
		wifi2: "radio0_band1",
		wifi3: "radio0_band3"
	};

	let renamed = false;
	let renames = [];

	/* Collect all renames first to avoid modifying during iteration
	 * Board override: ipq5332 → wifi1=5G (band1), wifi2=6G (band2)
	 */
	let bn = "";

	bn = trim(readfile("/tmp/sysinfo/board_name")) ?? "";
	if (match(lc(bn), /ipq5332/, "s")) {
		map["wifi1"] = "radio0_band1";
		map["wifi2"] = "radio0_band2";
	}
	for (let secname, s in config) {
		if (s[".type"] != "wifi-device")
			continue;
		let newname = map[secname];
		if (!newname)
			continue;
		renames[length(renames)] = { old: secname, new: newname, cfg: s };
	}

	/* Apply renames (print + in-memory update) */
	for (let r in renames) {
		print(`rename wireless.${r.old}='${r.new}'\n`);
		config[r.new] = r.cfg;
		delete config[r.old];
		renamed = true;
		commit = true;
	}

	/* Rebind iface.device after all device renames */
	if (renamed) {
		for (let secname, s in config) {
			if (s[".type"] != "wifi-iface")
				continue;
			let olddev = s.device;
			if (!olddev)
				continue;
			let newdev = map[olddev];
			if (!newdev)
				continue;
			print(`set wireless.${secname}.device='${newdev}'\n`);
			/* Keep in-memory config consistent */
			s.device = newdev;
			commit = true;
		}
	}
}

function generate_config(info, name, single_wiphy, id, radio_idx, is_scan) {
	let s = "wireless." + name;
	let si = "wireless.default_" + name;

	let band_name;
	if (!single_wiphy) {
		band_name = filter(bands_order, (b) => info.bands[b])[0];
	} else {
		let freq = radio_idx.first_freq;
		band_name = get_band(freq);
	}

	if (!band_name)
		return;

	let band = info.bands[band_name];
	let channel = band.default_channel;

	if (single_wiphy) {
		let start_freq = radio_idx.first_freq;
		let end_freq = radio_idx.last_freq;
		channels = get_channel_list_for_range(band, start_freq, end_freq);
		if (band_name == "6G") {
			let start_freq = radio_idx.first_freq;
			if (freq_to_channel(start_freq) >= 129)
				channel = 197;
			else
				channel = 49;
		}
		if (band_name == "5G") {
			let start_freq = radio_idx.first_freq;
			if (freq_to_channel(start_freq) == 36)
				channel = 36;
			else
				channel = 149;
		}
	} else {
		if (band_name == "5G") {
		       if (band.default_channel == "100")
				channel = 149;
			else
				channel = 36;
		}
		if (band_name == "6G") {
			if (band.default_channel == "129")
				channel = 197;
			else
				channel = 49;
		}
		/* Populate channels from the enabled-frequency range stored by wifi-detect.uc */
		if (band.first_channel && band.last_channel)
			channels = band.first_channel + "-" + band.last_channel;
	}

	if (band_name == "2G")
		channel = 6;

	let width = band.max_width;
	if (band_name == "2G")
		width = 20;
	else if (width > 80)
		width = 80;

	let htmode = filter(htmode_order, (m) => band[lc(m)])[0];
	if (htmode)
		htmode += width;
	else
		htmode = "NOHT";

	print(`set ${s}=wifi-device
set ${s}.type='mac80211'
set ${s}.${id}
set ${s}.band='${lc(band_name)}'
set ${s}.channel='${channel}'
`);

if (radio_idx != null) {
	print(`set ${s}.radio='${radio_idx.idx}'
`);
}

if (channels)
	print(`set ${s}.channels='${channels}'`);

print(`
set ${s}.htmode='${htmode}'
set ${s}.disabled='1'

set ${si}=wifi-iface
set ${si}.device='${name}'
set ${si}.network='lan'
set ${si}.mode='ap'
set ${si}.ssid='OpenWrt'
set ${si}.encryption='none'

`);

	/* Add vap_submode for scan radios */
        if (is_scan) {
		print(`set ${si}.vap_submode='scan'\n`);
	}

	if (band_name == "6G") {
		print(`set ${si}.encryption='sae'
		set ${si}.sae_pwe='1'
		set ${si}.key='0123456789'
`);
	}
}


/* Add missing radio option for existing multi-radio devices (sysupgrade case) */
function add_missing_radio_for_existing(phy, idx) {
	if (!phy.multi_radio)
		return;

	let multi_radio = phy.multi_radio;
	let hw_idx = 0;

	for (let radio_name in multi_radio) {
		let radio_idx = multi_radio[radio_name];
		if (!radio_idx || radio_idx.idx == null)
			continue;

		/* Device name matches generate_config(): radio{idx}_band{hw_idx} */
		let devname = "radio" + idx + "_band" + hw_idx;
		let dev = config[devname];

		if (!dev || dev[".type"] != "wifi-device") {
			hw_idx++;
			continue;
		}

		if (lc(dev.type ?? "") != "mac80211") {
			hw_idx++;
			continue;
		}

		/* Only add if radio option is missing */
		if (dev.radio != null) {
			hw_idx++;
			continue;
		}

		print(`set wireless.${devname}.radio='${radio_idx.idx}'\n`);
		dev.radio = radio_idx.idx;
		commit = true;
		sysupgrade = true;
		hw_idx++;
	}
}

if (has_qca_cfg80211()) {
	/* Rename first so band extraction works in translation */
	rename_devices_and_rebind_ifaces();
	translate_proprietary_to_ath_ud();

	/* Create marker file to trigger wifi startup after translation */
	let ret = system("touch /tmp/.wifi_needs_restart");
	if (ret) {
		warn("Failed to create WiFi restart marker file\n");
	}

	/* Exit after translation to prevent generate_config from overwriting */
	if (commit)
		print("commit wireless\n");
	exit(0);
}

for (let phy_name, phy in board.wlan) {
	let info = phy.info;
	let name;
	let single_wiphy = false;
	let is_scan_phy = is_scan_radio(phy_name);
	if (!info || !length(info.bands))
		continue;

	if (!phy.path)
		continue;

	let macaddr = trim(readfile(`/sys/class/ieee80211/${phy_name}/macaddress`));
	if (radio_exists(phy.path, macaddr, phy_name)) {
		add_missing_radio_for_existing(phy, idx);
		idx++;
		continue;
	}

	id = `phy='${phy_name}'`;
	if (match(phy_name, /^phy[0-9]/) || is_scan_phy)
		id = `path='${phy.path}'`;

	if (phy.multi_radio)
		single_wiphy = true;

	if (single_wiphy) {
		let multi_radio = phy.multi_radio;
		let hw_idx;
		for (let radio_name in multi_radio ) {
			let radio_idx = multi_radio[radio_name];
			name = "radio" + idx + "_band" + hw_idx++;
			generate_config(info, name, single_wiphy, id, radio_idx, false);
		}
	} else {
		name = "radio" + idx;
		generate_config(info, name, single_wiphy, id, NULL, is_scan_phy);
	}
	idx++;
	commit = true;
}

if (sysupgrade) {
	/* Create marker file to trigger wifi startup after translation */
	let ret = system("touch /tmp/.wifi_needs_restart");
	if (ret) {
		warn("Failed to create WiFi restart marker file\n");
	}
}


if (commit)
	print("commit wireless\n");
