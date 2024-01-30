SUMMARY = "cfg80211 interface configuration utility"
LICENSE = "GPL-2.0 WITH Linux-syscall-note"
SECTION = "console/network"
LIC_FILES_CHKSUM = "file://${DL_DIR}/mac80211-kernel/COPYING;md5=6bc538ed5bd9a7fc9398086aedcd7e46"
PKG_NAME = "open-mac80211"
SRC_URI = "https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/snapshot/linux-firmware-8afadbe.tar.gz"

PKG_KERNEL_SRC_URL = "https://git.codelinaro.org/clo/qsdk/kvalo/ath.git"
PKG_KERNEL_VERSION = "ef7197996efe"
SRC_URI[md5sum] = "21113fbd338eed6070c7f9b4c8939b43"
V_mac80211_config =" \
	\nCPTCFG_CFG80211=m \
	\nCPTCFG_MAC80211=m \
	\nCPTCFG_ATH_CARDS=m \
	\nCPTCFG_ATH_COMMON=m \
	\nCPTCFG_ATH11K=m \
	\nCPTCFG_ATH11K_PCI=m \
	\nCPTCFG_ATH11K_AHB=m \
	\nCPTCFG_ATH12K=m \
	\nCPTCFG_WLAN=y \
	\nCPTCFG_NL80211_TESTMODE=y \
	\nCPTCFG_CFG80211_WEXT=y \
	\nCPTCFG_CFG80211_INTERNAL_REGDB=y \
	\nCPTCFG_CFG80211_CERTIFICATION_ONUS=y \
	\nCPTCFG_MAC80211_RC_MINSTREL=y \
	\nCPTCFG_MAC80211_RC_MINSTREL_HT=y \
	\nCPTCFG_MAC80211_RC_DEFAULT_MINSTREL=y \
	\nCPTCFG_MAC80211_MESH=y \
	\nCPTCFG_CFG80211_DEBUGFS=y \
	\nCPTCFG_MAC80211_DEBUGFS=y \
	\nCPTCFG_ATH9K_DEBUGFS=y \
	\nCPTCFG_ATH9K_HTC_DEBUGFS=y \
	\nCPTCFG_ATH10K_DEBUGFS=y \
	\nCPTCFG_ATH11K_DEBUGFS=y \
	\nCPTCFG_ATH12K_DEBUGFS=y \
	\nCPTCFG_ATH10K_PKTLOG=y \
	\nCPTCFG_ATH11K_PKTLOG=y \
	\nCPTCFG_ATH12K_PKTLOG=y \
	\nCPTCFG_ATH_DEBUG=y \
	\nCPTCFG_ATH10K_DEBUG=y \
	\nCPTCFG_ATH11K_DEBUG=y \
	\nCPTCFG_ATH12K_DEBUG=y \
	\nCPTCFG_HWMON=y \
	\nCPTCFG_ATH9K_PCI=y \
	\nCPTCFG_ATH_USER_REGD=y \
	\nCPTCFG_ATH_REG_DYNAMIC_USER_REG_HINTS=y \
	\nCPTCFG_ATH_REG_DYNAMIC_USER_CERT_TESTING=y \
	\nCPTCFG_ATH11K_SPECTRAL=y \
	\nCPTCFG_ATH11K_CFR=y \
	\nCPTCFG_MAC80211_LEDS=y \
	\nCPTCFG_ATH12K_SPECTRAL=y \
	\nCPTCFG_ATH12K_AHB=y \
"
inherit module
DEPENDS +="linux-ipq libnl pkgconfig-native virtual/kernel"
addtask download before do_unpack after do_fetch
PKG_BACKPORTS_SOURCE_URL = "https://git.kernel.org/pub/scm/linux/kernel/git/backports/backports.git"
PKG_BACKPORTS_VERSION = "42a95ce7"
PKG_VERSION = "20220802"

SUBDIR = "backports-${PKG_VERSION}-${PKG_KERNEL_VERSION}"

T_DIR = "${S}/../../${SUBDIR}"
P_DIR = "${TOPDIR}/../poky/meta-ipq/recipes-openwifi/wlan-open/mac80211/patches"
I_DIR = "${TOPDIR}/../poky/meta-ipq/recipes-openwifi/wlan-open/"
CC_remove = "-fstack-protector-strong  -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security -Werror=format-security"
KBUILD_CFLAGS += "-Wno-error=implicit-function-declaration"
LC_ALL = "C"
backports_mlo_support() {
	sed -i 's/struct ieee80211_tx_info \*info;/ktime_t ack_hwtstamp;/' \
			${DL_DIR}/mac80211-source/patches/0097-skb-list/mac80211-status.patch
	sed -i 's/struct sk_buff \*skb;/u8 n_rates;/' \
			${DL_DIR}/mac80211-source/patches/0097-skb-list/mac80211-status.patch
	sed -i '13s/.*/ /' \
			${DL_DIR}/mac80211-source/patches/0097-skb-list/mac80211-status.patch
	sed -i '58,75d' ${DL_DIR}/mac80211-source/patches/0099-netlink-range/mac80211.patch
}
do_download() {
	if [ ! -f ${DL_DIR}/${SUBDIR}.tar.bz2 ];then
	git clone ${PKG_KERNEL_SRC_URL} ${DL_DIR}/mac80211-kernel || (rm -rf ${DL_DIR}/mac80211-kernel &&  git clone ${PKG_KERNEL_SRC_URL} ${DL_DIR}/mac80211-kernel)
	(cd ${DL_DIR}/mac80211-kernel;git remote add src ${PKG_KERNEL_SRC_URL}; git fetch src)

	(cd  ${DL_DIR}/mac80211-kernel&& git checkout ${PKG_KERNEL_VERSION})

	git clone ${PKG_BACKPORTS_SOURCE_URL} ${DL_DIR}/mac80211-source  ||
        (rm -rf ${DL_DIR}/mac80211-source && git  clone ${PKG_BACKPORTS_SOURCE_URL} ${DL_DIR}/mac80211-source)
	cp ${TOPDIR}/../poky/meta-ipq/recipes-openwifi/wlan-open/mac80211/files/copy-list.ath ${DL_DIR}/mac80211-source
	(cd  ${DL_DIR}/mac80211-source && git checkout ${PKG_BACKPORTS_VERSION} && rm -f ${DL_DIR}/mac80211-source/patches/0073-netdevice-mtu-range.cocci && rm -f ${DL_DIR}/mac80211-source/patches/0075-ndo-stats-64.cocci && rm -f ${DL_DIR}/mac80211-source/patches/0105-remove-const-from-rchan_callbacks.patch && backports_mlo_support &&./gentree.py --clean --copy-list ./copy-list.ath ${DL_DIR}/mac80211-kernel ${DL_DIR}/${SUBDIR})
	echo "before making tar"
	(cd ${DL_DIR}; if [ -z " tar --numeric-owner --owner=0 --group=0 --mode=a-s --sort=name ${TAR_TIMESTAMP:+--mtime="$TAR_TIMESTAMP"} -c ${SUBDIR} |  bzip2 -c > ${DL_DIR}/${SUBDIR}.tar.bz2" ]; then bzip2 -c > ${DL_DIR}/${SUBDIR}.tar.bz2; else : ; fi; tar --numeric-owner --owner=0 --group=0 --mode=a-s --sort=name ${TAR_TIMESTAMP:+--mtime="$TAR_TIMESTAMP"} -c ${SUBDIR} | bzip2 -c > ${DL_DIR}/${SUBDIR}.tar.bz2)

	else
	echo "Skipping backports download"
	fi;

	if [ -d ${T_DIR} ]; then
		rm -rf ${T_DIR}/
	fi;
	mkdir -p ${T_DIR}/
	bzcat ${DL_DIR}/${SUBDIR}.tar.bz2 | tar -C ${T_DIR}/.. -xf -

}

do_patch() {
	ls ${I_DIR}/mac80211/patches | xargs -I % sh -c 'echo "applying patch "%;patch -d${T_DIR} -f -p1 < ${I_DIR}/mac80211/patches/%'
	find ${T_DIR}/ '(' -name '*.orig' -o -name '.*.orig' ')' -exec rm -f {} \;
	tar -C ${T_DIR} -xf ${DL_DIR}/linux-firmware-8afadbe.tar.gz
	rm -rf ${T_DIR}/include/linux/ssb ${T_DIR}/include/linux/bcma ${T_DIR}/include/net/bluetooth
	rm -rf ${T_DIR}/include/linux/cordic.h  ${T_DIR}/include/linux/crc8.h ${T_DIR}/include/linux/eeprom_93cx6.h ${T_DIR}/include/linux/wl12xx.h ${T_DIR}/include/linux/spi/libertas_spi.h ${T_DIR}/include/net/ieee80211.h

}
CC_remove  = "${@bb.utils.contains('TUNE_ARCH','arm',' -mfloat-abi=hard -mcpu=cortex-a7 ','',d)}"
ARCH = "${@bb.utils.contains('TUNE_ARCH','arm','arm','arm64',d)}"

do_compile() {
	LINUX_IPQ_VERSION=`echo ${PREFERRED_VERSION_linux-yocto} | awk -F% '{print $1}'`
	echo -e "${V_mac80211_config}" > ${T_DIR}/.config
	sed -i 's/-e//g' ${T_DIR}/.config
	cat ${T_DIR}/.config
	make  -C ${T_DIR} CC="gcc" LD="${LD}"  ARCH="${ARCH}" EXTRA_CFLAGS="-I${T_DIR}/include/ -Wno-incompatible-pointer-types -Wno-discarded-qualifiers -Wno-int-conversion -Wno-implicit-function-declaration" MODPROBE=true     KLIB=/lib/modules/${KERNEL_VERSION}  KLIB_BUILD=${S}/../../../../linux-ipq/${LINUX_IPQ_VERSION}-${PR}/build KERNEL_SUBLEVEL=4 KBUILD_LDFLAGS_MODULE_PREREQ= olddefconfig
	rm -rf ${T_DIR}/modules
make -C ${T_DIR} CC="${CC}" LD="${LD}"  ARCH="${ARCH}" EXTRA_CFLAGS="-I${T_DIR}/include -Wall -Werror -Wno-incompatible-pointer-types -Wno-unused-variable -Wno-discarded-qualifiers -Wno-int-conversion -Wno-implicit-fallthrough" KLIB=/lib/modules/${KERNEL_VERSION}  KLIB_BUILD=${S}/../../../../linux-ipq/${LINUX_IPQ_VERSION}-${PR}/build KERNEL_SUBLEVEL=4 KBUILD_LDFLAGS_MODULE_PREREQ= modules
	cp ${T_DIR}/compat/compat.ko ${T_DIR}/drivers/net/wireless/ath/ath12k/ath12k.ko  ${T_DIR}/drivers/net/wireless/ath/ath11k/ath11k.ko ${T_DIR}/drivers/net/wireless/ath/ath11k/ath11k_ahb.ko ${T_DIR}/drivers/net/wireless/ath/ath11k/ath11k_pci.ko ${T_DIR}/drivers/net/wireless/ath/ath.ko ${T_DIR}/net/mac80211/mac80211.ko ${T_DIR}/net/wireless/cfg80211.ko ./


}

do_install() {
	install -m 0755 -d ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/mac80211
	install -m 0644 compat${KERNEL_OBJECT_SUFFIX} ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/mac80211
	install -m 0644 cfg80211${KERNEL_OBJECT_SUFFIX} ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/mac80211
	install -m 0644 mac80211${KERNEL_OBJECT_SUFFIX} ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/mac80211
	install -m 0644 ath${KERNEL_OBJECT_SUFFIX} ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/mac80211
	install -m 0644 ath11k${KERNEL_OBJECT_SUFFIX} ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/mac80211
	install -m 0644 ath11k_ahb${KERNEL_OBJECT_SUFFIX} ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/mac80211
	install -m 0644 ath11k_pci${KERNEL_OBJECT_SUFFIX} ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/mac80211
	install -m 0644 ath12k${KERNEL_OBJECT_SUFFIX} ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/mac80211
	install -d ${D}${includedir}/open-mac80211
}

S = "${WORKDIR}/git/${PKG_NAME}"
inherit pkgconfig
