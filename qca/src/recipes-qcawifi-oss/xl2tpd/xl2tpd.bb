SUMMARY = "Xelerance version of the Layer 2 Tunneling Protocol (L2TP) daemon"
SECTION = "network"
DEPENDS = "ppp virtual/kernel"

LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://LICENSE;md5=0636e73ff0215e8d672dc4c32c317bb3"
FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

SRCREV = "5faece38704ae32063abe1d25e199c03e6f44669"
SRC_URI = "git://github.com/xelerance/xl2tpd.git \
	   file://xl2tpd.conf \
	   file://xl2tp-secrets \
	   file://xl2tpd.init \
	   file://l2tp.sh \
	   file://options.xl2tpd \
	   file://100-makefile_opt_flags.patch \
	   file://110-makefile_dont_build_pfc.patch \
	   file://120-no-bsd-signal-in-musl.patch"

S = "${WORKDIR}/git"

inherit update-rc.d

do_compile () {
	oe_runmake CFLAGS="${CFLAGS} -DLINUX" LDFLAGS="${LDFLAGS}" PREFIX="${prefix}" KERNELSRC=${STAGING_KERNEL_DIR} all
}

do_install () {
	install -d ${D}/usr/sbin
	install -m 0755 ${S}/xl2tpd ${D}/usr/sbin
	install -m 0755 ${S}/xl2tpd-control ${D}/usr/sbin

	install -d ${D}/etc/init.d
	install -m 0755 ${WORKDIR}/xl2tpd.init ${D}/etc/init.d/xl2tpd

	install -d ${D}/etc/xl2tpd
	install -m 0755 ${WORKDIR}/xl2tpd.conf ${D}/etc/xl2tpd/xl2tpd.conf
	install -m 0755 ${WORKDIR}/xl2tp-secrets ${D}/etc/xl2tpd/xl2tp-secrets

	install -d ${D}/etc/ppp
	install -m 0755 ${WORKDIR}/options.xl2tpd ${D}/etc/ppp/options.xl2tpd

	install -d ${D}/lib/netifd/proto
	install -m 0755 ${WORKDIR}/l2tp.sh ${D}/lib/netifd/proto/l2tp.sh
}

FILES_${PN} = "/lib/* ${datadir_native}/* ${bindir}/* ${prefix}/* ${sysconfdir}/*"

CONFFILES_${PN} += "${sysconfdir}/xl2tpd.conf ${sysconfdir}/default/xl2tpd"

INITSCRIPT_PACKAGES = "${PN}"
INITSCRIPT_NAME_${PN} = "xl2tpd"
