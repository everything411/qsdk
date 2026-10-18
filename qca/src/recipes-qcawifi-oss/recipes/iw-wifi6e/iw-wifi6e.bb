DESCRIPTION = "cfg80211 interface configuration utility"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://COPYING;md5=878618a5c4af25e9b93ef0be1a93f774"

FILESEXTRAPATHS_prepend := "${THISDIR}/:"

PV = "5.9"
SRC_URI = "https://www.kernel.org/pub/software/network/iw/iw-${PV}.tar.xz \
	   file://patches \
	   "
SRC_URI[md5sum] = "7a47d6f749ec69afcaf351166fd12f6f"
SRC_URI[sha256sum] = "293a07109aeb7e36267cf59e3ce52857e9ffae3a6666eb8ac77894b1839fe1f2"

DEPENDS = "libnl"
S = "${WORKDIR}/iw-${PV}"

TARGET_CFLAGS += "-fpie -Wall -Werror"
TARGET_LDFLAGS += "-pie -L${STAGING_LIBDIR}"
TARGET_CPPFLAGS = "-I${STAGING_INCDIR}/libnl3 \
		   ${TAGET_CPPFLAGS} \
		   -DCONFIG_LIBNL20 \
		   -D_GNU_SOURCE"

inherit pkgconfig

do_configure() {
	echo "const char iw_version[] = \"${PV}\";" > ${S}/version.c
	rm -f ${S}/version.sh
	touch ${S}/version.sh
	chmod +x ${S}/version.sh
}

do_patch() {
	ls ${WORKDIR}/patches | xargs -I % sh -c 'patch -d${S} -p1 < ${WORKDIR}/patches/%'
}

do_compile_prepend() {
	CFLAGS="${TARGET_CPPFLAGS} ${TARGET_CFLAGS} -ffunction-sections -fdata-sections" \
	LDFLAGS="${TARGET_LDFLAGS} -Wl,--gc-sections" \
	export NL1FOUND="" \
	export NL2FOUND="" \
	export NL3FOUND="" \
	export NL31FOUND="" \
	export NL3xFOUND="Y" \
	export NLLIBNAME="libnl3" \
	LIBS="-lm -lnl-3" \
	export V=1
}

do_install() {
	install -d ${D}/usr/sbin
	install -m 0755 ${S}/iw ${D}/usr/sbin/
}

FILES_${PN} += "/usr/sbin/*"
