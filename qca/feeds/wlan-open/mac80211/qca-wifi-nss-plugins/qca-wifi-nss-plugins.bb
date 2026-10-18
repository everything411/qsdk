DESCRIPTION = "NSS WiFi Plugins kernel module"
LICENSE = "ISC"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/${LICENSE};md5=f3b90e78ea0cffb20bf5cca7947a896d"

inherit module

CLEANBROKEN = "1"

SRCPREFIX:echo = "../"
python () {
    import os
    topdir = d.getVar("TOPDIR")
    if os.path.isdir(os.path.join(topdir, "..", "src/ipq/mac80211/wlan-open")):
        d.setVar("WLAN_SRCPREFIX", "src/ipq/")
    else:
        d.setVar("WLAN_SRCPREFIX", "qca/src/")
}

KARCH:echo = "${ARCH}"

FILESPATH =+ "${TOPDIR}/../${WLAN_SRCPREFIX}wlan-open-extns/ath/plugin:"

SRC_URI = "file://qca-wifi-nss-plugins/"

DEPENDS = "virtual/kernel qca-nss-ecm qca-nss-ppe qca-nss-ppe-ds open-mac80211"

DEPENDS:echo = "virtual/kernel dataipa open-mac80211 qca-nss-ecm"

S = "${WORKDIR}/qca-wifi-nss-plugins"

PACKAGES += "kernel-module-qca-wifi-nss-plugins"

PACKAGECONFIG ??= ""

PACKAGECONFIG[ath] = ""

WIFI_NSS_MAKE_OPTS += " \
    ${@bb.utils.contains('PACKAGECONFIG', 'ath', 'QCA_WIFI_NSS_PLUGINS_OPEN_PROFILE_ENABLE=y', '', d)} \
    QCA_WIFI_NSS_PLUGINS_ECM_EMESH=y \
    QCA_WIFI_NSS_PLUGINS_ECM_FSE=y \
    QCA_WIFI_NSS_PLUGINS_ECM_NL=y \
    QCA_WIFI_NSS_PLUGINS_ECM_WIFI_CLASSIFIER=y \
    QCA_WIFI_NSS_PLUGINS_MSCS=y \
    QCA_WIFI_NSS_PLUGINS_PPE=y \
    QCA_WIFI_NSS_PLUGINS_PPEDS=y \
    "
WIFI_NSS_MAKE_OPTS:echo = " \
    QCA_WIFI_NSS_PLUGINS_IPA_FSE=y \
    QCA_WIFI_NSS_PLUGINS_ECM_FSE=y \
    QCA_WIFI_NSS_PLUGINS_ECM_WIFI_CLASSIFIER=y \
    "

EXTRA_CFLAGS += " \
    -I${STAGING_INCDIR}/qca-nss-ecm \
    -I${STAGING_INCDIR}/qca-nss-ppe \
    -I${STAGING_INCDIR}/qca-nss-ppe-ds \
    -I${STAGING_INCDIR}/open-mac80211 \
    -I${STAGING_DIR}/usr/include/ \
    "
KCFLAGS:append:echo = " \
    -I${STAGING_INCDIR} \
    -I${STAGING_INCDIR}/ath \
    -I${TOPDIR}/${SRCPREFIX}/src/dataipa/drivers/platform/msm/include/ \
    -I${TOPDIR}/${SRCPREFIX}/src/dataipa/drivers/platform/msm/include/uapi \
    -I${TOPDIR}/${SRCPREFIX}/src/qca-nss-ecm/exports \
    "

MODULE_EXTRA_SYMBOLS = " \
    ${STAGING_INCDIR}/qca-nss-ecm/Module.symvers \
    ${STAGING_INCDIR}/qca-nss-ppe/Module.symvers \
    ${STAGING_INCDIR}/qca-nss-ppe-ds/Module.symvers \
    ${STAGING_INCDIR}/open-mac80211/Module.symvers \
    "

MODULE_EXTRA_SYMBOLS:echo = " \
    ${STAGING_INCDIR}/dataipa/Module.symvers \
    ${STAGING_INCDIR}/open-mac80211/Module.symvers \
    ${STAGING_INCDIR}/qca-nss-ecm/Module.symvers \
    "

do_compile() {
    unset LDFLAGS
    make -C "${STAGING_KERNEL_BUILDDIR}" ${WIFI_NSS_MAKE_OPTS} \
        CROSS_COMPILE="${TARGET_PREFIX}" \
        ARCH="${KARCH}" \
        M="${S}" \
        KCFLAGS="${KCFLAGS}" \
        EXTRA_CFLAGS="${EXTRA_CFLAGS}" \
        KBUILD_EXTRA_SYMBOLS="${MODULE_EXTRA_SYMBOLS}" \
        modules
}

do_install() {
    install -d ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/qca-wifi-nss-plugins
    install -m 0644 qca-wifi-nss-plugins${KERNEL_OBJECT_SUFFIX} ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/qca-wifi-nss-plugins/
}

KERNEL_MODULE_AUTOLOAD += "qca-wifi-nss-plugins"
