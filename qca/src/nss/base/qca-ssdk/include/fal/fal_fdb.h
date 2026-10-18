/*
 * Copyright (c) 2012, 2015-2018, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

/**
 * @defgroup fal_fdb FAL_FDB
 * @{
 */
#ifndef _FAL_FDB_H_
#define _FAL_FDB_H_

#ifdef __cplusplus
extern "C" {
#endif                          /* __cplusplus */

#include "sw.h"
#include "fal/fal_type.h"
#include "fal_fdb_legacy.h"

    typedef struct
    {
        fal_mac_addr_t addr;
        a_uint16_t    fid;
        a_uint8_t load_balance;
    } fal_fdb_rfs_t;

    typedef struct
    {
        a_bool_t enable; /* enable port learn limit or not */
        a_uint32_t limit_num; /* port learn limit number */
        fal_fwd_cmd_t action; /* the action when port learn number exceed limit*/
    } fal_maclimit_ctrl_t;

#define FAL_FDB_DEL_STATIC   0x1

    typedef struct
    {
        a_bool_t port_en; /* enable port value matching or not */
        a_bool_t fid_en; /* enable fid value matching or not */
        a_bool_t multicast_en; /* enable multicast value matching or not */
    } fal_fdb_op_t;


    typedef enum
    {
        DIS_LAERNING = 0,
        HW_CTRL_LEARNING,
        SW_CTRL_LEARNING
    } fal_fdb_learning_ctrl;

    typedef enum
    {
        FAL_FDB_AGE_DISABLE = 0,
        FAL_FDB_AGE_ENABLE,
        FAL_FDB_AGE_ENABLE_TIMER_ONLY
    } fal_fdb_age_ctrl_t;

    sw_error_t
    fal_fdb_entry_add(a_uint32_t dev_id, const fal_fdb_entry_t * entry);

    sw_error_t
    fal_fdb_entry_flush(a_uint32_t dev_id, a_uint32_t flag);


    sw_error_t
    fal_fdb_entry_del_byport(a_uint32_t dev_id, a_uint32_t port_id, a_uint32_t flag);

    sw_error_t
    fal_fdb_entry_del_bymac(a_uint32_t dev_id, const fal_fdb_entry_t *entry);

    sw_error_t
    fal_fdb_entry_getfirst(a_uint32_t dev_id, fal_fdb_entry_t * entry);


#ifndef IN_FDB_MINI
    sw_error_t
    fal_fdb_entry_getnext(a_uint32_t dev_id, fal_fdb_entry_t * entry);
#endif

    sw_error_t
    fal_fdb_entry_search(a_uint32_t dev_id, fal_fdb_entry_t * entry);


sw_error_t
    fal_fdb_port_learn_set(a_uint32_t dev_id, fal_port_t port_id, a_bool_t enable);


#ifndef IN_FDB_MINI
    sw_error_t
    fal_fdb_port_learn_get(a_uint32_t dev_id, fal_port_t port_id, a_bool_t *enable);
#endif
    sw_error_t
    fal_fdb_port_learning_ctrl_set(a_uint32_t dev_id, fal_port_t port_id,
                                 a_bool_t enable, fal_fwd_cmd_t cmd);

    sw_error_t
    fal_fdb_port_learning_ctrl_get(a_uint32_t dev_id, fal_port_t port_id,
                                 a_bool_t *enable, fal_fwd_cmd_t *cmd);

    sw_error_t
    fal_fdb_port_stamove_ctrl_set(a_uint32_t dev_id, fal_port_t port_id,
                                 a_bool_t enable, fal_fwd_cmd_t cmd);

    sw_error_t
    fal_fdb_port_stamove_ctrl_get(a_uint32_t dev_id, fal_port_t port_id,
                                 a_bool_t *enable, fal_fwd_cmd_t *cmd);

    sw_error_t
    fal_fdb_aging_ctrl_set(a_uint32_t dev_id, fal_fdb_age_ctrl_t ctrl);

#ifndef IN_FDB_MINI
    sw_error_t
    fal_fdb_aging_ctrl_get(a_uint32_t dev_id, fal_fdb_age_ctrl_t * ctrl);
#endif

    sw_error_t
    fal_fdb_learning_ctrl_set(a_uint32_t dev_id, fal_fdb_learning_ctrl ctrl);

#ifndef IN_FDB_MINI
    sw_error_t
    fal_fdb_learning_ctrl_get(a_uint32_t dev_id, fal_fdb_learning_ctrl *ctrl);

#endif
    sw_error_t
    fal_fdb_aging_time_set(a_uint32_t dev_id, a_uint32_t * time);


    sw_error_t
    fal_fdb_aging_time_get(a_uint32_t dev_id, a_uint32_t * time);

    sw_error_t
    fal_fdb_entry_getnext_byindex(a_uint32_t dev_id, a_uint32_t * iterator, fal_fdb_entry_t * entry);

    sw_error_t
    fal_fdb_entry_extend_getnext(a_uint32_t dev_id, fal_fdb_op_t * option,
                        fal_fdb_entry_t * entry);

    sw_error_t
    fal_fdb_entry_extend_getfirst(a_uint32_t dev_id, fal_fdb_op_t * option,
                         fal_fdb_entry_t * entry);

#ifndef IN_FDB_MINI
    sw_error_t
    fal_fdb_entry_update_byport(a_uint32_t dev_id, fal_port_t old_port, fal_port_t new_port,
                     a_uint32_t fid, fal_fdb_op_t * option);
#endif
    sw_error_t
    fal_port_fdb_learn_limit_set(a_uint32_t dev_id, fal_port_t port_id,
                                 a_bool_t enable, a_uint32_t cnt);

    sw_error_t
    fal_port_fdb_learn_limit_get(a_uint32_t dev_id, fal_port_t port_id,
                                 a_bool_t * enable, a_uint32_t * cnt);

    sw_error_t
    fal_port_fdb_learn_exceed_cmd_set(a_uint32_t dev_id, fal_port_t port_id,
                                      fal_fwd_cmd_t cmd);


    sw_error_t
    fal_port_fdb_learn_exceed_cmd_get(a_uint32_t dev_id, fal_port_t port_id,
                                      fal_fwd_cmd_t * cmd);
#ifndef IN_FDB_MINI
    sw_error_t
    fal_fdb_port_learned_mac_counter_get(a_uint32_t dev_id, fal_port_t port_id,
                                  a_uint32_t * cnt);

    sw_error_t
    fal_fdb_port_add(a_uint32_t dev_id, a_uint32_t fid, fal_mac_addr_t * addr, fal_port_t port_id);

    sw_error_t
    fal_fdb_port_del(a_uint32_t dev_id, a_uint32_t fid, fal_mac_addr_t * addr, fal_port_t port_id);
#endif
    sw_error_t
    fal_fdb_port_maclimit_ctrl_set(a_uint32_t dev_id, fal_port_t port_id, fal_maclimit_ctrl_t * maclimit_ctrl);

    sw_error_t
    fal_fdb_port_maclimit_ctrl_get(a_uint32_t dev_id, fal_port_t port_id, fal_maclimit_ctrl_t * maclimit_ctrl);

    sw_error_t
    fal_fdb_entry_del_byfid(a_uint32_t dev_id, a_uint16_t fid, a_uint32_t flag);

#define fal_fdb_add 	fal_fdb_entry_add
#define fal_fdb_del_all	fal_fdb_entry_flush
#define fal_fdb_del_by_port	fal_fdb_entry_del_byport
#define fal_fdb_del_by_mac	fal_fdb_entry_del_bymac
#define fal_fdb_first		fal_fdb_entry_getfirst
#define fal_fdb_next		fal_fdb_entry_getnext
#define fal_fdb_find		fal_fdb_entry_search
#define fal_fdb_age_ctrl_set		fal_fdb_aging_ctrl_set
#define fal_fdb_age_ctrl_get		fal_fdb_aging_ctrl_get
#define fal_fdb_age_time_set		fal_fdb_aging_time_set
#define fal_fdb_age_time_get		fal_fdb_aging_time_get
#define fal_fdb_iterate			fal_fdb_entry_getnext_byindex
#define fal_fdb_extend_next		fal_fdb_entry_extend_getnext
#define fal_fdb_extend_first		fal_fdb_entry_extend_getfirst
#define fal_fdb_transfer		fal_fdb_entry_update_byport

#ifdef __cplusplus
}
#endif                          /* __cplusplus */
#endif                          /* _FAL_FDB_H_ */
/**
 * @}
 */
