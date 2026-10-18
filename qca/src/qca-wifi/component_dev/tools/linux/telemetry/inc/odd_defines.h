/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef ODD_DEFINES_H_
#define ODD_DEFINES_H_

#ifdef BUILD_PROFILE_OPEN
#define DEFAULT_NL80211_CMD_SOCK_ID				(777)
#define DEFAULT_NL80211_EVENT_SOCK_ID				(778)

#define QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION	(74)
#define QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS			(200)
#define QCA_NL80211_VENDOR_SUBCMD_TELEMETRIC_DATA		(334)
#define QCA_NL80211_VENDOR_SUBCMD_LIST_STA			(214)
#define QCA_NL80211_VENDOR_SUBCMD_PEER_STATS_CACHE_FLUSH	(178)

#define OL_ATH_PARAM_FLUSH_PEER_RATE_STATS			(405)
#define OL_ATH_PARAM_SHIFT					(0x1000)

#define IEEE80211_PARAM_GET_OPMODE				(616)
#define IEEE80211_M_HOSTAP					(6)
#define IEEE80211_PARAM_MLD_NETDEV_NAME				(798)

#define NL80211_ATTR_VENDOR_DATA				(197)

/**
 * enum qca_wlan_vendor_attr_peer_stats_cache_params - This enum defines
 * attributes required for QCA_NL80211_VENDOR_SUBCMD_PEER_STATS_CACHE_FLUSH
 * Information in these attributes is used to flush peer rate statistics from
 * the driver to user application.
 *
 * @QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_TYPE: Unsigned 32-bit attribute
 * Indicate peer statistics cache type.
 * The statistics types are 32-bit values from
 * enum qca_vendor_attr_peer_stats_cache_type.
 * @QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_PEER_MAC: Unsigned 8-bit array
 * of size 6 octets, representing the peer MAC address.
 * @QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_DATA: Opaque data attribute
 * containing buffer of statistics to send to application layer entity.
 * @QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_PEER_COOKIE: Unsigned 64-bit attribute
 * representing a cookie for peer unique session.
 */
enum qca_wlan_vendor_attr_peer_stats_cache_params {
	QCA_WLAN_VENDOR_ATTR_PEER_STATS_INVALID = 0,
	QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_TYPE = 1,
	QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_PEER_MAC = 2,
	QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_DATA = 3,
	QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_PEER_COOKIE = 4,
	/* Keep last */
	QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_LAST,
	QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_MAX =
		QCA_WLAN_VENDOR_ATTR_PEER_STATS_CACHE_LAST - 1
};

/**
 * enum qca_wlan_vendor_attr_stats: Defines attributes to be used in response of
 * QCA_NL80211_VENDOR_SUBCMD_TELEMETRIC_DATA vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_STATS_LEVEL: Used for stats levels like Basic or
 * Advance or Debug.
 *
 * @QCA_WLAN_VENDOR_ATTR_STATS_OBJECT: Required (u8)
 * Used with the command, carrying stats, to specify for which stats_object_e.
 *
 * @QCA_WLAN_VENDOR_ATTR_STATS_OBJ_ID: Used for Object ID like for STA MAC
 * address or for VAP or Radio or SoC respective interface name.
 *
 * QCA_WLAN_VENDOR_ATTR_STATS_SERVICEID: Used for sawf levels stats like per
 * peer or per peer per serviceclass.
 *
 * @QCA_WLAN_VENDOR_ATTR_STATS_PARENT_IF: Used for Parent Object interface name
 * like for STA VAP name, for VAP Radio interface name and for Radio SoC
 * interface name.
 *
 * @QCA_WLAN_VENDOR_ATTR_STATS_TYPE: Used for stats types like Data or
 * control.
 *
 * @QCA_WLAN_VENDOR_ATTR_STATS_RECURSIVE: Required (NESTED Flag)
 * Used with the command to specify the nested stats.
 *
 * @QCA_WLAN_VENDOR_ATTR_STATS_MULTI_REPLY: Set this flag if current reply
 * messageis holding data from previous reply.
 *
 * @QCA_WLAN_VENDOR_ATTR_STATS_MAX: Defines maximum attriutes can be used in
 * QCA_NL80211_VENDOR_SUBCMD_TELEMETRIC_DATA vendor command response.
 */
enum qca_wlan_vendor_attr_stats {
	QCA_WLAN_VENDOR_ATTR_STATS_LEVEL = 1,
	QCA_WLAN_VENDOR_ATTR_STATS_OBJECT,
	QCA_WLAN_VENDOR_ATTR_STATS_OBJ_ID,
	QCA_WLAN_VENDOR_ATTR_STATS_SERVICEID,
	QCA_WLAN_VENDOR_ATTR_STATS_PARENT_IF,
	QCA_WLAN_VENDOR_ATTR_STATS_TYPE,
	QCA_WLAN_VENDOR_ATTR_STATS_RECURSIVE,
	QCA_WLAN_VENDOR_ATTR_STATS_MULTI_REPLY,
	QCA_WLAN_VENDOR_ATTR_STATS_MAX,
};

/**
 * enum qca_wlan_vendor_attr_feat: Defines nested attributes to be used in
 * response of QCA_NL80211_VENDOR_SUBCMD_TELEMETRIC_DATA vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_ME: Used for Multicast Enhancement stats for a
 * particular stats object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_RX: Used for Rx stats for a particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_TX: Used for Tx stats for a particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_AST: Used for AST stats for a particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_CFR: Used for CFR stats for a particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_FWD: Used for BSS stats for a particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_RAW: Used for RAW mode stats for a particular
 * object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_TSO: Used for TSO stats for a particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_TWT: Used for TWT stats for a particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_VOW: Used for VOW  stats for a particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_WDI: Used for WDI stats for a particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_WMI: Used for WMI stats for a particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_IGMP: Used for IGMP stats for a particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_LINK: Used for Link related stats for a particular
 * object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_MESH: Used for Mesh related stats for a particular
 * object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_RATE: Used for Rate stats for a particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_NAWDS: Used for NAWDS related stats for a
 * particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_DELAY: Used for DELAY related stats for a
 * particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_JITTER: Used for JITTER related stats for a
 * particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_TXCAP: Used for TXCAP realted stats for a
 * particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_MONITOR: Used for MONITOR realted stats for a
 * particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_SAWFDELAY: Used for SAWFDELAY related stats for a
 * particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_SAWFTX: Used for SAWFTX related stats for a
 * particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_DETER: Used for DETERMINISTIC related stats for a
 * particular object.
 *
 * @QCA_WLAN_VENDOR_ATTR_FEAT_MAX: Defines Maximum count of feature attributes.
 */
enum qca_wlan_vendor_attr_feat {
	QCA_WLAN_VENDOR_ATTR_FEAT_ME = 1,
	QCA_WLAN_VENDOR_ATTR_FEAT_RX,
	QCA_WLAN_VENDOR_ATTR_FEAT_TX,
	QCA_WLAN_VENDOR_ATTR_FEAT_AST,
	QCA_WLAN_VENDOR_ATTR_FEAT_CFR,
	QCA_WLAN_VENDOR_ATTR_FEAT_FWD,
	QCA_WLAN_VENDOR_ATTR_FEAT_RAW,
	QCA_WLAN_VENDOR_ATTR_FEAT_TSO,
	QCA_WLAN_VENDOR_ATTR_FEAT_TWT,
	QCA_WLAN_VENDOR_ATTR_FEAT_VOW,
	QCA_WLAN_VENDOR_ATTR_FEAT_WDI,
	QCA_WLAN_VENDOR_ATTR_FEAT_WMI,
	QCA_WLAN_VENDOR_ATTR_FEAT_IGMP,
	QCA_WLAN_VENDOR_ATTR_FEAT_LINK,
	QCA_WLAN_VENDOR_ATTR_FEAT_MESH,
	QCA_WLAN_VENDOR_ATTR_FEAT_RATE,
	QCA_WLAN_VENDOR_ATTR_FEAT_NAWDS,
	QCA_WLAN_VENDOR_ATTR_FEAT_DELAY,
	QCA_WLAN_VENDOR_ATTR_FEAT_JITTER,
	QCA_WLAN_VENDOR_ATTR_FEAT_TXCAP,
	QCA_WLAN_VENDOR_ATTR_FEAT_MONITOR,
	QCA_WLAN_VENDOR_ATTR_FEAT_SAWFDELAY,
	QCA_WLAN_VENDOR_ATTR_FEAT_SAWFTX,
	QCA_WLAN_VENDOR_ATTR_FEAT_DETER,
	/**
	 * New attribute must be add before this.
	 * Also define the corresponding feature
	 * index in stats_feat_index_e.
	 */
	QCA_WLAN_VENDOR_ATTR_FEAT_MAX,
};

/**
 * enum qca_wlan_vendor_attr_telemetric: Defines attributes to be used in
 * request message of QCA_NL80211_VENDOR_SUBCMD_TELEMETRIC_DATA vendor command.
 *
 * @QCA_WLAN_VENDOR_ATTR_TELEMETRIC_LEVEL: Defines stats levels like Basic or
 * Advance or Debug.
 *
 * @QCA_WLAN_VENDOR_ATTR_TELEMETRIC_OBJECT: Defines stats objects like STA or
 * VAP or Radio or SoC.
 *
 * @QCA_WLAN_VENDOR_ATTR_TELEMETRIC_TYPE: Defines stats types like Data or
 * control.
 *
 * @QCA_WLAN_VENDOR_ATTR_TELEMETRIC_AGGREGATE: Defines aggregation flag for
 * driver agrregation.
 *
 * @QCA_WLAN_VENDOR_ATTR_TELEMETRIC_FEATURE_FLAG: Defines feature flags for
 * which stats is requested.
 *
 * @QCA_WLAN_VENDOR_ATTR_TELEMETRIC_STA_MAC: Defines STA MAC Address if the
 * request is for particular STA object.
 *
 * @QCA_WLAN_VENDOR_ATTR_TELEMETRIC_SERVICEID: Defines serviceid for sawf stats.
 *
 * @QCA_WLAN_VENDOR_ATTR_TELEMETRIC_MLD_LINK: Defines if stats request is for
 * interface which is part of MLD.
 *
 * @QCA_WLAN_VENDOR_ATTR_TELEMETRIC_MAX: Defines maximum attribute counts to be
 * used in QCA_NL80211_VENDOR_SUBCMD_TELEMETRIC_DATA vendor command request.
 */
enum qca_wlan_vendor_attr_telemetric {
	QCA_WLAN_VENDOR_ATTR_TELEMETRIC_LEVEL = 1,
	QCA_WLAN_VENDOR_ATTR_TELEMETRIC_OBJECT,
	QCA_WLAN_VENDOR_ATTR_TELEMETRIC_TYPE,
	QCA_WLAN_VENDOR_ATTR_TELEMETRIC_AGGREGATE,
	QCA_WLAN_VENDOR_ATTR_TELEMETRIC_FEATURE_FLAG,
	QCA_WLAN_VENDOR_ATTR_TELEMETRIC_STA_MAC,
	QCA_WLAN_VENDOR_ATTR_TELEMETRIC_SERVICEID,
	QCA_WLAN_VENDOR_ATTR_TELEMETRIC_MLD_LINK,
	QCA_WLAN_VENDOR_ATTR_TELEMETRIC_MAX,
};

#define IEEE80211_ADDR_LEN 6
#define IEEE80211_RATE_MAXSIZE 44
#define RRM_CAPS_LEN 5
#define MAX_NUM_OPCLASS_SUPPORTED 64
#define HEHANDLE_CAP_TXRX_MCS_NSS_SIZE 3
#define HEHANDLE_CAP_PHYINFO_SIZE 3

/**
 * enum wlan_band_id - Operational wlan band id
 * @WLAN_BAND_UNSPECIFIED: Band id not specified. Default to 2GHz/5GHz band
 * @WLAN_BAND_2GHZ: Band 2.4 GHz
 * @WLAN_BAND_5GHZ: Band 5 GHz
 * @WLAN_BAND_6GHZ: Band 6 GHz
 * @WLAN_BAND_MAX:  Max supported band
 */
enum wlan_band_id {
	WLAN_BAND_UNSPECIFIED = 0,
	WLAN_BAND_2GHZ = 1,
	WLAN_BAND_5GHZ = 2,
	WLAN_BAND_6GHZ = 3,
	/* Add new band definitions here */
	WLAN_BAND_MAX,
};

typedef __s64 time64_t;
typedef __u64 timeu64_t;

struct timespec64 {
	time64_t	tv_sec;                 /* seconds */
	long		tv_nsec;                /* nanoseconds */
};

typedef struct timespec64 timespec_t;

/*
 * Station information block; the mac address is used
 * to retrieve other data like stats, unicast key, etc.
 */
struct ieee80211req_sta_info {
	u_int16_t       isi_len;                /* length (mult of 4) */
	u_int16_t       isi_freq;               /* MHz */
	int32_t         isi_nf;                 /* noise floor */
	u_int16_t       isi_ieee;               /* IEEE channel number */
	u_int32_t       awake_time;             /* time is active mode */
	u_int32_t       ps_time;                /* time in power save mode */
	u_int64_t       isi_flags;              /* channel flags */
	u_int16_t       isi_state;              /* state flags */
	u_int8_t        isi_authmode;           /* authentication algorithm */
	int8_t          isi_rssi;
	int8_t          isi_min_rssi;
	int8_t          isi_max_rssi;
	u_int16_t       isi_capinfo;            /* capabilities */
	u_int16_t       isi_pwrcapinfo;         /* power capabilities */
	u_int8_t        isi_athflags;           /* Atheros capabilities */
	u_int8_t        isi_erp;                /* ERP element */
	u_int8_t        isi_ps;                 /* psmode */
	u_int8_t        isi_macaddr[IEEE80211_ADDR_LEN];
	u_int8_t        isi_nrates;             /* negotiated rates */
	u_int8_t        isi_rates[IEEE80211_RATE_MAXSIZE];
	u_int8_t        isi_txrate;             /* index to isi_rates[] */
	u_int32_t   isi_txratekbps; /* tx rate in Kbps, for 11n */
	u_int16_t       isi_ie_len;             /* IE length */
	u_int16_t       isi_associd;            /* assoc response */
	u_int16_t       isi_txpower;            /* current tx power */
	u_int16_t       isi_vlan;               /* vlan tag */
	u_int16_t       isi_txseqs[17];         /* seq to be transmitted */
	u_int16_t       isi_rxseqs[17];         /* seq previous for qos frames*/
	u_int16_t       isi_inact;              /* inactivity timer */
	u_int8_t        isi_uapsd;              /* UAPSD queues */
	u_int8_t        isi_opmode;             /* sta operating mode */
	u_int8_t        isi_cipher;
	u_int32_t       isi_assoc_time;         /* sta association time */
	timespec_t isi_tr069_assoc_time;   /* sta association time in timespec*/
	u_int16_t   isi_htcap;      /* HT capabilities */
	u_int32_t   isi_rxratekbps; /* rx rate in Kbps */
	/* We use this as a common variable for legacy rates
	 * and lln. We do not attempt to make it symmetrical
	 * to isi_txratekbps and isi_txrate, which seem to be
	 * separate due to legacy code.
	 */
	/* XXX frag state? */
	/* variable length IE data */
	u_int32_t isi_maxrate_per_client; /* Max rate per client */
	u_int16_t   isi_stamode;        /* Wireless mode for connected sta */
	u_int32_t isi_ext_cap;              /* Extended capabilities */
	u_int32_t isi_ext_cap2;              /* Extended capabilities 2 */
	u_int32_t isi_ext_cap3;              /* Extended capabilities 3 */
	u_int32_t isi_ext_cap4;              /* Extended capabilities 4 */
	u_int8_t isi_nss;         /* number of tx and rx chains */
	u_int8_t isi_supp_nss;    /* number of tx and rx chains supported */
	u_int8_t isi_is_256qam;    /* 256 QAM support */
	u_int8_t isi_operating_bands : 2; /* Operating bands */
#if ATH_SUPPORT_EXT_STAT
	u_int8_t  isi_chwidth;            /* communication band width */
	u_int32_t isi_vhtcap;             /* VHT capabilities */
#endif
#if ATH_EXTRA_RATE_INFO_STA
	u_int8_t isi_tx_rate_mcs;
	u_int8_t isi_tx_rate_flags;
	u_int8_t isi_rx_rate_mcs;
	u_int8_t isi_rx_rate_flags;
#endif
	u_int8_t isi_rrm_caps[RRM_CAPS_LEN];    /* RRM capabilities */
	u_int8_t isi_curr_op_class;
	u_int8_t isi_num_of_supp_class;
	u_int8_t isi_supp_class[MAX_NUM_OPCLASS_SUPPORTED];
	u_int8_t isi_nr_channels;
	u_int8_t isi_first_channel;
	u_int16_t isi_curr_mode;
	u_int8_t isi_beacon_measurement_support;
	enum wlan_band_id isi_band; /* band info: 2G,5G,6G */
	u_int8_t isi_is_he;
	u_int16_t isi_hecap_rxmcsnssmap[HEHANDLE_CAP_TXRX_MCS_NSS_SIZE];
	u_int16_t isi_hecap_txmcsnssmap[HEHANDLE_CAP_TXRX_MCS_NSS_SIZE];
	u_int32_t isi_hecap_phyinfo[HEHANDLE_CAP_PHYINFO_SIZE];
	u_int8_t isi_he_mu_capable;
#if WLAN_FEATURE_11BE
	u_int8_t isi_is_eht;
	uint32_t isi_ehtcap_rxmcsnssmap[EHTHANDLE_CAP_TXRX_MCS_NSS_SIZE];
	uint32_t isi_ehtcap_txmcsnssmap[EHTHANDLE_CAP_TXRX_MCS_NSS_SIZE];
	u_int32_t isi_ehtcap_phyinfo[EHTHANDLE_CAP_PHYINFO_SIZE];
#endif /* WLAN_FEATURE_11BE */
#if WLAN_FEATURE_11BE_MLO
	u_int8_t isi_is_mlo;
	u_int8_t isi_mldaddr[IEEE80211_ADDR_LEN];
	u_int8_t isi_num_links;
	struct mlo_link_peer_info isi_link_info[IEEE80211_MAX_MLD_LINKS];
#endif /* WLAN_FEATURE_11BE_MLO */
};
#endif /* BUILD_PROFILE_OPEN */
#endif
