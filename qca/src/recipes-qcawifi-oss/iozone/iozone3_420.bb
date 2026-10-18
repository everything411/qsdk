DESCRIPTION = "iozone"
SECTION = "iozone"
LICENSE = "iozone3"
LIC_FILES_CHKSUM = "file://iozone.c;beginline=266;endline=270;md5=f66ce2354f5c5fe44162a95418331f71"

SRC_URI = "http://www.iozone.org/src/current/${BPN}_${PV}.tar"

SRC_URI[md5sum] = "5205cd571c6e68440772f7e0af0712d6"

S = "${WORKDIR}/iozone3_420/src/current"

EXTRA_OEMAKE_arm = "linux-arm CC='${CC}' GCC='${CC}'"
EXTRA_OEMAKE = "linux CC='${CC}' GCC='${CC}'"

TARGET_CC_ARCH += "${LDFLAGS}"

do_install() {
	install -d ${D}/usr/sbin
	install -m 0755 ${S}/iozone ${D}/usr/sbin/
}
