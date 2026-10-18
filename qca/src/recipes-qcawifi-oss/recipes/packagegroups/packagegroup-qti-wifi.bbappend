SUMMARY = "QTI WIFI opensource package groups"
LICENSE = "BSD-3-Clause"
PACKAGE_ARCH = "${MACHINE_ARCH}"

RDEPENDS_packagegroup-qti-wifi += " \
        iw-wifi6e \
        uci \
	ipq-scripts \
        wireless-tools \
        qca-cnss \
        iperf \
        emesh-sp \
        qca-hyfi-bridge \
        "

