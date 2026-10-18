DESCRIPTION = "Button Hotplug Driver"
LICENSE = "GPL-2.0"
LIC_FILES_CHKSUM = "file://${COREBASE}/meta/files/common-licenses/${LICENSE};md5=801f80980d171dd6425610833a22dbe6"

inherit module

FILESEXTRAPATHS_prepend := "${THISDIR}/files/:"

SRC_URI = "file://Makefile \
	   file://button-hotplug.c \
	   "

DEPENDS = "virtual/kernel"

S = "${WORKDIR}/button-hotplug"

PACKAGES += "kernel-module-button-hotplug"
INSANE_SKIP_${PN} = "dev"

do_compile() {
	unset LDFLAGS
	make -C  "${STAGING_KERNEL_BUILDDIR}" \
		ARCH="${ARCH}" \
		CROSS_COMPILE='${TARGET_PREFIX}' \
		SUBDIRS="${S}/../" \
		modules
}
do_install() {
	install -d ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/${PN}
	install -m 0644 ../button-hotplug${KERNEL_OBJECT_SUFFIX} ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/${PN}
}

KERNEL_MODULE_AUTOLOAD += "button-hotplug"
module_autoload_button-hotplug = "button-hotplug"
