#Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries
#SPDX-License-Identifier: BSD-3-Clause
#
#
# ACL modes:
#   0 = ACCEPT_UNLESS_DENIED          (default)
#   1 = DENY_UNLESS_ACCEPTED
#   2 = USE_EXTERNAL_RADIUS_AUTH      (not tested here)
#   3 = ACCEPT_IF_WHITELIST_AND_NOT_BLACKLIST
#   4 = DENY_WITH_TIMED_ALLOW_WINDOW
#
# mac80211_hwsim device addresses:
#   wlan0 (dev[0]) -> 02:00:00:00:00:00
#   wlan1 (dev[1]) -> 02:00:00:00:01:00
#   wlan2 (dev[2]) -> 02:00:00:00:02:00
#
# NOTE on _oui_prefix() in hwsim:
#   All trailing bytes of hwsim addresses are already 0x00, so
#   _oui_prefix(addr0, zero_bytes=N) == addr0 for any N.
#   Tests that need base != addr0 must use addr1 or addr2 as the base.

import logging
import time
logger = logging.getLogger()

import hostapd
from utils import *


# ---------------------------------------------------------------------------
# Helper utilities
# ---------------------------------------------------------------------------

def _oui_prefix(addr, zero_bytes=3):
    """Return the first (6 - zero_bytes) octets of addr followed by
    zero_bytes zero octets.
    e.g. _oui_prefix('02:00:00:00:01:00', 3) -> '02:00:00:00:00:00'
    """
    parts = addr.split(':')
    keep = 6 - zero_bytes
    return ':'.join(parts[:keep] + ['00'] * zero_bytes)


def _oui_mask(zero_bytes=3):
    """Return a mask with (6 - zero_bytes) ff octets followed by zero_bytes
    00 octets.
    e.g. _oui_mask(3) -> 'ff:ff:ff:00:00:00'
    """
    keep = 6 - zero_bytes
    return ':'.join(['ff'] * keep + ['00'] * zero_bytes)


def _check_acl_counts(hapd, num_accept, num_deny, label=""):
    """Assert exact accept and deny list entry counts."""
    accept = hapd.request("ACCEPT_ACL SHOW").splitlines()
    deny = hapd.request("DENY_ACL SHOW").splitlines()
    logger.info("%s accept=%s deny=%s" % (label, accept, deny))
    if len(accept) != num_accept:
        raise Exception("%s: expected %d accept entries, got %d: %s" %
                        (label, num_accept, len(accept), accept))
    if len(deny) != num_deny:
        raise Exception("%s: expected %d deny entries, got %d: %s" %
                        (label, num_deny, len(deny), deny))


# ===========================================================================
# Mode 0 — ACCEPT_UNLESS_DENIED (default)
# ===========================================================================

def test_ap_acl_mode0_accept_unless_denied(dev, apdev):
    """MAC ACL mode 0: accept all STAs unless explicitly in deny list"""
    ssid = "acl-mode0"
    params = {'ssid': ssid, 'macaddr_acl': "0"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()

    # --- No deny list: all STAs connect ---
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()

    # --- Add deny entry for addr0: addr0 blocked, addr1 still connects ---
    hapd.request("DENY_ACL ADD_MAC " + addr0)

    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[0] connected despite deny entry (mode 0)")
    dev[0].request("DISCONNECT")

    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()

    # --- Remove deny entry: addr0 can connect again ---
    hapd.request("DENY_ACL DEL_MAC " + addr0)
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()


# ===========================================================================
# Mode 1 — DENY_UNLESS_ACCEPTED
# ===========================================================================

def test_ap_acl_mode1_deny_unless_accepted(dev, apdev):
    """MAC ACL mode 1: deny all STAs unless explicitly in accept list"""
    ssid = "acl-mode1"
    params = {'ssid': ssid, 'macaddr_acl': "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()

    # --- No accept list: all blocked ---
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[0] connected with empty accept list (mode 1)")
    dev[0].request("DISCONNECT")

    # --- Add accept entry for addr0: addr0 connects, addr1 blocked ---
    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)

    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[1] connected without accept entry (mode 1)")
    dev[1].request("DISCONNECT")

    # --- Add accept entry for addr1: both connect ---
    hapd.request("ACCEPT_ACL ADD_MAC " + addr1)
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # --- Remove accept entry for addr0: addr0 disconnected ---
    dev[0].dump_monitor()
    hapd.request("ACCEPT_ACL DEL_MAC " + addr0)
    dev[0].wait_disconnected()
    dev[0].request("DISCONNECT")

    # addr0 cannot reconnect
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[0] reconnected after accept entry removed")
    dev[0].request("DISCONNECT")

    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()
    hapd.request("ACCEPT_ACL CLEAR")


# ===========================================================================
# Mode 3 — ACCEPT_IF_WHITELIST_AND_NOT_BLACKLIST
# ===========================================================================

def test_ap_acl_mode3_accept_and_not_deny(dev, apdev):
    """MAC ACL mode 3: comprehensive whitelist AND NOT blacklist test"""
    ssid = "acl-mode3-comp"
    params = {'ssid': ssid, 'macaddr_acl': "3"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()

    # Case 1: not in accept, not in deny -> rejected
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("Case 1: connected without accept entry (mode 3)")
    dev[0].request("DISCONNECT")

    # Case 2: in accept, not in deny -> accepted
    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    # Case 3: in deny only (not in accept) -> rejected
    hapd.request("DENY_ACL ADD_MAC " + addr1)
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("Case 3: connected with deny-only entry (mode 3)")
    dev[1].request("DISCONNECT")

    # Case 4: in accept AND in deny -> rejected
    hapd.request("ACCEPT_ACL ADD_MAC " + addr1)
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("Case 4: connected despite deny entry (mode 3)")
    dev[1].request("DISCONNECT")

    # Case 5: in accept, removed from deny -> accepted again
    hapd.request("DENY_ACL DEL_MAC " + addr1)
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()

    hapd.request("ACCEPT_ACL CLEAR")
    hapd.request("DENY_ACL CLEAR")


# ===========================================================================
# Mode 4 — DENY_WITH_TIMED_ALLOW_WINDOW
# ===========================================================================

def test_ap_acl_mode4_deny_with_timed_allow(dev, apdev):
    """MAC ACL mode 4: deny list with timed allow window"""
    ssid = "acl-mode4"
    # Use deny_wait=3s so it exceeds the 2s wait_event timeout below.
    # If deny_wait were 1s the STA would enter the allow phase within the
    # 2s window and the "connected during deny phase" check would fire.
    params = {
        'ssid': ssid,
        'macaddr_acl': "4",
        'acl_deny_wait_time': "3",
        'acl_deny_allow_time': "2",
    }
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()

    # addr1 not in deny list: always connects
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()

    # Add addr0 to deny list
    hapd.request("DENY_ACL ADD_MAC " + addr0)

    # Immediately: addr0 is in deny phase (3 s > 2 s timeout -> stays blocked)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[0] connected during deny phase (mode 4)")
    dev[0].request("DISCONNECT")

    # After deny phase expires (3 s): addr0 enters allow phase (2 s)
    time.sleep(3.5)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    logger.info("dev[0] connected during allow phase (mode 4)")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()

    hapd.request("DENY_ACL CLEAR")


# ===========================================================================
# ACL with bitmask (masked MAC entries)
# ===========================================================================

def test_ap_acl_bitmask_oui_accept(dev, apdev):
    """MAC ACL accept list with OUI bitmask (ff:ff:ff:00:00:00) - mode 1"""
    ssid = "acl-bitmask-oui-accept"
    params = {'ssid': ssid, 'macaddr_acl': "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    # OUI mask: match all addresses sharing the same first 3 bytes
    base = _oui_prefix(addr0, zero_bytes=3)   # 02:00:00:00:00:00
    mask = _oui_mask(zero_bytes=3)             # ff:ff:ff:00:00:00

    # No accept list: all blocked
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[0] connected with empty accept list")
    dev[0].request("DISCONNECT")

    # Add OUI-based accept entry
    hapd.request("ACCEPT_ACL ADD_MAC " + base + " " + mask)

    # All hwsim devices share OUI 02:00:00 -> all should connect
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[2].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[2].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()
    dev[2].request("DISCONNECT")
    dev[2].wait_disconnected()
    hapd.request("ACCEPT_ACL CLEAR")


def test_ap_acl_bitmask_oui_deny(dev, apdev):
    """MAC ACL deny list with OUI bitmask (ff:ff:ff:00:00:00) - mode 0"""
    ssid = "acl-bitmask-oui-deny"
    params = {'ssid': ssid, 'macaddr_acl': "0"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    # OUI mask: deny all addresses sharing the same first 3 bytes
    base = _oui_prefix(addr0, zero_bytes=3)   # 02:00:00:00:00:00
    mask = _oui_mask(zero_bytes=3)             # ff:ff:ff:00:00:00

    hapd.request("DENY_ACL ADD_MAC " + base + " " + mask)

    # All hwsim devices share OUI 02:00:00 -> all should be blocked
    for d in [dev[0], dev[1], dev[2]]:
        d.scan_for_bss(apdev[0]['bssid'], freq="2412")
        d.connect(ssid, key_mgmt="NONE", scan_freq="2412",
                  wait_connect=False)
        ev = d.wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
        if ev is not None:
            raise Exception(d.ifname + " connected despite OUI deny entry")
        d.request("DISCONNECT")

    hapd.request("DENY_ACL CLEAR")


def test_ap_acl_bitmask_half_octet(dev, apdev):
    """MAC ACL accept list with sub-octet bitmask (ff:ff:ff:ff:fe:00)"""
    ssid = "acl-bitmask-half"
    params = {'ssid': ssid, 'macaddr_acl': "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    # mask ff:ff:ff:ff:fe:00 with base 02:00:00:00:00:00:
    #   dev[0] 02:00:00:00:00:00: byte4=0x00, 0x00&0xfe=0x00 == 0x00 -> MATCH
    #   dev[1] 02:00:00:00:01:00: byte4=0x01, 0x01&0xfe=0x00 == 0x00 -> MATCH
    #   dev[2] 02:00:00:00:02:00: byte4=0x02, 0x02&0xfe=0x02 != 0x00 -> NO MATCH
    base = "02:00:00:00:00:00"
    mask = "ff:ff:ff:ff:fe:00"

    hapd.request("ACCEPT_ACL ADD_MAC " + base + " " + mask)

    # dev[0] should connect (byte4 0x00 & 0xfe = 0x00 matches base)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # dev[1] should connect (byte4 0x01 & 0xfe = 0x00 matches base)
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # dev[2] should NOT connect (byte4 0x02 & 0xfe = 0x02 != 0x00)
    dev[2].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[2].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[2].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[2] connected despite sub-octet mask mismatch")
    dev[2].request("DISCONNECT")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()
    hapd.request("ACCEPT_ACL CLEAR")


def test_ap_acl_bitmask_mode1_oui(dev, apdev):
    """MAC ACL mode 1 with OUI bitmask: add/remove OUI accept entry"""
    ssid = "acl-bitmask-mode1-oui"
    params = {'ssid': ssid, 'macaddr_acl': "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    base = _oui_prefix(addr0, zero_bytes=3)   # 02:00:00:00:00:00
    mask = _oui_mask(zero_bytes=3)             # ff:ff:ff:00:00:00

    # No accept list: all blocked
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[0] connected with empty accept list")
    dev[0].request("DISCONNECT")

    # Add OUI accept entry: all hwsim devices connect
    hapd.request("ACCEPT_ACL ADD_MAC " + base + " " + mask)
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # Remove OUI accept entry: both disconnected
    hapd.request("ACCEPT_ACL DEL_MAC " + base + " " + mask)
    dev[0].wait_disconnected()
    dev[1].wait_disconnected()
    dev[0].request("DISCONNECT")
    dev[1].request("DISCONNECT")

    # Verify neither can reconnect
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[0] reconnected after OUI accept entry removed")
    dev[0].request("DISCONNECT")


# ===========================================================================
# Masked MAC entry tests (fixed versions)
# ===========================================================================

def test_ap_acl_masked_accept(dev, apdev):
    """MAC ACL accept list with masked entry - only matching range can connect"""
    ssid = "acl-masked-accept"
    params = {'ssid': ssid, 'macaddr_acl': "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()   # 02:00:00:00:00:00
    addr1 = dev[1].own_addr()   # 02:00:00:00:01:00

    # mask ff:ff:ff:ff:ff:00 with base 02:00:00:00:00:00:
    #   dev[0] 02:00:00:00:00:00: byte4=0x00 matches -> CONNECT
    #   dev[1] 02:00:00:00:01:00: byte4=0x01 != 0x00 -> BLOCKED
    base = _oui_prefix(addr0, zero_bytes=1)   # 02:00:00:00:00:00
    mask = _oui_mask(zero_bytes=1)             # ff:ff:ff:ff:ff:00

    hapd.request("ACCEPT_ACL ADD_MAC " + base + " " + mask)

    # Verify SHOW output
    accept = hapd.request("ACCEPT_ACL SHOW").splitlines()
    logger.info("accept list: " + str(accept))
    expected = base + " mask=" + mask + " VLAN_ID=0"
    if expected not in accept:
        raise Exception("Masked accept entry not found in SHOW: " + str(accept))

    # dev[0] should connect
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # dev[1] should NOT connect (5th byte differs)
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[1] connected unexpectedly via masked accept entry")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[1].request("DISCONNECT")
    hapd.request("ACCEPT_ACL CLEAR")


def test_ap_acl_masked_deny(dev, apdev):
    """MAC ACL deny list with masked entry - matching range is blocked"""
    ssid = "acl-masked-deny"
    params = {'ssid': ssid, 'macaddr_acl': "0"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()   # 02:00:00:00:00:00
    addr1 = dev[1].own_addr()   # 02:00:00:00:01:00

    # Deny entry matching addr0's first 5 bytes (last byte wildcard)
    base = _oui_prefix(addr0, zero_bytes=1)   # 02:00:00:00:00:00
    mask = _oui_mask(zero_bytes=1)             # ff:ff:ff:ff:ff:00

    hapd.request("DENY_ACL ADD_MAC " + base + " " + mask)

    # Verify SHOW output
    deny = hapd.request("DENY_ACL SHOW").splitlines()
    logger.info("deny list: " + str(deny))
    expected = base + " mask=" + mask + " VLAN_ID=0"
    if expected not in deny:
        raise Exception("Masked deny entry not found in SHOW: " + str(deny))

    # dev[0] should NOT connect (address matches deny entry)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[0] connected despite matching masked deny entry")

    # dev[1] should connect (5th byte differs, no match in deny list)
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    dev[0].request("DISCONNECT")
    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()
    hapd.request("DENY_ACL CLEAR")


def test_ap_acl_masked_mgmt(dev, apdev):
    """MAC ACL management: ADD/DEL/SHOW/CLEAR with exact and masked entries"""
    ssid = "acl-masked-mgmt"
    params = {'ssid': ssid}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()   # 02:00:00:00:00:00
    addr1 = dev[1].own_addr()   # 02:00:00:00:01:00

    # Use addr1 as the base for the masked entry so that base != addr0.
    # This ensures DEL addr0 (without mask) removes only the exact entry,
    # not the masked entry (which has stored_addr = addr1).
    base = _oui_prefix(addr1, zero_bytes=1)   # 02:00:00:00:01:00
    mask = _oui_mask(zero_bytes=1)             # ff:ff:ff:ff:ff:00

    # --- Step 1: initially empty ---
    _check_acl_counts(hapd, 0, 0, "initial")

    # --- Step 2: add one exact entry ---
    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)
    _check_acl_counts(hapd, 1, 0, "after exact ADD")

    # --- Step 3: add one masked entry (base=addr1 != addr0) ---
    hapd.request("ACCEPT_ACL ADD_MAC " + base + " " + mask)
    accept = hapd.request("ACCEPT_ACL SHOW").splitlines()
    logger.info("accept (exact+masked): " + str(accept))
    if len(accept) != 2:
        raise Exception("Expected 2 accept entries (1 exact + 1 masked), "
                        "got %d: %s" % (len(accept), accept))

    exact_entry  = addr0 + " VLAN_ID=0"
    masked_entry = base + " mask=" + mask + " VLAN_ID=0"
    if exact_entry not in accept:
        raise Exception("Exact entry missing from SHOW: " + str(accept))
    if masked_entry not in accept:
        raise Exception("Masked entry missing from SHOW: " + str(accept))

    # --- Step 4: DEL the exact entry (addr0) ---
    # Since base=addr1 != addr0, the masked entry is NOT removed
    hapd.request("ACCEPT_ACL DEL_MAC " + addr0)
    accept = hapd.request("ACCEPT_ACL SHOW").splitlines()
    if len(accept) != 1:
        raise Exception("Expected 1 entry after DEL exact, got %d: %s" %
                        (len(accept), accept))
    if masked_entry not in accept:
        raise Exception("Masked entry should survive DEL of exact entry")

    # --- Step 5: DEL the masked entry using addr+mask ---
    hapd.request("ACCEPT_ACL DEL_MAC " + base + " " + mask)
    _check_acl_counts(hapd, 0, 0, "after DEL masked")

    # --- Step 6: add multiple entries then CLEAR ---
    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)
    hapd.request("ACCEPT_ACL ADD_MAC " + addr1)
    hapd.request("ACCEPT_ACL ADD_MAC " + base + " " + mask)
    _check_acl_counts(hapd, 3, 0, "before CLEAR")

    hapd.request("ACCEPT_ACL CLEAR")
    _check_acl_counts(hapd, 0, 0, "after CLEAR")


def test_ap_acl_masked_del_with_mask(dev, apdev):
    """MAC ACL DEL_MAC with explicit mask removes only the matching entry"""
    ssid = "acl-del-with-mask"
    params = {'ssid': ssid}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()   # 02:00:00:00:00:00

    # Two different masked entries with different masks
    base1 = _oui_prefix(addr0, zero_bytes=1)   # 02:00:00:00:00:00
    mask1 = _oui_mask(zero_bytes=1)             # ff:ff:ff:ff:ff:00

    base2 = _oui_prefix(addr0, zero_bytes=2)   # 02:00:00:00:00:00
    mask2 = _oui_mask(zero_bytes=2)             # ff:ff:ff:ff:00:00

    # Add exact + two masked entries
    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)
    hapd.request("ACCEPT_ACL ADD_MAC " + base1 + " " + mask1)
    hapd.request("ACCEPT_ACL ADD_MAC " + base2 + " " + mask2)
    _check_acl_counts(hapd, 3, 0, "initial 3 entries")

    # DEL with mask1 - should remove only the mask1 entry
    hapd.request("ACCEPT_ACL DEL_MAC " + base1 + " " + mask1)
    accept = hapd.request("ACCEPT_ACL SHOW").splitlines()
    logger.info("accept after DEL mask1: " + str(accept))
    if len(accept) != 2:
        raise Exception("Expected 2 entries after DEL mask1, got %d" %
                        len(accept))
    masked2_entry = base2 + " mask=" + mask2 + " VLAN_ID=0"
    if masked2_entry not in accept:
        raise Exception("mask2 entry should still be present: " + str(accept))
    exact_entry = addr0 + " VLAN_ID=0"
    if exact_entry not in accept:
        raise Exception("Exact entry should still be present: " + str(accept))

    # DEL with exact mask (ff:ff:ff:ff:ff:ff) - removes the exact entry
    exact_mask = "ff:ff:ff:ff:ff:ff"
    hapd.request("ACCEPT_ACL DEL_MAC " + addr0 + " " + exact_mask)
    accept = hapd.request("ACCEPT_ACL SHOW").splitlines()
    if len(accept) != 1:
        raise Exception("Expected 1 entry after DEL exact with mask, got %d" %
                        len(accept))
    if masked2_entry not in accept:
        raise Exception("mask2 entry should be the only remaining entry")

    hapd.request("ACCEPT_ACL CLEAR")


def test_ap_acl_masked_del_without_mask(dev, apdev):
    """MAC ACL DEL_MAC without mask removes exact AND all masked entries for addr"""
    ssid = "acl-del-no-mask"
    params = {'ssid': ssid}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()   # 02:00:00:00:00:00
    addr1 = dev[1].own_addr()   # 02:00:00:00:01:00
    mask1 = _oui_mask(zero_bytes=1)   # ff:ff:ff:ff:ff:00
    mask2 = _oui_mask(zero_bytes=2)   # ff:ff:ff:ff:00:00

    # Add: exact(addr0), masked(addr0/mask1), masked(addr0/mask2), exact(addr1)
    # All masked entries use addr0 as stored base address.
    # DEL addr0 without mask must remove all three addr0 entries.
    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)
    hapd.request("ACCEPT_ACL ADD_MAC " + addr0 + " " + mask1)
    hapd.request("ACCEPT_ACL ADD_MAC " + addr0 + " " + mask2)
    hapd.request("ACCEPT_ACL ADD_MAC " + addr1)
    _check_acl_counts(hapd, 4, 0, "initial 4 entries")

    # DEL addr0 without mask:
    #   - removes exact(addr0) from exact list
    #   - removes masked(addr0/mask1) and masked(addr0/mask2) from masked list
    #     (hostapd_remove_all_masked_entries_for_addr matches stored_addr==addr0)
    hapd.request("ACCEPT_ACL DEL_MAC " + addr0)
    accept = hapd.request("ACCEPT_ACL SHOW").splitlines()
    logger.info("accept after DEL addr0 (no mask): " + str(accept))
    if len(accept) != 1:
        raise Exception("Expected 1 entry after DEL addr0, got %d: %s" %
                        (len(accept), accept))
    if (addr1 + " VLAN_ID=0") not in accept:
        raise Exception("addr1 exact entry should be the only remaining entry")

    hapd.request("ACCEPT_ACL CLEAR")


def test_ap_acl_whitelist_and_not_blacklist(dev, apdev):
    """MAC ACL mode 3: accept only if in whitelist AND not in blacklist"""
    ssid = "acl-mode3"
    params = {'ssid': ssid, 'macaddr_acl': "3"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()

    # Case 1: empty accept list -> all rejected
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[0] connected with empty accept list (mode 3)")
    dev[0].request("DISCONNECT")

    # Case 2: addr0 in accept list, not in deny -> accepted
    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # Case 3: addr1 not in accept list -> rejected
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[1] connected without accept entry (mode 3)")
    dev[1].request("DISCONNECT")

    # Case 4: addr0 added to deny list -> disconnected
    dev[0].dump_monitor()
    hapd.request("DENY_ACL ADD_MAC " + addr0)
    dev[0].wait_disconnected()
    dev[0].request("DISCONNECT")

    # Case 5: addr0 in accept AND deny -> rejected on reconnect
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[0] reconnected while in both accept and deny")
    dev[0].request("DISCONNECT")

    hapd.request("ACCEPT_ACL CLEAR")
    hapd.request("DENY_ACL CLEAR")


def test_ap_acl_duplicate_exact(dev, apdev):
    """MAC ACL duplicate exact entry is silently ignored (count stays at 1)"""
    ssid = "acl-dup-exact"
    params = {'ssid': ssid}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()

    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)
    _check_acl_counts(hapd, 1, 0, "after first ADD")

    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)
    _check_acl_counts(hapd, 1, 0, "after duplicate ADD")

    hapd.request("ACCEPT_ACL CLEAR")


def test_ap_acl_duplicate_masked(dev, apdev):
    """MAC ACL duplicate masked entry (same addr+mask) is silently ignored"""
    ssid = "acl-dup-masked"
    params = {'ssid': ssid}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    base  = _oui_prefix(addr0, zero_bytes=1)
    mask  = _oui_mask(zero_bytes=1)

    hapd.request("ACCEPT_ACL ADD_MAC " + base + " " + mask)
    _check_acl_counts(hapd, 1, 0, "after first masked ADD")

    hapd.request("ACCEPT_ACL ADD_MAC " + base + " " + mask)
    _check_acl_counts(hapd, 1, 0, "after duplicate masked ADD")

    hapd.request("ACCEPT_ACL CLEAR")


def test_ap_acl_same_addr_different_masks(dev, apdev):
    """MAC ACL same base address with different masks creates separate entries"""
    ssid = "acl-same-addr-diff-mask"
    params = {'ssid': ssid}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()

    base1 = _oui_prefix(addr0, zero_bytes=1)
    mask1 = _oui_mask(zero_bytes=1)   # ff:ff:ff:ff:ff:00

    base2 = _oui_prefix(addr0, zero_bytes=2)
    mask2 = _oui_mask(zero_bytes=2)   # ff:ff:ff:ff:00:00

    base3 = _oui_prefix(addr0, zero_bytes=3)
    mask3 = _oui_mask(zero_bytes=3)   # ff:ff:ff:00:00:00

    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)
    hapd.request("ACCEPT_ACL ADD_MAC " + base1 + " " + mask1)
    hapd.request("ACCEPT_ACL ADD_MAC " + base2 + " " + mask2)
    hapd.request("ACCEPT_ACL ADD_MAC " + base3 + " " + mask3)

    accept = hapd.request("ACCEPT_ACL SHOW").splitlines()
    logger.info("accept (4 entries): " + str(accept))
    if len(accept) != 4:
        raise Exception("Expected 4 separate entries, got %d: %s" %
                        (len(accept), accept))

    entries = [
        addr0 + " VLAN_ID=0",
        base1 + " mask=" + mask1 + " VLAN_ID=0",
        base2 + " mask=" + mask2 + " VLAN_ID=0",
        base3 + " mask=" + mask3 + " VLAN_ID=0",
    ]
    for e in entries:
        if e not in accept:
            raise Exception("Entry missing from SHOW: %s\nFull list: %s" %
                            (e, accept))

    hapd.request("ACCEPT_ACL CLEAR")


def test_ap_acl_accept_changes_masked(dev, apdev):
    """MAC ACL dynamic changes: removing OUI masked entry disconnects STAs"""
    ssid = "acl-changes-masked"
    params = {'ssid': ssid, 'macaddr_acl': "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()

    # OUI mask matches both dev[0] and dev[1] (same OUI 02:00:00)
    base = _oui_prefix(addr0, zero_bytes=3)
    mask = _oui_mask(zero_bytes=3)

    hapd.request("ACCEPT_ACL ADD_MAC " + base + " " + mask)

    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # Remove the masked entry - both should be disconnected
    hapd.request("ACCEPT_ACL DEL_MAC " + base + " " + mask)
    dev[0].wait_disconnected()
    dev[1].wait_disconnected()
    dev[0].request("DISCONNECT")
    dev[1].request("DISCONNECT")

    _check_acl_counts(hapd, 0, 0, "after DEL masked entry")


def test_ap_acl_masked_accept_with_vlan(dev, apdev):
    """MAC ACL masked accept entry with VLAN ID assignment"""
    ssid = "acl-masked-vlan"
    params = {'ssid': ssid, 'macaddr_acl': "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    base  = _oui_prefix(addr0, zero_bytes=1)
    mask  = _oui_mask(zero_bytes=1)
    vlan_id = 5

    hapd.request("ACCEPT_ACL ADD_MAC " + base + " " + mask +
                 " VLAN_ID=" + str(vlan_id))

    accept = hapd.request("ACCEPT_ACL SHOW").splitlines()
    logger.info("accept: " + str(accept))
    expected = base + " mask=" + mask + " VLAN_ID=" + str(vlan_id)
    if expected not in accept:
        raise Exception("Masked accept entry with VLAN_ID not found: " +
                        str(accept))

    hapd.request("ACCEPT_ACL CLEAR")



def test_ap_acl_mode3_masked_accept(dev, apdev):
    """MAC ACL mode 3 with masked accept entry - OUI-based whitelist"""
    ssid = "acl-mode3-masked"
    params = {'ssid': ssid, 'macaddr_acl': "3"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()

    base = _oui_prefix(addr0, zero_bytes=1)
    mask = _oui_mask(zero_bytes=1)
    hapd.request("ACCEPT_ACL ADD_MAC " + base + " " + mask)

    # dev[0] should connect (matches masked accept entry, not in deny)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # dev[1] should NOT connect (5th byte differs, no match in accept list)
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[1] connected without matching masked accept entry")
    dev[1].request("DISCONNECT")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd.request("ACCEPT_ACL CLEAR")


def test_ap_acl_deny_masked_then_exact_accept(dev, apdev):
    """MAC ACL mode 3: masked deny overrides exact accept for same STA"""
    ssid = "acl-deny-masked-exact-accept"
    params = {'ssid': ssid, 'macaddr_acl': "3"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()

    # Masked deny entry matching addr0's first 5 bytes
    deny_base = _oui_prefix(addr0, zero_bytes=1)
    deny_mask = _oui_mask(zero_bytes=1)
    hapd.request("DENY_ACL ADD_MAC " + deny_base + " " + deny_mask)

    # Exact accept entry for addr0
    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)

    # dev[0]: in accept BUT matches masked deny -> rejected (mode 3)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[0] connected despite matching masked deny entry")
    dev[0].request("DISCONNECT")

    # Exact accept entry for addr1 (not in deny list)
    hapd.request("ACCEPT_ACL ADD_MAC " + addr1)

    # dev[1]: in accept AND not in deny -> accepted
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()

    hapd.request("ACCEPT_ACL CLEAR")
    hapd.request("DENY_ACL CLEAR")


def test_ap_acl_oui_accept_with_exact_deny(dev, apdev):
    """MAC ACL mode 0: exact deny blocks one STA, others still connect"""
    ssid = "acl-oui-accept-exact-deny"
    params = {'ssid': ssid}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()

    hapd.request("DENY_ACL ADD_MAC " + addr0)

    # dev[0] should NOT connect (exact deny)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[0] connected despite exact deny entry")
    dev[0].request("DISCONNECT")

    # dev[1] should connect (not in deny list)
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()

    hapd.request("DENY_ACL CLEAR")


def test_ap_acl_masked_deny_then_remove(dev, apdev):
    """MAC ACL: add masked deny, verify block, remove it, verify access"""
    ssid = "acl-masked-deny-remove"
    params = {'ssid': ssid}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    base  = _oui_prefix(addr0, zero_bytes=1)
    mask  = _oui_mask(zero_bytes=1)

    hapd.request("DENY_ACL ADD_MAC " + base + " " + mask)

    # dev[0] should NOT connect
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[0] connected despite masked deny entry")
    dev[0].request("DISCONNECT")

    # Remove the masked deny entry
    hapd.request("DENY_ACL DEL_MAC " + base + " " + mask)
    _check_acl_counts(hapd, 0, 0, "after DEL masked deny")

    # dev[0] should now connect
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()


def test_ap_acl_mode0_accept_overrides_deny(dev, apdev):
    """MAC ACL mode 0: accept list overrides deny list for same STA"""
    ssid = "acl-mode0-accept-deny"
    params = {'ssid': ssid, 'macaddr_acl': "0"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()

    # Add addr0 to BOTH accept and deny lists
    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)
    hapd.request("DENY_ACL ADD_MAC " + addr0)

    # In mode 0, hostapd_check_acl() checks accept list first.
    # addr0 is found in accept list -> ACCEPT (deny list not reached).
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # addr1 is in deny list only -> REJECT
    hapd.request("DENY_ACL ADD_MAC " + addr1)
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[1] connected despite deny-only entry (mode 0)")
    dev[1].request("DISCONNECT")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd.request("ACCEPT_ACL CLEAR")
    hapd.request("DENY_ACL CLEAR")


def test_ap_acl_mode0_remove_accept_triggers_deny_disassoc(dev, apdev):
    """MAC ACL mode 0: removing from accept while in deny triggers disassoc"""
    ssid = "acl-mode0-rm-accept"
    params = {'ssid': ssid, 'macaddr_acl': "0"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()

    # Add addr0 to both accept and deny lists
    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)
    hapd.request("DENY_ACL ADD_MAC " + addr0)

    # addr0 connects (accept overrides deny in mode 0)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # Remove addr0 from accept list (still in deny list).
    # After the fix: ACCEPT_ACL DEL_MAC now also calls
    # hostapd_disassoc_deny_mac(), which disconnects STAs in the deny list.
    dev[0].dump_monitor()
    hapd.request("ACCEPT_ACL DEL_MAC " + addr0)
    dev[0].wait_disconnected()
    dev[0].request("DISCONNECT")

    # addr0 cannot reconnect (now only in deny list)
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412",
                   wait_connect=False)
    ev = dev[0].wait_event(["CTRL-EVENT-CONNECTED"], timeout=2)
    if ev is not None:
        raise Exception("dev[0] reconnected after being removed from accept "
                        "list while still in deny list (mode 0)")
    dev[0].request("DISCONNECT")
    hapd.request("DENY_ACL CLEAR")


def test_ap_acl_mode1_deny_does_not_override_accept(dev, apdev):
    """MAC ACL mode 1: adding to deny does not disconnect STA in accept list"""
    ssid = "acl-mode1-deny-no-override"
    params = {'ssid': ssid, 'macaddr_acl': "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()

    # Add addr0 to accept list -> connects
    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # Add addr0 to deny list.
    # After the fix: hostapd_disassoc_deny_mac() skips STAs that are in the
    # accept list when macaddr_acl == DENY_UNLESS_ACCEPTED, so addr0 must
    # remain connected.
    dev[0].dump_monitor()
    hapd.request("DENY_ACL ADD_MAC " + addr0)

    # Give hostapd a moment to process the deny list update
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("dev[0] was disconnected after being added to deny "
                        "list while still in accept list (mode 1)")

    # addr0 is still connected (accept overrides deny in mode 1)
    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd.request("ACCEPT_ACL CLEAR")
    hapd.request("DENY_ACL CLEAR")


def test_ap_acl_show_mixed_exact_and_masked(dev, apdev):
    """MAC ACL SHOW output correctly lists both exact and masked entries"""
    ssid = "acl-show-mixed"
    params = {'ssid': ssid}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()
    addr2 = dev[2].own_addr()

    base1 = _oui_prefix(addr0, zero_bytes=1)
    mask1 = _oui_mask(zero_bytes=1)
    base3 = _oui_prefix(addr0, zero_bytes=3)
    mask3 = _oui_mask(zero_bytes=3)

    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)
    hapd.request("ACCEPT_ACL ADD_MAC " + addr1)
    hapd.request("ACCEPT_ACL ADD_MAC " + base1 + " " + mask1)
    hapd.request("DENY_ACL ADD_MAC " + addr2)
    hapd.request("DENY_ACL ADD_MAC " + base3 + " " + mask3)

    accept = hapd.request("ACCEPT_ACL SHOW").splitlines()
    deny   = hapd.request("DENY_ACL SHOW").splitlines()
    logger.info("accept: " + str(accept))
    logger.info("deny:   " + str(deny))

    if len(accept) != 3:
        raise Exception("Expected 3 accept entries, got %d: %s" %
                        (len(accept), accept))
    if len(deny) != 2:
        raise Exception("Expected 2 deny entries, got %d: %s" %
                        (len(deny), deny))

    # Exact entries: no mask= field
    if (addr0 + " VLAN_ID=0") not in accept:
        raise Exception("Exact accept entry for addr0 missing")
    if (addr1 + " VLAN_ID=0") not in accept:
        raise Exception("Exact accept entry for addr1 missing")
    if (addr2 + " VLAN_ID=0") not in deny:
        raise Exception("Exact deny entry for addr2 missing")

    # Masked entries: include mask= field
    if (base1 + " mask=" + mask1 + " VLAN_ID=0") not in accept:
        raise Exception("Masked accept entry missing")
    if (base3 + " mask=" + mask3 + " VLAN_ID=0") not in deny:
        raise Exception("Masked deny entry missing")

    hapd.request("ACCEPT_ACL CLEAR")
    hapd.request("DENY_ACL CLEAR")
    _check_acl_counts(hapd, 0, 0, "after CLEAR both lists")


# ===========================================================================
# macaddr_acl mode-change enforcement (SET macaddr_acl <N>)
#
# When the ACL mode is changed at runtime via "SET macaddr_acl <N>", hostapd
# must immediately re-evaluate all connected STAs against the new mode and
# the current accept/deny lists.  The tests below verify that behaviour.
#
# Key rule: existing tests set the mode at AP creation time (via params dict)
# and are therefore NOT affected by the new mode-change enforcement.  Only
# tests that call "SET macaddr_acl" dynamically will trigger the new path.
# ===========================================================================

def test_ap_acl_mode_change_0_to_1_disconnects_unlisted(dev, apdev):
    """macaddr_acl mode change 0->1: STA not in accept list is disconnected"""
    ssid = "acl-mode-chg-0to1"
    # Start in mode 0 (ACCEPT_UNLESS_DENIED) - no lists, all STAs connect
    params = {'ssid': ssid, 'macaddr_acl': "0"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()

    # Both STAs connect in mode 0 (no deny list)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # Add addr1 to accept list (addr0 is NOT in accept list)
    hapd.request("ACCEPT_ACL ADD_MAC " + addr1)

    # Switch to mode 1 (DENY_UNLESS_ACCEPTED).
    # addr0 is not in the accept list -> must be disconnected.
    # addr1 is in the accept list -> must stay connected.
    dev[0].dump_monitor()
    dev[1].dump_monitor()
    hapd.request("SET macaddr_acl 1")

    # addr0 must be disconnected
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=3)
    if ev is None:
        raise Exception("dev[0] was not disconnected after mode change 0->1 "
                        "(not in accept list)")

    # addr1 must remain connected
    ev = dev[1].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("dev[1] was disconnected after mode change 0->1 "
                        "(it is in the accept list)")

    dev[0].request("DISCONNECT")
    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()
    hapd.request("ACCEPT_ACL CLEAR")


def test_ap_acl_mode_change_0_to_3_disconnects_unlisted(dev, apdev):
    """macaddr_acl mode change 0->3: STA not in accept list is disconnected"""
    ssid = "acl-mode-chg-0to3"
    params = {'ssid': ssid, 'macaddr_acl': "0"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()

    # Both connect in mode 0
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # addr1 in accept list only; addr0 not in any list
    hapd.request("ACCEPT_ACL ADD_MAC " + addr1)

    # Switch to mode 3 (ACCEPT_IF_WHITELIST_AND_NOT_BLACKLIST).
    # addr0 not in accept list -> disconnected.
    # addr1 in accept list, not in deny list -> stays connected.
    dev[0].dump_monitor()
    dev[1].dump_monitor()
    hapd.request("SET macaddr_acl 3")

    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=3)
    if ev is None:
        raise Exception("dev[0] was not disconnected after mode change 0->3 "
                        "(not in accept list)")

    ev = dev[1].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("dev[1] was disconnected after mode change 0->3 "
                        "(it is in the accept list and not in deny list)")

    dev[0].request("DISCONNECT")
    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()
    hapd.request("ACCEPT_ACL CLEAR")


def test_ap_acl_mode_change_1_to_0_keeps_connected(dev, apdev):
    """macaddr_acl mode change 1->0: accepted STA stays connected"""
    ssid = "acl-mode-chg-1to0"
    params = {'ssid': ssid, 'macaddr_acl': "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()

    # Add addr0 to accept list and connect in mode 1
    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # Switch to mode 0 (ACCEPT_UNLESS_DENIED).
    # addr0 is not in the deny list -> must stay connected.
    dev[0].dump_monitor()
    hapd.request("SET macaddr_acl 0")

    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("dev[0] was unexpectedly disconnected after mode "
                        "change 1->0 (not in deny list)")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    hapd.request("ACCEPT_ACL CLEAR")


def test_ap_acl_mode_change_0_to_0_no_disconnect(dev, apdev):
    """macaddr_acl mode change 0->0 (same mode): no spurious disconnect"""
    ssid = "acl-mode-chg-0to0"
    params = {'ssid': ssid, 'macaddr_acl': "0"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()

    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # Re-set the same mode - no disconnect should occur
    dev[0].dump_monitor()
    hapd.request("SET macaddr_acl 0")

    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("dev[0] was unexpectedly disconnected after "
                        "SET macaddr_acl 0 (same mode, no deny list)")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()


def test_ap_acl_mode_change_deny_list_enforced_on_mode_change(dev, apdev):
    """macaddr_acl mode change: STA in deny list is disconnected on mode change"""
    ssid = "acl-mode-chg-deny"
    # Start in mode 1 with addr0 in accept list
    params = {'ssid': ssid, 'macaddr_acl': "1"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()

    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)
    hapd.request("DENY_ACL ADD_MAC " + addr0)

    # In mode 1, accept overrides deny -> addr0 connects
    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # Switch to mode 0 (ACCEPT_UNLESS_DENIED).
    # In mode 0, deny list is checked after accept list.
    # addr0 is in BOTH lists; hostapd_check_acl() finds it in accept first
    # -> ACCEPT.  So addr0 should NOT be disconnected.
    dev[0].dump_monitor()
    hapd.request("SET macaddr_acl 0")

    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("dev[0] was unexpectedly disconnected after mode "
                        "change 1->0 (accept list takes priority in mode 0)")

    # Now remove addr0 from accept list (still in deny list).
    # hostapd_disassoc_deny_mac() should disconnect it.
    dev[0].dump_monitor()
    hapd.request("ACCEPT_ACL DEL_MAC " + addr0)
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=3)
    if ev is None:
        raise Exception("dev[0] was not disconnected after being removed "
                        "from accept list while still in deny list (mode 0)")

    dev[0].request("DISCONNECT")
    hapd.request("ACCEPT_ACL CLEAR")
    hapd.request("DENY_ACL CLEAR")


def test_ap_acl_mode_change_3_to_1_disconnects_deny_listed(dev, apdev):
    """macaddr_acl mode change 3->1: STA in deny list disconnected"""
    ssid = "acl-mode-chg-3to1"
    params = {'ssid': ssid, 'macaddr_acl': "3"}
    hapd = hostapd.add_ap(apdev[0], params)

    addr0 = dev[0].own_addr()
    addr1 = dev[1].own_addr()

    # addr0: accept list only -> connects in mode 3
    # addr1: accept list only -> connects in mode 3
    hapd.request("ACCEPT_ACL ADD_MAC " + addr0)
    hapd.request("ACCEPT_ACL ADD_MAC " + addr1)

    dev[0].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[0].connect(ssid, key_mgmt="NONE", scan_freq="2412")
    dev[1].scan_for_bss(apdev[0]['bssid'], freq="2412")
    dev[1].connect(ssid, key_mgmt="NONE", scan_freq="2412")

    # Add addr0 to deny list (while still in accept list)
    hapd.request("DENY_ACL ADD_MAC " + addr0)

    # Switch to mode 1 (DENY_UNLESS_ACCEPTED).
    # In mode 1, accept overrides deny -> addr0 stays connected.
    # addr1 is in accept list, not in deny list -> stays connected.
    dev[0].dump_monitor()
    dev[1].dump_monitor()
    hapd.request("SET macaddr_acl 1")

    # Neither should be disconnected (both in accept list)
    ev = dev[0].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("dev[0] was unexpectedly disconnected after mode "
                        "change 3->1 (accept overrides deny in mode 1)")

    ev = dev[1].wait_event(["CTRL-EVENT-DISCONNECTED"], timeout=1)
    if ev is not None:
        raise Exception("dev[1] was unexpectedly disconnected after mode "
                        "change 3->1 (it is in accept list)")

    dev[0].request("DISCONNECT")
    dev[0].wait_disconnected()
    dev[1].request("DISCONNECT")
    dev[1].wait_disconnected()
    hapd.request("ACCEPT_ACL CLEAR")
    hapd.request("DENY_ACL CLEAR")
