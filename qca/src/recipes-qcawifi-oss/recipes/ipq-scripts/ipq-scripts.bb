DESCRIPTION = "OpenWRT scripts needed for IPQ"

LICENSE = "ISC"
PR = "r0"
LIC_FILES_CHKSUM = "file://etc/init.d/ipq-boot;md5=634dd0d9fe3681b22d4676433df6a594"

inherit pkgconfig

FILESEXTRAPATHS_prepend := "${THISDIR}/:"
SRC_URI = "file://files"

S = "${WORKDIR}/files"

do_install_prepend() {
    sed -i "s/%PATH%/\/usr\/sbin:\/usr\/bin:\/sbin:\/bin/g" ${WORKDIR}/files/sbin/hotplug-call
}

do_install() {
    install -d ${D}/lib
    install -d ${D}/lib/functions
    install -d ${D}/sbin
    install -d ${D}/etc/uci-defaults/
    install -d ${D}/etc/init.d/

    install -m 0755 ${WORKDIR}/files/lib/ipq806x.sh ${D}/lib/
    install -m 0755 ${WORKDIR}/files/lib/functions.sh ${D}/lib/
    install -m 0755 ${WORKDIR}/files/lib/functions/service.sh ${D}/lib/functions/
    install -m 0755 ${WORKDIR}/files/lib/functions/uci-defaults.sh ${D}/lib/functions/
    install -m 0755 ${WORKDIR}/files/sbin/wifi ${D}/sbin/
    install -m 0755 ${WORKDIR}/files/sbin/hotplug-call ${D}/sbin/
    install -m 0755 ${WORKDIR}/files/etc/rc.common ${D}/etc/rc.common
    install -m 0755 ${WORKDIR}/files/etc/uci-defaults/network ${D}/etc/uci-defaults/
    install -m 0755 ${WORKDIR}/files/etc/init.d/ipq-boot ${D}/etc/init.d/
    install -m 0755 ${WORKDIR}/files/etc/init.d/network ${D}/etc/init.d/
}

FILES_${PN} += "/lib/* /lib/functions/* /sbin/* /etc/uci-defaults/* /etc/init.d/*"
