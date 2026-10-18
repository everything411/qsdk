DESCRIPTION = "UPX Compression Tool"
SECTION = "utils"
LICENSE = "GPL"
PR = "r0"
LIC_FILES_CHKSUM = "file://README;beginline=106;endline=116;md5=684fc1f3d04826f36a54dc910bd412e0"

SRC_URI = "https://mirrors.edge.kernel.org/caf_mirrored_source/quic/qsdk/upx-${PV}-amd64_linux.tar.xz"

SRC_URI[md5sum] = "78a9650320a850868fc46cbb59a9150a"
SRC_URI[sha256sum] = "ac75f5172c1c530d1b5ce7215ca9e94586c07b675a26af3b97f8421b8b8d413d"

S = "${WORKDIR}/upx-${PV}-amd64_linux"

inherit native

do_install() {
	install -d ${D}${bindir_nativesdk}
	install -m 0755 ${S}/upx ${D}${bindir_nativesdk}
}

FILES_${PN} += "${bindir_nativesdk}/upx"
FILES_${PN} += "${SYSROOT_DESTDIR}${bindir_crossscripts}/upx"

sysroot_stage_all_append() {
	install -d ${SYSROOT_DESTDIR}${bindir_crossscripts}
	install -m 0755 ${D}${bindir_nativesdk}/upx ${SYSROOT_DESTDIR}${bindir_crossscripts}
}
