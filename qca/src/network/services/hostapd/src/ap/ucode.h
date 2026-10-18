#ifndef __HOSTAPD_AP_UCODE_H
#define __HOSTAPD_AP_UCODE_H

#include "utils/ucode.h"
#include "robust_av.h"
#include "nft.h"

struct hostapd_data;
#ifdef CONFIG_IEEE80211AX
struct nft_rule_params;
#endif

struct hostapd_ucode_bss {
#ifdef UCODE_SUPPORT
	int idx;
#endif
};

struct hostapd_ucode_iface {
#ifdef UCODE_SUPPORT
	int idx;
	int radio_id;
#endif
};

#ifdef UCODE_SUPPORT

int hostapd_ucode_init(struct hapd_interfaces *ifaces);

void hostapd_ucode_free(void);
void hostapd_ucode_free_iface(struct hostapd_iface *iface);
void hostapd_ucode_add_bss(struct hostapd_data *hapd);
void hostapd_ucode_free_bss(struct hostapd_data *hapd);
void hostapd_ucode_reload_bss(struct hostapd_data *hapd);
bool hostapd_ucode_update_radio_mask(char *ifname, u8 hw_idx);
void hostapd_ucode_update_interfaces();

#ifdef CONFIG_QCN_EXTN
void hostapd_ucode_chsw_result_ev_notify(struct hostapd_data *hapd,
					 int freq, int ret);
void hostapd_ucode_notify_acs_completed(struct hostapd_iface *iface, int success);
void hostapd_ucode_notify_acs_start(struct hostapd_iface *iface);
int hostapd_ucode_get_sta_channel_per_band(struct hostapd_iface *iface,
					   int band,
					   struct hostapd_freq_params *freq);
#endif

#ifdef CONFIG_IEEE80211AX
void hostapd_ucode_config_nft_table(char *table, bool add);
void hostapd_ucode_config_nft_chain(struct hostapd_data *hapd, char *table,
				    char *chain, bool add);
void hostapd_ucode_config_nft_rule(struct hostapd_data *hapd, struct hostapd_nft_rule_params *rparams,
				   bool add);
#endif

#else

static inline int hostapd_ucode_init(struct hapd_interfaces *ifaces)
{
	return -EINVAL;
}
static inline void hostapd_ucode_free(void)
{
}
static inline void hostapd_ucode_free_iface(struct hostapd_iface *iface)
{
}
static inline void hostapd_ucode_reload_bss(struct hostapd_data *hapd)
{
}
static inline void hostapd_ucode_add_bss(struct hostapd_data *hapd)
{
}
static inline void hostapd_ucode_free_bss(struct hostapd_data *hapd)
{
}
static inline bool hostapd_ucode_update_radio_mask(char *ifname, u8 hw_idx)
{
	return true;
}
static inline void hostapd_ucode_update_interfaces()
{
}
#ifdef CONFIG_QCN_EXTN
static inline void
hostapd_ucode_chsw_result_ev_notify(struct hostapd_data *hapd,
				    int freq, int ret)
{
}
static inline void
hostapd_ucode_notify_acs_completed(struct hostapd_iface *iface, int success)
{
}
static inline void
hostapd_ucode_notify_acs_start(struct hostapd_iface *iface)
{
}
static inline int
hostapd_ucode_get_sta_channel_per_band(struct hostapd_iface *iface,
				       int band,
				       struct hostapd_freq_params *freq)
{
        return -EINVAL;
}
#endif
#ifdef CONFIG_IEEE80211AX
static inline void hostapd_ucode_config_nft_table(char *table, bool add)
{
}
static inline void hostapd_ucode_config_nft_chain(struct hostapd_data *hapd,
						  char *table, char *chain,
						  bool add)
{
}
static inline void
hostapd_ucode_config_nft_rule(struct hostapd_data *hapd,
			      struct hostapd_nft_rule_params *rparams,
			      bool add)
{
}
#endif
#endif

#endif
