# Test cases for process coordination
# Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.

import os
import stat
import subprocess
import tempfile

from hwsim import HWSimRadio
from utils import *

def test_proc_coord_hostapd(dev, apdev, params):
    """Process coordination in hostapd"""
    dir = "/tmp/hostapd-proc-coord"
    os.mkdir(dir)
    os.chmod(dir, stat.S_IRUSR | stat.S_IWUSR | stat.S_IXUSR)

    with HWSimRadio(use_mlo=True, n_channels=2) as (hapd_radio, hapd_iface):
        hapd_iface2 = hapd_iface + '-2'
        cmd = ['iw', 'phy' + str(hapd_radio), 'interface', 'add',
               hapd_iface2, 'type', '__ap']
        proc = subprocess.Popen(cmd, stderr=subprocess.STDOUT,
                                stdout=subprocess.PIPE, shell=False)
        out, err = proc.communicate()
        logger.debug("iw output: " + out.decode())

        fd, fname = tempfile.mkstemp(dir='/tmp', prefix='hostapd-cfg-')
        f = os.fdopen(fd, 'w')
        f.write("driver=nl80211\n")
        f.write("ctrl_interface=/var/run/hostapd\n")
        f.write("hw_mode=g\n")
        f.write("channel=1\n")
        f.write("ieee80211n=1\n")
        f.write("ssid=foo\n")
        f.close()

        pid1 = params['prefix'] + ".hostapd.pid-1"
        cmd = ['../../hostapd/hostapd', '-ddKtB', '-P', pid1, '-f',
               params['prefix'] + ".hostapd-log-1",
               '-i', hapd_iface,
               '-z', dir,
               fname]
        res = subprocess.check_call(cmd)
        if res != 0:
            raise Exception("Could not start hostapd: %s" % str(res))

        pid2 = params['prefix'] + ".hostapd.pid-2"
        cmd = ['../../hostapd/hostapd', '-ddKtB', '-P', pid2, '-f',
               params['prefix'] + ".hostapd-log-2",
               '-i', hapd_iface2,
               '-z', dir,
               fname]
        res = subprocess.check_call(cmd)
        if res != 0:
            raise Exception("Could not start hostapd: %s" % str(res))

        time.sleep(2)
        for i in range(20):
            if os.path.exists(pid1) and os.path.exists(pid2):
                break
            time.sleep(0.2)

        if not (os.path.exists(pid1) and os.path.exists(pid2)):
            raise Exception("hostapd did not create PID file.")

        hapd1 = hostapd.Hostapd(hapd_iface)
        hapd1.ping()

        hapd2 = hostapd.Hostapd(hapd_iface2)
        hapd2.ping()

        if "OK" not in hapd1.request("PROC_COORD_TEST 0"):
            raise Exception("PROC_COORD_TEST failed")

        ev = hapd1.wait_event(["PROC-COORD-TEST"], timeout=5)
        if ev is None:
            raise Exception("hapd1 did not report PROC-COORD-TEST")
        if " RX-RESP " not in ev:
            raise Exception("Unexpected hapd1 PROC-COORD-TEST contents")
        if " msg_len=1" not in ev:
            raise Exception("Unexpected hapd1 PROC-COORD-TEST msg_len")

        ev = hapd2.wait_event(["PROC-COORD-TEST"], timeout=5)
        if ev is None:
            raise Exception("hapd2 did not report PROC-COORD-TEST")
        if " RX " not in ev:
            raise Exception("Unexpected hapd2 PROC-COORD-TEST contents")

        hapd1.request("TERMINATE")
        hapd2.request("TERMINATE")
        time.sleep(1)

    os.rmdir(dir)
