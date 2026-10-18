/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef NSS_PHY_PTP_H
#define NSS_PHY_PTP_H

#define FAL_PTP_MSG_INVALID	0xff

#include "nss_phy_linux_wrapper.h"

typedef enum {
	FAL_OC_CLOCK_MODE = 0, /* OC clock mode */
	FAL_BC_CLOCK_MODE,     /* BC clock mode */
	FAL_E2ETC_CLOCK_MODE,  /* E2E TC clock mode */
	FAL_P2PTC_CLOCK_MODE   /* P2P TC clock mode */
} fal_ptp_clock_mode_t;

typedef enum {
	FAL_ONE_STEP_MODE = 0, /* one step mode */
	FAL_TWO_STEP_MODE,     /* two step mode */
	FAL_AUTO_MODE	       /* step mode is selected through packet field */
} fal_ptp_step_mode_t;

typedef struct {
	bool ptp_en;	         /* enable/disable ptp feature */
	fal_ptp_clock_mode_t clock_mode; /* clock mode */
	fal_ptp_step_mode_t step_mode;   /* step mode */
} fal_ptp_config_t;

typedef enum {
	FAL_REF_CLOCK_LOCAL = 0,/* use local clock as reference clock */
	FAL_REF_CLOCK_SYNCE,	/* use synce clock as reference clock */
	FAL_REF_CLOCK_EXTERNAL,	/* use external clock as reference clock */
} fal_ptp_reference_clock_t;

typedef enum {
	FAL_RX_TS_MDIO = 0,	/* timestamp will be saved to MDIO register */
	FAL_RX_TS_EMBED,	/* timestamp will be saved to packet PTP header */
} fal_ptp_rx_timestamp_mode_t;

typedef struct{
	u64 seconds;         /* second field */
	u32 nanoseconds;     /* nano second field */
	u32 fracnanoseconds; /* fraction nano second field */
} fal_ptp_time_t;

typedef enum {
	FAL_RX_DIRECTION = 0,     /* ptp rx direction */
	FAL_TX_DIRECTION          /* ptp tx direction */
} fal_ptp_direction_t;

typedef struct{
	u32 sequence_id;		/* packet field: sequenceId */
	u64 clock_identify;	/* packet field: ClockIdentify */
	u32 port_number;		/* packet field: SourcePortID */
	u32 msg_type;		/* packet field: message type */
	u8 domain_number;	/* packet field: domain number */
	u8 minor_ver;		/* packet field: minor version ptp */
	u8 major_sdoid;		/* packet field: marjor sdoid */
	u8 minor_sdoid;		/* packet field: minor sdoid */
	u32 msgtype_spec;	/* packet field: message type specific */
} fal_ptp_pkt_info_t;

typedef enum {
	FAL_GM_PPSIN_MODE = 0,	/* use PSSIN mode to sync nanoseconds */
	FAL_GM_HWPLL_MODE,	/* use HWPLL mode to sync nanoseconds */
	FAL_GM_SWPLL_MODE,	/* use SWPLL mode to sync nanoseconds */
} fal_ptp_grandmaster_ns_sync_mode_t;

typedef enum {  /* maximun frequency offset of rtc_clk from PPS source */
	FAL_GM_MAXFREQ_1PPM = 0,
	FAL_GM_MAXFREQ_10PPM,
	FAL_GM_MAXFREQ_50PPM,
	FAL_GM_MAXFREQ_100PPM,
	FAL_GM_MAXFREQ_150PPM,
	FAL_GM_MAXFREQ_200PPM,    /* 802.3 standard select this value */
	FAL_GM_MAXFREQ_250PPM,
	FAL_GM_MAXFREQ_300PPM,
	FAL_GM_MAXFREQ_0PPM
} fal_ptp_grandmaster_maxfreq_offset_t;

typedef struct {
	bool grandmaster_mode_en;	/* enable/disable grandmaster mode */
	bool grandmaster_second_sync_en; /* GPS receiver second sync feature */
	fal_ptp_grandmaster_maxfreq_offset_t freq_offset; /* for HWPLL */
	bool right_shift_in_kp; /* for HWPLL, left or right shift */
	u32 kp_value; /* for HWPLL, proportional part coefficient */
	bool right_shift_in_ki; /* for HWPLL, left or right shift */
	u32 ki_value; /* for HWPLL, integral part coefficient */
	fal_ptp_grandmaster_ns_sync_mode_t ns_sync_mode; /* nano second sync mode */
} fal_ptp_grandmaster_mode_t;

typedef struct {
	bool address_check_en; /* address should matched when regard it as PTP packet */
	bool ipv6_udp_checksum_recal_en; /* recalculate ipv6 udp checksum on TX direction */
	bool version_check_en; /* PTP version should matched when regard it as PTP packet */
	u32 ptp_version;    /* ptp version number to match */
	bool ipv4_udp_checksum_force_zero_en; /* force ipv4 checksum to 0 on TX direction */
	bool ipv6_embed_udp_checksum_force_zero_en; /* force RX ipv6 checksum to 0 */
} fal_ptp_security_t;

typedef struct {
	bool negative_in_latency;	/* ingress latency value is positive or negative */
	u32 in_latency;		/* ingress latency value */
	u32 out_phase;		/* adjust the phase of PPS outout signal */
	u32 out_pulse_width;	/* adjust the pulse width of PPS outout signal */
} fal_ptp_pps_signal_control_t;

typedef struct {
	bool eg_asym_en;	  /* enable egress asymmetry correction */
	bool in_asym_en;	  /* enable ingress asymmetry correction */
	u32 eg_asym_value; /* egress asymmetry correction value */
	u32 in_asym_value; /* ingress asymmetry correction value */
} fal_ptp_asym_correction_t;

typedef enum {
	FAL_WAVE_FREQ = 0,	/* select wave period as SYNC_CLKO_PTP output */
	FAL_PULSE_10MS,		/* select pulse 10ms as SYNC_CLKO_PTP output */
	FAL_TRIGGER0_GPIO,	/* select trigger0 status as SYNC_CLKO_PTP output */
	FAL_RX_PTP_STATE,	/* select RX PTP state as SYNC_CLKO_PTP output */
} fal_ptp_waveform_type_t;

typedef struct {
	fal_ptp_waveform_type_t waveform_type; /* select the wave output type */
	bool wave_align_pps_out_en;	/* for FAL_WAVE_FREQ, enable wave align pps output */
	u64 wave_period;		/* for FAL_WAVE_FREQ, wave period */
} fal_ptp_output_waveform_t;

enum {
	FAL_UART_START_POLARITY_HIGH_EN = 0,	/* polarity of start bit is low/high */
	FAL_UART_MSB_FIRST_EN,			/* output LSB/MSB first */
	FAL_UART_PARITY_CHECK_EN,		/* parity check is enable/disable */
	FAL_UART_AUTO_TOD_OUT_EN,		/* enable output TOD via UART automatically */
	FAL_UART_AUTO_TOD_IN_EN			/* enable input TOD via UART automatically */
};

enum {
	FAL_UART_RX_BUFFER_DATA_PRESENT = 0,	/* RX BUFFER has data */
	FAL_UART_RX_BUFFER_FULL,		/* RX BUFFER is full */
	FAL_UART_RX_BUFFER_ALMOST_FULL,		/* RX BUFFER is full-1 */
	FAL_UART_RX_BUFFER_HALF_FULL,		/* RX BUFFER is half full */
	FAL_UART_RX_BUFFER_ALMOST_EMPTY,	/* RX BUFFER is 1 */
	FAL_UART_TX_BUFFER_FULL,		/* TX BUFFER is full */
	FAL_UART_TX_BUFFER_ALMOST_FULL,		/* TX BUFFER is full-1 */
	FAL_UART_TX_BUFFER_HALF_FULL,		/* TX BUFFER is half full */
	FAL_UART_TX_BUFFER_ALMOST_EMPTY,	/* TX BUFFER is 1 */
};

typedef struct {
	u16 baud_config;	/* baud_config = (125 * 10^6)/(16)/baud_rate for speed <= 1G */
				/* baud_config = (200 * 10^6)/(16)/baud_rate for speed >= 2.5 G */
	u32 uart_config_bmp;	/* refer to FAL_UART_START_POLARITY_HIGH_EN */
	bool reset_buf_en;	/* reset TOD UART RX/TX buffer, self clearing */
	u32 buf_status_bmp;	/* refer to FAL_UART_RX_BUFFER_DATA_PRESENT */
	u16 tx_buf_value;	/* the uart data to transport */
	u16 rx_buf_value;	/* the uart data received */
} fal_ptp_tod_uart_t;

enum {
	FAL_ENHANCED_TS_ETH_TYPE_EN = 0,     /* timestamp the matched ethernet type */
	FAL_ENHANCED_TS_DMAC_EN,             /* timestamp the matched dst mac */
	FAL_ENHANCED_TS_RESV_DMAC_EN,        /* timestamp the matched ptp dst mac */
	FAL_ENHANCED_TS_IPV4_L4_PROTO_EN,    /* timestamp the matched layer 4 protocol id */
	FAL_ENHANCED_TS_IPV4_DIP_EN,         /* timestamp the matched ipv4 dst addr */
	FAL_ENHANCED_TS_RESV_IPV4_DIP_EN,    /* timestamp the matched ptp ipv4 dst addr */
	FAL_ENHANCED_TS_IPV6_NEXT_HEADER_EN, /* timestamp the matched ipv6 next header field */
	FAL_ENHANCED_TS_IPV6_DIP_EN,         /* timestamp the matched ipv6 dst addr */
	FAL_ENHANCED_TS_RESV_IPV6_DIP_EN,    /* timestamp the matched ptp ipv6 dst addr */
	FAL_ENHANCED_TS_UDP_DPORT_EN,        /* timestamp the matched udp dport number */
	FAL_ENHANCED_TS_RESV_UDP_DPORT_EN,   /* timestamp the matched udp dport number */
	FAL_ENHANCED_TS_Y1731_EN,            /* timestamp the received Y1731 frame */
	FAL_ENHANCED_TS_Y1731_TIMESTAMP_INSERT_EN, /* enable inserting RX Y1731 timestamp */
	FAL_ENHANCED_TS_Y1731_MAC_EN /* TX direction check smac, RX direction check dmac */
};

enum {
	FAL_ENHANCED_TS_ETH_TYPE_STATUS = 0,        /* the MAC type of received packet matches
						      the filter setting */
	FAL_ENHANCED_TS_DMAC_STATUS,                /* the dest MAC of received packet matches
						      the filter setting */
	FAL_ENHANCED_TS_RESV_PRIM_DMAC_STATUS,      /* the dest MAC of received packet matches
						      PTP primary multicast MAC address */
	FAL_ENHANCED_TS_RESV_PDELAY_DMAC_STATUS,    /* the dest MAC of received packet matches
						      PTP peer delay multicast MAC address */
	FAL_ENHANCED_TS_IPV4_L4_PROTO_STATUS,       /* the L4 layer protocol of received packet
						      matched the filter setting */
	FAL_ENHANCED_TS_IPV4_DIP_STATUS,            /* the IPv4 dest address of received packet
						      matched the filter setting */
	FAL_ENHANCED_TS_RESV_IPV4_PRIM_DIP_STATUS,  /* the IPv4 dest address of received packet
						      matches PTP primary multicast ipv4 addr */
	FAL_ENHANCED_TS_RESV_IPV4_PDELAY_DIP_STATUS,/* the IPv4 dest address of received packet
							matches PTP peer delay multicast addr */
	FAL_ENHANCED_TS_IPV6_NEXT_HEADER_STATUS,    /* the next header fielf of receviced packet
						       matches the filter setting */
	FAL_ENHANCED_TS_IPV6_DIP_STATUS,            /* the ipv6 dest addr of received packet
						       matches the filter setting */
	FAL_ENHANCED_TS_RESV_IPV6_PRIM_DIP_STATUS,  /* the ipv6 dest address of received packet
						       matches PTP primary multicast ipv6 addr */
	FAL_ENHANCED_TS_RESV_IPV6_PDELAY_DIP_STATUS,/* the ipv6 dest address of received packet
						       matches PTP peer delay multicast addr */
	FAL_ENHANCED_TS_UDP_DPORT_STATUS,           /* the UDP dest port of received packet
						       matches the filter setting */
	FAL_ENHANCED_TS_RESV_UDP_DPORT_STATUS,      /* the UDP dest port of received packet
						       matches udp dport of ptp event packet */
	FAL_ENHANCED_TS_Y1731_MATCH_STATUS          /* the received frame is Y.1731 OAM frame */
};

typedef struct {
	bool filt_en;	/* enable/disable filter feature */
	u32 enhance_ts_conf_bmp; /* refer to FAL_ENHANCED_TS_ETH_TYPE_EN */
	u32 eth_type;            /* ethernet type value */
	u8 dmac_addr[6];       /* dest mac address */
	u32 ipv4_l4_proto;       /* ipv4 layer 4 protocol field */
	u32 ipv4_dip;        /* ipv4 dest ip addr */
	u32 ipv6_dip[4];        /* ipv6 dest ip addr */
	u32 udp_dport;           /* udp dest port */
	u8 y1731_mac_addr[6];	/* TX check smac, RX check dmac */
	u32 enhance_ts_status_bmp; 	/* refer to FAL_ENHANCED_TS_ETH_TYPE_STATUS */
	u32 enhance_ts_status_pre_bmp;	/* TX NOT SUPPORT */
	u32 y1731_identity;	/* save Y1731 identify value */
	u32 y1731_identity_pre;	/* TX NOT SUPPORT */
	fal_ptp_time_t timestamp;	/* save packet timestamp when matched */
	fal_ptp_time_t timestamp_pre;	/* TX NOT SUPPORT */
} fal_ptp_enhanced_ts_engine_t;

typedef struct {
	bool trigger_en;            /* enable trigger or not */
	bool output_force_en;       /* force trigger output a force value */
	int output_force_value;         /* the forced value */
	int patten_select;              /* trigger pattern:
					   0 single rising edge;
					   1 single falling edge;
					   2 trigger pulse;
					   3 trigger periodic waveform;
					   4 toggle mode;
					 */
	int late_operation;             /* if later, trigger immediately */
	int notify;                     /* report the completion of trigger */
	int trigger_effect;             /* write 1 to generate a high pulse when trigger happen */
	fal_ptp_time_t tim;             /* the trigger timestamp */
} fal_ptp_trigger_conf_t;

typedef struct {
	int trigger_finished;           /* trigger finished or not */
	int trigger_active;             /* trigger is active or not */
	int trigger_error;              /* trigger error status
					   0 no error
					   1 trigger time prior to current time
					   2 initial value error for edge trigger
					 */
} fal_ptp_trigger_status_t;

typedef struct {
	fal_ptp_trigger_conf_t trigger_conf;
	fal_ptp_trigger_status_t trigger_status;
} fal_ptp_trigger_t;

typedef struct {
	int status_clear;              /* clear event status register */
	int notify_event;              /* notify event through interrupt */
	int single_multi_select;       /* 1 for single or 0 for multi event capture */
	bool fall_edge_en;         /* enable falling edge detection */
	bool rise_edge_en;         /* enable rising edge detection */
} fal_ptp_capture_conf_t;

typedef struct {
	int event_detected;            /* event detected or not */
	int fall_rise_edge_detected;   /* 0 for rising edge, 1 for falling edge detected */
	int single_multi_detected;     /* 0 for single event, 1 for multi event detected */
	int event_missed_cnt;          /* the number of events missed */
	fal_ptp_time_t tim;            /* event timestamp */
} fal_ptp_capture_status_t;

typedef struct {
	fal_ptp_capture_conf_t capture_conf;
	fal_ptp_capture_status_t capture_status;
} fal_ptp_capture_t;

enum {
	FAL_PTP_INTR_EXPAND = 0,/* expand interrupt, FAL_PTP_INTR_TX_GTSE ~ FAL_PTP_INTR_CAP1 */
	FAL_PTP_INTR_RX,	/* ptp pkt rx interrupt */
	FAL_PTP_INTR_TX,	/* ptp pkt tx interrupt */
	FAL_PTP_INTR_TX_GTSE,	/* gtse pkt tx interrupt */
	FAL_PTP_INTR_RX_GTSE,	/* gtse pkt rx interrupt */
	FAL_PTP_INTR_TX_BUF,	/* uart tod tx buffer half full interrupt */
	FAL_PTP_INTR_RX_BUF,	/* uart tod rx buffer half full interrupt */
	FAL_PTP_INTR_PPS_OUT,	/* PPS output interrupt */
	FAL_PTP_INTR_PPS_IN,	/* PPS input interrupt */
	FAL_PTP_INTR_10MS,	/* each 10ms interrupt */
	FAL_PTP_INTR_TRIG0,	/* trigger0 interrupt */
	FAL_PTP_INTR_TRIG1,	/* trigger1 interrupt */
	FAL_PTP_INTR_CAP0,	/* capture0 interrupt */
	FAL_PTP_INTR_CAP1	/* capture1 interrupt */
};

typedef struct {
	u32 intr_mask;
	u32 intr_status;
} fal_ptp_interrupt_t;

typedef enum {
	FAL_PTP_RTC_SRC_PRE_PORT = 0, /* RTC clock source from previous mht port 1->2->3->4->1 */
	FAL_PTP_RTC_SRC_MHT_PORT,     /* RTC clock source can be specified any mht port id */
	FAL_PTP_RTC_SRC_EXT,          /* RTC clock source from external port such as NAPA port */
	FAL_PTP_RTC_SRC_DIS,          /* RTC clock source disabled */
} fal_ptp_rtc_src_type_t;

enum {
	ptp_security_set,
	ptp_link_delay_set,
	ptp_rx_crc_recalc_status_get,
	ptp_tod_uart_set,
	ptp_enhanced_timestamp_engine_get,
	ptp_pps_signal_control_set,
	ptp_timestamp_get,
	ptp_asym_correction_get,
	ptp_rtc_time_snapshot_status_get,
	ptp_capture_set,
	ptp_rtc_adjfreq_set,
	ptp_asym_correction_set,
	ptp_pkt_timestamp_set,
	ptp_rtc_time_get,
	ptp_rtc_time_set,
	ptp_pkt_timestamp_get,
	ptp_interrupt_set,
	ptp_trigger_set,
	ptp_pps_signal_control_get,
	ptp_capture_get,
	ptp_rx_crc_recalc_enable,
	ptp_security_get,
	ptp_increment_sync_from_clock_status_get,
	ptp_tod_uart_get,
	ptp_enhanced_timestamp_engine_set,
	ptp_rtc_time_clear,
	ptp_reference_clock_set,
	ptp_output_waveform_set,
	ptp_rx_timestamp_mode_set,
	ptp_grandmaster_mode_set,
	ptp_config_set,
	ptp_trigger_get,
	ptp_rtc_adjfreq_get,
	ptp_grandmaster_mode_get,
	ptp_rx_timestamp_mode_get,
	ptp_rtc_adjtime_set,
	ptp_link_delay_get,
	ptp_increment_sync_from_clock_enable,
	ptp_config_get,
	ptp_output_waveform_get,
	ptp_interrupt_get,
	ptp_rtc_time_snapshot_enable,
	ptp_reference_clock_get,
	ptp_rtc_sync_set,
	ptp_rtc_sync_get,
};

struct nss_phy_ptp_ops {
	int (*ptp_security_set)(struct nss_phy_device *nss_phydev,
			fal_ptp_security_t *sec);
	int (*ptp_link_delay_set)(struct nss_phy_device *nss_phydev,
			fal_ptp_time_t *time);
	int (*ptp_rx_crc_recalc_status_get)(struct nss_phy_device *nss_phydev,
			bool *status);
	int (*ptp_tod_uart_set)(struct nss_phy_device *nss_phydev,
			fal_ptp_tod_uart_t *tod_uart);
	int (*ptp_enhanced_timestamp_engine_get)(struct nss_phy_device *nss_phydev,
			fal_ptp_direction_t direction,
			fal_ptp_enhanced_ts_engine_t *ts_engine);
	int (*ptp_pps_signal_control_set)(struct nss_phy_device *nss_phydev,
			fal_ptp_pps_signal_control_t *sig_control);
	int (*ptp_timestamp_get)(struct nss_phy_device *nss_phydev,
			fal_ptp_direction_t direction,
			fal_ptp_pkt_info_t *pkt_info, fal_ptp_time_t *time);
	int (*ptp_asym_correction_get)(struct nss_phy_device *nss_phydev,
			fal_ptp_asym_correction_t* asym_cf);
	int (*ptp_rtc_time_snapshot_status_get)(struct nss_phy_device *nss_phydev,
			bool *status);
	int (*ptp_capture_set)(struct nss_phy_device *nss_phydev,
			u32 capture_id,
			fal_ptp_capture_t *capture);
	int (*ptp_rtc_adjfreq_set)(struct nss_phy_device *nss_phydev,
			fal_ptp_time_t *time);
	int (*ptp_asym_correction_set)(struct nss_phy_device *nss_phydev,
			fal_ptp_asym_correction_t *asym_cf);
	int (*ptp_pkt_timestamp_set)(struct nss_phy_device *nss_phydev,
			fal_ptp_time_t *time);
	int (*ptp_rtc_time_get)(struct nss_phy_device *nss_phydev,
			fal_ptp_time_t *time);
	int (*ptp_rtc_time_set)(struct nss_phy_device *nss_phydev,
			fal_ptp_time_t *time);
	int (*ptp_pkt_timestamp_get)(struct nss_phy_device *nss_phydev,
			fal_ptp_time_t *time);
	int (*ptp_interrupt_set)(struct nss_phy_device *nss_phydev,
			fal_ptp_interrupt_t *interrupt);
	int (*ptp_trigger_set)(struct nss_phy_device *nss_phydev,
			u32 trigger_id,
			fal_ptp_trigger_t *triger);
	int (*ptp_pps_signal_control_get)(struct nss_phy_device *nss_phydev,
			fal_ptp_pps_signal_control_t *sig_control);
	int (*ptp_capture_get)(struct nss_phy_device *nss_phydev,
			u32 capture_id,
			fal_ptp_capture_t *capture);
	int (*ptp_rx_crc_recalc_enable)(struct nss_phy_device *nss_phydev,
			bool status);
	int (*ptp_security_get)(struct nss_phy_device *nss_phydev,
			fal_ptp_security_t *sec);
	int (*ptp_increment_sync_from_clock_status_get)(struct nss_phy_device *nss_phydev,
			bool *status);
	int (*ptp_tod_uart_get)(struct nss_phy_device *nss_phydev,
			fal_ptp_tod_uart_t *tod_uart);
	int (*ptp_enhanced_timestamp_engine_set)(struct nss_phy_device *nss_phydev,
			fal_ptp_direction_t direction,
			fal_ptp_enhanced_ts_engine_t *ts_engine);
	int (*ptp_rtc_time_clear)(struct nss_phy_device *nss_phydev);
	int (*ptp_reference_clock_set)(struct nss_phy_device *nss_phydev,
			fal_ptp_reference_clock_t ref_clock);
	int (*ptp_output_waveform_set)(struct nss_phy_device *nss_phydev,
			fal_ptp_output_waveform_t *waveform);
	int (*ptp_rx_timestamp_mode_set)(struct nss_phy_device *nss_phydev,
			fal_ptp_rx_timestamp_mode_t ts_mode);
	int (*ptp_grandmaster_mode_set)(struct nss_phy_device *nss_phydev,
			fal_ptp_grandmaster_mode_t *gm_mode);
	int (*ptp_config_set)(struct nss_phy_device *nss_phydev,
			fal_ptp_config_t *config);
	int (*ptp_trigger_get)(struct nss_phy_device *nss_phydev,
			u32 trigger_id,
			fal_ptp_trigger_t *triger);
	int (*ptp_rtc_adjfreq_get)(struct nss_phy_device *nss_phydev,
			fal_ptp_time_t *time);
	int (*ptp_grandmaster_mode_get)(struct nss_phy_device *nss_phydev,
			fal_ptp_grandmaster_mode_t *gm_mode);
	int (*ptp_rx_timestamp_mode_get)(struct nss_phy_device *nss_phydev,
			fal_ptp_rx_timestamp_mode_t *ts_mode);
	int (*ptp_rtc_adjtime_set)(struct nss_phy_device *nss_phydev,
			fal_ptp_time_t *time);
	int (*ptp_link_delay_get)(struct nss_phy_device *nss_phydev,
			fal_ptp_time_t *time);
	int (*ptp_increment_sync_from_clock_enable)(struct nss_phy_device *nss_phydev,
			bool status);
	int (*ptp_config_get)(struct nss_phy_device *nss_phydev,
			fal_ptp_config_t *config);
	int (*ptp_output_waveform_get)(struct nss_phy_device *nss_phydev,
			fal_ptp_output_waveform_t *waveform);
	int (*ptp_interrupt_get)(struct nss_phy_device *nss_phydev,
			fal_ptp_interrupt_t *interrupt);
	int (*ptp_rtc_time_snapshot_enable)(struct nss_phy_device *nss_phydev,
			bool status);
	int (*ptp_reference_clock_get)(struct nss_phy_device *nss_phydev,
			fal_ptp_reference_clock_t *ref_clock);
	int (*ptp_rtc_sync_set)(struct nss_phy_device *nss_phydev,
			fal_ptp_rtc_src_type_t src_type, u32 src_id);
	int (*ptp_rtc_sync_get) (struct nss_phy_device *nss_phydev,
			fal_ptp_rtc_src_type_t *src_type, u32 *src_id);
};

int nss_phy_ptp_ops_init(struct nss_phy_ptp_ops *ptp_ops);
#endif
