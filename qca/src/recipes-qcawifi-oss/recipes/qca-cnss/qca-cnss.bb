DESCRIPTION = "QCA CNSS Wifi Platform Driver"

FILESPATH =+ "${WORKSPACE}:"
FILESEXTRAPATHS_prepend := "${THISDIR}/files:"

LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://main.c;\
beginline=3;endline=11;md5=9a875901b222235fd4cff01d8736ab90"

inherit autotools-brokensep module qperf

do_unpack[deptask] = "do_populate_sysroot"
DEPENDS = "virtual/kernel"
SRC_URI = "file://ipq/qca-cnss"

S = "${WORKDIR}/ipq/qca-cnss"

FILES_${PN}+="/usr/lib/modules/${KERNEL_VERSION}/extra/"

EXTRA_OEMAKE += "TARGET_SUPPORT=${BASEMACHINE}"

EXTRA_OECONF += "--with-kernel-src=${STAGING_KERNEL_DIR} \
                 --with-kernel=${STAGING_KERNEL_BUILDDIR} \
                 --with-sanitized-headers=${STAGING_KERNEL_BUILDDIR}/usr/include"

export CONFIG_BUILD_YOCTO="y"

do_install_append() {
	install -m 0755 -d ${D}/usr/include
	cp ${S}/include/cnss2.h ${D}/usr/include/
	install -d ${D}${nonarch_base_libdir}/modules/${KERNEL_VERSION}/extra
	install -m 0755 ${S}/ipq_cnss2.ko ${D}/${nonarch_base_libdir}/modules/${KERNEL_VERSION}/extra/
}
