SUMMARY = "Tools for the Linux Standard Wireless Extension Subsystem"
HOMEPAGE = "http://www.hpl.hp.com/personal/Jean_Tourrilhes/Linux/Tools.html"
LICENSE = "GPLv2 & (LGPLv2.1 | MPL-1.1 | BSD)"
LIC_FILES_CHKSUM = "file://COPYING;md5=94d55d512a9ba36caa9b7df079bae19f"
FILESEXTRAPATHS_prepend := "${THISDIR}/patches:"

SECTION = "base"
PE = "1"
PR = "r5"

DEFAULT_PREFERENCE = "-1"

SRC_URI = "http://www.hpl.hp.com/personal/Jean_Tourrilhes/Linux/wireless_tools.${PV}.tar.gz \
	   file://001-debian.patch \
           file://002-fix-iwconfig-power-argument-parsing.patch \
           file://003-we_essential_def.patch \
           file://004-increase_iwlist_buffer.patch \
           file://005-Fix-32-octet-SSID-not-getting-displayed.patch \
           file://006-display-txpower-in-decimal.patch \
           file://007-Fix-invokes-undefined-behavior-warning.patch \
           file://07-support-for-ssid.patch \
           file://100-fix-iwpriv-limit.patch \
           file://101-WAR-to-display-correct-Max-bit-rates.patch \
           file://102-iwlist-scan-timeout.patch \
           file://103-displaying-all-supported-keymgmts-ciphers.patch \
           file://103-Refine-the-description-of-NF-in-iwconfig-command.patch \
          "
SRC_URI[md5sum] = "e06c222e186f7cc013fd272d023710cb"
SRC_URI[sha256sum] = "6fb80935fe208538131ce2c4178221bab1078a1656306bce8909c19887e2e5a1"

S = "${WORKDIR}/wireless_tools.29"
INSANE_SKIP_${PN} += "already-stripped"

CFLAGS =+ "-I${S}"
EXTRA_OEMAKE = "-e 'BUILD_SHARED=y' \
		'INSTALL_DIR=${D}${base_sbindir}' \
		'INSTALL_LIB=${D}${libdir}' \
		'INSTALL_INC=${D}${includedir}' \
		'INSTALL_MAN=${D}${mandir}'"

do_compile() {
	oe_runmake iwmulticall
}

do_install() {
	oe_runmake PREFIX=${D} install-iwmulticall install-dynamic install-hdr
}

PACKAGES = "libiw libiw-dev ${PN} ${PN}-doc ${PN}-dbg"

FILES_libiw = "${libdir}/*.so.*"
FILES_libiw-dev = "${libdir}/*.a ${libdir}/*.so ${includedir}"
FILES_${PN} = "${bindir} ${sbindir}/iw* ${base_sbindir} ${base_bindir} ${sysconfdir}/network"
