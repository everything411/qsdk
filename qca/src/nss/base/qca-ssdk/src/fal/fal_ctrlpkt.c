/*
 * Copyright (c) 2016-2017, 2021, The Linux Foundation. All rights reserved.
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


/**
 * @defgroup fal_ctrlpkt FAL_CTRLPKT
 * @{
 */
#include "sw.h"
#include "fal_ctrlpkt.h"
#include "hsl_api.h"
#include "adpt.h"

sw_error_t fal_mgmtctrl_ethtype_profile_set(a_uint32_t dev_id, a_uint32_t profile_id, a_uint32_t ethtype)
    DEFINE_FAL_FUNC_ADPT_EXPORT(mgmtctrl_ethtype_profile_set, dev_id, profile_id, ethtype)

sw_error_t fal_mgmtctrl_ethtype_profile_get(a_uint32_t dev_id, a_uint32_t profile_id, a_uint32_t * ethtype)
    DEFINE_FAL_FUNC_ADPT_EXPORT(mgmtctrl_ethtype_profile_get, dev_id, profile_id, ethtype)

sw_error_t fal_mgmtctrl_rfdb_profile_set(a_uint32_t dev_id, a_uint32_t profile_id, fal_mac_addr_t *addr)
    DEFINE_FAL_FUNC_ADPT_EXPORT(mgmtctrl_rfdb_profile_set, dev_id, profile_id, addr)

sw_error_t fal_mgmtctrl_rfdb_profile_get(a_uint32_t dev_id, a_uint32_t profile_id, fal_mac_addr_t *addr)
    DEFINE_FAL_FUNC_ADPT_EXPORT(mgmtctrl_rfdb_profile_get, dev_id, profile_id, addr)

sw_error_t fal_mgmtctrl_ctrlpkt_profile_add(a_uint32_t dev_id, fal_ctrlpkt_profile_t *ctrlpkt)
    DEFINE_FAL_FUNC_ADPT_EXPORT(mgmtctrl_ctrlpkt_profile_add, dev_id, ctrlpkt)

sw_error_t fal_mgmtctrl_ctrlpkt_profile_del(a_uint32_t dev_id, fal_ctrlpkt_profile_t *ctrlpkt)
    DEFINE_FAL_FUNC_ADPT_EXPORT(mgmtctrl_ctrlpkt_profile_del, dev_id, ctrlpkt)

sw_error_t fal_mgmtctrl_ctrlpkt_profile_getfirst(a_uint32_t dev_id, fal_ctrlpkt_profile_t *ctrlpkt)
    DEFINE_FAL_FUNC_ADPT_EXPORT(mgmtctrl_ctrlpkt_profile_getfirst, dev_id, ctrlpkt)

sw_error_t fal_mgmtctrl_ctrlpkt_profile_getnext(a_uint32_t dev_id, fal_ctrlpkt_profile_t *ctrlpkt)
    DEFINE_FAL_FUNC_ADPT_EXPORT(mgmtctrl_ctrlpkt_profile_getnext, dev_id, ctrlpkt)

sw_error_t fal_mgmtctrl_vpgroup_set(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t vpgroup_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(mgmtctrl_vpgroup_set, dev_id, port_id, vpgroup_id)

sw_error_t fal_mgmtctrl_vpgroup_get(a_uint32_t dev_id, fal_port_t port_id, a_uint32_t *vpgroup_id)
    DEFINE_FAL_FUNC_ADPT_EXPORT(mgmtctrl_vpgroup_get, dev_id, port_id, vpgroup_id)

sw_error_t fal_mgmtctrl_tunnel_decap_set(a_uint32_t dev_id, a_uint32_t cpu_code_id, a_bool_t enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(mgmtctrl_tunnel_decap_set, dev_id, cpu_code_id, enable)

sw_error_t fal_mgmtctrl_tunnel_decap_get(a_uint32_t dev_id, a_uint32_t cpu_code_id, a_bool_t *enable)
    DEFINE_FAL_FUNC_ADPT_EXPORT(mgmtctrl_tunnel_decap_get, dev_id, cpu_code_id, enable)



