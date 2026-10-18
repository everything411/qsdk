/*
*
* Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: ISC
*
*/
#ifndef _HELPER_H
#define _HELPER_H

#include <stdio.h>
#include <string.h>
#include <stdbool.h>
// #include "json_hal_common.h"

#define CHAR char
#define RETURN_ERR -1
#define RETURN_OK 0

extern char retValue[256];
extern char snbuf[256];

int execute(char *command, char *result);
uint64_t execute_int(const char* cmd);
char* gpon_get_equipment_id();
uint64_t gpon_get_unicast_packets_sent();
uint64_t gpon_get_unicast_packets_received();
uint64_t gpon_get_discard_packets_sent();
uint64_t gpon_get_discard_packets_received();
uint64_t gpon_get_multicast_packets_sent();
uint64_t gpon_get_multicast_packets_received();
uint64_t gpon_get_broadcast_packets_sent();
uint64_t gpon_get_broadcast_packets_received();
uint64_t gpon_get_eth_packets_sent();
uint64_t gpon_get_eth_packets_received();
uint64_t gpon_get_eth_errors_sent();
uint64_t gpon_get_eth_errors_received();
char* gpon_get_rxp_signal_level();
char* gpon_get_txp_signal_level();
char* gpon_get_current_voltage();
char* gpon_get_current_bias();
char* gpon_get_current_temperature();
char* gpon_get_nominal_bitrate();
char* gpon_get_pm_modulefirmwareversion();
char* gpon_get_PM_status();
char* gpon_get_PM_ponmode();
char* gpon_get_pm_operationalstate();
char* gpon_get_ploam_vendorid();
char* gpon_get_ploam_serialnumber();
uint64_t gpon_get_onu_ani_stats(const char* keyword);
uint64_t gpon_get_ani_stats_packets_sent();
uint64_t gpon_get_ani_stats_packets_received();
uint64_t gpon_get_ani_stats_discard_packets_sent();
uint64_t gpon_get_ani_stats_discard_packets_received();
uint64_t gpon_get_onu_ani_tc_pm(const char* keyword);
uint64_t gpon_get_ploam_micerrors();
uint64_t gpon_get_downstream_message_count();
uint64_t gpon_get_ranging_time();
uint64_t gpon_get_upstream_message_count();
uint64_t gpon_get_omci_baselinemessagecount();
uint64_t gpon_get_omci_extendedmessagecount();
uint64_t gpon_get_omci_micerrors();
uint64_t gpon_get_ploam_onuid();
int gpon_get_pm_alarm_los();
int gpon_get_pm_alarm_lof();
uint64_t gpon_get_pm_alarm_lcdg();
int gpon_get_pm_alarm_tf();
int gpon_get_gem_transmittedframes( int index);
int gpon_get_gem_receivedframes(int index);
char* gpon_get_ploam_txmessagecount();
char* gpon_get_ploam_rxmessagecount();
char* gpon_get_gem_directions(int index);
char* gpon_get_gem_traffctype(int index);
int gpon_get_pm_alarm_sf();
int gpon_get_pm_alarm_sd();
uint64_t gpon_get_pm_alarm_rogue();
int gpon_get_gem_port_id(int index);
int gpon_get_pm_alarm_tf();
int gpon_get_pm_alarm_suf();
int gpon_get_pm_alarm_mem();
int gpon_get_pm_alarm_dact();
int gpon_get_pm_alarm_dis();
int gpon_get_pm_alarm_lods();
int gpon_get_tc_perf_signal_fail();
int gpon_get_tc_perf_signal_degrade();
int gpon_get_frame_header_hec_errors();
int gpon_get_xgem_key_errors();
uint64_t gpon_get_ani_errors_sent();
uint64_t gpon_get_ani_errors_received();
int gpon_get_software_number_of_entries();
int gpon_get_eth_uni_number_of_entries();
int gpon_get_port_number_of_entries();
uint64_t gpon_get_eth_bytes_sent();
uint64_t gpon_get_eth_bytes_received();
int gpon_get_sw_img_is_committed(int index);
int gpon_get_sw_img_is_active(int index);
int gpon_get_sw_img_is_valid(int index);
char* gpon_get_eth_uni_status();
char* gpon_get_eth_uni_name();
char* gpon_get_eth_uni_interdomain_id();
uint64_t gpon_get_onu_ani_last_change();
void gpon_set_xpon_onu_enable(int val);
int gpon_get_xpon_eth_uni_enable();
void gpon_set_xpon_onu_ani_enable(int val);
uint64_t gpon_get_ploam_txmesgcount();
uint64_t gpon_get_ploam_rxmesgcount();
int gpon_get_rxp_signallevel();
int gpon_get_txp_signallevel();
int gpon_get_voltage();
int gpon_get_bias();
int gpon_get_temperature();

#endif
