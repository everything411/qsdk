DESCRIPTION = "uci"
SECTION = "uci"
LICENSE = "LGPLv2.1"
PR = "r0"
LIC_FILES_CHKSUM = "file://uci.h;endline=13;md5=0ee862ed12171ee619c8c2eb7eff77f2"

DEPENDS = "json-c libubox lua"

RDEPENDS_${PN} = "lua"

inherit cmake pkgconfig

SRC_URI = "https://mirrors.edge.kernel.org/caf_mirrored_source/quic/qsdk/uci-2019-09-01-415f9e48.tar.xz \
        file://config_files \
	file://patches/0001-copy-instead-of-rename-ucicommit.patch \
"

SRC_URI[md5sum] = "0b02010be03b4f1c55e0fc57390e0460"

S = "${WORKDIR}/uci-2019-09-01-415f9e48"

EXTRA_OECMAKE += '-DLIBARCH=${baselib} \
    -DLUAPATH=${libdir}/lua \
    -DCMAKE_MODULE_LINKER_FLAGS:STRING="-L${STAGING_LIBDIR}" \
    -DCMAKE_EXE_LINKER_FLAGS:STRING="-L${STAGING_LIBDIR}" \
    -DCMAKE_SHARED_LINKER_FLAGS:STRING="-L${STAGING_LIBDIR}" \
    -DCMAKE_FIND_ROOT_PATH=${STAGING_DIR_HOST}'

do_install_append() {
    mv ${D}/usr/bin ${D}/sbin
    cp -r ${WORKDIR}/config_files/* ${D}/
}

FILES_${PN} += "${libdir}/* ${baselib}/* ${sysconfdir}/*"

FILES_${PN}-dbg += "${libdir}/lua/.debug"

FILES_${PN}-dev = "/usr/include/*"

INSANE_SKIP_${PN} = "dev-so"

BBCLASSEXTEND += "native"
