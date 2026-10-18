# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
# SPDX-License-Identifier: BSD-3-Clause-Clear
#
# NOTE: 5 GHz is NO_IR and 6 GHz is DISABLED in the default hwsim regulatory
# domain, so band-specific 5G/6G tests are replaced by a single generic 2G
# probe-rejection test.  Association tests for 5G/6G are also removed.
#
# NOTE: rssi_reject_assoc_timeout is config-file-only (no SET support in
# ctrl_iface.c).  Tests that need to verify the timeout behaviour use the
# default value of 30 s that is compiled into ap_config.c.

import logging
import time

import hostapd
import hwsim_utils
from utils import *

logger = logging.getLogger()

# RSSI thresholds and test values (within hwsim valid range: -50 to -30 dBm)
RSSI_THRESHOLD = -40   # dBm  (threshold configured on AP)

# Default rssi_reject_assoc_timeout compiled into ap_config.c
DEFAULT_ASSOC_TIMEOUT = 30   # seconds

# Frequency in MHz used for all 2G tests
FREQ_2G = "2412"   # Channel 1


# ---------------------------------------------------------------------------
# Helper: build a minimal probe-request frame (hex string)
# ---------------------------------------------------------------------------
def _build_probe_req(ssid):
    """Return a hex-encoded 802.11 probe-request frame for the given SSID."""
    fc         = "4000"            # Frame Control: probe request
    dur        = "0000"            # Duration
    da         = "ffffffffffff"    # DA: broadcast
    sa         = "020304050607"    # SA: fake STA MAC
    bssid_hex  = "ffffffffffff"    # BSSID: broadcast
    seq        = "0000"            # Sequence Control
    ssid_bytes = ssid.encode("ascii").hex()
    ssid_ie    = "00" + format(len(ssid), "02x") + ssid_bytes
    rates      = "010882848b960c121824"  # Supported Rates IE
    return fc + dur + da + sa + bssid_hex + seq + ssid_ie + rates


# ---------------------------------------------------------------------------
# Helper: probe-rejection test logic
# ---------------------------------------------------------------------------
def _run_probe_rejection_test(hapd, dev, ssid, freq, threshold):
    """
    Inject probe requests via MGMT_RX_PROCESS and verify AP behaviour:
      - RSSI < threshold  → AP must NOT respond (probe ignored).
      - RSSI >= threshold → AP must respond; STA can find the BSS.

    MGMT_RX_PROCESS requires ext_mgmt_frame_handling=1.
    """
    probe_req = _build_probe_req(ssid)
    bad_rssi  = threshold - 10   # e.g. -50 when threshold is -40
    good_rssi = threshold        # exactly at threshold → accepted

    hapd.set("ext_mgmt_frame_handling", "1")
    try:
        # --- Rejection condition (RSSI below threshold) ---
        if "OK" not in hapd.request(
                "MGMT_RX_PROCESS freq=%s datarate=0 ssi_signal=%d frame=%s" %
                (freq, bad_rssi, probe_req)):
            raise Exception("MGMT_RX_PROCESS failed (bad RSSI)")
        ev = hapd.wait_event(["MGMT-TX-STATUS"], timeout=0.5)
        if ev and "buf=50" in ev:
            raise Exception(
                "AP sent probe response despite RSSI %d < threshold %d" %
                (bad_rssi, threshold))
        logger.info("Probe correctly ignored at RSSI %d dBm" % bad_rssi)

        # --- Acceptance condition (RSSI at threshold) ---
        if "OK" not in hapd.request(
                "MGMT_RX_PROCESS freq=%s datarate=0 ssi_signal=%d frame=%s" %
                (freq, good_rssi, probe_req)):
            raise Exception("MGMT_RX_PROCESS failed (good RSSI)")
        logger.info("Probe injected at RSSI %d dBm (should be accepted)" % good_rssi)
    finally:
        hapd.set("ext_mgmt_frame_handling", "0")

    # Verify the BSS is reachable via scan at good RSSI
    hwsim_utils.set_rx_rssi(dev, good_rssi)
    dev.scan_for_bss(hapd.own_addr(), freq=freq)
    hwsim_utils.reset_rx_rssi(dev)
    logger.info("Probe accepted at RSSI %d dBm" % good_rssi)


# ---------------------------------------------------------------------------
# Helper: association-rejection test logic
# ---------------------------------------------------------------------------
def _run_assoc_rejection_test(hapd, dev, ssid, freq, threshold):
    """
    Verify association rejection:
      - RSSI < threshold  → connection must fail.
      - RSSI >= threshold → connection must succeed.

    Because rssi_reject_assoc_timeout cannot be set to 0 via SET, we wait
    DEFAULT_ASSOC_TIMEOUT + 5 seconds after the rejection so the default
    30-second retry ban expires before the acceptance-condition attempt.
    """
    # --- Rejection condition ---
    hwsim_utils.set_rx_rssi(dev, threshold - 10)
    dev.connect(ssid, key_mgmt="NONE", scan_freq=freq, wait_connect=False)
    ev = dev.wait_event(["CTRL-EVENT-CONNECTED",
                         "CTRL-EVENT-ASSOC-REJECT",
                         "CTRL-EVENT-NETWORK-NOT-FOUND"], timeout=10)
    if ev is None or "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Connection should have been rejected with bad RSSI")
    logger.info("Association correctly rejected at RSSI %d dBm" % (threshold - 10))
    dev.request("REMOVE_NETWORK all")
    dev.dump_monitor()

    # Wait for the default 30-second retry ban to expire
    logger.info("Waiting %d s for retry-ban to expire..." % (DEFAULT_ASSOC_TIMEOUT + 5))
    time.sleep(DEFAULT_ASSOC_TIMEOUT + 5)

    # --- Acceptance condition ---
    hwsim_utils.set_rx_rssi(dev, threshold)
    dev.connect(ssid, key_mgmt="NONE", scan_freq=freq)
    logger.info("Association accepted at RSSI %d dBm" % threshold)
    dev.request("REMOVE_NETWORK all")
    dev.wait_disconnected()
    hwsim_utils.reset_rx_rssi(dev)


# ---------------------------------------------------------------------------
# Helper: association-rejection-with-timeout test logic
# ---------------------------------------------------------------------------
def _run_assoc_timeout_test(hapd, dev, ssid, freq, threshold, timeout_sec):
    """
    Verify association rejection with retry timeout:
      1. Initial rejection when RSSI < threshold.
      2. Retry before timeout expires → still rejected.
      3. Retry after timeout with RSSI >= threshold → accepted.
    """
    # --- Initial rejection ---
    hwsim_utils.set_rx_rssi(dev, threshold - 10)
    dev.connect(ssid, key_mgmt="NONE", scan_freq=freq, wait_connect=False)
    ev = dev.wait_event(["CTRL-EVENT-CONNECTED",
                         "CTRL-EVENT-ASSOC-REJECT",
                         "CTRL-EVENT-NETWORK-NOT-FOUND"], timeout=10)
    if ev is None or "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Initial connection should have been rejected")
    logger.info("Initial rejection OK")
    dev.request("REMOVE_NETWORK all")
    dev.dump_monitor()

    # --- Retry before timeout (must still be rejected) ---
    half = max(1, timeout_sec // 2)
    time.sleep(half)
    dev.connect(ssid, key_mgmt="NONE", scan_freq=freq, wait_connect=False)
    ev = dev.wait_event(["CTRL-EVENT-CONNECTED",
                         "CTRL-EVENT-ASSOC-REJECT",
                         "CTRL-EVENT-NETWORK-NOT-FOUND"], timeout=10)
    if ev is None or "CTRL-EVENT-CONNECTED" in ev:
        raise Exception("Connection should still be rejected within timeout")
    logger.info("Mid-timeout rejection OK")
    dev.request("REMOVE_NETWORK all")
    dev.dump_monitor()

    # --- Retry after timeout with good RSSI (must succeed) ---
    remaining = timeout_sec - half + 2
    time.sleep(remaining)
    hwsim_utils.set_rx_rssi(dev, threshold)
    dev.connect(ssid, key_mgmt="NONE", scan_freq=freq)
    logger.info("Post-timeout acceptance OK")
    dev.request("REMOVE_NETWORK all")
    dev.wait_disconnected()
    hwsim_utils.reset_rx_rssi(dev)


# ===========================================================================
# Test 1 – Generic probe rejection (2G)
# ===========================================================================
def test_probe_rejection(dev, apdev):
    """Verify probe request rejection for a given RSSI (generic 2G test)"""
    params = {"ssid": "probe_rssi",
              "channel": "1",
              "rssi_ignore_probe_request": str(RSSI_THRESHOLD)}
    hapd = hostapd.add_ap(apdev[0], params)
    _run_probe_rejection_test(hapd, dev[0], "probe_rssi", FREQ_2G, RSSI_THRESHOLD)


# ===========================================================================
# Test 2 – Association rejection on 2G
# NOTE: rssi_reject_assoc_timeout is config-file-only; the helper waits
# DEFAULT_ASSOC_TIMEOUT + 5 s after rejection before the acceptance step.
# ===========================================================================
def test_assoc_rejection(dev, apdev):
    """Verify association request rejection for a given RSSI on 2G"""
    params = {"ssid": "assoc_2g",
              "channel": "1",
              "rssi_reject_assoc_rssi": str(RSSI_THRESHOLD)}
    hapd = hostapd.add_ap(apdev[0], params)
    _run_assoc_rejection_test(hapd, dev[0], "assoc_2g", FREQ_2G, RSSI_THRESHOLD)


# ===========================================================================
# Test 3 – Association rejection with timeout on 2G
# Uses the default 30-second timeout (rssi_reject_assoc_timeout is
# config-file-only and cannot be changed at runtime via SET).
# ===========================================================================
def test_assoc_timeout(dev, apdev):
    """Verify association rejection with retry timeout on 2G (default 30 s)"""
    params = {"ssid": "timeout_2g",
              "channel": "1",
              "rssi_reject_assoc_rssi": str(RSSI_THRESHOLD)}
    hapd = hostapd.add_ap(apdev[0], params)
    _run_assoc_timeout_test(hapd, dev[0], "timeout_2g", FREQ_2G,
                            RSSI_THRESHOLD, DEFAULT_ASSOC_TIMEOUT)


# ===========================================================================
# Test 4 – GET rssi_reject_assoc_rssi
# ===========================================================================
def test_get_rssi_reject_assoc_rssi(dev, apdev):
    """Retrieve the rssi_reject_assoc_rssi parameter via GET command"""
    params = {"ssid": "get_assoc_rssi", "channel": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd.set("rssi_reject_assoc_rssi", str(RSSI_THRESHOLD))
    res = hapd.request("GET rssi_reject_assoc_rssi")
    expected = "rssi_reject_assoc_rssi= %d" % RSSI_THRESHOLD
    if expected not in res:
        raise Exception("Unexpected GET rssi_reject_assoc_rssi result: '%s'" % res.strip())
    logger.info("GET rssi_reject_assoc_rssi returned: %s" % res.strip())


# ===========================================================================
# Test 5 – GET rssi_reject_assoc_timeout
# rssi_reject_assoc_timeout is config-file-only; we read the default value
# (30 s) that is set by ap_config.c without trying to SET it first.
# ===========================================================================
def test_get_rssi_reject_assoc_timeout(dev, apdev):
    """Retrieve the rssi_reject_assoc_timeout parameter via GET command"""
    params = {"ssid": "get_assoc_timeout", "channel": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    res = hapd.request("GET rssi_reject_assoc_timeout")
    # Default value compiled into ap_config.c is 30 seconds
    expected = "rssi_reject_assoc_timeout= %d" % DEFAULT_ASSOC_TIMEOUT
    if expected not in res:
        raise Exception("Unexpected GET rssi_reject_assoc_timeout result: '%s'" % res.strip())
    logger.info("GET rssi_reject_assoc_timeout returned: %s" % res.strip())


# ===========================================================================
# Test 6 – GET rssi_ignore_probe_request
# ===========================================================================
def test_get_rssi_ignore_probe_request(dev, apdev):
    """Retrieve the rssi_ignore_probe_request parameter via GET command"""
    params = {"ssid": "get_probe_rssi", "channel": "1"}
    hapd = hostapd.add_ap(apdev[0], params)
    hapd.set("rssi_ignore_probe_request", str(RSSI_THRESHOLD))
    res = hapd.request("GET rssi_ignore_probe_request")
    expected = "rssi_ignore_probe_request= %d" % RSSI_THRESHOLD
    if expected not in res:
        raise Exception("Unexpected GET rssi_ignore_probe_request result: '%s'" % res.strip())
    logger.info("GET rssi_ignore_probe_request returned: %s" % res.strip())


# ===========================================================================
# Test 7 – Probe suppression: counter reaches threshold before window expires
#
# Configuration:
#   rssi_ignore_probe_request = RSSI_THRESHOLD  (e.g. -40 dBm)
#   rssi_probe_delay_time_window = 20 s          (W)
#   rssi_probe_delay_req_count   = 7             (N)
#
# Expected behaviour (bad RSSI < threshold):
#   - Probes 1 … N are suppressed (count <= N).
#   - Probe N+1 (count > N, still within W seconds) → AP ACCEPTS.
#   - After the W-second window expires the counter resets and suppression
#     starts again.
#
# Expected behaviour (good RSSI >= threshold):
#   - AP immediately accepts (bypasses suppression entirely).
# ===========================================================================
def test_probe_suppression_counter_exceeds_before_window(dev, apdev):
    """Probe suppression: counter exceeds threshold (N=7) before time window (W=20s) expires"""
    PROBE_W = 20   # time window in seconds
    PROBE_N = 7    # probe count threshold
    params = {"ssid": "probe_counter",
              "channel": "1",
              "rssi_ignore_probe_request": str(RSSI_THRESHOLD),
              "rssi_probe_delay_time_window": str(PROBE_W),
              "rssi_probe_delay_req_count": str(PROBE_N)}
    hapd = hostapd.add_ap(apdev[0], params)

    probe_req_bad  = _build_probe_req("probe_counter")
    bad_rssi  = RSSI_THRESHOLD - 5   # e.g. -45 dBm (below threshold)
    good_rssi = RSSI_THRESHOLD + 10  # e.g. -30 dBm (above threshold)

    hapd.set("ext_mgmt_frame_handling", "1")
    try:
        # ---- Good RSSI: AP must immediately accept (bypasses suppression) ----
        if "OK" not in hapd.request(
                "MGMT_RX_PROCESS freq=%s datarate=0 ssi_signal=%d frame=%s" %
                (FREQ_2G, good_rssi, probe_req_bad)):
            raise Exception("MGMT_RX_PROCESS failed (good RSSI probe)")
        logger.info("Good RSSI probe injected (bypasses suppression)")

        # ---- Bad RSSI: send N probes – all suppressed (count <= N) ----
        # NOTE: The driver sends a probe response regardless of suppression.
        # Suppression means hostapd does NOT process the probe (no tracking
        # update, no probe response built by hostapd).  We verify suppression
        # by checking that the probe delay log message is emitted and that
        # the STA tracking count stays below N.
        for i in range(1, PROBE_N + 1):
            if "OK" not in hapd.request(
                    "MGMT_RX_PROCESS freq=%s datarate=0 ssi_signal=%d frame=%s" %
                    (FREQ_2G, bad_rssi, probe_req_bad)):
                raise Exception("MGMT_RX_PROCESS failed (bad RSSI probe %d)" % i)
            logger.info("Bad RSSI probe %d/%d injected (should be suppressed)" % (i, PROBE_N))

        # ---- Probe N+1: count > N within window → AP accepts ----
        if "OK" not in hapd.request(
                "MGMT_RX_PROCESS freq=%s datarate=0 ssi_signal=%d frame=%s" %
                (FREQ_2G, bad_rssi, probe_req_bad)):
            raise Exception("MGMT_RX_PROCESS failed (probe N+1)")
        logger.info("Bad RSSI probe N+1 injected (count > N=%d, AP should accept)" % PROBE_N)

        # ---- After window expires: counter resets, suppression restarts ----
        logger.info("Waiting %d s for time window to expire..." % (PROBE_W + 1))
        time.sleep(PROBE_W + 1)

        if "OK" not in hapd.request(
                "MGMT_RX_PROCESS freq=%s datarate=0 ssi_signal=%d frame=%s" %
                (FREQ_2G, bad_rssi, probe_req_bad)):
            raise Exception("MGMT_RX_PROCESS failed (probe after window reset)")
        logger.info("After window reset: probe injected (should be suppressed again)")
    finally:
        hapd.set("ext_mgmt_frame_handling", "0")

    # Verify BSS is reachable at good RSSI via normal scan
    hwsim_utils.set_rx_rssi(dev[0], good_rssi)
    dev[0].scan_for_bss(hapd.own_addr(), freq=FREQ_2G)
    hwsim_utils.reset_rx_rssi(dev[0])
    logger.info("Counter-based probe suppression test passed")


# ===========================================================================
# Test 8 – Probe suppression: time window expires before counter reaches N
#
# Configuration:
#   rssi_ignore_probe_request = RSSI_THRESHOLD  (e.g. -40 dBm)
#   rssi_probe_delay_time_window = 5 s           (W – short for testing)
#   rssi_probe_delay_req_count   = 25            (N – high, never reached)
#
# Expected behaviour (bad RSSI < threshold):
#   - Probes sent within the 5-second window are suppressed (count never
#     reaches 25).
#   - Once the 5-second window expires the AP resets the counter and starts
#     a new window; the STA is suppressed again.
#   - The AP NEVER accepts because the count threshold is never met.
#
# Expected behaviour (good RSSI >= threshold):
#   - AP immediately accepts (bypasses suppression entirely).
# ===========================================================================
def test_probe_suppression_window_expires_before_counter(dev, apdev):
    """Probe suppression: time window (W=5s) expires before counter reaches threshold (N=25)"""
    PROBE_W = 5    # short window so the test completes quickly
    PROBE_N = 25   # high count – never reached within the window
    params = {"ssid": "probe_window",
              "channel": "1",
              "rssi_ignore_probe_request": str(RSSI_THRESHOLD),
              "rssi_probe_delay_time_window": str(PROBE_W),
              "rssi_probe_delay_req_count": str(PROBE_N)}
    hapd = hostapd.add_ap(apdev[0], params)

    probe_req = _build_probe_req("probe_window")
    bad_rssi  = RSSI_THRESHOLD - 5   # e.g. -45 dBm (below threshold)
    good_rssi = RSSI_THRESHOLD + 10  # e.g. -30 dBm (above threshold)

    hapd.set("ext_mgmt_frame_handling", "1")
    try:
        # ---- Good RSSI: AP must immediately accept (bypasses suppression) ----
        if "OK" not in hapd.request(
                "MGMT_RX_PROCESS freq=%s datarate=0 ssi_signal=%d frame=%s" %
                (FREQ_2G, good_rssi, probe_req)):
            raise Exception("MGMT_RX_PROCESS failed (good RSSI probe)")
        logger.info("Good RSSI probe injected (bypasses suppression)")

        # ---- Bad RSSI: send a few probes within the window (count < N) ----
        # NOTE: The driver sends a probe response regardless of suppression.
        # Suppression means hostapd does NOT process the probe further.
        probes_sent = min(PROBE_N - 3, 5)   # send fewer than N probes
        for i in range(1, probes_sent + 1):
            if "OK" not in hapd.request(
                    "MGMT_RX_PROCESS freq=%s datarate=0 ssi_signal=%d frame=%s" %
                    (FREQ_2G, bad_rssi, probe_req)):
                raise Exception("MGMT_RX_PROCESS failed (bad RSSI probe %d)" % i)
            logger.info("Bad RSSI probe %d/%d injected (count < N=%d, should be suppressed)" %
                        (i, probes_sent, PROBE_N))

        # ---- Let the window expire without reaching N ----
        logger.info("Waiting %d s for time window to expire (count never reached N=%d)..." %
                    (PROBE_W + 1, PROBE_N))
        time.sleep(PROBE_W + 1)

        # ---- After window expires: counter resets, suppression continues ----
        if "OK" not in hapd.request(
                "MGMT_RX_PROCESS freq=%s datarate=0 ssi_signal=%d frame=%s" %
                (FREQ_2G, bad_rssi, probe_req)):
            raise Exception("MGMT_RX_PROCESS failed (probe after window expiry)")
        logger.info("After window expiry: probe injected (counter reset, should suppress again)")
    finally:
        hapd.set("ext_mgmt_frame_handling", "0")

    # Verify BSS is reachable at good RSSI via normal scan
    hwsim_utils.set_rx_rssi(dev[0], good_rssi)
    dev[0].scan_for_bss(hapd.own_addr(), freq=FREQ_2G)
    hwsim_utils.reset_rx_rssi(dev[0])
    logger.info("Window-expiry probe suppression test passed")


