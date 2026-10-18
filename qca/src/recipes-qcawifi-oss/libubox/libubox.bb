DESCRIPTION = "libubox"
SECTION = "libubox"
LICENSE = "ISC"
PR = "r0"
LIC_FILES_CHKSUM = "file://utils.h;endline=17;md5=4d7fac50d952764bf582f5dc34d3b6b9"

DEPENDS = "json-c lua5.1"

inherit cmake pkgconfig

SRC_URI = "git://source.codeaurora.org/quic/qsdk/libubox;protocol=https;branch=openwrt/master"
SRCREV = "10429bccd0dc5d204635e110a7a8fae7b80d16cb"

SRC_URI[md5sum] = "eb1c1cfbdfb7f36f0c09d89dd98d641b"

S = "${WORKDIR}/git"

EXTRA_OECMAKE += '-DLIBARCH=${baselib} \
                 -DLUAPATH=/usr/lib/lua \
                 -DCMAKE_MODULE_LINKER_FLAGS:STRING="-L${STAGING_LIBDIR}" \
                 -DCMAKE_EXE_LINKER_FLAGS:STRING="-L${STAGING_LIBDIR}" \
                 -DCMAKE_SHARED_LINKER_FLAGS:STRING="-L${STAGING_LIBDIR}" \
                 -DCMAKE_FIND_ROOT_PATH=${STAGING_DIR_HOST}'

FILES_${PN} += "${libdir}/*"

FILES_${PN}-dbg += "${libdir}/lua/.debug"

FILES_${PN}-dev = "/usr/include/*"

INSANE_SKIP_${PN} = "dev-so"

BBCLASSEXTEND += "native"
