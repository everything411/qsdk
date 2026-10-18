# UHR tests
#Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
#SPDX-License-Identifier: BSD-3-Clause
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import binascii

import hostapd
from utils import *
from hwsim import HWSimRadio
import hwsim_utils
from wpasupplicant import WpaSupplicant

def uhr_mld_enable_ap(iface, link_id, params):
    hapd = hostapd.add_mld_link(iface, link_id, params)
    hapd.enable()

    ev = hapd.wait_event(["AP-ENABLED", "AP-DISABLED"], timeout=1)
    if ev is None:
        raise Exception("AP startup timed out")
    if "AP-ENABLED" not in ev:
        raise Exception("AP startup failed")

    return hapd

def uhr_verify_wifi_version(dev):
    status = dev.get_status()
    logger.info("station status: " + str(status))

    if 'wifi_generation' not in status:
        raise Exception("Missing wifi_generation information")
    if status['wifi_generation'] != "8":
        raise Exception("Unexpected wifi_generation value: " + status['wifi_generation'])

def _uhr_get_links_bitmap(wpas, name):
    vfile = "/sys/kernel/debug/ieee80211/%s/netdev:%s/%s" % \
        (wpas.get_driver_status_field("phyname"), wpas.ifname, name)

    if wpas.cmd_execute(["ls", vfile])[0] != 0:
        logger.info("%s not supported in mac80211: %s" % (name, vfile))
        return 0

    res, out = wpas.cmd_execute(["cat", vfile], shell=True)
    if res != 0:
        raise Exception("Failed to read %s" % vfile)

    logger.info("%s=%s" % (name, out))
    return int(out, 16)

def _uhr_valid_links(wpas):
    return _uhr_get_links_bitmap(wpas, "valid_links")

def _uhr_active_links(wpas):
    return _uhr_get_links_bitmap(wpas, "active_links")

def _uhr_verify_links(wpas, valid_links=0, active_links=0):
    vlinks = _uhr_valid_links(wpas)
    if vlinks != valid_links:
        raise Exception("Unexpected valid links (0x%04x != 0x%04x)" % (vlinks, valid_links))

    alinks = _uhr_active_links(wpas)
    if alinks != active_links:
        raise Exception("Unexpected active links (0x%04x != 0x%04x)" % (alinks, active_links))

def _5ghz_chanwidth_to_bw(op):
    return {
        0: "40",
        1: "80",
        2: "160",
        3: "80+80",
    }.get(op, "20")

def _6ghz_op_class_to_bw(op):
    return {
        131: "20",
        132: "40",
        133: "80",
        134: "160",
        137: "320",
    }.get(op, "20")

def uhr_320mhz_supported():
    import subprocess
    cmd = subprocess.Popen(["iw", "reg", "get"], stdout=subprocess.PIPE)
    out, err = cmd.communicate()
    return "@ 320)" in out.decode()

def uhr_verify_status(wpas, hapd, freq, bw, is_ht=False, is_vht=False,
                      mld=False, valid_links=0, active_links=0):
    status = hapd.get_status()

    logger.info("hostapd STATUS: " + str(status))
    if is_ht and status["ieee80211n"] != "1":
        raise Exception("Unexpected STATUS ieee80211n value")
    if is_vht and status["ieee80211ac"] != "1":
        raise Exception("Unexpected STATUS ieee80211ac value")
    if status["ieee80211ax"] != "1":
        raise Exception("Unexpected STATUS ieee80211ax value")
    if status["ieee80211be"] != "1":
        raise Exception("Unexpected STATUS ieee80211be value")
    if status["ieee80211bn"] != "1":
        raise Exception("Unexpected STATUS ieee80211bn value")

    sta = hapd.get_sta(wpas.own_addr())
    logger.info("hostapd STA: " + str(sta))
    if sta['addr'] == 'FAIL':
        raise Exception("hostapd " + hapd.ifname + " did not have a STA entry for the STA " + wpas.own_addr())
    if is_ht and "[HT]" not in sta['flags']:
        raise Exception("Missing STA flag: HT")
    if is_vht and "[VHT]" not in sta['flags']:
        raise Exception("Missing STA flag: VHT")
    if "[HE]" not in sta['flags']:
        raise Exception("Missing STA flag: HE")
    if "[EHT]" not in sta['flags']:
        raise Exception("Missing STA flag: EHT")
    if "[UHR]" not in sta['flags']:
        raise Exception("Missing STA flag: UHR")

    sig = wpas.request("SIGNAL_POLL").splitlines()
    # TODO: With MLD connection, signal poll logic is still not implemented.
    # While mac80211 maintains the station using the MLD address, the
    # information is maintained in the link stations, but it is not sent to
    # user space yet.
    if not mld:
        if "FREQUENCY=%s" % freq not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(1): " + str(sig))
        if "WIDTH=%s MHz" % bw not in sig:
            raise Exception("Unexpected SIGNAL_POLL value(2): " + str(sig))

    # Active links are updated in async work after the connection.
    # Sleep a bit to allow it to run.
    time.sleep(0.1)
    _uhr_verify_links(wpas, valid_links, active_links)

def test_uhr_open(dev, apdev):
    """UHR AP with open mode configuration"""
    with HWSimRadio(use_mlo=True) as (hapd_radio, hapd_iface), \
         HWSimRadio(use_mlo=True) as (wpas_radio, wpas_iface):

        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(wpas_iface)

        params = {"ssid": "uhr",
                  "hw_mode": "g",
                  "channel": "1",
                  "ieee80211ax": "1",
                  "ieee80211be": "1",
                  "ieee80211bn": "1"}
        try:
            hapd = uhr_mld_enable_ap(hapd_iface, 0, params)
        except HwsimSkip:
            raise
        except Exception as e:
            if str(e) == "Failed to set hostapd parameter ieee80211bn":
                raise HwsimSkip("UHR not supported")
            raise
        if hapd.get_status_field("ieee80211bn") != "1":
            raise Exception("AP STATUS did not indicate ieee80211bn=1")
        wpas.connect("uhr", key_mgmt="NONE", scan_freq="2412")
        sta = hapd.get_sta(wpas.own_addr())
        if "[UHR]" not in sta['flags']:
            raise Exception("Missing STA flag: UHR")
        status = wpas.request("STATUS")
        if "wifi_generation=8" not in status:
            raise Exception("STA STATUS did not indicate wifi_generation=8")

def test_prefer_uhr_20(dev, apdev):
    """UHR AP on a 20 MHz channel"""
    with HWSimRadio(use_mlo=True) as (hapd0_radio, hapd0_iface), \
         HWSimRadio(use_mlo=True) as (hapd1_radio, hapd1_iface), \
         HWSimRadio(use_mlo=True) as (wpas_radio, wpas_iface):

        wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
        wpas.interface_add(wpas_iface)

        params = {"ssid": "uhr",
                  "hw_mode": "g",
                  "channel": "1",
                  "ieee80211ax": "1",
                  "ieee80211be": "1",
                  "ieee80211n": "1",
                  "ieee80211bn": "1"}
        try:
            hapd0 = uhr_mld_enable_ap(hapd0_iface, 0, params)

            params["ieee80211bn"] = "0"
            hapd1 = uhr_mld_enable_ap(hapd1_iface, 0, params)
        except HwsimSkip:
            raise
        except Exception as e:
            if str(e) == "Failed to set hostapd parameter ieee80211bn":
                raise HwsimSkip("UHR not supported")
            raise

        wpas.connect("uhr", key_mgmt="NONE", scan_freq="2412")
        if wpas.get_status_field('bssid') != hapd0.own_addr():
            raise Exception("wpas connected to unexpected AP")

        est = wpas.get_bss(hapd0.own_addr())['est_throughput']
        if est != "172103":
            raise Exception("Unexpected BSS1 est_throughput: " + est)

def _test_uhr_5ghz(dev, apdev, channel, chanwidth, ccfs1, ccfs2=0,
                   uhr_oper_puncturing_override=None,
                   he_ccfs1=None, he_oper_chanwidth=None):
    if he_ccfs1 is None:
        he_ccfs1 = ccfs1
    if he_oper_chanwidth is None:
        he_oper_chanwidth = chanwidth

    try:
        params = {"ssid": "uhr",
                  "country_code": "US",
                  "hw_mode": "a",
                  "channel": str(channel),
                  "ieee80211n": "1",
                  "ieee80211ac": "1",
                  "ieee80211ax": "1",
                  "ieee80211be": "1",
		  "ieee80211bn": "1",
                  "vht_oper_chwidth": str(he_oper_chanwidth),
                  "vht_oper_centr_freq_seg0_idx": str(he_ccfs1),
                  "vht_oper_centr_freq_seg1_idx": str(ccfs2),
                  "he_oper_chwidth": str(he_oper_chanwidth),
                  "he_oper_centr_freq_seg0_idx": str(he_ccfs1),
                  "he_oper_centr_freq_seg1_idx": str(ccfs2),
                  "eht_oper_centr_freq_seg0_idx": str(ccfs1),
                  "eht_oper_chwidth": str(chanwidth)}

        if he_oper_chanwidth == 0:
            if channel < he_ccfs1:
                  params["ht_capab"] = "[HT40+]"
            elif channel > he_ccfs1:
                  params["ht_capab"] = "[HT40-]"
        else:
            params["ht_capab"] = "[HT40+]"
            if he_oper_chanwidth == 2:
                params["vht_capab"] = "[VHT160]"
            elif he_oper_chanwidth == 3:
                params["vht_capab"] = "[VHT160-80PLUS80]"

        if uhr_oper_puncturing_override:
            params['eht_oper_puncturing_override'] = uhr_oper_puncturing_override

        freq = 5000 + channel * 5
        bw = "20"
        if chanwidth != 0 or channel != ccfs1:
            bw = _5ghz_chanwidth_to_bw(chanwidth)

        with HWSimRadio(use_mlo=True) as (hapd_radio, hapd_iface), \
             HWSimRadio(use_mlo=True) as (wpas_radio, wpas_iface):

            wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
            wpas.interface_add(wpas_iface)

            try:
                hapd = uhr_mld_enable_ap(hapd_iface, 0, params)
            except HwsimSkip:
                raise

            wpas.connect("uhr", key_mgmt="NONE", scan_freq=str(freq))
            uhr_verify_status(wpas, hapd, freq, bw, is_ht=True, is_vht=True)
            uhr_verify_wifi_version(wpas)
            hwsim_utils.test_connectivity(wpas, hapd)

            if uhr_oper_puncturing_override:
                hapd.set("eht_oper_puncturing_override", "0x0")
                tmp = hapd.request("UPDATE_BEACON")
                time.sleep(1)

            wpas.request("DISCONNECT")
            wpas.wait_disconnected()
            hapd.wait_sta_disconnect()
    finally:
        set_world_reg(apdev[0], None, dev[0])

def test_uhr_5ghz_20mhz(dev, apdev):
    """UHR with 20 MHz channel width on 5 GHz"""
    _test_uhr_5ghz(dev, apdev, 36, 0, 36, 0)

def test_uhr_5ghz_40mhz_low(dev, apdev):
    """UHR with 40 MHz channel width on 5 GHz - secondary channel above"""
    _test_uhr_5ghz(dev, apdev, 36, 0, 38, 0)

def test_uhr_5ghz_40mhz_high(dev, apdev):
    """UHR with 80 MHz channel width on 5 GHz - secondary channel below"""
    _test_uhr_5ghz(dev, apdev, 40, 0, 38, 0)

def test_uhr_5ghz_80mhz_1(dev, apdev):
    """UHR with 80 MHz channel width on 5 GHz - primary=149"""
    _test_uhr_5ghz(dev, apdev, 36, 1, 42, 0)

def test_uhr_5ghz_80mhz_2(dev, apdev):
    """UHR with 80 MHz channel width on 5 GHz - primary=149"""
    _test_uhr_5ghz(dev, apdev, 149, 1, 155, 0)

def test_uhr_5ghz_80p80mhz(dev, apdev):
    """UHR with 80+80 MHz channel width on 5 GHz"""
    _test_uhr_5ghz(dev, apdev, 36, 3, 42, 155)

def _test_uhr_6ghz(dev, apdev, channel, op_class, ccfs1):
    check_sae_capab(dev[0])

    # CA enables 320 MHz channels without NO-IR restriction
    dev[0].cmd_execute(['iw', 'reg', 'set', 'CA'])
    wait_regdom_changes(dev[0])

    try:
        ssid = "uhr_6ghz_sae"
        passphrase = "12345678"
        params = hostapd.he_wpa2_params(ssid=ssid, passphrase=passphrase)
        params["ieee80211bn"] = "1"
        params["ieee80211be"] = "1"
        params["channel"] = str(channel)
        params["op_class"] = str(op_class)
        params["he_oper_centr_freq_seg0_idx"] = str(ccfs1)
        params["eht_oper_centr_freq_seg0_idx"] = str(ccfs1)
        params["country_code"] = "CA"

        if not he_6ghz_supported():
            raise HwsimSkip("6 GHz frequency is not supported")
        if op_class == 137 and not uhr_320mhz_supported():
            raise HwsimSkip("320 MHz channels are not supported")

        with HWSimRadio(use_mlo=True) as (hapd_radio, hapd_iface), \
             HWSimRadio(use_mlo=True) as (wpas_radio, wpas_iface):

            wpas = WpaSupplicant(global_iface='/tmp/wpas-wlan5')
            wpas.interface_add(wpas_iface)
            check_sae_capab(wpas)

            hapd = uhr_mld_enable_ap(hapd_iface, 0, params)
            status = hapd.get_status()
            logger.info("hostapd STATUS: " + str(status))
            if hapd.get_status_field("ieee80211ax") != "1":
                raise Exception("STATUS did not indicate ieee80211ax=1")

            if hapd.get_status_field("ieee80211be") != "1":
                raise Exception("STATUS did not indicate ieee80211be=1")

            if hapd.get_status_field("ieee80211bn") != "1":
                raise Exception("STATUS did not indicate ieee80211bn=1")

            wpas.set("sae_pwe", "1")
            wpas.set("sae_groups", "")

            freq = 5950 + channel * 5
            bw = _6ghz_op_class_to_bw(op_class)

            wpas.connect(ssid, key_mgmt="SAE", psk=passphrase, ieee80211w="2",
                         scan_freq=str(freq))
            hapd.wait_sta()

            uhr_verify_status(wpas, hapd, freq, bw)
            uhr_verify_wifi_version(wpas)
            sta = hapd.get_sta(wpas.own_addr())
            if 'supp_op_classes' not in sta:
                raise Exception("supp_op_classes not indicated")
            supp_op_classes = binascii.unhexlify(sta['supp_op_classes'])
            if op_class not in supp_op_classes:
                raise Exception("STA did not indicate support for opclass %d" % op_class)
            hwsim_utils.test_connectivity(wpas, hapd)
            wpas.request("DISCONNECT")
            wpas.wait_disconnected()
            hapd.wait_sta_disconnect()
    finally:
        dev[0].set("sae_pwe", "0")
        dev[0].cmd_execute(['iw', 'reg', 'set', '00'])

def test_uhr_6ghz_20mhz(dev, apdev):
    """UHR with 20 MHz channel width on 6 GHz"""
    _test_uhr_6ghz(dev, apdev, 1, 131, 1)

def test_uhr_6ghz_40mhz(dev, apdev):
    """UHR with 40 MHz channel width on 6 GHz"""
    _test_uhr_6ghz(dev, apdev, 1, 132, 3)

def test_uhr_6ghz_80mhz(dev, apdev):
    """UHR with 80 MHz channel width on 6 GHz"""
    _test_uhr_6ghz(dev, apdev, 1, 133, 7)

def test_uhr_6ghz_160mhz(dev, apdev):
    """UHR with 160 MHz channel width on 6 GHz"""
    _test_uhr_6ghz(dev, apdev, 1, 134, 15)

def test_uhr_6ghz_320mhz(dev, apdev):
    """UHR with 320 MHz channel width on 6 GHz"""
    _test_uhr_6ghz(dev, apdev, 1, 137, 31)
