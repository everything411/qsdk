// Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
// SPDX-License-Identifier: ISC

let libubus = require("ubus");
import { open, readfile } from "fs";

let ubus = libubus.connect();

let main_obj = {
	forward_to_hostapd: {
		args: {
			phy: "",
			radio: 0,
			up: true,
			frequency: 0,
			chan_width: 0,
			sec_chan_offset: 0,
			center_freq1: 0,
			center_freq2: 0,
			csa: true,
			csa_count: 0,
			punct_bitmap: 0,
			mon_ifaces: "",
			is_dfs: false,
			wpa_state: "",
               },
		call: function(req) {
			let iface_msg = {
				phy: req.args.phy,
				radio: req.args.radio,
				up: req.args.up,
				frequency: req.args.frequency,
				chan_width: req.args.chan_width,
				sec_chan_offset: req.args.sec_chan_offset,
				center_freq1: req.args.center_freq1,
				center_freq2: req.args.center_freq2,
				csa: req.args.csa,
				csa_count: req.args.csa_count,
				punct_bitmap: req.args.punct_bitmap,
				mon_ifaces: "",
				is_dfs: req.args.is_dfs,
				wpa_state: req.args.wpa_state,
			};
			rptr_mgr.printf(`rptr_mgr.uc: forward_to_hostapd`);
			ubus.call("hostapd", "apsta_state", iface_msg);
		}
	},
	csa_finish_event_to_supp: {
		args: {
			freq: 0,
		},
		call: function(req) {
			rptr_mgr.printf(`rptr_mgr.uc: csa_finish_event_to_supp`);
			ubus.call("wpa_supplicant", "csa_finish_event", { freq: req.args.freq } );
		}
	},
	acs_completed: {
		args: {
			event: "",
			success: 0,
			channel: 0,
			freq: 0,
		},
		call: function(req) {
			rptr_mgr.printf(`acs_completed event received in rptr_mgr.uc success=${req.args.success} channel=${req.args.channel} freq=${req.args.freq}`);
			ubus.call("wpa_supplicant", "start_scan_post_acs", { } );
		}
	},
};

rptr_mgr.data.ubus = ubus;
rptr_mgr.data.obj = ubus.publish("rptr_mgr", main_obj);
