# Legend: Y = YES, N = NO, F = FUTURE
# |Feature      | ATH | ILQ |
# |-------------|-----|-----|
# | Premium     | N   | Y   |
# | Opensync    | N   | Y   |
# | BigEndian   | N   | Y   |
# | DPDK        | N   | N   |
# | Cov         | N   | Y   |
# | Open        | Y   | N   |
# | Sfu         | Y   | N   |
# | QBuilder    | N   | N   |
# | Enterprise  | N   | N   |
# | Minent      | N   | Y   |
# | MinentOpen  | Y   | N   |
# | 256         | Y   | Y   |
# | 512         | Y   | Y   |
# | Default     | Y   | Y   |
# | 16M         | N   | N   |

NSS_COMMON:= kmod-qca-nss-dp \
	kmod-qca-nss-debug

NSS_CRYPTO_MINENT:= kmod-qca-nss-crypto \
	kmod-qca-nss-cfi-cryptoapi \
	-kmod-qca-nss-cfi-ocf \
	kmod-qca-nss-drv-ipsecmgr \
	kmod-qca-nss-drv-ipsecmgr-xfrm \
	-kmod-crypto-ocf \
	-kmod-qca-nss-drv-ipsecmgr-klips

NSS_FW_MINENT:= qca-nss-fw-al-enterprise \
		qca-nss-fw-al-enterprise_custC \
		qca-nss-fw-eip-al

NSS_MACSEC:= \
	kmod-qca-nss-macsec \
	qca-wpa-supplicant-macsec \
	qca-hostap-macsec \
	qca-hapd-supp-macsec

NSS_NETFN_MINENT:= kmod-qca-nss-netfn-pkt-steer \
	kmod-qca-nss-netfn-capwap \
	kmod-qca-nss-netfn-dtls \
	kmod-qca-nss-netfn-capwapmgr \
	kmod-qca-nss-flowmgr

QCA_ECM_STANDARD:= kmod-qca-nss-ecm-standard

QCA_ECM_ENTERPRISE:= kmod-qca-nss-ecm-noload

QCA_ECM_PREMIUM:= kmod-qca-nss-ecm-premium

NSS_PPE_256:= kmod-qca-nss-ppe \
	kmod-qca-nss-ppe-vp \
	kmod-qca-nss-ppe-bridge-mgr \
	kmod-qca-nss-ppe-pppoe-mgr \
	kmod-qca-nss-ppe-ds \
	kmod-qca-nss-ppe-dsa-mgr

NSS_PPE_16M:= kmod-qca-nss-ppe \
	kmod-qca-nss-ppe-vp \
	kmod-qca-nss-ppe-bridge-mgr \
	kmod-qca-nss-ppe-pppoe-mgr \
	kmod-qca-nss-ppe-lag-mgr \
	kmod-qca-nss-ppe-dsa-mgr

NSS_PPE:= kmod-qca-nss-ppe \
	kmod-qca-nss-ppe-vp \
	kmod-qca-nss-ppe-bridge-mgr \
	kmod-qca-nss-ppe-pppoe-mgr \
	kmod-qca-nss-ppe-lag-mgr \
	kmod-qca-nss-ppe-tunipip6 \
	kmod-qca-nss-ppe-gre-mgr \
	kmod-qca-nss-ppe-vxlanmgr \
	kmod-qca-nss-ppe-mapt \
	kmod-qca-nss-ppe-rule \
	kmod-qca-nss-ppe-qdisc \
	kmod-qca-nss-ppe-ds \
	kmod-qca-nss-ppe-mirror-test \
	kmod-qca-nss-ppe-l2tp \
	kmod-qca-nss-ppe-dsa-mgr

NSS_CLIENTS_STANDARD:= kmod-qca-ovsmgr kmod-qca-nss-dscpstats

NSS_CRYPTO:= kmod-qca-nss-crypto kmod-qca-nss-cfi-cryptoapi \
        kmod-qca-nss-eip kmod-qca-nss-eip-crypto kmod-qca-nss-eip-ipsec

NSS_NETFN_TCPST:= nss-tcp-st-cli

NSS_SOCK_OFFLOAD:= kmod-qca-nss-netfn-sk-offload

NSS_UDP_ST:= kmod-nss-udp-st-drv nss-udp-st

NSS_NSM:= qca-nsm-app

NSS_FLS:= kmod-qca-nss-fls kmod-qca-nss-fls-lite

OPENVPN:= -openvpn-easy-rsa openvpn-openssl luci-app-openvpn

UDP_CLF:= udp-clf

SAL_QOS:= qca-sal-qos-test qca-sal-rule-test

SWITCH_SSDK_NOHNAT_PKGS:= kmod-qca-ssdk-nohnat qca-ssdk-shell swconfig kmod-qca8k kmod-qce2k

QCA_PHY_PKGS:= kmod-qca-nss-phy kmod-qcom-lnx-phy kmod-qca8084 kmod-aqr-phy \
	       kmod-qca8xxx-phc kmod-qca-mdio kmod-qcom-lnx-phy-macsec

MACSEC_OPEN_PKGS:= kmod-qca-nss-macsec wpad-macsec

NSS_L2TP:= kmod-l2tp kmod-l2tp-ip kmod-l2tp-eth

NSS_PLUGINS:=kmod-qca-nss-wifi-plugins
ATH_CLIENT:=kmod-qca-nss-ppe-ath-clients

NETWORKING:=mcproxy -dnsmasq dnsmasq-dhcpv6 bridge ip-bridge ip-full mwan3 \
	rp-pppoe-relay iptables-mod-extra iputils-tracepath iputils-tracepath6 \
	luci-app-upnp luci-app-ddns luci-proto-ipv6 \
	kmod-nf-nathelper-extra kmod-nf-nathelper \
	kmod-ipt-nathelper-rtsp nftables kmod-nft-netdev \
	kmod-nft-offload kmod-bonding vxlan kmod-gre6 conntrack linuxptp

NETWORKING_256MB:=-dnsmasq dnsmasq-dhcpv6 bridge ip-full \
	rp-pppoe-relay iputils-tracepath iputils-tracepath6 linuxptp

NETWORKING_8MB:=dnsmasq -dnsmasq-dhcpv6 kmod-nf-nathelper-extra \
		kmod-ipt-nathelper-rtsp linuxptp

NETWORKING_16MB:=-dnsmasq dnsmasq-dhcpv6 kmod-nf-nathelper-extra kmod-ipt-nathelper-rtsp ip \
		 rp-pppoe-relay linuxptp

NPT66:= kmod-ipt-nat6 iptables-mod-nat-extra

CD_ROUTER:=kmod-ipt-ipopt kmod-bonding kmod-ipt-sctp kmod-ipt-raw kmod-ipt-raw6 lacpd \
	arptables ds-lite 6rd ddns-scripts xl2tpd \
	quagga quagga-ripd quagga-zebra quagga-watchquagga quagga-vtysh \
	kmod-ipv6 ip6tables iptables-mod-ipsec iptables-mod-filter \
	isc-dhcp-relay-ipv6 rp-pppoe-server ppp-mod-pptp iptables-mod-physdev

CD_ROUTER_256MB:=kmod-ipv6 ddns-scripts rp-pppoe-server

QOS:=tc-full kmod-sched kmod-sched-core kmod-sched-prio kmod-sched-red \
	kmod-sched-cake kmod-sched-pie kmod-sched-act-police kmod-sched-act-ipt \
	kmod-sched-connmark kmod-ifb iptables iptables-mod-filter \
	iptables-mod-ipopt iptables-mod-conntrack-extra

#These packages depend on SWITCH_SSDK_NOHNAT_PKGS
IGMPSNOOPING_RSTP:=rstp qca-mcs-apps

IPSEC:=kmod-ipsec kmod-ipsec4 kmod-ipsec6

NSS_USERSPACE_OSS:=ppecfg ppe_mcast_mgr

EMESH_SP:=kmod-emesh-sp

DPDK:=dpdk-tools kmod-qca-nss-dpdk-cfgmgr kmod-nss-ppe-uio

EXTRA_NETWORKING:= $(CD_ROUTER) kmod-qca-nss-macsec \
	$(MACSEC_OPEN_PKGS) $(NSS_CRYPTO) $(NSS_CLIENTS_STANDARD) $(NSS_PLUGINS) $(ATH_CLIENT)

STRONGSWAN:=strongswan strongswan-default strongswan-mod-ctr strongswan-mod-gcm strongswan-mod-kdf strongswan-mod-openssl strongswan-mod-uci

MMAP_TELEMETRY:= kmod-qca-mmap-telemetry

MAP_PKGS:=map 464xlat tayga

SXC_DRVS:= kmod-qca-sdc-drv kmod-qca-sac-drv

NSS_512:= $(NSS_COMMON) \
	$(SWITCH_SSDK_NOHNAT_PKGS) $(QCA_PHY_PKGS) \
	$(CD_ROUTER) $(SAL_QOS) $(UDP_CLF) kmod-qca-nss-fls-lite \
	$(NETWORKING) $(OPENVPN) \
	$(IGMPSNOOPING_RSTP) $(IPSEC) $(QOS) $(QCA_ECM_PREMIUM) $(NSS_PPE) $(NSS_USERSPACE_OSS) \
	$(NSS_MACSEC) $(NSS_CRYPTO) $(NSS_CLIENTS_STANDARD) \
	$(MAP_PKGS) -lacpd $(EMESH_SP) \
	kmod-macvlan $(NSS_L2TP) $(MMAP_TELEMETRY) $(SXC_DRVS)

NSS_OPEN:=$(NSS_PPE) $(NSS_COMMON) $(NSS_USERSPACE_OSS)\
	$(QCA_ECM_PREMIUM) $(STRONGSWAN) $(NETWORKING) $(CD_ROUTER) \
	$(SWITCH_SSDK_NOHNAT_PKGS) $(QCA_PHY_PKGS) $(IGMPSNOOPING_RSTP) $(MAP_PKGS) \
	$(EXTRA_NETWORKING) \
	$(EMESH_SP) $(NPT66)\
	$(NSS_NSM) $(SAL_QOS) $(IPSEC) $(NSS_CRYPTO) $(QOS) -lacpd \
	kmod-qca-ovsmgr -qca-mcs-apps \
	$(NSS_FLS) $(UDP_CLF) kmod-macvlan $(NSS_L2TP) \
	$(NSS_UDP_ST) $(NSS_NETFN_TCPST) $(NSS_SOCK_OFFLOAD) $(MMAP_TELEMETRY) $(SXC_DRVS)

NSS_SFU:=$(NSS_COMMON) $(NSS_USERSPACE_OSS)\
	$(SWITCH_SSDK_NOHNAT_PKGS) $(QCA_PHY_PKGS) $(IGMPSNOOPING_RSTP) -qca-mcs-apps \
	$(NSS_PPE_256) $(CD_ROUTER_256MB) \
	$(QOS) $(SAL_QOS) \
	kmod-qca-nss-macsec kmod-macvlan $(NSS_UDP_ST) $(NSS_NETFN_TCPST) $(NETWORKING_256MB) \
	$(NSS_SOCK_OFFLOAD) kmod-vxlan

NSS_256:=$(NSS_COMMON) \
	$(SWITCH_SSDK_NOHNAT_PKGS) $(QCA_PHY_PKGS) \
	$(NSS_USERSPACE_OSS) $(CD_ROUTER_256MB) $(NSS_PPE_256) \
	$(NETWORKING_256MB) $(QCA_ECM_STANDARD) -lacpd \
	$(IGMPSNOOPING_RSTP) $(SAL_QOS) $(UDP_CLF) kmod-qca-nss-fls-lite \
	kmod-qca-ovsmgr -qca-mcs-apps kmod-macvlan \
	kmod-vxlan

NSS_MINENT_OPEN:=$(NSS_COMMON) \
	$(SWITCH_SSDK_NOHNAT_PKGS) $(QCA_PHY_PKGS) \
	$(NETWORKING) $(QOS) \
	$(QCA_ECM_ENTERPRISE) $(MACSEC_OPEN_PKGS) $(NSS_CRYPTO_MINENT) \
	$(IPSEC) $(STRONGSWAN) $(CD_ROUTER) \
	-lacpd -kmod-qca-nss-ecm-wifi-plugin \
	$(NSS_PPE) $(NSS_USERSPACE_OSS) \
	$(NSS_NETFN_MINENT) $(NSS_FW_MINENT)

#Below are the deprecated profiles.
NSS_DPDK:=$(NSS_PPE) $(NSS_COMMON) \
	$(QCA_ECM_PREMIUM) $(NETWORKING) $(CD_ROUTER) $(MAP_PKGS) \
	$(SWITCH_SSDK_NOHNAT_PKGS) $(QCA_PHY_PKGS) $(IGMPSNOOPING_RSTP) -lacpd \
	$(NSS_CLIENTS_STANDARD) $(NSS_CRYPTO) \
	$(IPSEC) $(QOS) $(NSS_USERSPACE_OSS)\
	$(OPENVPN) kmod-macvlan \
	$(NSS_NSM) $(SAL_QOS) $(NPT66) \
	$(NSS_FLS) $(UDP_CLF) $(NSS_L2TP) $(EMESH_SP) $(DPDK)

NSS_QBUILDER:=kmod-qca-nss-dp kmod-qca-nss-drv -kmod-qca-nss-gmac \
	-qca-nss-fw2-retail qca-nss-fw-hk-retail qca-nss-fw-cp-retail qca-nss-fw-mp-retail \
	kmod-qca-ssdk-nohnat qca-ssdk-shell \
	kmod-qca-nss-sfe \
	qca-mcs-apps \
	qca-nss-fw-eip-hk qca-nss-fw-eip-cp kmod-qca-ovsmgr kmod-qca-nss-dscpstats

NSS_ENTERPRISE:=$(NSS_COMMON) \
	$(SWITCH_SSDK_NOHNAT_PKGS) $(QCA_PHY_PKGS) \
	$(IGMPSNOOPING_RSTP) $(NETWORKING) $(QOS) \
	$(QCA_ECM_ENTERPRISE) $(NSS_MACSEC) $(NSS_CRYPTO) \
	$(IPSEC) $(STRONGSWAN) $(CD_ROUTER) \
	-lacpd \
	$(NSS_PPE) $(NSS_USERSPACE_OSS) kmod-qca-nss-drv-mscs

NSS_16M:=$(NSS_COMMON) $(NSS_PPE_16M) \
	$(SWITCH_SSDK_NOHNAT_PKGS) $(QCA_PHY_PKGS) \
	$(NETWORKING_16MB) \
	$(IGMPSNOOPING_RSTP) $(QCA_ECM_STANDARD)
