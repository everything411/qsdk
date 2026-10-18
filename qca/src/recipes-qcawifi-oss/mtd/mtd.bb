DESCRIPTION = "mtd utility"
SECTION = "utils"
LICENSE = "GPL-2.0+"

LIC_FILES_CHKSUM = "file://mtd.h;md5=b812bd1a7340e5de71994213d6e2f647"
PR = "r0"

DEPENDS = "libubox"

inherit autotools

SRC_URI = "file://build"

S = "${WORKDIR}/build"

do_compile() {
	make -f ${S}/Makefile mtd
}

do_install() {
	install -d ${D}/sbin
	install -m 0755 mtd ${D}/sbin/mtd
}

FILES_${PN} += "/sbin/*"
BBCLASSEXTEND += "native"
