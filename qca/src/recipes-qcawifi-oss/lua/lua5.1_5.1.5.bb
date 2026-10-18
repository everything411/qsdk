DESCRIPTION = "Lua is a powerful light-weight programming language designed \
for extending applications."
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://COPYRIGHT;md5=59bdd99bb82238f238cf5c65c21604fd"
HOMEPAGE = "http://www.lua.org/"

PR = "r2"

SRC_URI = "http://www.lua.org/ftp/lua-${PV}.tar.gz \
           file://bitwise_operators.patch \
           file://lua5.1.pc \
           file://luaorg_1.patch \
           file://luaorg_2.patch \
	   file://01-remove-readline-dependency.patch \
"
S = "${WORKDIR}/lua-${PV}"

inherit pkgconfig binconfig

UCLIBC_PATCHES += "file://uclibc-pthread.patch"
SRC_URI_append_libc-uclibc = "${UCLIBC_PATCHES}"

TARGET_CC_ARCH += " -fPIC ${LDFLAGS}"
EXTRA_OEMAKE = "'CC=${CC} -fPIC' 'MYCFLAGS=${CFLAGS} -DLUA_USE_LINUX -fPIC' MYLDFLAGS='${LDFLAGS}'"

do_configure_prepend() {
    sed -i -e s:/usr/local:${prefix}:g src/luaconf.h
    sed -i -e s:lib/lua/5.1/:${base_libdir}/lua/:g src/luaconf.h
}

do_compile () {
    oe_runmake linux
}

do_install () {
    oe_runmake \
        'INSTALL_TOP=${D}${prefix}' \
        'INSTALL_BIN=${D}${bindir}' \
        'INSTALL_INC=${D}${includedir}/' \
        'INSTALL_MAN=${D}${mandir}/man1' \
        'INSTALL_SHARE=${D}${datadir}/lua' \
        'INSTALL_LIB=${D}${libdir}' \
        'INSTALL_CMOD=${D}${libdir}/lua/5.1' \
        install
    install -d ${D}${libdir}/pkgconfig
    install -m 0644 ${WORKDIR}/lua5.1.pc ${D}${libdir}/pkgconfig/lua5.1.pc
    install -m 0755 ${S}/src/lua.h ${D}/usr/include/
    install -m 0755 ${S}/src/liblua.a ${D}/usr/lib/
    install -m 0755 ${S}/etc/lua.pc ${D}/usr/lib/pkgconfig/
    install -m 0755 ${S}/src/lua ${D}/usr/bin/
    install -m 0755 ${S}/src/luac ${D}/usr/bin/
}

FILES_${PN} += "${libdir}/* ${datadir_native}/* ${bindir}/* ${prefix}/*"

BBCLASSEXTEND = "native"

SRC_URI[md5sum] = "2e115fe26e435e33b0d5c022e4490567"
SRC_URI[sha256sum] = "2640fc56a795f29d28ef15e13c34a47e223960b0240e8cb0a82d9b0738695333"
