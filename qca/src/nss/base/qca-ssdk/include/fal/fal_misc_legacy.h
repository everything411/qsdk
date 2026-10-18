/*
 * Copyright (c) 2012, 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_misc_legacy FAL_MISC_LEGACY
 * @{
 */
#ifndef _FAL_MISC_LEGACY_H_
#define _FAL_MISC_LEGACY_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal/fal_type.h"

#ifndef IN_MISC_MINI
    sw_error_t fal_arp_status_set(a_uint32_t dev_id, a_bool_t enable);



    sw_error_t fal_arp_status_get(a_uint32_t dev_id, a_bool_t * enable);
#endif


    sw_error_t fal_frame_max_size_set(a_uint32_t dev_id, a_uint32_t size);



    sw_error_t fal_frame_max_size_get(a_uint32_t dev_id, a_uint32_t * size);



    sw_error_t
    fal_port_unk_sa_cmd_set(a_uint32_t dev_id, fal_port_t port_id,
                            fal_fwd_cmd_t cmd);

    sw_error_t
    fal_port_unk_uc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                               a_bool_t enable);

	sw_error_t
    fal_port_unk_mc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                               a_bool_t enable);

	sw_error_t
    fal_port_bc_filter_set(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t enable);

    sw_error_t
    fal_port_unk_sa_cmd_get(a_uint32_t dev_id, fal_port_t port_id,
                            fal_fwd_cmd_t * cmd);

    sw_error_t
    fal_port_unk_uc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                               a_bool_t * enable);

    sw_error_t
    fal_port_unk_mc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                               a_bool_t * enable);


    sw_error_t
    fal_port_bc_filter_get(a_uint32_t dev_id, fal_port_t port_id,
                           a_bool_t * enable);


    sw_error_t
    fal_cpu_port_status_set(a_uint32_t dev_id, a_bool_t enable);


#ifndef IN_MISC_MINI
    sw_error_t
    fal_cpu_port_status_get(a_uint32_t dev_id, a_bool_t * enable);



    sw_error_t
    fal_bc_to_cpu_port_set(a_uint32_t dev_id, a_bool_t enable);



    sw_error_t
    fal_bc_to_cpu_port_get(a_uint32_t dev_id, a_bool_t * enable);



    sw_error_t
    fal_port_dhcp_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);



    sw_error_t
    fal_port_dhcp_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t * enable);


    sw_error_t
    fal_arp_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd);


    sw_error_t
    fal_arp_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd);
#endif

    sw_error_t
    fal_eapol_cmd_set(a_uint32_t dev_id, fal_fwd_cmd_t cmd);

#ifndef IN_MISC_MINI
    sw_error_t
    fal_eapol_cmd_get(a_uint32_t dev_id, fal_fwd_cmd_t * cmd);
#endif
    sw_error_t
    fal_eapol_status_set(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t enable);
#ifndef IN_MISC_MINI
    sw_error_t
    fal_eapol_status_get(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t * enable);

    sw_error_t
    fal_ripv1_status_set(a_uint32_t dev_id, a_bool_t enable);

    sw_error_t
    fal_ripv1_status_get(a_uint32_t dev_id, a_bool_t * enable);


    sw_error_t
    fal_port_arp_req_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);


    sw_error_t
    fal_port_arp_req_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable);


    sw_error_t
    fal_port_arp_ack_status_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);


    sw_error_t
    fal_port_arp_ack_status_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable);
#endif

    sw_error_t
    fal_intr_mask_set(a_uint32_t dev_id, a_uint32_t intr_mask);


    sw_error_t
    fal_intr_mask_get(a_uint32_t dev_id, a_uint32_t * intr_mask);


    sw_error_t
    fal_intr_status_get(a_uint32_t dev_id, a_uint32_t * intr_status);


    sw_error_t
    fal_intr_status_clear(a_uint32_t dev_id, a_uint32_t intr_status);


    sw_error_t
    fal_intr_mask_mac_linkchg_set(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t enable);


    sw_error_t
    fal_intr_mask_mac_linkchg_get(a_uint32_t dev_id, a_uint32_t port_id, a_bool_t * enable);

    sw_error_t
    fal_intr_status_mac_linkchg_get(a_uint32_t dev_id, fal_pbmp_t *port_bitmap);

    sw_error_t
    fal_intr_status_mac_linkchg_clear(a_uint32_t dev_id);

#ifndef IN_MISC_MINI
    sw_error_t
    fal_cpu_vid_en_set(a_uint32_t dev_id, a_bool_t enable);

    sw_error_t
    fal_cpu_vid_en_get(a_uint32_t dev_id, a_bool_t * enable);

	sw_error_t
    fal_global_macaddr_set(a_uint32_t dev_id, fal_mac_addr_t * addr);

	sw_error_t
    fal_global_macaddr_get(a_uint32_t dev_id, fal_mac_addr_t * addr);


	sw_error_t
	fal_lldp_status_set(a_uint32_t dev_id, a_bool_t enable);



	sw_error_t
	fal_lldp_status_get(a_uint32_t dev_id, a_bool_t * enable);

	sw_error_t
	fal_frame_crc_reserve_set(a_uint32_t dev_id, a_bool_t enable);



	sw_error_t
	fal_frame_crc_reserve_get(a_uint32_t dev_id, a_bool_t * enable);
#endif

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_MISC_LEGACY_H_ */
/**
 * @}
 */
