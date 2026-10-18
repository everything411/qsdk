/*
*
* Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: ISC
*
*/

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "data_model.h"
#include "data_model_helper.h"
#include <stdlib.h> /* calloc() */
#include <string.h> /* strdup() */

#include "onu_hal_macros.h"
#include "onu_hal_trace.h"

static const char XPON_ONU[] = "xpon_onu.x";
static const char SW_IMG_1[] = "xpon_onu.x.software_image.1";
static const char SW_IMG_2[] = "xpon_onu.x.software_image.2";
static const char ETH_UNI_1[] = "xpon_onu.x.ethernet_uni.1";
static const char ETH_UNI_STATS[] = "xpon_onu.x.ethernet_uni.1.stats";
static const char ANI_1[] = "xpon_onu.x.ani.1";
static const char ANI_STATS[] = "xpon_onu.x.ani.1.stats";
static const char ONU_ACTIVATION[] = "xpon_onu.x.ani.1.tc.onu_activation";
static const char ANU_TC_PM_PHY[] = "xpon_onu.x.ani.1.tc.pm.phy";
static const char TC_PM_GEM[] = "xpon_onu.x.ani.1.tc.pm.gem";
static const char TC_PM_PLOAM[] = "xpon_onu.x.ani.1.tc.pm.ploam";
static const char TC_PM_OMCI[] = "xpon_onu.x.ani.1.tc.pm.omci";
static const char TC_GEM[] = "xpon_onu.x.ani.1.tc.gem";
static const char GEM_PORT_0[] = "xpon_onu.x.ani.1.tc.gem.port.0";
static const char GEM_PORT0_PM[] = "xpon_onu.x.ani.1.tc.gem.port.0.pm";
static const char GEM_PORT_1[] = "xpon_onu.x.ani.1.tc.gem.port.1";
static const char GEM_PORT1_PM[] = "xpon_onu.x.ani.1.tc.gem.port.1.pm";
static const char GEM_PORT_2[] = "xpon_onu.x.ani.1.tc.gem.port.2";
static const char GEM_PORT2_PM[] = "xpon_onu.x.ani.1.tc.gem.port.2.pm";
static const char GEM_PORT_3[] = "xpon_onu.x.ani.1.tc.gem.port.3";
static const char GEM_PORT3_PM[] = "xpon_onu.x.ani.1.tc.gem.port.3.pm";
static const char GEM_PORT_4[] = "xpon_onu.x.ani.1.tc.gem.port.4";
static const char GEM_PORT4_PM[] = "xpon_onu.x.ani.1.tc.gem.port.4.pm";
static const char GEM_PORT_5[] = "xpon_onu.x.ani.1.tc.gem.port.5";
static const char GEM_PORT5_PM[] = "xpon_onu.x.ani.1.tc.gem.port.5.pm";
static const char GEM_PORT_6[] = "xpon_onu.x.ani.1.tc.gem.port.6";
static const char GEM_PORT6_PM[] = "xpon_onu.x.ani.1.tc.gem.port.6.pm";
static const char GEM_PORT_7[] = "xpon_onu.x.ani.1.tc.gem.port.7";
static const char GEM_PORT7_PM[] = "xpon_onu.x.ani.1.tc.gem.port.7.pm";
static const char GEM_PORT_8[] = "xpon_onu.x.ani.1.tc.gem.port.8";
static const char GEM_PORT8_PM[] = "xpon_onu.x.ani.1.tc.gem.port.8.pm";
static const char PERF_THRESHOLDS[] = "xpon_onu.x.ani.1.tc.performance_thresholds";
static const char ALARMS[] = "xpon_onu.x.ani.1.tc.alarms";
static const char TRANSCEIVER_1[] = "xpon_onu.x.ani.1.transceiver.1";
static const char TRANSCEIVER_2[] = "xpon_onu.x.ani.1.transceiver.2";

static const char XPON_ONU_ONE[] = "xpon_onu.1";
static const char XPON_ONU_TWO[] = "xpon_onu.2";

static const char METHOD_GET[] = "get";
static const char METHOD_ENABLE[] = "enable";
static const char METHOD_DISABLE[] = "disable";
static const char METHOD_GET_PARAMS[] = "get_params";

/* To test dm:object-changed notification */
static uint32_t s_transceiver_1_vendor_rev = 1;
static uint32_t s_onu_state = 2;


typedef enum _obj_id {
    id_xpon_onu       = 0,
    id_sw_img_1,
    id_sw_img_2,
    id_eth_uni_1,
    id_eth_uni_stats,
    id_ani_1,
    id_ani_stats,
    id_onu_activation,
    id_anu_tc_pm_phy,
    id_tc_pm_gem,
    id_tc_pm_ploam,
    id_tc_pm_omci,
    id_tc_gem,
    id_gem_port_0,
    id_gem_port0_pm,
    id_gem_port_1,
    id_gem_port1_pm,
    id_gem_port_2,
    id_gem_port2_pm,
    id_gem_port_3,
    id_gem_port3_pm,
    id_gem_port_4,
    id_gem_port4_pm,
    id_gem_port_5,
    id_gem_port5_pm,
    id_gem_port_6,
    id_gem_port6_pm,
    id_gem_port_7,
    id_gem_port7_pm,
    id_gem_port_8,
    id_gem_port8_pm,
    id_perf_thresholds,
    id_alarms,
    id_transceiver_1,
    id_transceiver_2,
    n_objects
} obj_id_t;

typedef struct _object_info {
    obj_id_t id;
    const char* name;
    const struct ubus_method* methods;
    uint32_t n_methods;
} object_info_t;

enum {
    PARAM_NAME,
    MAX_PARAMS
};

static int32_t s_rx_power = -2000;
static int32_t s_tx_power = -3000;
static uint32_t s_voltage = 0;
static uint32_t s_bias = 0;
static int32_t s_temperature = -274;

static int common_method_handler(struct ubus_context* ctx, struct ubus_object* obj,
                                 struct ubus_request_data* req, const char* method,
                                 UNUSED struct blob_attr* msg);

static const struct ubus_method METHODS_GET_ONLY[] = {
    { .name = METHOD_GET, .handler = common_method_handler, .policy = NULL, .n_policy = 0 }
};

static const struct ubus_method METHODS_GET_ENABLE_AND_DISABLE[] = {
    { .name = METHOD_GET, .handler = common_method_handler, .policy = NULL, .n_policy = 0 },
    { .name = METHOD_ENABLE, .handler = common_method_handler, .policy = NULL, .n_policy = 0 },
    { .name = METHOD_DISABLE, .handler = common_method_handler, .policy = NULL, .n_policy = 0 }
};

static const struct blobmsg_policy GET_PARAMS_POLICY[] = {
    { .name = "names", .type = BLOBMSG_TYPE_STRING }
};

static struct ubus_method METHODS_TRANSCEIVER[] = {
    { .name = METHOD_GET, .handler = common_method_handler, .policy = NULL, .n_policy = 0 },
    { .name = METHOD_GET_PARAMS, .handler = common_method_handler, .policy = GET_PARAMS_POLICY, .n_policy = 1 }
};

static const object_info_t OBJECT_INFO[n_objects] = {
    {
        .id = id_xpon_onu,
        .name = XPON_ONU,
        .methods = METHODS_GET_ENABLE_AND_DISABLE,
        .n_methods = ARRAY_SIZE(METHODS_GET_ENABLE_AND_DISABLE)
    },
    {
        .id = id_sw_img_1,
        .name = SW_IMG_1,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_sw_img_2,
        .name = SW_IMG_2,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_eth_uni_1,
        .name = ETH_UNI_1,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_eth_uni_stats,
        .name = ETH_UNI_STATS,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_ani_1,
        .name = ANI_1,
        .methods = METHODS_GET_ENABLE_AND_DISABLE,
        .n_methods = ARRAY_SIZE(METHODS_GET_ENABLE_AND_DISABLE)
    },
    {
        .id = id_ani_stats,
        .name = ANI_STATS,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_onu_activation,
        .name = ONU_ACTIVATION,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_anu_tc_pm_phy,
        .name = ANU_TC_PM_PHY,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_tc_pm_gem,
        .name = TC_PM_GEM,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_tc_pm_ploam,
        .name = TC_PM_PLOAM,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_tc_pm_omci,
        .name = TC_PM_OMCI,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_tc_gem,
        .name = TC_GEM,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_gem_port_0,
        .name = GEM_PORT_0,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_gem_port0_pm,
        .name = GEM_PORT0_PM,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_gem_port_1,
        .name = GEM_PORT_1,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_gem_port1_pm,
        .name = GEM_PORT1_PM,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
        {
        .id = id_gem_port_2,
        .name = GEM_PORT_2,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_gem_port2_pm,
        .name = GEM_PORT2_PM,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
        {
        .id = id_gem_port_3,
        .name = GEM_PORT_3,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_gem_port3_pm,
        .name = GEM_PORT3_PM,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
        {
        .id = id_gem_port_4,
        .name = GEM_PORT_4,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_gem_port4_pm,
        .name = GEM_PORT4_PM,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
        {
        .id = id_gem_port_5,
        .name = GEM_PORT_5,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_gem_port5_pm,
        .name = GEM_PORT5_PM,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
        {
        .id = id_gem_port_6,
        .name = GEM_PORT_6,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_gem_port6_pm,
        .name = GEM_PORT6_PM,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
        {
        .id = id_gem_port_7,
        .name = GEM_PORT_7,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_gem_port7_pm,
        .name = GEM_PORT7_PM,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
        {
        .id = id_gem_port_8,
        .name = GEM_PORT_8,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_gem_port8_pm,
        .name = GEM_PORT8_PM,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_perf_thresholds,
        .name = PERF_THRESHOLDS,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_alarms,
        .name = ALARMS,
        .methods = METHODS_GET_ONLY,
        .n_methods = ARRAY_SIZE(METHODS_GET_ONLY)
    },
    {
        .id = id_transceiver_1,
        .name = TRANSCEIVER_1,
        .methods = METHODS_TRANSCEIVER,
        .n_methods = ARRAY_SIZE(METHODS_TRANSCEIVER)
    },
    {
        .id = id_transceiver_2,
        .name = TRANSCEIVER_2,
        .methods = METHODS_TRANSCEIVER,
        .n_methods = ARRAY_SIZE(METHODS_TRANSCEIVER)
    }
};

static const char DIGITS[] = { '0', '1', '2' };

static struct blob_buf b;

static struct ubus_context* s_ctx = NULL;
static int s_onu_nr = 1;

static bool s_onu_enabled = false;
static bool s_ani_one_enabled = false;


typedef struct _request {
    struct ubus_request_data req;
    struct uloop_timeout timeout;
    char* obj_name;
    char* method_name;
    char* param_name;
} request_t;

static object_wrapper_t* s_objects[n_objects];


static void request_delete(request_t* req) {
    when_null(req, exit);
    free(req->obj_name);
    free(req->method_name);
    free(req->param_name);
    free(req);
exit:
    return;
}

static bool str_equal(const char* const str1, const char* const str2, size_t str1_len) {
    return ((str1_len == strlen(str2)) &&
            (strncmp(str1, str2, str1_len) == 0)) ? true : false;
}

static void data_model_for_get_method(const char* name) {
    const size_t name_len = strlen(name);
    char path[128];
    snprintf(path, 128, "%s", name);
    const int index_of_onu_nr = strlen("xpon_onu.");
    path[index_of_onu_nr] = 'x';
    /**
     * 'path' has same length as 'name'. So code below can use 'name_len'
     * for both.
     */

    if(str_equal(path, XPON_ONU, name_len)) 
    {
        SAH_TRACE_INFO("path='%s' => XPON_ONU", path);
        blobmsg_add_u8(&b, "enable", s_onu_enabled ? 1 : 0);
        // blobmsg_add_u8(&b, "enable", 1);
        blobmsg_add_string(&b, "version", gpon_get_pm_modulefirmwareversion());
        blobmsg_add_string(&b, "equipment_id", gpon_get_equipment_id());
        blobmsg_add_u32(&b, "software_number_of_entries", gpon_get_software_number_of_entries());
        blobmsg_add_u32(&b, "ethernet_uni_number_of_entries", gpon_get_eth_uni_number_of_entries());
        blobmsg_add_u32(&b, "ani_number_of_entries", 1);
        if(str_equal(name, XPON_ONU_ONE, name_len)) 
        {
            SAH_TRACE_INFO("name='%s' => XPON_ONU_ONE", name);
            blobmsg_add_string(&b, "name", "ONU_ONE");
        } 
        else if(str_equal(name, XPON_ONU_TWO, name_len)) 
        {
            SAH_TRACE_INFO("name='%s' => XPON_ONU_TWO", name);
            blobmsg_add_string(&b, "name", "ONU_TWO");
        }
    } 
    
    else if(str_equal(path, SW_IMG_1, name_len)) 
    {
        SAH_TRACE_INFO("path='%s' => SW_IMG_1", path);
        blobmsg_add_u32(&b, "id", 0);
        blobmsg_add_u8(&b, "is_committed", gpon_get_sw_img_is_committed(1));
        blobmsg_add_u8(&b, "is_active", gpon_get_sw_img_is_active(1));
        blobmsg_add_u8(&b, "is_valid", gpon_get_sw_img_is_valid(1));
        blobmsg_add_string(&b, "version", gpon_get_pm_modulefirmwareversion());
    } 
    
    else if(str_equal(path, SW_IMG_2, name_len)) 
    {
        SAH_TRACE_INFO("path='%s' => SW_IMG_2", path);
        blobmsg_add_u32(&b, "id", 1);
        blobmsg_add_u8(&b, "is_committed", gpon_get_sw_img_is_committed(2));
        blobmsg_add_u8(&b, "is_active", gpon_get_sw_img_is_active(2));
        blobmsg_add_u8(&b, "is_valid", gpon_get_sw_img_is_valid(2));
        blobmsg_add_string(&b, "version", gpon_get_pm_modulefirmwareversion());
    } 
    
    else if(str_equal(path, ETH_UNI_1, name_len))
    {
        SAH_TRACE_INFO("path='%s' => ETH_UNI_1", path);
        blobmsg_add_u8(&b, "enable", gpon_get_xpon_eth_uni_enable());
        blobmsg_add_string(&b, "name", gpon_get_eth_uni_name());
        blobmsg_add_string(&b, "alias", "alias");
        blobmsg_add_string(&b, "status", gpon_get_eth_uni_status());
        blobmsg_add_string(&b, "ani_list", "MyAni");
        blobmsg_add_string(&b, "last_change", "last change");
        blobmsg_add_string(&b, "lower_layers", "lower_layers");
        blobmsg_add_string(&b, "interdomain_id", gpon_get_eth_uni_interdomain_id());
        blobmsg_add_string(&b, "interdomain_name", "");
    } 
    
    else if (str_equal(path, ETH_UNI_STATS, name_len))
    {
        SAH_TRACE_INFO("path='%s' => ETH_UNI_STATS", path);
        blobmsg_add_u64(&b, "bytes_sent", gpon_get_eth_bytes_sent());
        blobmsg_add_u64(&b, "bytes_received", gpon_get_eth_bytes_received());
        blobmsg_add_u64(&b, "packets_sent", gpon_get_eth_packets_sent());
        blobmsg_add_u64(&b, "packets_received", gpon_get_eth_packets_received());
        blobmsg_add_u64(&b, "errors_sent", gpon_get_eth_errors_sent());
        blobmsg_add_u64(&b, "errors_received", gpon_get_eth_errors_received());
        blobmsg_add_u64(&b, "unicast_packets_sent", gpon_get_unicast_packets_sent());
        blobmsg_add_u64(&b, "unicast_packets_received", gpon_get_unicast_packets_received());
        blobmsg_add_u64(&b, "discard_packets_sent", gpon_get_discard_packets_sent());
        blobmsg_add_u64(&b, "discard_packets_received", gpon_get_discard_packets_received());
        blobmsg_add_u64(&b, "multicast_packets_sent", gpon_get_multicast_packets_sent());
        blobmsg_add_u64(&b, "multicast_packets_received", gpon_get_multicast_packets_received());
        blobmsg_add_u64(&b, "broadcast_packets_sent", gpon_get_broadcast_packets_sent());
        blobmsg_add_u64(&b, "broadcast_packets_received", gpon_get_broadcast_packets_received());
        blobmsg_add_u64(&b, "unknown_proto_packets_received", 0);
    }

    else if(str_equal(path, ANI_1, name_len)) 
    {
        SAH_TRACE_INFO("path='%s' => ANI_1", path);
        blobmsg_add_u8(&b, "enable", s_ani_one_enabled ? 1 : 0);
        blobmsg_add_string(&b, "name", "MyANI");
        blobmsg_add_string(&b, "alias", "alias");
        blobmsg_add_u64(&b, "last_change", gpon_get_onu_ani_last_change());
        blobmsg_add_string(&b, "status", gpon_get_PM_status());
        blobmsg_add_string(&b, "pon_mode", "XGS-PON");
        blobmsg_add_u8(&b, "transceiver_number_of_entries", 1);
    } 
    
    else if(str_equal(path, ANI_STATS, name_len))
    {
        SAH_TRACE_INFO("path='%s' => ANI_STATS", path);
        blobmsg_add_u64(&b, "bytes_sent", gpon_get_onu_ani_stats("us_byte_out"));
        blobmsg_add_u64(&b, "bytes_received", gpon_get_onu_ani_stats("ds_byte_in"));  
        blobmsg_add_u64(&b, "packets_sent", gpon_get_ani_stats_packets_sent());  
        blobmsg_add_u64(&b, "packets_received", gpon_get_ani_stats_packets_received());  
        blobmsg_add_u64(&b, "errors_sent", gpon_get_ani_errors_sent());
        blobmsg_add_u64(&b, "errors_received", gpon_get_ani_errors_received()); 
        blobmsg_add_u64(&b, "unicast_packets_sent", gpon_get_onu_ani_stats("us_ucast"));  
        blobmsg_add_u64(&b, "unicast_packets_received", gpon_get_onu_ani_stats("ds_ucast"));  
        blobmsg_add_u64(&b, "discard_packets_sent", gpon_get_ani_stats_discard_packets_sent());  
        blobmsg_add_u64(&b, "discard_packets_received", gpon_get_ani_stats_discard_packets_received());  
        blobmsg_add_u64(&b, "multicast_packets_sent", gpon_get_onu_ani_stats("us_mcast"));  
        blobmsg_add_u64(&b, "multicast_packets_received", gpon_get_onu_ani_stats("ds_mcast"));  
        blobmsg_add_u64(&b, "broadcast_packets_sent", gpon_get_onu_ani_stats("us_bcast"));  
        blobmsg_add_u64(&b, "broadcast_packets_received", gpon_get_onu_ani_stats("ds_bcast"));  
        blobmsg_add_u64(&b, "unknown_proto_packets_received", 0); // always 0

    }

    else if(str_equal(path, ONU_ACTIVATION, name_len)) {
        SAH_TRACE_INFO("path='%s' => ONU_ACTIVATION", path);
        char onu_state[3];
        snprintf(onu_state, 3, "O%u", s_onu_state);
        blobmsg_add_string(&b, "onu_state", gpon_get_pm_operationalstate());  
        blobmsg_add_string(&b, "vendor_id", gpon_get_ploam_vendorid());  
        blobmsg_add_string(&b, "serial_number", gpon_get_ploam_serialnumber()); 
        blobmsg_add_u32(&b, "onu_id", gpon_get_ploam_onuid());  
    }

    else if(str_equal(path, ANU_TC_PM_PHY, name_len)) 
    {
        SAH_TRACE_INFO("path='%s' => ANU_TC_PM_PHY", path);
        blobmsg_add_u64(&b, "corrected_fec_bytes", gpon_get_onu_ani_tc_pm("*fec_byte_corrected"));  
        blobmsg_add_u64(&b, "corrected_fec_codewords", gpon_get_onu_ani_tc_pm("fec_codewd_corrected"));  
        blobmsg_add_u64(&b, "uncorrectable_fec_codewords", gpon_get_onu_ani_tc_pm("fec_codewd_uncorrected"));  
        blobmsg_add_u64(&b, "total_fec_codewords", gpon_get_onu_ani_tc_pm("fec_codewd"));  
        blobmsg_add_u64(&b, "psbd_hec_error_count", gpon_get_onu_ani_tc_pm("xgem_hd_hec_err"));  
        blobmsg_add_u64(&b, "header_hec_error_count", gpon_get_onu_ani_tc_pm("fs_hec_err"));  
        blobmsg_add_u64(&b, "unknown_profile", gpon_get_onu_ani_tc_pm("unknown_bst_prof"));  
    }
    
    else if(str_equal(path, TC_PM_GEM, name_len)) {
        SAH_TRACE_INFO("path='%s' => TC_PM_GEM", path);
        blobmsg_add_u64(&b, "frames_sent", gpon_get_gem_transmittedframes(1));  
        blobmsg_add_u64(&b, "frames_received", gpon_get_gem_receivedframes(1));  
        blobmsg_add_u64(&b, "frame_header_hec_errors", gpon_get_frame_header_hec_errors());  
        blobmsg_add_u64(&b, "key_errors", gpon_get_xgem_key_errors());  
    }
    
    else if(str_equal(path, TC_PM_PLOAM, name_len)){
        SAH_TRACE_INFO("path='%s' => TC_PM_PLOAM", path);
        blobmsg_add_u64(&b, "mic_errors", gpon_get_ploam_micerrors());  
        blobmsg_add_u64(&b, "downstream_message_count", gpon_get_ploam_rxmesgcount());  
        blobmsg_add_u64(&b, "ranging_time", gpon_get_ranging_time());  
        blobmsg_add_u64(&b, "upstream_message_count", gpon_get_ploam_txmesgcount());  
    }
    
    else if(str_equal(path, TC_PM_OMCI, name_len)){
        SAH_TRACE_INFO("path='%s' => TC_PM_OMCI", path);
        blobmsg_add_u64(&b, "baseline_messages_received", gpon_get_omci_baselinemessagecount());  
        blobmsg_add_u64(&b, "extended_messages_received", gpon_get_omci_extendedmessagecount());  
        blobmsg_add_u64(&b, "mic_errors", gpon_get_omci_micerrors());  
    }

    else if(str_equal(path, TC_GEM, name_len)){
        SAH_TRACE_INFO("path='%s' => TC_GEM", path);
        blobmsg_add_u32(&b, "port_number_of_entries", gpon_get_port_number_of_entries());
    }

    else if(str_equal(path, GEM_PORT_0, name_len)) {
        SAH_TRACE_INFO("path='%s' => GEM_PORT_0", path);
        blobmsg_add_u32(&b, "port_id", gpon_get_gem_port_id(0));  
        blobmsg_add_string(&b, "direction", gpon_get_gem_directions(0));  
        blobmsg_add_string(&b, "port_type", gpon_get_gem_traffctype(0));  
    } 
    
    else if (str_equal(path, GEM_PORT1_PM, name_len))
    {
        blobmsg_add_u64(&b, "frames_sent", gpon_get_gem_transmittedframes(0));  
        blobmsg_add_u64(&b, "frames_received", gpon_get_gem_receivedframes(0));  
    }
    
    else if(str_equal(path, GEM_PORT_1, name_len)) {
        SAH_TRACE_INFO("path='%s' => GEM_PORT_1", path);
        blobmsg_add_u32(&b, "port_id", gpon_get_gem_port_id(1));  
        blobmsg_add_string(&b, "direction", gpon_get_gem_directions(1));  
        blobmsg_add_string(&b, "port_type", gpon_get_gem_traffctype(1));  
    } 
    
    else if (str_equal(path, GEM_PORT1_PM, name_len))
    {
        blobmsg_add_u64(&b, "frames_sent", gpon_get_gem_transmittedframes(1));  
        blobmsg_add_u64(&b, "frames_received", gpon_get_gem_receivedframes(1));  
    }

    else if(str_equal(path, GEM_PORT_2, name_len)) {
        SAH_TRACE_INFO("path='%s' => GEM_PORT_2", path);
        blobmsg_add_u32(&b, "port_id", gpon_get_gem_port_id(2));  
        blobmsg_add_string(&b, "direction", gpon_get_gem_directions(2));  
        blobmsg_add_string(&b, "port_type", gpon_get_gem_traffctype(2));  
    } 
    
    else if (str_equal(path, GEM_PORT2_PM, name_len)) 
    {
        blobmsg_add_u64(&b, "frames_sent", gpon_get_gem_transmittedframes(2));  
        blobmsg_add_u64(&b, "frames_received", gpon_get_gem_receivedframes(2));  
    }

    else if(str_equal(path, GEM_PORT_3, name_len)) {
        SAH_TRACE_INFO("path='%s' => GEM_PORT_3", path);
        blobmsg_add_u32(&b, "port_id", gpon_get_gem_port_id(3));  
        blobmsg_add_string(&b, "direction", gpon_get_gem_directions(3));  
        blobmsg_add_string(&b, "port_type", gpon_get_gem_traffctype(3));  
    } 
    
    else if (str_equal(path, GEM_PORT3_PM, name_len)) 
    {
        blobmsg_add_u64(&b, "frames_sent", gpon_get_gem_transmittedframes(3));  
        blobmsg_add_u64(&b, "frames_received", gpon_get_gem_receivedframes(3));  
    }

    else if(str_equal(path, GEM_PORT_4, name_len)) {
        SAH_TRACE_INFO("path='%s' => GEM_PORT_4", path);
        blobmsg_add_u32(&b, "port_id", gpon_get_gem_port_id(4));  
        blobmsg_add_string(&b, "direction", gpon_get_gem_directions(4));  
        blobmsg_add_string(&b, "port_type", gpon_get_gem_traffctype(4));  
    } 
    
    else if (str_equal(path, GEM_PORT4_PM, name_len)) 
    {
        blobmsg_add_u64(&b, "frames_sent", gpon_get_gem_transmittedframes(4));  
        blobmsg_add_u64(&b, "frames_received", gpon_get_gem_receivedframes(4));  
    }

    else if(str_equal(path, GEM_PORT_5, name_len)) {
        SAH_TRACE_INFO("path='%s' => GEM_PORT_5", path);
        blobmsg_add_u32(&b, "port_id", gpon_get_gem_port_id(5));  
        blobmsg_add_string(&b, "direction", gpon_get_gem_directions(5));  
        blobmsg_add_string(&b, "port_type", gpon_get_gem_traffctype(5));  
    } 
    
    else if (str_equal(path, GEM_PORT5_PM, name_len)) 
    {
        blobmsg_add_u64(&b, "frames_sent", gpon_get_gem_transmittedframes(5));  
        blobmsg_add_u64(&b, "frames_received", gpon_get_gem_receivedframes(5));  
    }

    else if(str_equal(path, GEM_PORT_6, name_len)) {
        SAH_TRACE_INFO("path='%s' => GEM_PORT_6", path);
        blobmsg_add_u32(&b, "port_id", gpon_get_gem_port_id(6));  
        blobmsg_add_string(&b, "direction", gpon_get_gem_directions(6));  
        blobmsg_add_string(&b, "port_type", gpon_get_gem_traffctype(6));  
    } 
    
    else if (str_equal(path, GEM_PORT6_PM, name_len)) 
    {
        blobmsg_add_u64(&b, "frames_sent", gpon_get_gem_transmittedframes(6));  
        blobmsg_add_u64(&b, "frames_received", gpon_get_gem_receivedframes(6));  
    }

    else if(str_equal(path, GEM_PORT_7, name_len)) {
        SAH_TRACE_INFO("path='%s' => GEM_PORT_7", path);
        blobmsg_add_u32(&b, "port_id", gpon_get_gem_port_id(7));  
        blobmsg_add_string(&b, "direction", gpon_get_gem_directions(7));  
        blobmsg_add_string(&b, "port_type", gpon_get_gem_traffctype(7));  
    } 
    
    else if (str_equal(path, GEM_PORT7_PM, name_len)) 
    {
        blobmsg_add_u64(&b, "frames_sent", gpon_get_gem_transmittedframes(7));  
        blobmsg_add_u64(&b, "frames_received", gpon_get_gem_receivedframes(7));  
    }
    
    else if(str_equal(path, GEM_PORT_8, name_len)) {
        SAH_TRACE_INFO("path='%s' => GEM_PORT_8", path);
        blobmsg_add_u32(&b, "port_id", gpon_get_gem_port_id(8));
        blobmsg_add_string(&b, "direction", gpon_get_gem_directions(8));  
        blobmsg_add_string(&b, "port_type", gpon_get_gem_traffctype(8));  
    } 
    
    else if (str_equal(path, GEM_PORT8_PM, name_len)) 
    {
        blobmsg_add_u64(&b, "frames_sent", gpon_get_gem_transmittedframes(8));  
        blobmsg_add_u64(&b, "frames_received", gpon_get_gem_receivedframes(8));  
    }

    else if(str_equal(path, PERF_THRESHOLDS, name_len)) 
    {
        SAH_TRACE_INFO("path='%s' => PERF_THRESHOLDS", path);
        blobmsg_add_u32(&b, "signal_fail", gpon_get_tc_perf_signal_fail());  
        blobmsg_add_u32(&b, "signal_degrade", gpon_get_tc_perf_signal_degrade());  
    } 
    
    else if(str_equal(path, ALARMS, name_len))
    {
        SAH_TRACE_INFO("path='%s' => ALARMS", path);
        blobmsg_add_u8(&b, "los", gpon_get_pm_alarm_los());  
        blobmsg_add_u8(&b, "lof", gpon_get_pm_alarm_lof());  
        blobmsg_add_u8(&b, "sf", gpon_get_pm_alarm_sf());  
        blobmsg_add_u8(&b, "sd", gpon_get_pm_alarm_sd());  
        blobmsg_add_u8(&b, "lcdg", gpon_get_pm_alarm_lcdg());  
        blobmsg_add_u8(&b, "tf", gpon_get_pm_alarm_tf());  
        blobmsg_add_u8(&b, "suf", gpon_get_pm_alarm_suf());  
        blobmsg_add_u8(&b, "mem", gpon_get_pm_alarm_mem());  
        blobmsg_add_u8(&b, "dact", gpon_get_pm_alarm_dact());  
        blobmsg_add_u8(&b, "dis", gpon_get_pm_alarm_dis());  
        blobmsg_add_u8(&b, "mis", 0);  
        blobmsg_add_u8(&b, "pee", 0);  
        blobmsg_add_u8(&b, "rdi", 0);  
        blobmsg_add_u8(&b, "lods", gpon_get_pm_alarm_lods());  
        blobmsg_add_u8(&b, "rogue", gpon_get_pm_alarm_rogue());  
    } 
    
    else if(str_equal(path, TRANSCEIVER_1, name_len)) 
    {
        SAH_TRACE_INFO("path='%s' => TRANSCEIVER_1", path);

        char vendor_rev[64];
        snprintf(vendor_rev, 64, "Version_%d", s_transceiver_1_vendor_rev);

        blobmsg_add_u32(&b, "id", 0);
        blobmsg_add_u32(&b, "identifier", 2);
        blobmsg_add_string(&b, "vendor_name", "Qualcomm");  
        blobmsg_add_string(&b, "vendor_part_number", "QCF8000");  
        blobmsg_add_string(&b, "vendor_revision", gpon_get_pm_modulefirmwareversion());  
        blobmsg_add_string(&b, "pon_mode", "XGS-PON");
        blobmsg_add_string(&b, "connector", "SC"); 
        blobmsg_add_u64(&b, "nominal_bit_rate_downstream", gpon_get_nominalbitrate());
        blobmsg_add_u64(&b, "nominal_bit_rate_upstream", gpon_get_nominalbitrate());
        s_rx_power=gpon_get_rxp_signallevel();
        blobmsg_add_u32(&b, "rx_power", s_rx_power);
        s_tx_power=gpon_get_txp_signallevel();
        blobmsg_add_u32(&b, "tx_power", s_tx_power);  
        s_voltage=gpon_get_voltage();
        blobmsg_add_u32(&b, "voltage", s_voltage); 
        s_bias=gpon_get_bias();
        blobmsg_add_u32(&b, "bias", s_bias);
        s_temperature=gpon_get_temperature();
        blobmsg_add_u32(&b, "temperature", s_temperature);
    } 
    
    else if(str_equal(path, TRANSCEIVER_2, name_len)) 
    {
        SAH_TRACE_INFO("path='%s' => TRANSCEIVER_2", path);
        blobmsg_add_u32(&b, "id", 1);  
        blobmsg_add_u32(&b, "identifier", 25);  
        blobmsg_add_string(&b, "vendor_name", "Qualcomm");  
        blobmsg_add_string(&b, "pon_mode", "XGS-PON");  
    } 
    
    else 
    {
        SAH_TRACE_ERROR("Unknown object: %s", name);
    }
}


static void set_enable(const char* const obj_name, bool enable) {
    printf("--------------- inside set_enable function -----------------\n");
    char path[128];
    snprintf(path, 128, "%s", obj_name);
    const int index_of_onu_nr = strlen("xpon_onu.");
    path[index_of_onu_nr] = 'x';
    const size_t path_len = strlen(obj_name);

    if(str_equal(path, XPON_ONU, path_len))
    {
        if(s_onu_enabled != enable)
        {
            SAH_TRACE_INFO("s_onu_enabled: %d -> %d", s_onu_enabled, enable);
            s_onu_enabled = enable;
            blobmsg_add_u8(&b, "enable", s_onu_enabled);
            gpon_set_xpon_onu_enable(enable);
        }
        blobmsg_add_u8(&b, "enable", s_onu_enabled);
    } 

    else if(str_equal(path, ANI_1, path_len)) 
    {
        if(s_ani_one_enabled != enable) 
        {
            SAH_TRACE_INFO("s_ani_one_enabled: %d -> %d", s_ani_one_enabled, enable);
            s_ani_one_enabled = enable;
            blobmsg_add_u8(&b, "enable", s_ani_one_enabled);
            gpon_set_xpon_onu_ani_enable(enable);
        }
        blobmsg_add_u8(&b, "enable", s_ani_one_enabled);
    }

    else 
    {
        SAH_TRACE_ERROR("Unknown object: '%s'", path);
    }
}

static void data_model_for_get_params_method(const char* const obj_name,
                                                 const char* const param_name) {
    char path[128];
    snprintf(path, 128, "%s", obj_name);
    const int index_of_onu_nr = strlen("xpon_onu.");
    path[index_of_onu_nr] = 'x';
    const size_t path_len = strlen(path);

    if(str_equal(path, TRANSCEIVER_1, path_len) ||
       str_equal(path, TRANSCEIVER_2, path_len)) {

        const size_t len = strlen(param_name);

        if(str_equal(param_name, "rx_power", len)) {
            blobmsg_add_u32(&b, "rx_power", s_rx_power);
            ++s_rx_power;
        } else if(str_equal(param_name, "tx_power", len)) {
            blobmsg_add_u32(&b, "tx_power", s_tx_power);
            ++s_tx_power;
        } else if(str_equal(param_name, "voltage", len)) {
            blobmsg_add_u32(&b, "voltage", s_voltage);
            ++s_voltage;
        } else if(str_equal(param_name, "bias", len)) {
            blobmsg_add_u32(&b, "bias", s_bias);
            ++s_bias;
        } else if(str_equal(param_name, "temperature", len)) {
            blobmsg_add_u32(&b, "temperature", s_temperature);
            ++s_temperature;
        } else {
            SAH_TRACE_ERROR("object: %s: unknown param_name: %s", obj_name, param_name);
        }
    } else {
        SAH_TRACE_ERROR("Unknown object: %s", obj_name);
    }
}


static void method_cb(struct uloop_timeout* t) {
    request_t* req = container_of(t, request_t, timeout);
    when_null(req, exit);
    when_null_trace(req->method_name, exit, ERROR, "method_name is NULL");
    const char* const method = req->method_name; /* alias */
    const size_t method_len = strlen(method);

    blob_buf_init(&b, 0);
    if(str_equal(method, METHOD_GET, method_len)) {
        data_model_for_get_method(req->obj_name);
    } else if(str_equal(method, METHOD_ENABLE, method_len)) {
        set_enable(req->obj_name, true);
    } else if(str_equal(method, METHOD_DISABLE, method_len)) {
        set_enable(req->obj_name, false);
    } else if(str_equal(method, METHOD_GET_PARAMS, method_len)) {
        data_model_for_get_params_method(req->obj_name, req->param_name);
    } else {
        SAH_TRACE_ERROR("Unknown method: '%s'", method);
    }

    if(ubus_send_reply(s_ctx, &req->req, b.head)) {
        SAH_TRACE_ERROR("Failed to send msg");
    }

    ubus_complete_deferred_request(s_ctx, &req->req, 0);

exit:
    request_delete(req);
    return;
}

static int common_method_handler(struct ubus_context* ctx, struct ubus_object* obj,
                                 struct ubus_request_data* req, const char* method,
                                 struct blob_attr* msg) {
    int rc = UBUS_STATUS_UNKNOWN_ERROR;

    when_null_trace(obj, exit, ERROR, "obj is NULL");
    when_null_trace(obj->name, exit, ERROR, "obj->name is NULL");

    SAH_TRACE_INFO("%s.%s()", obj->name, method);

    request_t* mreq = (request_t*) calloc(1, sizeof(request_t));
    when_null_trace(mreq, exit, ERROR, "Failed to allocate mem for request_t");

    ubus_defer_request(ctx, req, &mreq->req);

    mreq->obj_name = strdup(obj->name);
    mreq->method_name = strdup(method);
    mreq->timeout.cb = method_cb;

    struct blob_attr* tb[MAX_PARAMS];
    if(str_equal(method, METHOD_GET_PARAMS, strlen(method))) {
        blobmsg_parse(GET_PARAMS_POLICY, ARRAY_SIZE(GET_PARAMS_POLICY), tb, blob_data(msg), blob_len(msg));
        if(tb[PARAM_NAME] != NULL) {
            char* param_name = blobmsg_data(tb[PARAM_NAME]);
            if(param_name) {
                SAH_TRACE_DEBUG("param: %s", param_name);
                mreq->param_name = strdup(param_name);
            } else {
                SAH_TRACE_ERROR("%s.%s(): failed to get param name(s)", obj->name, method);
                request_delete(mreq);
                goto exit;
            }
        } else {
            SAH_TRACE_ERROR("%s.%s(): failed to get param name(s)", obj->name, method);
            request_delete(mreq);
            goto exit;
        }
    }

    uloop_timeout_set(&mreq->timeout, 1000);

    rc = 0;

exit:
    return rc;
}

static void delete_object_wrapper(object_wrapper_t* obj) {
    if(obj->ubus_obj.type) {
        free(obj->ubus_obj.type);
    }
    if(obj->name) {
        free(obj->name);
    }
    free(obj);
}


static object_wrapper_t* create_object_wrapper(obj_id_t id, const char* path) {
    object_wrapper_t* obj = (object_wrapper_t*) calloc(1, sizeof(object_wrapper_t));
    when_null_trace(obj, exit, ERROR, "Failed to allocate mem");

    obj->ubus_obj.type = (struct ubus_object_type*) calloc(1, sizeof(struct ubus_object_type));
    if(NULL == obj->ubus_obj.type) {
        SAH_TRACE_ERROR("Failed to allocate mem for ubus_object_type");
        free(obj);
        return NULL;
    }

    obj->name = strdup(path);

    obj->ubus_obj.name = obj->name;
    obj->ubus_obj.methods = OBJECT_INFO[id].methods;
    obj->ubus_obj.n_methods = OBJECT_INFO[id].n_methods;

    obj->ubus_obj.type->name = obj->name;
    obj->ubus_obj.type->methods = OBJECT_INFO[id].methods;
    obj->ubus_obj.type->n_methods = OBJECT_INFO[id].n_methods;

exit:
    return obj;
}

static int register_object(obj_id_t id) {
    int rc = 1;

    if(id >= n_objects) {
        SAH_TRACE_ERROR("id=%d >= n_objects=%d", id, n_objects);
        goto exit;
    }

    char path[128];
    const int index = strlen("xpon_onu."); /* index of 'x' char */

    snprintf(path, 128, "%s", OBJECT_INFO[id].name);
    /* Replace 'x' char with ONU nr: '1' or '2'. */
    path[index] = DIGITS[s_onu_nr];

    object_wrapper_t* obj = create_object_wrapper(id, path);
    when_null_trace(obj, exit, ERROR,
                    "Failed to create object for %s", path);

    if(ubus_add_object(s_ctx, &obj->ubus_obj) != UBUS_STATUS_OK) {
        SAH_TRACE_ERROR("Failed to add '%s' to ubus", path);
        delete_object_wrapper(obj);
        goto exit;
    }
    s_objects[id] = obj;

    rc = 0;
exit:
    return rc;
}

static void unregister_object(obj_id_t id) {
    if(id >= n_objects) {
        SAH_TRACE_ERROR("id=%d >= n_objects=%d", id, n_objects);
        return;
    }
    if(s_objects[id] == NULL) {
        SAH_TRACE_DEBUG("Object with id=%d does not exist", id);
        return;
    }

    object_wrapper_t* obj = s_objects[id];

    if(ubus_remove_object(s_ctx, &obj->ubus_obj)) {
        SAH_TRACE_ERROR("Failed to remove '%s' from ubus", obj->name);
    }
    delete_object_wrapper(obj);
    s_objects[id] = NULL;
}

static int dm_advertise_onu(int onu_nr) {
    int rc = 1;

    const size_t n_digits = ARRAY_SIZE(DIGITS);
    if(onu_nr > (int) n_digits) {
        SAH_TRACE_ERROR("Invalid onu_nr [%d]", onu_nr);
        goto exit;
    }

    size_t i;

    for(i = 0; i < id_transceiver_2; ++i) {
        if(register_object(i) != 0) {
            goto exit;
        }
    }

    rc = 0;

exit:
    return rc;
}

int dm_init(struct ubus_context* ctx, int onu_nr) {
    s_ctx = ctx;
    s_onu_nr = onu_nr;

    return dm_advertise_onu(onu_nr);
}

object_wrapper_t* dm_get_xpon_onu_object(void) {
    return s_objects[id_xpon_onu];
}

int dm_register_transceiver_two(void) {
    if(s_objects[id_transceiver_2] != NULL) {
        SAH_TRACE_ERROR("transceiver.2 already exists");
        return 1;
    }

    return register_object(id_transceiver_2);
}

void dm_unregister_transceiver_two(void) {
    unregister_object(id_transceiver_2);
}

void dm_change_transceiver_one_vendor_rev(void) {
    ++s_transceiver_1_vendor_rev;
}

void dm_change_onu_activation_onu_state(void) {
    ++s_onu_state;
    if(s_onu_state > 9) {
        s_onu_state = 1;
    }
}

void dm_cleanup(void) {
    int i;

    for(i = 0; i < n_objects; ++i) {
        if(s_objects[i]) {
            unregister_object(i);
        }
    }
}
