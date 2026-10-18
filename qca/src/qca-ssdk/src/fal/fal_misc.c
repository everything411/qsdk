/*
 * Copyright (c) 2012, 2017, 2020, The Linux Foundation. All rights reserved.
 *
 * Copyright (c) 2022,2024, Qualcomm Innovation Center, Inc. All rights reserved.
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
* @defgroup fal_gen FAL_MISC
* @{
*/
#include "sw.h"
#include "fal_misc.h"
#include "hsl_api.h"
#include "adpt.h"
#include "hsl_phy.h"

sw_error_t fal_arp_status_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(arp_status_set, dev_id, enable)

sw_error_t fal_arp_status_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(arp_status_get, dev_id, enable)

sw_error_t fal_frame_max_size_set(a_uint32_t dev_id, a_uint32_t size)
    DEFINE_FAL_FUNC_HSL_EXPORT(frame_max_size_set, dev_id, size)

sw_error_t fal_frame_max_size_get(a_uint32_t dev_id, a_uint32_t * size)
    DEFINE_FAL_FUNC_HSL_EXPORT(frame_max_size_get, dev_id, size)

sw_error_t fal_port_unk_sa_cmd_set(a_uint32_t dev_id, fal_port_t port_id, fal_fwd_cmd_t cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_unk_sa_cmd_set, dev_id, port_id, cmd)

sw_error_t fal_port_unk_uc_filter_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_unk_uc_filter_set, dev_id, port_id, enable)

sw_error_t fal_port_unk_mc_filter_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_unk_mc_filter_set, dev_id, port_id, enable)

sw_error_t fal_port_bc_filter_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_bc_filter_set, dev_id, port_id, enable)

sw_error_t fal_cpu_port_status_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(cpu_port_status_set, dev_id, enable)

sw_error_t fal_eapol_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(eapol_cmd_set, dev_id, cmd)

sw_error_t fal_eapol_status_set(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(eapol_status_set, dev_id, port_id, enable)

sw_error_t fal_intr_mask_set(a_uint32_t dev_id, a_uint32_t intr_mask)
    DEFINE_FAL_FUNC_HSL_EXPORT(intr_mask_set, dev_id, intr_mask)

sw_error_t fal_intr_mask_get(a_uint32_t dev_id, a_uint32_t * intr_mask)
    DEFINE_FAL_FUNC_HSL_EXPORT(intr_mask_get, dev_id, intr_mask)

sw_error_t fal_intr_status_get(a_uint32_t dev_id, a_uint32_t * intr_status)
    DEFINE_FAL_FUNC_HSL_EXPORT(intr_status_get, dev_id, intr_status)

sw_error_t fal_intr_status_clear(a_uint32_t dev_id, a_uint32_t intr_status)
    DEFINE_FAL_FUNC_HSL_EXPORT(intr_status_clear, dev_id, intr_status)

sw_error_t fal_intr_port_link_mask_set(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t intr_mask_flag)
    DEFINE_FAL_PORT_PHY_FUNC(intr_mask_set, dev_id, port_id, intr_mask_flag)

sw_error_t fal_intr_port_link_mask_get(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t * intr_mask_flag)
    DEFINE_FAL_PORT_PHY_FUNC(intr_mask_get, dev_id, port_id, intr_mask_flag)

sw_error_t fal_intr_port_link_status_get(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t * intr_mask_flag)
    DEFINE_FAL_PORT_PHY_FUNC(intr_status_get, dev_id, port_id, intr_mask_flag)

sw_error_t fal_intr_mask_mac_linkchg_set(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(intr_mask_mac_linkchg_set, dev_id, port_id, enable)

sw_error_t fal_intr_mask_mac_linkchg_get(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(intr_mask_mac_linkchg_get, dev_id, port_id, enable)

sw_error_t fal_intr_status_mac_linkchg_get(a_uint32_t dev_id, fal_pbmp_t* port_bitmap)
    DEFINE_FAL_FUNC_HSL_EXPORT(intr_status_mac_linkchg_get, dev_id, port_bitmap)

sw_error_t fal_intr_status_mac_linkchg_clear(a_uint32_t dev_id)
    DEFINE_FAL_FUNC_HSL_EXPORT(intr_status_mac_linkchg_clear, dev_id)

#ifndef IN_MISC_MINI
sw_error_t fal_port_unk_sa_cmd_get(a_uint32_t dev_id, fal_port_t port_id, fal_fwd_cmd_t * cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_unk_sa_cmd_get, dev_id, port_id, cmd)

sw_error_t fal_port_unk_uc_filter_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_unk_uc_filter_get, dev_id, port_id, enable)

sw_error_t fal_port_unk_mc_filter_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_unk_mc_filter_get, dev_id, port_id, enable)

sw_error_t fal_port_bc_filter_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_bc_filter_get, dev_id, port_id, enable)

sw_error_t fal_cpu_port_status_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(cpu_port_status_get, dev_id, enable)

sw_error_t fal_bc_to_cpu_port_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(bc_to_cpu_port_set, dev_id, enable)

sw_error_t fal_bc_to_cpu_port_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(bc_to_cpu_port_get, dev_id, enable)

sw_error_t fal_port_dhcp_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_dhcp_set, dev_id, port_id, enable)

sw_error_t fal_port_dhcp_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_dhcp_get, dev_id, port_id, enable)

sw_error_t fal_arp_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(arp_cmd_set, dev_id, cmd)

sw_error_t fal_arp_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(arp_cmd_get, dev_id, cmd)

sw_error_t fal_eapol_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd)
    DEFINE_FAL_FUNC_HSL_EXPORT(eapol_cmd_get, dev_id, cmd)

sw_error_t fal_eapol_status_get(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(eapol_status_get, dev_id, port_id, enable)

sw_error_t fal_ripv1_status_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(ripv1_status_set, dev_id, enable)

sw_error_t fal_ripv1_status_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(ripv1_status_get, dev_id, enable)

sw_error_t fal_port_arp_req_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_arp_req_status_set, dev_id, port_id, enable)

sw_error_t fal_port_arp_req_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_arp_req_status_get, dev_id, port_id, enable)

sw_error_t fal_port_arp_ack_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_arp_ack_status_set, dev_id, port_id, enable)

sw_error_t fal_port_arp_ack_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(port_arp_ack_status_get, dev_id, port_id, enable)

sw_error_t fal_cpu_vid_en_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(cpu_vid_en_set, dev_id, enable)

sw_error_t fal_cpu_vid_en_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(cpu_vid_en_get, dev_id, enable)

sw_error_t fal_global_macaddr_set(a_uint32_t dev_id, fal_mac_addr_t * addr)
    DEFINE_FAL_FUNC_HSL_EXPORT(global_macaddr_set, dev_id, addr)

sw_error_t fal_global_macaddr_get(a_uint32_t dev_id, fal_mac_addr_t * addr)
    DEFINE_FAL_FUNC_HSL_EXPORT(global_macaddr_get, dev_id, addr)

sw_error_t fal_lldp_status_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(lldp_status_set, dev_id, enable)

sw_error_t fal_lldp_status_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(lldp_status_get, dev_id, enable)

sw_error_t fal_frame_crc_reserve_set(a_uint32_t dev_id, a_bool_t enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(frame_crc_reserve_set, dev_id, enable)

sw_error_t fal_frame_crc_reserve_get(a_uint32_t dev_id, a_bool_t * enable)
    DEFINE_FAL_FUNC_HSL_EXPORT(frame_crc_reserve_get, dev_id, enable)
#endif
