/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022,2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all copies.
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


/**
 * @defgroup fal_ptp FAL_PTP
 * @{
 */
#include "sw.h"
#include "adpt.h"
#include "hsl_phy.h"

sw_error_t fal_ptp_security_set(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_security_t *sec)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_security_set, dev_id, port_id, sec)

sw_error_t fal_ptp_link_delay_set(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_time_t *time)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_link_delay_set, dev_id, port_id, time)

sw_error_t fal_ptp_rx_crc_recalc_status_get(a_uint32_t dev_id, a_uint32_t port_id, bool *status)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_rx_crc_recalc_status_get, dev_id, port_id, status)

sw_error_t fal_ptp_tod_uart_set(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_tod_uart_t *tod_uart)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_tod_uart_set, dev_id, port_id, tod_uart)

sw_error_t fal_ptp_enhanced_timestamp_engine_get(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_direction_t direction, fal_ptp_enhanced_ts_engine_t *ts_engine)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_enhanced_timestamp_engine_get, dev_id, port_id, direction, ts_engine)

sw_error_t fal_ptp_pps_signal_control_set(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_pps_signal_control_t *sig_control)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_pps_signal_control_set, dev_id, port_id, sig_control)

sw_error_t fal_ptp_timestamp_get(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_direction_t direction, fal_ptp_pkt_info_t *pkt_info, fal_ptp_time_t *time)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_timestamp_get, dev_id, port_id, direction, pkt_info, time)

sw_error_t fal_ptp_asym_correction_get(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_asym_correction_t* asym_cf)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_asym_correction_get, dev_id, port_id, asym_cf)

sw_error_t fal_ptp_rtc_time_snapshot_status_get(a_uint32_t dev_id, a_uint32_t port_id, bool *status)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_rtc_time_snapshot_status_get, dev_id, port_id, status)

sw_error_t fal_ptp_capture_set(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t capture_id, fal_ptp_capture_t *capture)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_capture_set, dev_id, port_id, capture_id, capture)

sw_error_t fal_ptp_rtc_adjfreq_set(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_time_t *time)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_rtc_adjfreq_set, dev_id, port_id, time)

sw_error_t fal_ptp_asym_correction_set(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_asym_correction_t *asym_cf)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_asym_correction_set, dev_id, port_id, asym_cf)

sw_error_t fal_ptp_pkt_timestamp_set(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_time_t *time)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_pkt_timestamp_set, dev_id, port_id, time)

sw_error_t fal_ptp_rtc_time_get(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_time_t *time)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_rtc_time_get, dev_id, port_id, time)

sw_error_t fal_ptp_rtc_time_set(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_time_t *time)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_rtc_time_set, dev_id, port_id, time)

sw_error_t fal_ptp_pkt_timestamp_get(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_time_t *time)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_pkt_timestamp_get, dev_id, port_id, time)

sw_error_t fal_ptp_interrupt_set(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_interrupt_t *interrupt)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_interrupt_set, dev_id, port_id, interrupt)

sw_error_t fal_ptp_trigger_set(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t trigger_id, fal_ptp_trigger_t *triger)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_trigger_set, dev_id, port_id, trigger_id, triger)

sw_error_t fal_ptp_pps_signal_control_get(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_pps_signal_control_t *sig_control)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_pps_signal_control_get, dev_id, port_id, sig_control)

sw_error_t fal_ptp_capture_get(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t capture_id, fal_ptp_capture_t *capture)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_capture_get, dev_id, port_id, capture_id, capture)

sw_error_t fal_ptp_rx_crc_recalc_enable(a_uint32_t dev_id, a_uint32_t port_id, bool status)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_rx_crc_recalc_enable, dev_id, port_id, status)

sw_error_t fal_ptp_security_get(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_security_t *sec)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_security_get, dev_id, port_id, sec)

sw_error_t fal_ptp_increment_sync_from_clock_status_get(a_uint32_t dev_id, a_uint32_t port_id, bool *status)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_increment_sync_from_clock_status_get, dev_id, port_id, status)

sw_error_t fal_ptp_tod_uart_get(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_tod_uart_t *tod_uart)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_tod_uart_get, dev_id, port_id, tod_uart)

sw_error_t fal_ptp_enhanced_timestamp_engine_set(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_direction_t direction, fal_ptp_enhanced_ts_engine_t *ts_engine)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_enhanced_timestamp_engine_set, dev_id, port_id, direction, ts_engine)

sw_error_t fal_ptp_rtc_time_clear(a_uint32_t dev_id, a_uint32_t port_id)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_rtc_time_clear, dev_id, port_id)

sw_error_t fal_ptp_reference_clock_set(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_reference_clock_t ref_clock)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_reference_clock_set, dev_id, port_id, ref_clock)

sw_error_t fal_ptp_output_waveform_set(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_output_waveform_t *waveform)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_output_waveform_set, dev_id, port_id, waveform)

sw_error_t fal_ptp_rx_timestamp_mode_set(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_rx_timestamp_mode_t ts_mode)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_rx_timestamp_mode_set, dev_id, port_id, ts_mode)

sw_error_t fal_ptp_grandmaster_mode_set(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_grandmaster_mode_t *gm_mode)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_grandmaster_mode_set, dev_id, port_id, gm_mode)

sw_error_t fal_ptp_config_set(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_config_t *config)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_config_set, dev_id, port_id, config)

sw_error_t fal_ptp_trigger_get(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t trigger_id, fal_ptp_trigger_t *triger)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_trigger_get, dev_id, port_id, trigger_id, triger)

sw_error_t fal_ptp_rtc_adjfreq_get(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_time_t *time)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_rtc_adjfreq_get, dev_id, port_id, time)

sw_error_t fal_ptp_grandmaster_mode_get(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_grandmaster_mode_t *gm_mode)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_grandmaster_mode_get, dev_id, port_id, gm_mode)

sw_error_t fal_ptp_rx_timestamp_mode_get(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_rx_timestamp_mode_t *ts_mode)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_rx_timestamp_mode_get, dev_id, port_id, ts_mode)

sw_error_t fal_ptp_rtc_adjtime_set(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_time_t *time)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_rtc_adjtime_set, dev_id, port_id, time)

sw_error_t fal_ptp_link_delay_get(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_time_t *time)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_link_delay_get, dev_id, port_id, time)

sw_error_t fal_ptp_increment_sync_from_clock_enable(a_uint32_t dev_id, a_uint32_t port_id, bool status)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_increment_sync_from_clock_enable, dev_id, port_id, status)

sw_error_t fal_ptp_config_get(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_config_t *config)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_config_get, dev_id, port_id, config)

sw_error_t fal_ptp_output_waveform_get(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_output_waveform_t *waveform)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_output_waveform_get, dev_id, port_id, waveform)

sw_error_t fal_ptp_interrupt_get(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_interrupt_t *interrupt)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_interrupt_get, dev_id, port_id, interrupt)

sw_error_t fal_ptp_rtc_time_snapshot_enable(a_uint32_t dev_id, a_uint32_t port_id, bool status)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_rtc_time_snapshot_enable, dev_id, port_id, status)

sw_error_t fal_ptp_reference_clock_get(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_reference_clock_t *ref_clock)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_reference_clock_get, dev_id, port_id, ref_clock)

sw_error_t fal_ptp_rtc_sync_set(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_rtc_src_type_t src_type, a_uint32_t src_id)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_rtc_sync_set, dev_id, port_id, src_type, src_id)

sw_error_t fal_ptp_rtc_sync_get(a_uint32_t dev_id, a_uint32_t port_id, fal_ptp_rtc_src_type_t *src_type, a_uint32_t *src_id)
    DEFINE_FAL_PORT_PHY_PTP_FUNC(ptp_rtc_sync_get, dev_id, port_id, src_type, src_id)

