/*
*
* Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: ISC
*
*/
#ifndef __data_model_h__
#define __data_model_h__

#include "libubus.h"

typedef struct _object_wrapper {
    struct ubus_object ubus_obj;
    char* name;
} object_wrapper_t;


int dm_init(struct ubus_context* ctx, int onu_nr);
object_wrapper_t* dm_get_xpon_onu_object(void);
int dm_register_transceiver_two(void);
void dm_unregister_transceiver_two(void);
void dm_change_transceiver_one_vendor_rev(void);
void dm_change_onu_activation_onu_state(void);
void dm_cleanup(void);

#endif
