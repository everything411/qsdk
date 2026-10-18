
FILESEXTRAPATHS_prepend := "${THISDIR}/${PN}:"

SRC_URI += "file://fstab \
            file://rc.local \
            file://setmac \
           "

inherit update-rc.d

do_install_append() {
	install -d ${D}/${sysconfdir}
	install -m 0644 ${WORKDIR}/fstab ${D}${sysconfdir}/fstab
	install -d ${D}/etc/init.d/
	install -m 0755 ${WORKDIR}/rc.local ${D}/etc/init.d/rc.local
	install -d ${D}/sbin/
	install -m 0755 ${WORKDIR}/setmac ${D}/sbin/setmac
	install -m 0755 ${WORKDIR}/share/dot.profile ${D}/home/root/.profile
	cat >> ${D}/home/root/.bashrc << EOT
enable -n echo
EOT

}

INITSCRIPT_PACKAGES = "base-files"
INITSCRIPT_NAME = "rc.local"
INITSCRIPT_PARAMS = "start 99 S ."
