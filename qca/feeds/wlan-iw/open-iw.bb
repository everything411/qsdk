SUMMARY = "cfg80211 interface configuration utility"
HOMEPAGE = "http://wireless.kernel.org/en/users/Documentation/iw"
LICENSE = "ISC"
SECTION = "console/network"
LIC_FILES_CHKSUM = "file://COPYING;md5=878618a5c4af25e9b93ef0be1a93f774"

PKG_NAME = "iw"
PKG_VERSION = "5.19"


FILESEXTRAPATHS_append := "${THISDIR}/package/network/utils/iw/patches:"
SRC_URI = "https://www.kernel.org/pub/software/network/iw/${PKG_NAME}-${PKG_VERSION}.tar.xz"

SRC_URI[md5sum] = "fd17ca2dd5f160a5d9e5fd3f8a69f416"
S = "${WORKDIR}/${PKG_NAME}-${PKG_VERSION}"
inherit pkgconfig

DEPENDS += "libnl"
TARGET_CFLAGS = " -fpie -Wall -Werror -O2 -Wno-sign-compare "
TARGET_LDFLAGS = "-pie"
EXTRA_OEMAKE = ""

LC_ALL = "C"

do_patch() {
	ls ${THISDIR}/package/network/utils/iw/patches | xargs -I % sh -c 'echo "applying patch "%;patch -d${S} -f -p1 < ${THISDIR}/package/network/utils/iw/patches/%'
}

do_compile() {
	echo "patch is done"
	sed -i 's/-Wsign-compare//g' ${S}/Makefile
	make -C ${S} V=1
}

do_configure() {
	echo "const char iw_version[] = \"${PKG_VERSION}\";" > ${WORKDIR}/${PKG_NAME}-${PKG_VERSION}/version.c
	rm -f ${WORKDIR}/${PKG_NAME}-${PKG_VERSION}/version.sh
	touch ${WORKDIR}/${PKG_NAME}-${PKG_VERSION}/version.sh
	chmod +x ${WORKDIR}/${PKG_NAME}-${PKG_VERSION}/version.sh

}
FILES_${PN} += "/usr/sbin/*"

do_install() {
	echo "after compile"
	install -m 0755 -d ${D}/usr/sbin

	cp ${S}/iw ${D}/usr/sbin/
}
