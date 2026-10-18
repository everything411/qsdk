DESCRIPTION = "Samba Network Share"
SECTION = "console/network"
LICENSE = "GPLv3"
LIC_FILES_CHKSUM = "file://../COPYING;md5=d32239bcb673463ab874e80d47fae504"
INCOMPATIBLE_LICENSE = ""

SAMBA_MIRROR = "https://download.samba.org/pub/samba"

inherit autotools-brokensep update-rc.d

SRC_URI += "${SAMBA_MIRROR}/samba-${PV}.tar.gz \
    file://smb.conf.template \
    file://samba.init \
    file://samba.config \
    file://100-configure_fixes.patch;patchdir=.. \
    file://110-multicall.patch;patchdir=.. \
"
SRC_URI[md5sum] = "76da2fa64edd94a0188531e7ecb27c4e"
SRC_URI[sha256sum] = "8f2c8a7f2bd89b0dfd228ed917815852f7c625b2bc0936304ac3ed63aaf83751"

S = "${WORKDIR}/samba-${PV}/source3"

FILES_${PN} = "${sbindir}/smbd \
               ${sbindir}/nmbd \
               ${sbindir}/samba_multicall \
               ${sysconfdir}/samba \
               ${sysconfdir}/init.d/samba \
               ${sysconfdir}/config/samba \
"
CONFFILES_${PN} = "${sysconfdir}/samba/smb.conf.template"

EXTRA_OECONF += "\
           ac_cv_file__proc_sys_kernel_core_pattern=yes \
           ac_cv_header_zlib_h=no \
           ac_cv_lib_attr_getxattr=no \
           ac_cv_path_PYTHON=/not/exist \
           ac_cv_path_PYTHON_CONFIG=/not/exist \
           ac_cv_search_getxattr=no \
           libreplace_cv_HAVE_GETADDRINFO=yes \
           libreplace_cv_HAVE_IFACE_IFCONF=yes \
           linux_getgrouplist_ok=no \
           LINUX_LFS_SUPPORT=yes \
           SMB_BUILD_CC_NEGATIVE_ENUM_VALUES=yes \
           samba_cv_CC_NEGATIVE_ENUM_VALUES=yes \
           samba_cv_HAVE_BROKEN_GETGROUPS=no \
           samba_cv_HAVE_FTRUNCATE_EXTEND=yes \
           samba_cv_HAVE_GETTIMEOFDAY_TZ=yes \
           samba_cv_HAVE_IFACE_IFCONF=yes \
           samba_cv_HAVE_KERNEL_OPLOCKS_LINUX=yes \
           samba_cv_HAVE_SECURE_MKSTEMP=yes \
           samba_cv_HAVE_WRFILE_KEYTAB=no \
           samba_cv_have_setresuid=yes \
           samba_cv_have_setresgid=yes \
           samba_cv_have_setreuid=yes \
           samba_cv_linux_getgrouplist_ok=yes \
           samba_cv_USE_SETREUID=yes \
           samba_cv_USE_SETRESUID=yes \
           samba_cv_zlib_1_2_3=no \
           --disable-avahi \
           --disable-cups \
           --disable-nls \
           --disable-pie \
           --disable-relro \
           --disable-shared-libs \
           --disable-static \
           --disable-swat \
           --with-libiconv=${STAGING_LIBDIR}/.. \
           --without-automount \
           --with-configdir=${sysconfdir}/samba \
           --with-privatedir=${sysconfdir}/samba/private \
           --with-lockdir=${localstatedir}/lock \
           --with-piddir=${localstatedir}/run \
           --with-logfilebase=${localstatedir}/log/samba \
           --with-sendfile-support \
           --without-acl-support \
           --without-cluster-support \
           --without-ads \
           --without-krb5 \
           --without-ldap \
           --without-pam \
           --without-winbind \
           --without-libtdb \
           --without-libtalloc \
           --without-libnetapi \
           --without-libsmbclient \
           --without-libsmbsharemodes \
"

do_configure_prepend () {
    ./script/mkversion.sh
    if [ ! -e acinclude.m4 ]; then
        touch aclocal.m4
        cat aclocal.m4 > acinclude.m4
    fi
}

do_configure() {
    gnu-configize --force
    oe_runconf
}

do_compile () {
     base_do_compile
}

do_install_append() {
    mkdir -p ${D}${sbindir}
    mkdir -p ${D}${sysconfdir}
    ln -sf /usr/sbin/samba_multicall ${D}${sbindir}/smbd
    ln -sf /usr/sbin/samba_multicall ${D}${sbindir}/nmbd
    install -D -m 755 ${WORKDIR}/samba.init ${D}${sysconfdir}/init.d/samba
    install -D -m 755 ${WORKDIR}/samba.config ${D}${sysconfdir}/config/samba
    install -D -m 644 ${WORKDIR}/smb.conf.template ${D}${sysconfdir}/samba/
    rmdir "${D}${localstatedir}/lock"
    rmdir "${D}${localstatedir}/run"
    rm -rf "${D}${libdir}/"
    rm -rf "${D}${bindir}/"
    rm -rf "${D}${localstatedir}/"
}

INITSCRIPT_PACKAGES = "${PN}"
INITSCRIPT_PARAMS = "defaults"
INITSCRIPT_NAME_${PN} = "samba"
