SUMMARY = "Tools for managining ART packet filtering"
DESCRIPTION = " Arptables is a User space tool used to setup and maintain ART rules in kernel"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://arptables-standalone.c;md5=8f0e2eff46c02fff23add1ca3a7f8160"
RDEPENDS_${PN} = "bash perl"

inherit update-alternatives

S = "${WORKDIR}/arptables-v0.0.4"


EXTRA_OEMAKE = "\
        'CFLAGS=${CFLAGS} -I${S}/include -DARPTABLES_VERSION=\"0.0.4\"' 'BUILDDIR=${S}'"

SRC_URI = "https://www.codeaurora.org/mirrored_source/quic/qsdk/arptables-v0.0.4.tar.gz"

SRC_URI[md5sum] = "c2e99c3aa9d78c9dfa30710ca3168182"
SCR_URI[sha256sum] = "277985e29ecd93bd759a58242cad0e02ba9d4a6e1b7795235e3b507661bc0049"


do_install () {
              install -d ${D}${sbindir}
              oe_runmake 'DESTDIR=${D}' install
              install -m 0755 ${D}/usr/local/sbin/arptables* ${D}${sbindir}
              rm -rf ${D}/usr/local
}

PARALLEL_MAKE = ""
BBCLASSEXTEND = "native"
