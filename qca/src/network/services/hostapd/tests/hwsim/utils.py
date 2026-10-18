# Testing utilities
# Copyright (c) 2013-2019, Jouni Malinen <j@w1.fi>
#
# This software may be distributed under the terms of the BSD license.
# See README for more details.

import binascii
import os
import socket
import struct
import subprocess
import time
import remotehost
import logging
import re
logger = logging.getLogger()
import hostapd

def get_ifnames():
    ifnames = []
    with open("/proc/net/dev", "r") as f:
        lines = f.readlines()
        for l in lines:
            val = l.split(':', 1)
            if len(val) == 2:
                ifnames.append(val[0].strip(' '))
    return ifnames

class HwsimSkip(Exception):
    def __init__(self, reason):
        self.reason = reason
    def __str__(self):
        return self.reason

def long_duration_test(func):
    func.long_duration_test = True
    return func

class fail_test(object):
    _test_fail = 'TEST_FAIL'
    _get_fail = 'GET_FAIL'

    def __init__(self, dev, count, funcs, *args):
        self._dev = dev
        self._funcs = [(count, funcs)]

        args = list(args)
        while args:
            count = args.pop(0)
            funcs = args.pop(0)
            self._funcs.append((count, funcs))
    def __enter__(self):
        patterns = ' '.join(['%d:%s' % (c, f) for c, f in self._funcs])
        cmd = '%s %s' % (self._test_fail, patterns)
        if "OK" not in self._dev.request(cmd):
            raise HwsimSkip("TEST_FAIL not supported")
    def __exit__(self, type, value, traceback):
        pending = self._dev.request(self._get_fail)
        if type is None:
            expected = ' '.join(['0:%s' % f for c, f in self._funcs])
            if pending != expected:
                # Ensure the failure cannot trigger in the future
                self._dev.request('%s 0:' % self._test_fail)
                raise Exception("Not all failures triggered (pending: %s)" % pending)
        else:
            logger.info("Pending failures at time of exception: %s" % pending)

class alloc_fail(fail_test):
    _test_fail = 'TEST_ALLOC_FAIL'
    _get_fail = 'GET_ALLOC_FAIL'

def wait_fail_trigger(dev, cmd, note="Failure not triggered", max_iter=40,
		      timeout=0.05):
    for i in range(0, max_iter):
        if dev.request(cmd).startswith("0:"):
            break
        if i == max_iter - 1:
            raise Exception(note)
        time.sleep(timeout)

def require_under_vm():
    if os.getenv('VM') != 'VM':
        raise HwsimSkip("Not running under VM")

def iface_is_in_bridge(bridge, ifname):
    fname = "/sys/class/net/"+ifname+"/brport/bridge"
    if not os.path.exists(fname):
        return False
    if not os.path.islink(fname):
        return False
    truebridge = os.path.basename(os.readlink(fname))
    if bridge == truebridge:
        return True
    return False

def skip_with_fips(dev, reason="Not supported in FIPS mode"):
    res = dev.get_capability("fips")
    if res and 'FIPS' in res:
        raise HwsimSkip(reason)

def check_ext_key_id_capa(dev):
    res = dev.get_driver_status_field('capa.flags')
    if (int(res, 0) & 0x8000000000000000) == 0:
        raise HwsimSkip("Extended Key ID not supported")

def skip_without_tkip(dev):
    res = dev.get_capability("fips")
    if "TKIP" not in dev.get_capability("pairwise") or \
       "TKIP" not in dev.get_capability("group"):
        raise HwsimSkip("Cipher TKIP not supported")

def check_wep_capa(dev):
    if "WEP40" not in dev.get_capability("group"):
        raise HwsimSkip("WEP not supported")

def check_sae_capab(dev):
    if "SAE" not in dev.get_capability("auth_alg"):
        raise HwsimSkip("SAE not supported")

def check_sae_pk_capab(dev):
    capab = dev.get_capability("sae")
    if capab is None or "PK" not in capab:
        raise HwsimSkip("SAE-PK not supported")

def check_owe_capab(dev):
    if "OWE" not in dev.get_capability("key_mgmt"):
        raise HwsimSkip("OWE not supported")

def check_erp_capa(dev):
    capab = dev.get_capability("erp")
    if not capab or 'ERP' not in capab:
        raise HwsimSkip("ERP not supported in the build")

def check_fils_capa(dev):
    capa = dev.get_capability("fils")
    if capa is None or "FILS" not in capa:
        raise HwsimSkip("FILS not supported")

def check_fils_sk_pfs_capa(dev):
    capa = dev.get_capability("fils")
    if capa is None or "FILS-SK-PFS" not in capa:
        raise HwsimSkip("FILS-SK-PFS not supported")

def check_imsi_privacy_support(dev):
    tls = dev.request("GET tls_library")
    if tls.startswith("OpenSSL"):
        return
    raise HwsimSkip("IMSI privacy not supported with this TLS library: " + tls)

def check_tls_tod(dev):
    tls = dev.request("GET tls_library")
    if not tls.startswith("OpenSSL") and \
       not tls.startswith("wolfSSL") and \
       not tls.startswith("internal") and \
       not tls.startswith("mbed TLS"):
        raise HwsimSkip("TLS TOD-TOFU/STRICT not supported with this TLS library: " + tls)

def vht_supported():
    cmd = subprocess.Popen(["iw", "reg", "get"], stdout=subprocess.PIPE)
    out, err = cmd.communicate()
    reg = out.decode()
    if "@ 80)" in reg or "@ 160)" in reg:
        return True
    return False

def eht_320mhz_supported():
    cmd = subprocess.Popen(["iw", "reg", "get"], stdout=subprocess.PIPE)
    out, err = cmd.communicate()
    reg = out.decode()
    if "@ 320)" in reg:
        return True
    return False

def he_6ghz_supported(freq=5975):
    cmd = subprocess.Popen(["iw", "reg", "get"],
                           stdout=subprocess.PIPE)
    out, err = cmd.communicate()
    reg_rules = out.decode().splitlines()
    for rule in reg_rules:
        m = re.search(r"\s*\(\d+\s*-\s*\d+", rule)
        if not m:
            continue
        freqs = re.findall(r"\d+", m.group(0))
        if int(freqs[0]) <= freq and freq <= int(freqs[1]):
            return True

    return False

# This function checks whether the provided dev, which may be either
# WpaSupplicant or Hostapd supports CSA.
def csa_supported(dev):
    res = dev.get_driver_status()
    if (int(res['capa.flags'], 0) & 0x80000000) == 0:
        raise HwsimSkip("CSA not supported")

def get_phy(ap, ifname=None):
    phy = "phy3"
    try:
        hostname = ap['hostname']
    except:
        hostname = None
    host = remotehost.Host(hostname)

    if ifname == None:
        ifname = ap['ifname']
    status, buf = host.execute(["iw", "dev", ifname, "info"])
    if status != 0:
        raise Exception("iw " + ifname + " info failed")
    lines = buf.split("\n")
    for line in lines:
        if "wiphy" in line:
            words = line.split()
            phy = "phy" + words[1]
            break
    return phy

def iw_dev_ssid(dev, ifname):
    buf = iw_dev_info(dev, ifname)
    return iw_dev_ssid_from_info(buf)

def iw_dev(dev):
    status, buf = dev.cmd_execute(["iw", "dev"])
    if status != 0:
        raise Exception("iw dev failed")
    return buf

def log_iw_dev(dev, note=None, buf=None):
    if note:
        logger.info("iw dev: %s", note)
    if buf is None:
        try:
            buf = iw_dev(dev)
        except Exception as e:
            logger.info("iw dev: <failed: %s>", str(e))
            return
    lines = buf.splitlines()
    if not lines:
        logger.info("iw dev: <empty>")
        return
    for line in lines:
        logger.info("iw dev: %s", line)

def iw_dev_info(dev, ifname):
    status, buf = dev.cmd_execute(["iw", "dev", ifname, "info"])
    if status != 0:
        raise Exception("iw dev %s info failed" % ifname)
    return buf

def iw_dev_ssid_from_info(buf):
    for line in buf.splitlines():
        line = line.strip()
        if line.startswith("ssid "):
            return line[5:]
    return None

def log_iw_dev_info(dev, ifname, buf=None):
    if buf is None:
        buf = iw_dev_info(dev, ifname)
    lines = buf.splitlines()
    if not lines:
        logger.info("iw dev %s info: <empty>", ifname)
        return
    for line in lines:
        logger.info("iw dev %s info: %s", ifname, line)

def wait_iw_dev_ssid(dev, ifname, ssid, timeout=2):
    logger.info("iw dev %s info: wait ssid=%s", ifname, ssid)
    buf = iw_dev_info(dev, ifname)
    log_iw_dev_info(dev, ifname, buf)
    start = os.times()[4]
    while True:
        cur = iw_dev_ssid_from_info(buf)
        if cur == ssid:
            log_iw_dev_info(dev, ifname, buf)
            logger.info("iw dev %s info: ssid=%s", ifname, cur)
            return
        if os.times()[4] > start + timeout:
            log_iw_dev_info(dev, ifname, buf)
            raise Exception("iw dev %s info did not report SSID %s (got: %s)" %
                            (ifname, ssid, str(cur)))
        time.sleep(0.05)
        buf = iw_dev_info(dev, ifname)

def wait_iw_dev_no_ssid(dev, ifname, timeout=2):
    logger.info("iw dev %s info: wait ssid=None", ifname)
    buf = iw_dev_info(dev, ifname)
    log_iw_dev_info(dev, ifname, buf)
    start = os.times()[4]
    while True:
        cur = iw_dev_ssid_from_info(buf)
        if cur is None:
            log_iw_dev_info(dev, ifname, buf)
            logger.info("iw dev %s info: ssid=None", ifname)
            return
        if os.times()[4] > start + timeout:
            log_iw_dev_info(dev, ifname, buf)
            raise Exception("iw dev %s info still reports SSID %s" %
                            (ifname, cur))
        time.sleep(0.05)
        buf = iw_dev_info(dev, ifname)

def parse_ie(buf):
    ret = {}
    data = binascii.unhexlify(buf)
    while len(data) >= 2:
        ie, elen = struct.unpack('BB', data[0:2])
        data = data[2:]
        if elen > len(data):
            break
        ret[ie] = data[0:elen]
        data = data[elen:]
    return ret

def wait_regdom_changes(dev):
    for i in range(10):
        ev = dev.wait_event(["CTRL-EVENT-REGDOM-CHANGE"], timeout=0.1)
        if ev is None:
            break

def clear_country(dev):
    logger.info("Try to clear country")
    id = dev[1].add_network()
    dev[1].set_network(id, "mode", "2")
    dev[1].set_network_quoted(id, "ssid", "country-clear")
    dev[1].set_network(id, "key_mgmt", "NONE")
    dev[1].set_network(id, "frequency", "2412")
    dev[1].set_network(id, "scan_freq", "2412")
    dev[1].select_network(id)
    ev = dev[1].wait_event(["CTRL-EVENT-CONNECTED"])
    if ev:
        dev[0].connect("country-clear", key_mgmt="NONE", scan_freq="2412")
        dev[1].request("DISCONNECT")
        dev[0].wait_disconnected()
        dev[0].request("DISCONNECT")
        dev[0].request("ABORT_SCAN")
        time.sleep(1)
        dev[0].dump_monitor()
        dev[1].dump_monitor()

def clear_regdom(hapd, dev, count=1):
    disable_hapd(hapd)
    clear_regdom_dev(dev, count)

def disable_hapd(hapd):
    if hapd:
        hapd.request("DISABLE")
        time.sleep(0.1)

def clear_regdom_dev(dev, count=1):
    for i in range(count):
        dev[i].request("DISCONNECT")
    for i in range(count):
        dev[i].disconnect_and_stop_scan()
    dev[0].cmd_execute(['iw', 'reg', 'set', '00'])
    wait_regdom_changes(dev[0])
    country = dev[0].get_driver_status_field("country")
    logger.info("Country code at the end: " + country)
    if country != "00":
        clear_country(dev)
    for i in range(count):
        dev[i].flush_scan_cache()

def radiotap_build():
    radiotap_payload = struct.pack('BB', 0x08, 0)
    radiotap_payload += struct.pack('BB', 0, 0)
    radiotap_payload += struct.pack('BB', 0, 0)
    radiotap_hdr = struct.pack('<BBHL', 0, 0, 8 + len(radiotap_payload),
                               0xc002)
    return radiotap_hdr + radiotap_payload

def start_monitor(ifname, freq=2412):
    subprocess.check_call(["iw", ifname, "set", "type", "monitor"])
    subprocess.call(["ip", "link", "set", "dev", ifname, "up"])
    subprocess.check_call(["iw", ifname, "set", "freq", str(freq)])

    ETH_P_ALL = 3
    sock = socket.socket(socket.AF_PACKET, socket.SOCK_RAW,
                         socket.htons(ETH_P_ALL))
    sock.bind((ifname, 0))
    sock.settimeout(0.5)
    return sock

def stop_monitor(ifname):
    subprocess.call(["ip", "link", "set", "dev", ifname, "down"])
    subprocess.call(["iw", ifname, "set", "type", "managed"])

def clear_scan_cache(apdev):
    ifname = apdev['ifname']
    hostapd.cmd_execute(apdev, ['ifconfig', ifname, 'up'])
    hostapd.cmd_execute(apdev, ['iw', ifname, 'scan', 'trigger', 'freq', '2412',
                                'flush'])
    time.sleep(0.1)
    hostapd.cmd_execute(apdev, ['ifconfig', ifname, 'down'])

def set_world_reg(apdev0=None, apdev1=None, dev0=None):
    if apdev0:
        hostapd.cmd_execute(apdev0, ['iw', 'reg', 'set', '00'])
    if apdev1:
        hostapd.cmd_execute(apdev1, ['iw', 'reg', 'set', '00'])
    if dev0:
        dev0.cmd_execute(['iw', 'reg', 'set', '00'])
    time.sleep(0.1)

def sysctl_write(val):
    subprocess.call(['sysctl', '-w', val], stdout=open('/dev/null', 'w'))

def var_arg_call(fn, dev, apdev, params):
    if fn.__code__.co_argcount > 2:
        return fn(dev, apdev, params)
    elif fn.__code__.co_argcount > 1:
        return fn(dev, apdev)
    return fn(dev)

def cloned_wrapper(wrapper, fn):
    # we need the name set right for selecting / printing etc.
    wrapper.__name__ = fn.__name__
    wrapper.__doc__ = fn.__doc__
    # reparent to the right module for module filtering
    wrapper.__module__ = fn.__module__
    return wrapper

def disable_ipv6(fn):
    def wrapper(dev, apdev, params):
        require_under_vm()
        try:
            sysctl_write('net.ipv6.conf.all.disable_ipv6=1')
            sysctl_write('net.ipv6.conf.default.disable_ipv6=1')
            var_arg_call(fn, dev, apdev, params)
        finally:
            sysctl_write('net.ipv6.conf.all.disable_ipv6=0')
            sysctl_write('net.ipv6.conf.default.disable_ipv6=0')
    return cloned_wrapper(wrapper, fn)

def parse_bool(s):
    # Try parsing as integer of any base (expected 10 or 16),
    # if that fails, try "True"/"False" literals
    s = s.strip()
    try:
        return bool(int(s, 0))
    except ValueError as e:
        if s == 'True':
            return True
        elif s == 'False':
            return False
        else:
            raise e
