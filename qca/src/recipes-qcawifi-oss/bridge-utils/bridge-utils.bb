DESCRIPTION = "baridge-utils"
SECTION = "bridge-utils"
LICENSE = "GPLv2"
PR = "r0"
LIC_FILES_CHKSUM = "file://COPYING;md5=f9d20a453221a1b7e32ae84694da2c37"

inherit autotools-brokensep update-alternatives

SRC_URI = "git://source.codeaurora.org/quic/qsdk/bridge-utils;branch=korg/master"
SRCREV = "fd647340f15f868573a119bb38cbe8caa7c59a2c"

SRC_URI[md5sum] = "8a29aaef3c6d32c3a111b005615a1055"

S = "${WORKDIR}/git"

do_install_append() {
	install -d ${D}/usr/sbin
	install -m 0755 ${S}/brctl/brctl ${D}/usr/sbin
}
