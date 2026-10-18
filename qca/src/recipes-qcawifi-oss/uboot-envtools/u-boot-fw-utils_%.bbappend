LICENSE = "GPLv2+"

FILESEXTRAPATHS_prepend := "${THISDIR}/files:"
SRC_URI += "file://config_files"

do_configure_append() {
	touch ${S}/configs/${UBOOT_MACHINE}
}

do_install_append() {
	rm ${D}${sysconfdir}/fw_env.config
	install -d ${D}/lib/
	install -m 0755 ${WORKDIR}/config_files/uboot-envtools.sh ${D}/lib/
	install -d ${D}/etc/uci-defaults/
	install -m 0755 ${WORKDIR}/config_files/ipq806x ${D}/etc/uci-defaults/30_uboot-envtools
}

FILES_${PN} += "${libdir}/* ${baselib}/* ${sysconfdir}/*"
