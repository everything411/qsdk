#Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#SPDX-License-Identifier: BSD-3-Clause
#
#Copyright (c) 2013-2018, Jouni Malinen <j@w1.fi>
#
#This software may be distributed under the terms of the BSD license.
#See README for more details.

import logging
logger = logging.getLogger()
import time

import hostapd
from utils import *

from test_ap_acs import force_prev_ap_on_24g
from test_ap_acs import force_prev_ap_on_5g
from test_ap_acs import force_prev_ap_on_6g
from test_ap_acs import wait_acs

def get_phy(dev, ifname):
    ret, phy = dev.cmd_execute(["cat", "/sys/class/net/" + ifname + "/phy80211/name"], shell=False)
    if ret == 0 and phy.strip():
        return phy.strip()
    return "phy0"

def set_hwsim_environment(dev, phy, noise=-95, busy_pct=0):
    logger.info("Setting survey noise to %s and busy_pct to %s on %s" % (str(noise), str(busy_pct), phy))
    dev.cmd_execute(["echo", str(noise), ">", "/sys/kernel/debug/ieee80211/" + phy + "/hwsim/survey_noise"], shell=True)
    dev.cmd_execute(["echo", str(busy_pct), ">", "/sys/kernel/debug/ieee80211/" + phy + "/hwsim/survey_time_busy_pct"], shell=True)

def inject_fake_bss(dev, phy, freq=2412, mac="02:00:00:00:00:01", ssid="Fake_AP", bw=20, ht=0, vht=0, he=0, eht=0):
    logger.info("Injecting fake BSS %s on %s at %s (BW=%d HT=%d VHT=%d HE=%d EHT=%d)" % (ssid, phy, str(freq), bw, ht, vht, he, eht))
    cmd_str = "%s %s %s %d %d %d %d %d" % (str(freq), mac, ssid, bw, ht, vht, he, eht)
    dev.cmd_execute(["echo", "'" + cmd_str + "'", ">", "/sys/kernel/debug/ieee80211/" + phy + "/hwsim/inject_fake_bss"], shell=True)

def clear_fake_bss(dev, phy):
    logger.info("Clearing fake BSS on %s" % phy)
    dev.cmd_execute(["echo", "clear", ">", "/sys/kernel/debug/ieee80211/" + phy + "/hwsim/inject_fake_bss"], shell=True)

def test_ap_qacs(dev, apdev):
    """Automatic channel selection"""
    force_prev_ap_on_24g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['channel'] = '0'
    params['qacs_enable'] = '1'
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
    wait_acs(hapd)

    freq = hapd.get_status_field("freq")
    if int(freq) < 2400:
        raise Exception("Unexpected frequency")
    logger.info("Requesting last ACS report for 2G band")
    out = hapd.request("ACS show_report")
    logger.info("ACS_REPORT output:\n" + out)

    dev[0].connect("test-acs", psk="12345678", scan_freq=freq)

def test_ap_qacs_5ghz(dev, apdev):
    """Automatic channel selection on 5 GHz"""
    try:
        hapd = None
#        force_prev_ap_on_5g(apdev[0])
        params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
        params['hw_mode'] = 'a'
        params['channel'] = '0'
        params['qacs_enable'] = '1'
        params['country_code'] = 'US'
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
        wait_acs(hapd)
        freq = hapd.get_status_field("freq")
        if int(freq) < 5000:
            raise Exception("Unexpected frequency")
        logger.info("Requesting last ACS report for 5G band")
        out = hapd.request("ACS show_report")
        logger.info("ACS_REPORT output:\n" + out)

        dev[0].connect("test-acs", psk="12345678", scan_freq=freq)
        dev[0].wait_regdom(country_ie=True)
    finally:
        clear_regdom(hapd, dev)

def test_ap_qacs_eht20(dev, apdev):
    """Automatic channel selection for EHT320 (offset 0)"""
    run_ap_qacs_eht20(dev, apdev, 0)

def run_ap_qacs_eht20(dev, apdev, bw32_offset):
    check_sae_capab(dev[0])
    try:
        hapd = None
        force_prev_ap_on_6g(apdev[0])
        params = hostapd.he_wpa2_params(ssid="test-acs", passphrase="12345678")
        params['hw_mode'] = 'a'
        params["ieee80211ax"] = "1"
        params["ieee80211be"] = "1"
        params['channel'] = '0'
        params['qacs_enable'] = '1'
        params['op_class'] = '131'
        params['ieee80211w'] = '2'
        params['country_code'] = 'CA'
        params['acs_num_scans'] = '1'
        params['ieee80211w'] = '2'
        params['wpa_key_mgmt'] = 'SAE-EXT-KEY'
        hapd = hostapd.add_ap(apdev[0], params)
        freq = hapd.get_status_field("freq")
        if int(freq) < 5900:
            raise Exception("Unexpected frequency")
        logger.info("Requesting last ACS report for 6G band")
        out = hapd.request("ACS show_report")
        logger.info("ACS_REPORT output:\n" + out)
        dev[0].set("sae_groups", "")
        dev[0].connect("test-acs", psk="12345678", key_mgmt="SAE-EXT-KEY",
                       ieee80211w="2", scan_freq=freq)
        dev[0].wait_regdom(country_ie=True)
    finally:
        clear_regdom(hapd, dev)


def test_ap_qacs_eht320(dev, apdev):
    """Automatic channel selection for EHT320 (offset 0)"""
    run_ap_qacs_eht320(dev, apdev, 0)


def run_ap_qacs_eht320(dev, apdev, bw32_offset):
    check_sae_capab(dev[0])
    try:
        hapd = None
        force_prev_ap_on_6g(apdev[0])
        params = hostapd.he_wpa2_params(ssid="test-acs", passphrase="12345678")
        params['hw_mode'] = 'a'
        params["ieee80211ax"] = "1"
        params["ieee80211be"] = "1"
        params['channel'] = '0'
        params['qacs_enable'] = '1'
        params['op_class'] = '137'
        params['eht_bw320_offset'] = str(bw32_offset)
        params['ieee80211w'] = '2'
        params['country_code'] = 'CA'
        params['acs_num_scans'] = '1'
        params['ieee80211w'] = '2'
        params['wpa_key_mgmt'] = 'SAE-EXT-KEY'
        hapd = hostapd.add_ap(apdev[0], params)
        freq = hapd.get_status_field("freq")
        if int(freq) < 5900:
            raise Exception("Unexpected frequency")
        logger.info("Requesting last ACS report for 6G band")
        out = hapd.request("ACS show_report")
        logger.info("ACS_REPORT output:\n" + out)
        dev[0].set("sae_groups", "")
        dev[0].connect("test-acs", psk="12345678", key_mgmt="SAE-EXT-KEY",
                       ieee80211w="2", scan_freq=freq)
        hapd.wait_sta()
        dev[0].wait_regdom(country_ie=True)
        sig = dev[0].request("SIGNAL_POLL").splitlines()
        logger.info("SIGNAL_POLL: " + str(sig))
        if "WIDTH=320 MHz" not in sig:
            raise Exception("Station did not report 320 MHz bandwidth")
        seg0 = int(hapd.get_status_field("eht_oper_centr_freq_seg0_idx"))
        offset = int(hapd.get_status_field("eht_bw320_offset"))
        chan_1 = [31, 95, 159]
        chan_2 = [63, 127, 191]
        if bw32_offset == 0:
            if offset != 1 and offset != 2:
                raise Exception("Unexpected eht_bw320_offset: %d" % offset)
            if seg0 not in chan_1 and seg0 not in chan_2:
                raise Exception("Unexpected seg0: %d" % seg0)
        if bw32_offset == 1:
            if offset != 1:
                raise Exception("Unexpected eht_bw320_offset: %d" % offset)
            if seg0 not in chan_1:
                raise Exception("Unexpected seg0: %d" % seg0)
        if bw32_offset == 2:
            if offset != 2:
                raise Exception("Unexpected eht_bw320_offset: %d" % offset)
            if seg0 not in chan_2:
                raise Exception("Unexpected seg0: %d" % seg0)

        dev[0].request("DISCONNECT")
        dev[0].wait_disconnected()
        hapd.wait_sta_disconnect()
    finally:
        clear_regdom(hapd, dev)

def test_ap_qacs_40mhz(dev, apdev):
    """Automatic channel selection for 40 MHz channel"""
    run_ap_qacs_40mhz(dev, apdev, '[HT40+]')

def test_ap_qacs_40mhz_he(dev, apdev):
    """Automatic channel selection for 40 MHz channel (HE)"""
    run_ap_qacs_40mhz(dev, apdev, '[HT40+]', he=True, allow20=True)

def test_ap_qacs_40mhz_plus_or_minus(dev, apdev):
    """Automatic channel selection for 40 MHz channel (plus or minus)"""
    run_ap_qacs_40mhz(dev, apdev, '[HT40+][HT40-]')

def run_ap_qacs_40mhz(dev, apdev, ht_capab, he=False, allow20=False):
    clear_scan_cache(apdev[0])
    force_prev_ap_on_24g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['channel'] = '0'
    params['qacs_enable'] = '1'
    params['ht_capab'] = ht_capab
    if he:
        params['ieee80211ax'] = '1'
        params['he_oper_chwidth'] = '0'
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
    wait_acs(hapd)

    freq = hapd.get_status_field("freq")
    if int(freq) < 2400:
        raise Exception("Unexpected frequency")

    out = hapd.request("ACS show_report")
    logger.info("ACS_REPORT output:\n" + out)

    sec = hapd.get_status_field("secondary_channel")
    if int(sec) == 0:
        if allow20:
            logger.info("Fallback to 20 MHz detected")
        else:
            raise Exception("Secondary channel not set")

    dev[0].connect("test-acs", psk="12345678", scan_freq=freq)

def test_ap_qacs_40mhz_minus(dev, apdev):
    """Automatic channel selection for HT40- channel"""
    clear_scan_cache(apdev[0])
    force_prev_ap_on_24g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-acs", passphrase="12345678")
    params['channel'] = '0'
    params['qacs_enable'] = '1'
    params['ht_capab'] = '[HT40-]'
    params['acs_num_scans'] = '1'
    hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
    wait_acs(hapd)

    freq = hapd.get_status_field("freq")
    if int(freq) < 2400:
        raise Exception("Unexpected frequency")

    out = hapd.request("ACS show_report")
    logger.info("ACS_REPORT output:\n" + out)

    sec = hapd.get_status_field("secondary_channel")
    if int(sec) != -1:
        raise Exception("Unexpected secondary_channel: " + sec)

    dev[0].connect("test-acs", psk="12345678", scan_freq=freq)
    sig = dev[0].request("SIGNAL_POLL").splitlines()
    logger.info("SIGNAL_POLL: " + str(sig))
    if "WIDTH=40 MHz" not in sig:
        raise Exception("Station did not report 40 MHz bandwidth")


def test_ap_qacs_dynamic(dev, apdev):
    """Test dynamic survey parameters and fake BSS injection with ACS"""

    logger.info("Setting up dynamic survey environment and injecting fake BSS...")

    phy = get_phy(dev[0], apdev[0]['ifname'])

    # Inject fake BSS and configure survey noise before starting AP
    inject_fake_bss(dev[0], phy, 2412, "02:00:00:00:00:01", "Fake_AP_1")
    inject_fake_bss(dev[0], phy, 2412, "02:00:00:00:00:02", "Fake_AP_2")
    inject_fake_bss(dev[0], phy, 2437, "02:00:00:00:00:03", "Fake_AP_3")
    set_hwsim_environment(dev[0], phy, noise=-95, busy_pct=0)

    try:
        params = hostapd.wpa2_params(ssid="test-dynamic-acs", passphrase="12345678")
        params['hw_mode'] = 'g'
        params['channel'] = '0'
        params['qacs_enable'] = '1'
        params['acs_num_scans'] = '1'

        logger.info("Starting hostapd with ACS enabled...")
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
        wait_acs(hapd)

        channel = hapd.get_status_field("channel")
        freq = hapd.get_status_field("freq")
        logger.info("==================================================")
        logger.info("ACS selected channel: " + str(channel) + " (Freq: " + str(freq) + " MHz)")
        logger.info("==================================================")

        if int(freq) < 2400:
            raise Exception("Unexpected frequency")

        out = hapd.request("ACS show_report")
        logger.info("ACS_REPORT output:\n" + out)
        out = hapd.request("ACS show_neighbor_report")
        logger.info("ACS_NEIGHBOR_REPORT output:\n" + out)

    finally:
        # Cleanup
        clear_fake_bss(dev[0], phy)
        set_hwsim_environment(dev[0], phy, -92, 0)

def test_ap_qacs_dynamic_5ghz(dev, apdev):
    """Test dynamic survey parameters and fake BSS injection with ACS on 5 GHz"""

    logger.info("Setting up dynamic 5 GHz survey environment and injecting fake BSS...")

    phy = get_phy(dev[0], apdev[0]['ifname'])

    inject_fake_bss(dev[0], phy, 5180, "02:00:00:00:10:01", "Fake_AP_5G_1")
    inject_fake_bss(dev[0], phy, 5180, "02:00:00:00:10:02", "Fake_AP_5G_2")
    inject_fake_bss(dev[0], phy, 5220, "02:00:00:00:10:03", "Fake_AP_5G_3")
    set_hwsim_environment(dev[0], phy, noise=-95, busy_pct=0)

    try:
        params = hostapd.wpa2_params(ssid="test-dynamic-acs-5g", passphrase="12345678")
        params['hw_mode'] = 'a'
        params['channel'] = '0'
        params['qacs_enable'] = '1'
        params['acs_num_scans'] = '1'
        params['country_code'] = 'US'

        logger.info("Starting hostapd with ACS enabled on 5 GHz...")
        hapd = hostapd.add_ap(apdev[0], params, wait_enabled=False)
        wait_acs(hapd)

        channel = hapd.get_status_field("channel")
        freq = hapd.get_status_field("freq")
        logger.info("==================================================")
        logger.info("ACS selected channel: " + str(channel) + " (Freq: " + str(freq) + " MHz)")
        logger.info("==================================================")

        if int(freq) < 5000:
            raise Exception("Unexpected frequency")

        out = hapd.request("ACS show_report")
        logger.info("ACS_REPORT output:\n" + out)
        out = hapd.request("ACS show_neighbor_report")
        logger.info("ACS_NEIGHBOR_REPORT output:\n" + out)

    finally:
        clear_fake_bss(dev[0], phy)
        set_hwsim_environment(dev[0], phy, -95, 0)
        clear_regdom(hapd, dev)


def test_ap_primary_chanlist(dev, apdev):
    """Primary channel list: set, get, and clear via ctrl_iface"""
    force_prev_ap_on_24g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-primary-chanlist", passphrase="12345678")
    params['channel'] = '1'
    params['qacs_enable'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    # Set a primary channel list restricted to channels 6 and 11
    res = hapd.request("SET_PRIMARY_CHANS 6 11")
    if "FAIL" in res:
        raise Exception("SET_PRIMARY_CHANS 6 11 failed: " + res)

    # Wait for channel switch triggered by the new primary channel list
    ev = hapd.wait_event(["AP-CSA-FINISHED"], timeout=10)
    if ev is None:
        raise Exception("AP-CSA-FINISHED event not received after SET_PRIMARY_CHANS")
    logger.info("AP-CSA-FINISHED event: " + ev)
    if "freq=2437" not in ev and "freq=2462" not in ev:
        raise Exception("AP did not switch to channel 6 (2437 MHz) or 11 (2462 MHz), got: " + ev)

    # Verify the list is reflected back
    out = hapd.request("GET_PRIMARY_CHANS")
    if "FAIL" in out:
        raise Exception("GET_PRIMARY_CHANS failed: " + out)
    logger.info("GET_PRIMARY_CHANS output: " + out.strip())
    chans = out.strip().split()
    if "6" not in chans or "11" not in chans:
        raise Exception("Expected channels 6 and 11 in primary list, got: " + out.strip())

    # Clear the primary channel list (trailing space + empty string passes "" to handler)
    res = hapd.request("SET_PRIMARY_CHANS ")
    if "FAIL" in res:
        raise Exception("SET_PRIMARY_CHANS (clear) failed: " + res)

    out = hapd.request("GET_PRIMARY_CHANS")
    if "DISABLED" not in out:
        raise Exception("Expected DISABLED after clearing primary chan list, got: " + out.strip())

def test_ap_block_chanlist(dev, apdev):
    """ACS block channel list: set, get, and clear via ctrl_iface"""
    force_prev_ap_on_24g(apdev[0])
    params = hostapd.wpa2_params(ssid="test-block-chanlist", passphrase="12345678")
    params['channel'] = '1'
    params['qacs_enable'] = '1'
    hapd = hostapd.add_ap(apdev[0], params)

    # Block channels 1 and 6
    res = hapd.request("ACS set_block_chan_list 1 6")
    if "FAIL" in res:
        raise Exception("ACS set_block_chan_list failed: " + res)

    # Wait for channel switch away from blocked channels
    ev = hapd.wait_event(["AP-CSA-FINISHED"], timeout=10)
    if ev is None:
        raise Exception("AP-CSA-FINISHED event not received after ACS set_block_chan_list")
    logger.info("AP-CSA-FINISHED event: " + ev)
    if "freq=2412" in ev or "freq=2437" in ev:
        raise Exception("AP switched to blocked channel 1 (2412 MHz) or 6 (2437 MHz), got: " + ev)

    out = hapd.request("ACS get_block_chan_list")
    if "FAIL" in out:
        raise Exception("ACS get_block_chan_list failed: " + out)
    logger.info("ACS get_block_chan_list output: " + out.strip())
    if "1" not in out or "6" not in out:
        raise Exception("Expected channels 1 and 6 in block list, got: " + out.strip())

    # Clear the block list
    res = hapd.request("ACS clear_block_chan_list")
    if "FAIL" in res:
        raise Exception("ACS clear_block_chan_list failed: " + res)

    out = hapd.request("ACS get_block_chan_list")
    if "empty" not in out.lower():
        raise Exception("Expected empty block list after clear, got: " + out.strip())
