/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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

#include "sw.h"
#include "fal_sfp.h"
#include "adpt.h"
#include "hsl_api.h"


sw_error_t fal_sfp_diag_ctrl_status_get(a_uint32_t dev_id, a_uint32_t port_id, fal_sfp_ctrl_status_t *ctrl_status)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sfp_diag_ctrl_status_get, dev_id, port_id, ctrl_status)

sw_error_t fal_sfp_diag_extenal_calibration_const_get(a_uint32_t dev_id, a_uint32_t port_id, fal_sfp_cal_const_t *cal_const)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sfp_diag_extenal_calibration_const_get, dev_id, port_id, cal_const)

sw_error_t fal_sfp_link_length_get(a_uint32_t dev_id, a_uint32_t port_id, fal_sfp_link_length_t *link_len)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sfp_link_length_get, dev_id, port_id, link_len)

sw_error_t fal_sfp_diag_internal_threshold_get(a_uint32_t dev_id, a_uint32_t port_id, fal_sfp_internal_threshold_t *threshold)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sfp_diag_internal_threshold_get, dev_id, port_id, threshold)

sw_error_t fal_sfp_diag_realtime_get(a_uint32_t dev_id, a_uint32_t port_id, fal_sfp_realtime_diag_t *real_diag)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sfp_diag_realtime_get, dev_id, port_id, real_diag)

sw_error_t fal_sfp_laser_wavelength_get(a_uint32_t dev_id, a_uint32_t port_id, fal_sfp_laser_wavelength_t *laser_wavelen)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sfp_laser_wavelength_get, dev_id, port_id, laser_wavelen)

sw_error_t fal_sfp_option_get(a_uint32_t dev_id, a_uint32_t port_id, fal_sfp_option_t *option)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sfp_option_get, dev_id, port_id, option)

sw_error_t fal_sfp_checkcode_get(a_uint32_t dev_id, a_uint32_t port_id, fal_sfp_cc_type_t cc_type, a_uint8_t *ccode)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sfp_checkcode_get, dev_id, port_id, cc_type, ccode)

sw_error_t fal_sfp_diag_alarm_warning_flag_get(a_uint32_t dev_id, a_uint32_t port_id, fal_sfp_alarm_warn_flag_t *alarm_warn_flag)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sfp_diag_alarm_warning_flag_get, dev_id, port_id, alarm_warn_flag)

sw_error_t fal_sfp_device_type_get(a_uint32_t dev_id, a_uint32_t port_id, fal_sfp_dev_type_t *sfp_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sfp_device_type_get, dev_id, port_id, sfp_id)

sw_error_t fal_sfp_vendor_info_get(a_uint32_t dev_id, a_uint32_t port_id, fal_sfp_vendor_info_t *vender_info)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sfp_vendor_info_get, dev_id, port_id, vender_info)

sw_error_t fal_sfp_transceiver_code_get(a_uint32_t dev_id, a_uint32_t port_id, fal_sfp_transc_code_t *transc_code)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sfp_transceiver_code_get, dev_id, port_id, transc_code)

sw_error_t fal_sfp_ctrl_rate_get(a_uint32_t dev_id, a_uint32_t port_id, fal_sfp_rate_t *rate_limit)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sfp_ctrl_rate_get, dev_id, port_id, rate_limit)

sw_error_t fal_sfp_enhanced_cfg_get(a_uint32_t dev_id, a_uint32_t port_id, fal_sfp_enhanced_cfg_t *enhanced_feature)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sfp_enhanced_cfg_get, dev_id, port_id, enhanced_feature)

sw_error_t fal_sfp_rate_encode_get(a_uint32_t dev_id, a_uint32_t port_id, fal_sfp_rate_encode_t *encode)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sfp_rate_encode_get, dev_id, port_id, encode)

sw_error_t fal_sfp_eeprom_data_get(a_uint32_t dev_id, a_uint32_t port_id, fal_sfp_data_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sfp_eeprom_data_get, dev_id, port_id, entry)

sw_error_t fal_sfp_eeprom_data_set(a_uint32_t dev_id, a_uint32_t port_id, fal_sfp_data_t *entry)
    DEFINE_FAL_FUNC_ADPT_EXPORT(sfp_eeprom_data_set, dev_id, port_id, entry)

