/*
 * hostapd / IEEE 802.11 Management
 * Copyright (c) 2002-2009, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef IEEE802_11_H
#define IEEE802_11_H

#include "utils/list.h"
#include "hostapd.h"

struct hostapd_iface;
struct hostapd_data;
struct sta_info;
struct hostapd_frame_info;
struct ieee80211_ht_capabilities;
struct ieee80211_vht_capabilities;
struct ieee80211_mgmt;
struct radius_sta;
enum ieee80211_op_mode;
struct mac_acl_entry;
enum oper_chan_width;
struct ieee802_11_elems;
struct sae_pk;
struct sae_pt;
struct sae_password_entry;
struct mld_info;
struct mld_link_info;
struct rsn_pmksa_cache_entry;

#define BITRATE_5_5_MBPS 55

#define INCLUDE_ELEMENT_IN_BEACON        BIT(0)
#define INCLUDE_ELEMENT_IN_PROBE_RESP    BIT(1)

enum colocation_mode {
	NO_COLOCATED_6GHZ,
	STANDALONE_6GHZ,
	COLOCATED_6GHZ,
	COLOCATED_LOWER_BAND,
};

enum colocation_mode get_colocation_mode(struct hostapd_data *hapd);

enum link_parse_type {
	LINK_PARSE_ASSOC,
	LINK_PARSE_REASSOC,
	LINK_PARSE_RECONF,
	LINK_PARSE_UHR_RECONF_ASSOC,
	LINK_PARSE_UHR_RECONF_LINK,
};

#define LINK_RECONF_GROUP_KDE_MAX_LEN 255

#define MBSSID_NON_TX_DEF_OPTIONAL_ELEM_SIZE 160
#define MBSSID_NON_TX_DEF_VENDOR_ELEM_SIZE 128
#define MAX_MBSSID_NONINHERIT_ELEM_SIZE 100

#ifdef CONFIG_QCN_EXTN
#define MBSSID_NON_TX_OPTIONAL_ELEM_SIZE(bss)             \
	((bss)->conf->bss_extn.nontx_optional_elem_size)

#define MBSSID_NON_TX_VENDOR_ELEM_SIZE(bss)               \
	((bss)->conf->bss_extn.nontx_vendor_elem_size)
#else
#define MBSSID_NON_TX_OPTIONAL_ELEM_SIZE(bss)             \
	(MBSSID_NON_TX_DEF_OPTIONAL_ELEM_SIZE)

#define MBSSID_NON_TX_VENDOR_ELEM_SIZE(bss)               \
	(MBSSID_NON_TX_DEF_VENDOR_ELEM_SIZE)
#endif

struct link_reconf_req_info {
	struct dl_list list;
	u16 status;
	u8 link_id;
	u8 local_addr[ETH_ALEN];
	u8 peer_addr[ETH_ALEN];
	size_t sta_prof_len;
	u8 sta_prof[];
};

struct link_reconf_req_list {
	u8 sta_mld_addr[ETH_ALEN];
	u8 dialog_token;
	u16 links_add_ok;
	u16 links_del_ok;
	u16 new_valid_links;
	struct dl_list del_req; /* list of struct link_reconf_req_info */
	struct dl_list add_req; /* list of struct link_reconf_req_info */
};

int ieee802_11_mgmt(struct hostapd_data *hapd, const u8 *buf, size_t len,
		    struct hostapd_frame_info *fi);
void ieee802_11_mgmt_cb(struct hostapd_data *hapd, const u8 *buf, size_t len,
			u16 stype, int ok);
void hostapd_2040_coex_action(struct hostapd_data *hapd,
			      const struct ieee80211_mgmt *mgmt, size_t len);

int hostapd_config_read_maclist(const char *fname,
				struct mac_acl_entry **acl, int *num);
#ifdef NEED_AP_MLME
int ieee802_11_get_mib(struct hostapd_data *hapd, char *buf, size_t buflen);
int ieee802_11_get_mib_sta(struct hostapd_data *hapd, struct sta_info *sta,
			   char *buf, size_t buflen);
#else /* NEED_AP_MLME */
static inline int ieee802_11_get_mib(struct hostapd_data *hapd, char *buf,
				     size_t buflen)
{
	return 0;
}

static inline int ieee802_11_get_mib_sta(struct hostapd_data *hapd,
					 struct sta_info *sta,
					 char *buf, size_t buflen)
{
	return 0;
}
#endif /* NEED_AP_MLME */
struct wpa_state_machine *get_wpa_sm_from_ft_ds_list(struct hostapd_data *hapd,
						     uint8_t *sta_mld_addr);
void
initiate_assoc_response(struct hostapd_data *hapd, struct sta_info *sta,
			     int resp, int reassoc,
			     uint8_t *tmp, const u8 *pos, int left,
			     int omit_rsnxe, uint8_t *sa, int rssi,
			     bool set_beacon);

u16 hostapd_own_capab_info(struct hostapd_data *hapd);
void ap_ht2040_timeout(void *eloop_data, void *user_data);
u8 * hostapd_eid_ext_capab(struct hostapd_data *hapd, u8 *eid,
			   bool mbssid_complete);
u8 * hostapd_eid_qos_map_set(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_supp_rates(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_ext_supp_rates(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_rm_enabled_capab(struct hostapd_data *hapd, u8 *eid,
				  size_t len);
u8 * hostapd_eid_ht_capabilities(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_ht_operation(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_vht_capabilities(struct hostapd_data *hapd, u8 *eid, u32 nsts);
u8 * hostapd_eid_vht_operation(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_vendor_vht(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_chsw_wrapper(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_txpower_envelope(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_he_capab(struct hostapd_data *hapd, u8 *eid,
			  enum ieee80211_op_mode opmode);
u8 * hostapd_eid_he_operation(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_he_mu_edca_parameter_set(struct hostapd_data *hapd, u8 *eid,
					  bool is_epcs);
u8 * hostapd_eid_spatial_reuse(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_he_6ghz_band_cap(struct hostapd_data *hapd, u8 *eid);
#ifdef CONFIG_IEEE80211BN
u8 * hostapd_eid_smd_ie(struct hostapd_data *hapd, u8 *eid);
#endif /* CONFIG_IEEE80211BN */

int hostapd_ht_operation_update(struct hostapd_iface *iface);
void ieee802_11_send_sa_query_req(struct hostapd_data *hapd,
				  const u8 *addr, const u8 *trans_id);
void hostapd_get_ht_capab(struct hostapd_data *hapd,
			  struct ieee80211_ht_capabilities *ht_cap,
			  struct ieee80211_ht_capabilities *neg_ht_cap);
void hostapd_get_vht_capab(struct hostapd_data *hapd,
			   struct ieee80211_vht_capabilities *vht_cap,
			   struct ieee80211_vht_capabilities *neg_vht_cap);
void hostapd_get_he_capab(struct hostapd_data *hapd,
			  const struct ieee80211_he_capabilities *he_cap,
			  struct ieee80211_he_capabilities *neg_he_cap,
			  size_t he_capab_len);
void hostapd_get_eht_capab(struct hostapd_data *hapd,
			   const struct ieee80211_eht_capabilities *src,
			   struct ieee80211_eht_capabilities *dest,
			   size_t len);
void hostapd_get_uhr_capab(const struct ieee80211_uhr_capabilities *src,
			   struct ieee80211_uhr_capabilities *dest,
			   size_t len);
int add_associated_sta(struct hostapd_data *hapd,
		       struct sta_info *sta, int reassoc);
u8 * hostapd_eid_eht_ml_beacon(struct hostapd_data *hapd,
			       struct mld_info *mld_info,
			       u8 *eid, bool include_mld_id,
			       u8 include_ext_cap, bool is_uhr_sta,
			       bool include_bss_load_age, bool skip_uhr_extn);
u8 * hostapd_eid_eht_ml_assoc(struct hostapd_data *hapd, struct sta_info *info,
			      u8 *eid, u8 include_ext_cap);
size_t hostapd_eid_eht_basic_ml_len(struct hostapd_data *hapd,
				    struct sta_info *info,
				    bool include_mld_id, bool include_pbcc,
				    u8 include_ext_cap, bool is_uhr_sta,
				    bool include_bss_load_age, bool skip_uhr_extn);
size_t hostapd_eid_eht_ml_beacon_len(struct hostapd_data *hapd,
				     struct mld_info *info,
				     bool include_mld_id,
				     u8 include_ext_cap, bool is_uhr_sta,
				     bool include_bss_load_age, bool skip_uhr_extn);
size_t hostapd_eid_eht_ml_len(struct hostapd_data *hapd, struct mld_info *info,
			      bool include_mld_id, bool include_bpcc,
			      u8 include_ext_cap, bool is_uhr_sta,
			      bool include_bss_load_age, bool skip_uhr_extn);
u8 * hostapd_eid_eht_basic_ml_common(struct hostapd_data *hapd,
				     u8 *eid, struct mld_info *mld_info,
				     bool include_mld_id, bool include_bpcc,
				     u8 include_ext_cap, bool is_smd,
				     bool is_uhr_sta, bool include_bss_load_age,
				     bool skip_uhr_extn);
struct wpabuf * hostapd_ml_auth_resp(struct hostapd_data *hapd);
const u8 * auth_skip_fixed_fields(struct hostapd_data *hapd,
				  const struct ieee80211_mgmt *mgmt,
				  size_t len);
const u8 * hostapd_process_ml_auth(struct hostapd_data *hapd,
				   const struct ieee80211_mgmt *mgmt,
				   size_t len);
const u8 * skip_ml_auth_fixed_fields(struct hostapd_data *hapd,
				     const struct ieee80211_mgmt *mgmt,
				     size_t len);
u16 hostapd_process_ml_assoc_req(struct hostapd_data *hapd,
				 struct ieee802_11_elems *elems,
				 struct sta_info *sta);
int hostapd_process_ml_assoc_req_addr(struct hostapd_data *hapd,
				      const u8 *basic_mle, size_t basic_mle_len,
				      u8 *mld_addr);
int hostapd_get_aid(struct hostapd_data *hapd, struct sta_info *sta);
int hostapd_get_wds_mld_sta_uid(struct hostapd_data *hapd, struct sta_info *sta);
void hostapd_free_wds_mld_sta_uid(struct hostapd_data *hapd, struct sta_info *sta);
void hostapd_set_sta_flag_to_partner_links(struct hostapd_data *hapd,
					   struct sta_info *sta);
u16 copy_sta_ht_capab(struct hostapd_data *hapd, struct sta_info *sta,
		      const u8 *ht_capab);
u16 copy_sta_vendor_vht(struct hostapd_data *hapd, struct sta_info *sta,
			const u8 *ie, size_t len);

int update_ht_state(struct hostapd_data *hapd, struct sta_info *sta);
void ht40_intolerant_add(struct hostapd_iface *iface, struct sta_info *sta);
void ht40_intolerant_remove(struct hostapd_iface *iface, struct sta_info *sta);
u16 copy_sta_vht_capab(struct hostapd_data *hapd, struct sta_info *sta,
		       const u8 *vht_capab);
u16 copy_sta_vht_oper(struct hostapd_data *hapd, struct sta_info *sta,
		      const u8 *vht_oper);
u16 set_sta_vht_opmode(struct hostapd_data *hapd, struct sta_info *sta,
		       const u8 *vht_opmode);
u16 copy_sta_he_capab(struct hostapd_data *hapd, struct sta_info *sta,
		      enum ieee80211_op_mode opmode, const u8 *he_capab,
		      size_t he_capab_len);
u16 copy_sta_he_6ghz_capab(struct hostapd_data *hapd, struct sta_info *sta,
			   const u8 *he_6ghz_capab);
int hostapd_get_he_twt_responder(struct hostapd_data *hapd,
				 enum ieee80211_op_mode mode);
bool hostapd_get_ht_vht_twt_responder(struct hostapd_data *hapd);
void hostapd_wfa_capab(struct hostapd_data *hapd, struct sta_info *sta,
		       const u8 *pos, const u8 *end);
u8 * hostapd_eid_cca(struct hostapd_data *hapd, u8 *eid);
/* IE validation for link reconfiguration */
int check_assoc_ies(struct hostapd_data *hapd, struct sta_info *sta,
                   const u8 *ies, size_t ies_len, enum link_parse_type type);
void hostapd_tx_status(struct hostapd_data *hapd, const u8 *addr,
		       const u8 *buf, size_t len, int ack);
void ieee802_11_rx_from_unknown(struct hostapd_data *hapd, const u8 *src,
				int wds);
u8 * hostapd_eid_assoc_comeback_time(struct hostapd_data *hapd,
				     struct sta_info *sta, u8 *eid);
void ieee802_11_sa_query_action(struct hostapd_data *hapd,
				const struct ieee80211_mgmt *mgmt,
				size_t len);
u8 * hostapd_eid_interworking(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_adv_proto(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_roaming_consortium(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_time_adv(struct hostapd_data *hapd, u8 *eid);
size_t hostapd_eid_channel_usage_len(struct hostapd_data *hapd);
u8 * hostapd_eid_channel_usage(struct hostapd_data *hapd, u8 *eid,
								size_t limit);
u8 * hostapd_eid_country(struct hostapd_data *hapd, u8 *eid, int max_len);
size_t hostapd_eid_country_len(struct hostapd_data *hapd);
u8 * hostapd_eid_time_zone(struct hostapd_data *hapd, u8 *eid);
int hostapd_update_time_adv(struct hostapd_data *hapd);
void hostapd_client_poll_ok(struct hostapd_data *hapd, const u8 *addr);
u8 * hostapd_eid_bss_max_idle_period(struct hostapd_data *hapd, u8 *eid,
				     u16 value);

int auth_sae_init_committed(struct hostapd_data *hapd, struct sta_info *sta);
#ifdef CONFIG_SAE
void sae_clear_retransmit_timer(struct hostapd_data *hapd,
				struct sta_info *sta);
void sae_accept_sta(struct hostapd_data *hapd, struct sta_info *sta);
int sae_sm_step(struct hostapd_data *hapd, struct sta_info *sta,
		u16 auth_transaction, u16 status_code, int allow_reuse,
		int *sta_removed);
void sae_sme_send_external_auth_status(struct hostapd_data *hapd,
				       struct sta_info *sta, u16 status);
int sae_status_success(struct hostapd_data *hapd, u16 status_code);
#else /* CONFIG_SAE */
static inline void sae_clear_retransmit_timer(struct hostapd_data *hapd,
					      struct sta_info *sta)
{
}
#endif /* CONFIG_SAE */

void hostap_ft_ds_ml_sta_timeout(void *eloop_ctx, void *timeout_ctx);

u8 * hostapd_eid_rm_enabled_capab(struct hostapd_data *hapd,
						 u8 *eid, size_t len);

#ifdef CONFIG_MBO

u8 * hostapd_eid_mbo(struct hostapd_data *hapd, u8 *eid, size_t len);

u8 hostapd_mbo_ie_len(struct hostapd_data *hapd);
u8 * hostapd_eid_ess_report(struct hostapd_data *hapd, u8 *eid,
			    size_t len);
u8 * hostapd_eid_ap_channel_report(struct hostapd_data *hapd, u8 *eid,
				   size_t len);

u8 * hostapd_eid_mbo_rssi_assoc_rej(struct hostapd_data *hapd, u8 *eid,
				    size_t len, int delta);

#else /* CONFIG_MBO */

static inline u8 * hostapd_eid_mbo(struct hostapd_data *hapd, u8 *eid,
				   size_t len)
{
	return eid;
}

static inline u8 hostapd_mbo_ie_len(struct hostapd_data *hapd)
{
	return 0;
}

static inline u8 * hostapd_eid_ap_channel_report(struct hostapd_data *hapd,
						  u8 *eid, size_t len)
{
	return eid;
}

#endif /* CONFIG_MBO */

#define INVALID_EDGE 0xFFF
#define INVALID_DBR    100
#define INVALID_PSD (-1270) /* -127 multiplied by 10 */

/* Have the entire 6Ghz band as single range */
#define DEFAULT_LOW_6GFREQ     5925
#define DEFAULT_HIGH_6GFREQ    7125
#define MAX_PUNC_MASK_LIMITS      3
#define CHWIDTH_20               20  /* Channel width 20 */
#define CHWIDTH_40               40  /* Channel width 40 */
#define CHWIDTH_80               80  /* Channel width 80 */
#define CHWIDTH_160             160  /* Channel width 160 */
#define CHWIDTH_320             320  /* Channel width 320 */

/* in the bitmap 0 indicates no puncturing and 1 indicated that sub channel is
 * punctured
 */
#define PUNCTURE_INVALID     0xFFFF
#define PUNCTURE_NONE        0x0000
#define PUNCTURE_80MHZ_MASK  0x000F
#define PUNCTURE_160MHZ_MASK 0x00FF
#define PUNCTURE_320MHZ_MASK 0xFFFF
#define PUNCTURE_40MHZ_MASK  0x0003

/**
 * struct punct_mask - Structure to hold puncture mask limits
 * @offset: Array of offsets for puncture mask limits
 * @dbr: Array of dbr values corresponding to the offsets
 *
 * This structure is used to define the puncture mask limits for different
 * bandwidths. The `offset` array holds the offset values, and the `dbr` array
 * holds the corresponding dbr values. The size of both arrays is defined by
 * `MAX_PUNC_MASK_LIMITS`.
 */
struct punct_mask {
	s16 offset[MAX_PUNC_MASK_LIMITS];
	s16 dbr[MAX_PUNC_MASK_LIMITS];
};

/**
 * enum puncture_type - Enumeration of puncture types
 * @PUNCTURE_TYPE_EDGE: Represents edge puncture type
 * @PUNCTURE_TYPE_INTERIM_20_PLUS: Represents interim puncture type with 20 MHz
 * plus
 * @PUNCTURE_TYPE_INTERIM_20: Represents interim puncture type with 20 MHz
 * @PUNCTURE_TYPE_INVALID: Represents an invalid puncture type
 *
 * This enumeration defines the different types of punctures that can occur
 * within a given bandwidth. Each type specifies a unique puncture pattern
 * and is used to determine the appropriate mask limits for the puncture.
 */
enum puncture_type {
	PUNCTURE_TYPE_EDGE = 0,
	PUNCTURE_TYPE_INTERIM_20_PLUS,
	PUNCTURE_TYPE_INTERIM_20,
	PUNCTURE_TYPE_INVALID,
};

/**
 * pdbm1, pdbm2 and pdbm3 - Array of dbr values for puncture mask type
 * PUNCTURE_TYPE_EDGE, PUNCTURE_TYPE_INTERIM_20_PLUS and
 * PUNCTURE_TYPE_INTERIM_20 respectively.
 */
static const s16 pdbm1[3] = {0, -200, -280};
static const s16 pdbm2[3] = {0, -200, -250};
static const s16 pdbm3[3] = {0, -200, -230};

#define CHAN_MAX_PSD_POWER   127

/**
 * get_min_psd_values - Calculate the minimum PSD values for a given frequency
 * and bandwidth
 * @afc_rsp_info: Pointer to the AFC response information structure containing
 * frequency-specific PSD limits
 * @freq: Frequency for which the minimum PSD values are to be calculated
 * @cfreq: Center frequency of the channel
 * @punc_bitmap: Bitmap indicating the punctured sub-channels
 * @bw: Bandwidth of the channel
 * @min_psd: Pointer to the variable where the minimum PSD value will be stored
 *
 * This function calculates the minimum PSD (Power Spectral Density) values for
 * a given frequency and bandwidth. It determines the puncture type and
 * calculates the regulatory mask values based on the puncture mask limits. The
 * minimum PSD value is then calculated by iterating through the adjacent
 * frequencies and applying the regulatory mask values.
 */
void
get_min_psd_values(struct afc_sp_reg_info *afc_rsp_info, u16 freq, u16 cfreq,
		   u16 punc_bitmap, u16 bw, s16 *min_psd);

/**
 * hostapd_get_eirp_arr_for_6ghz - Get EIRP array for 6 GHz band.
 * @iface: hostapd interface data structure.
 * @freq: Primary channel frequency in MHz.
 * @cen320: Center frequency for 320 MHz operation, if applicable.
 * @chanwidth: Channel width for which EIRP is being calculated.
 * @client_type: Client type for which EIRP is being calculated.
 * @max_eirp_arr: Output array to store maximum EIRP values for each
 * bandwidth.
 * @pwr_mode: Power mode for EIRP calculation.
 * @tx_pwr_intrpn: Interpretation of maximum transmit power.
 *
 */
void
hostapd_get_eirp_arr_for_6ghz(struct hostapd_iface *iface,
			      u16 freq,
			      u8 cen320,
			      enum chan_width chanwidth,
			      u8 client_type,
			      s8 *max_eirp_arr,
			      u8 pwr_mode,
			      enum max_tx_pwr_interpretation tx_pwr_intrpn);

/**
 * get_chan_list() - Locate 6 GHz channel ranges in list
 *
 * Derives indices and counts for 11ax/11be channel ranges within @chan_data
 * relative to the operating channel in @hapd.
 *
 * @hapd: Hostapd BSS context
 * @non_11be_start_idx: Output start index of non-11be channels
 * @chan_start_idx: Output start index of the operating channel window
 * @non_11be_chan_count: Output count of non-11be channels
 * @total_chan_count: Output total channels to consider from start index
 * @chan_data: Ordered channel list to examine
 *
 * Return: 0 on success, -1 on invalid indices or list
 */

int get_chan_list(struct hostapd_data *hapd, int *non_11be_start_idx,
		  int *chan_start_idx, int *non_11be_chan_count,
		  int *total_chan_count, struct ieee_chan_data chan_data);

/**
 * set_ieee_order_chan_list() - Build ordered channel list
 *
 * Populates @chan_data with an IEEE-ordered list of channels from @mode,
 * ordered for processing PSD/EIRP per client regulatory mode.
 *
 * @mode: Current hardware mode and channels
 * @chan_data: Output channel list container (allocated/populated)
 * @client_mode: Regulatory client mode (LPI/SP/subordinate)
 *
 * Return: 0 on success, -1 on failure to build channel list
 */

int set_ieee_order_chan_list(struct hostapd_hw_modes *mode,
			     struct ieee_chan_data *chan_data,
			     enum nl80211_regulatory_power_modes client_mode);

/**
 * get_psd_values() - Build PSD values for 6 GHz TPE
 *
 * Computes per-channel PSD (EIRP/MHz) values for an ordered channel set
 * and populates the Transmit Power Envelope (TPE) arrays, taking into
 * account client regulatory mode, AP power mode, puncturing and power
 * interpretation. The values written to the output arrays are encoded in
 * 0.5 dB steps (i.e., value = PSD[dBm/MHz] * 2).
 *
 * @hapd: Hostapd BSS context
 * @non_11be_start_idx: Start index in @chan_data for the non-11be range
 *                      to be exported in @tx_pwr_array
 * @chan_start_idx: Start index in @chan_data for the total channel range
 * @non_11be_chan_count: Number of non-11be channels in the range
 * @total_chan_count: Total number of channels in the range
 * @tx_pwr_count: Output count corresponding to @tx_pwr_array
 * @tx_pwr_array: Output PSD values for the non-11be range (0.5 dB units)
 * @tx_pwr_ext_count: Output count corresponding to @tx_pwr_ext_array
 * @tx_pwr_ext_array: Output PSD values for the remaining channels
 *                    (0.5 dB units)
 * @client_mode: Regulatory client mode (LPI/SP/subordinate)
 * @chan_data: Ordered channel list container
 * @pwr_mode: Regulatory AP power mode
 * @tx_pwr_intrpn: Maximum transmit power interpretation (PSD/EIRP)
 *
 * Return: 0 on success, -1 on error
 */
int get_psd_values(struct hostapd_data *hapd, int non_11be_start_idx,
		   int chan_start_idx, int non_11be_chan_count,
		   int total_chan_count, u8 *tx_pwr_count,
		   s8 *tx_pwr_array, u8 *tx_pwr_ext_count,
		   s8 *tx_pwr_ext_array, u8 client_mode, struct ieee_chan_data chan_data,
		   u8 pwr_mode, enum max_tx_pwr_interpretation tx_pwr_intrpn);


void ap_copy_sta_supp_op_classes(struct sta_info *sta,
				 const u8 *supp_op_classes,
				 size_t supp_op_classes_len);

u8 * hostapd_eid_fils_indic(struct hostapd_data *hapd, u8 *eid, int hessid);
void ieee802_11_finish_fils_auth(struct hostapd_data *hapd,
				 struct sta_info *sta, int success,
				 struct wpabuf *erp_resp,
				 const u8 *msk, size_t msk_len);
u8 * owe_assoc_req_process(struct hostapd_data *hapd, struct sta_info *sta,
			   const u8 *owe_dh, u8 owe_dh_len,
			   u8 *owe_buf, size_t owe_buf_len, u16 *status);
u16 owe_process_rsn_ie(struct hostapd_data *hapd, struct sta_info *sta,
		       const u8 *rsn_ie, size_t rsn_ie_len,
		       const u8 *owe_dh, size_t owe_dh_len,
		       const u8 *link_addr);
u16 owe_validate_request(struct hostapd_data *hapd, const u8 *peer,
			 const u8 *rsn_ie, size_t rsn_ie_len,
			 const u8 *owe_dh, size_t owe_dh_len);
void fils_hlp_timeout(void *eloop_ctx, void *eloop_data);
void fils_hlp_finish_assoc(struct hostapd_data *hapd, struct sta_info *sta);
void handle_auth_fils(struct hostapd_data *hapd, struct sta_info *sta,
		      const u8 *pos, size_t len, u16 auth_alg,
		      u16 auth_transaction, u16 status_code,
		      void (*cb)(struct hostapd_data *hapd,
				 struct sta_info *sta,
				 u16 resp, struct wpabuf *data, int pub));

size_t hostapd_eid_owe_trans_len(struct hostapd_data *hapd);
u8 * hostapd_eid_owe_trans(struct hostapd_data *hapd, u8 *eid, size_t len);

size_t hostapd_eid_dpp_cc_len(struct hostapd_data *hapd);
u8 * hostapd_eid_dpp_cc(struct hostapd_data *hapd, u8 *eid, size_t len);

int get_tx_parameters(struct sta_info *sta, int ap_max_chanwidth,
		      int ap_seg1_idx, int *bandwidth, int *seg1_idx);

void auth_sae_process_commit(void *eloop_ctx, void *user_ctx);
u8 * hostapd_eid_rsnxe(struct hostapd_data *hapd, u8 *eid, size_t len, u64 capab_mask);
u8 * hostapd_eid_smd(struct hostapd_data *hapd, u8 *eid);
u16 check_ext_capab(struct hostapd_data *hapd, struct sta_info *sta,
		    const u8 *ext_capab_ie, size_t ext_capab_ie_len);
size_t hostapd_eid_rnr_len(struct hostapd_data *hapd, u32 type,
			   bool include_mld_params);
u8 * hostapd_eid_rnr(struct hostapd_data *hapd, u8 *eid, u32 type,
		     bool include_mld_params);
int ieee802_11_set_radius_info(struct hostapd_data *hapd, struct sta_info *sta,
			       int res, struct radius_sta *info);
size_t hostapd_eid_eht_capab_len(struct hostapd_data *hapd,
				 enum ieee80211_op_mode opmode);
u8 * hostapd_eid_eht_capab(struct hostapd_data *hapd, u8 *eid,
			   enum ieee80211_op_mode opmode);
int hostapd_sp_implied_key_mgmt(const struct hostapd_bss_config *conf);
u8 * hostapd_eid_eht_operation(struct hostapd_data *hapd, u8 *eid);
bool eht_mu_mask_valid(u8 mask);
void hostapd_update_ecu_params(struct hostapd_data *hapd);
void hostapd_reset_uhr_cu_params (struct hostapd_data *hapd);
u16 hostapd_ml_process_reconf_link(struct hostapd_data *hapd,
			       struct sta_info *assoc_sta, const u8 *ies,
			       size_t ies_len, u8 link_id, const u8 *link_addr, u8 type);
u8 * hostapd_eid_uhr_capab(struct hostapd_data *hapd, u8 *eid,
			   enum ieee80211_op_mode opmode);
u8 * hostapd_eid_uhr_operation(struct hostapd_data *hapd, u8 *eid, bool is_bcn);
u8 * hostapd_eid_tx_power_indication(struct hostapd_data *hapd, u8 *eid);
size_t hostapd_eid_tx_power_indication_len(void);
size_t hostapd_security_profile_ie_len(struct hostapd_data *hapd);
u8 *hostapd_eid_security_profile(struct hostapd_data *hapd, u8 *eid);
u8 * hostapd_eid_uhr_params_update(struct hostapd_data *hapd, u8 *eid,
				   bool skip_post_phase, bool from_user);
size_t hostapd_eid_uhr_params_update_len(struct hostapd_data *hapd,
					 bool no_post_phase, bool from_user);
int hostapd_npca_primary_chan_validate(struct hostapd_data *hapd,
				       const char *val_str);
u8 hostapd_npca_get_primary_chan(struct hostapd_data *hapd,
				 const struct hostapd_uhr_npca_params *npca);
u16 copy_sta_eht_capab(struct hostapd_data *hapd, struct sta_info *sta,
		       enum ieee80211_op_mode opmode,
		       const u8 *he_capab, size_t he_capab_len,
		       const u8 *eht_capab, size_t eht_capab_len);
u16 copy_sta_uhr_capab(struct hostapd_data *hapd, struct sta_info *sta,
		       const u8 *uhr_capab, size_t uhr_capab_len);
void hostapd_parse_smd_ie(struct hostapd_data *hapd, struct sta_info *sta,
			const u8 *ies, size_t ies_len);
size_t hostapd_eid_mbssid_len(struct hostapd_data *hapd, u32 frame_type,
			      u8 *elem_count, const u8 *known_bss,
			      size_t known_bss_len, size_t *rnr_len,
			      bool bcast_prb_resp, void *params,
			      bool *is_len_calc_failed);
u8 * hostapd_eid_mbssid(struct hostapd_data *hapd, u8 *eid, u8 *end,
			unsigned int frame_stype, u8 elem_count,
			u8 **elem_offset,
			const u8 *known_bss, size_t known_bss_len, u8 *rnr_eid,
			u8 *rnr_count, u8 **rnr_offset, size_t rnr_len,
			u32 *elemid_modified_bmap,
			bool bcast_prb_resp, void *params);
void hostapd_eid_update_cu_info(struct hostapd_data *hapd, u16 *elemid_modified,
				const u8 *eid_pos, size_t eid_len,
				enum elemid_cu eid_cu);
bool hostapd_is_multiple_link_mld(struct hostapd_data *hapd);
int sae_password_bind(struct hostapd_data *hapd, const u8 *addr,
		      const char *password);
u16 hostapd_critical_update_capab(struct hostapd_data *hapd);
const char * sae_get_password(struct hostapd_data *hapd,
			      struct sta_info *sta, const u8 *rx_id,
			      size_t rx_id_len,
			      struct sae_password_entry **pw_entry,
			      struct sae_pt **s_pt, const struct sae_pk **s_pk);
struct sta_info * hostapd_ml_get_assoc_sta(struct hostapd_data *hapd,
					   struct sta_info *sta,
					   struct hostapd_data **assoc_hapd);
int hostapd_process_assoc_ml_info(struct hostapd_data *hapd,
				  struct sta_info *sta,
				  const u8 *ies, size_t ies_len,
				  bool reassoc, int tx_link_status,
				  bool offload,
				  bool *set_beacon);
#ifdef RDK_ONEWIFI
u16 send_assoc_resp(struct hostapd_data *hapd, struct sta_info *sta,
                       const u8 *addr, u16 status_code, int reassoc,
                       const u8 *ies, size_t ies_len, int rssi,
                       int omit_rsnxe);
#endif
void ml_deinit_link_reconf_req(struct link_reconf_req_list **req_list_ptr);
int ieee80211_ml_process_link(struct hostapd_data *hapd,
			      struct hostapd_data *phapd,
			      struct sta_info *origin_sta,
			      struct mld_link_info *link,
			      const u8 *ies, size_t ies_len,
			      enum link_parse_type type,
			      bool offload,
			      bool *set_beacon);

void ieee80211_ml_build_assoc_resp(struct hostapd_data *hapd,
				   struct hostapd_data *phapd,
				   struct sta_info *sta,
				   struct mld_link_info *link);

void ieee802_11_rx_protected_eht_action(struct hostapd_data *hapd,
					struct sta_info *sta,
					const struct ieee80211_mgmt *mgmt,
					size_t len);
void hostapd_link_reconf_resp_tx_status(struct hostapd_data *hapd,
					struct sta_info *sta,
					const struct ieee80211_mgmt *mgmt,
					size_t len, int ok);

#ifdef CONFIG_IEEE80211BE
void hostapd_epcs_timeout_handler(void *eloop_ctx, void *timeout_ctx);
int hostapd_configure_epcs(struct hostapd_data *hapd,
				  struct sta_info *sta,
				  enum qos_mgmt_req_type req_type);
void
hostapd_free_reconf_sta_add_params(struct hostapd_sta_add_params *params);
#endif /* CONFIG_IEEE80211BE */

/**
 * hostapd_get_20mhz_psd_for_rnr - Fetch 20 MHz PSD value for RNR element in 0.5 dBm scale
 * @hapd: Pointer to hostapd_data structure
 *
 * Return: PSD value in dBm/MHz (signed 8-bit), or CHAN_MIN_TX_POWER
 *         (-64) on error
 */
s8 hostapd_get_20mhz_psd_for_rnr(struct hostapd_data *hapd);
u8 * hostapd_fragment_multi_link_element(struct wpabuf *buf, u8 *pos);
unsigned int wnm_neighbor_report_get_pref_link_mask(const u8 *neigh_rep,
						    size_t neigh_rep_len);
int send_auth_reply(struct hostapd_data *hapd, struct sta_info *sta,
			   const u8 *dst,
			   u16 auth_alg, u16 auth_transaction, u16 resp,
			   const u8 *ies, size_t ies_len, const char *dbg);
int start_unsolicited_sa_query(struct hostapd_data *hapd, struct sta_info *sta);

struct non_inheritance_elem {
	u8 elem_list[MAX_MBSSID_NONINHERIT_ELEM_SIZE];
	u8 ext_elem_list[MAX_MBSSID_NONINHERIT_ELEM_SIZE];
	u8 ext_elem_len;
	u8 elem_len;
};

u8 * hostapd_eid_mbssid_nontx_optional_ie(struct hostapd_data *bss,
					  void *tx_params,
					  struct non_inheritance_elem *non_inherit_ie,
					  u8 *eid, ssize_t *nontx_prof_len,
					  u8 frame_type);

void ieee80211_send_eap_req(struct hostapd_data *hapd, struct sta_info *sta,
			    u8 type, u16 auth_transaction, u16 status,
			    struct rsn_pmksa_cache_entry *cached_pmk,
			    const u8 *eap_req, size_t eap_req_len);

bool hostapd_find_auth_comeback_sta(struct hostapd_data *hapd, const u8 *addr,
				    const u8 *ies, size_t ies_len,
				    struct hostapd_data **o_hapd,
				    struct sta_info **o_sta);

u8 * hostapd_add_auth_comeback(struct hostapd_data *hapd, struct sta_info *sta,
			       u8 *pos, size_t max_len, u16 auth_alg);
#endif /* IEEE802_11_H */
