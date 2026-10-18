/*
 * Copyright (c) 2014, 2017, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


#include <stdarg.h>
#include "sw.h"
#include "ssdk_init.h"
#include "sw_api.h"
#include "sw_api_us.h"
#include "api_access.h"

sw_error_t
sw_uk_exec(a_uint32_t api_id, ...)
{
    unsigned long value[SW_MAX_API_PARAM] = { 0 };
    unsigned long rtn = SW_OK, i;
    va_list arg_ptr;
    a_uint32_t nr_param = 0;

    if((nr_param = sw_api_param_nums(api_id)) == 0)
    {
        return SW_NOT_SUPPORTED;
    }

    if (nr_param > SW_MAX_API_PARAM - 2)
        return SW_BAD_PARAM;

    value[0] = (unsigned long)api_id;
    value[1] = (unsigned long)&rtn;

    va_start(arg_ptr, api_id);
    for (i = 0; i < nr_param; i++)
    {
        value[i + 2] = va_arg(arg_ptr, unsigned long);
    }
    va_end(arg_ptr);
    sw_uk_if(value);

    return rtn;
}

sw_error_t
ssdk_init(a_uint32_t dev_id, ssdk_init_cfg * cfg)
{
    sw_error_t rv;

    rv = sw_uk_init(cfg->nl_prot);
    return rv;
}

sw_error_t
ssdk_cleanup(void)
{
    sw_error_t rv;

    rv = sw_uk_cleanup();
    return rv;
}

