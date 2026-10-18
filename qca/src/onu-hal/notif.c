/*
*
* Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: ISC
*
*/

#include "notif.h"
#include <stdio.h>

#include "data_model.h" /* dm_get_xpon_onu_object() */
#include "onu_hal_trace.h"

struct ubus_context* s_ctx = NULL;

static struct blob_buf b;

typedef enum _notif_case {
    notif_dm_instance_added = 0,
    notif_dm_instance_removed,
    notif_dm_object_changed_transceiver_one,
    notif_dm_object_changed_onu_activation,
    notif_omci_reset_mib
} notif_case_t;

static const char* notif_case_to_string(notif_case_t notif) {
    switch(notif) {
    case notif_dm_instance_added:                 return "dm:instance-added";
    case notif_dm_instance_removed:               return "dm:instance-removed";
    case notif_dm_object_changed_transceiver_one: return "dm:object-changed";
    case notif_dm_object_changed_onu_activation:  return "dm:object-changed";
    case notif_omci_reset_mib:                    return "omci:reset_mib";
    default: break;
    }
    return NULL;
}

void notif_init(struct ubus_context* ctx) {
    s_ctx = ctx;
}

static void send_notif_common(notif_case_t notif) {
    when_null_trace(s_ctx, exit, ERROR, "ctx is NULL");

    object_wrapper_t* obj = dm_get_xpon_onu_object();
    when_null_trace(obj, exit, ERROR, "xpon_onu object is NULL");
    when_null_trace(obj->name, exit, ERROR, "xpon_onu object has no name");

    const char* const notification = notif_case_to_string(notif);
    when_null_trace(notification, exit, ERROR, "No notification name");

    SAH_TRACE_DEBUG("notification='%s'", notification);

    struct blob_attr* msg = NULL;
    char path[128];
    blob_buf_init(&b, 0);
    switch(notif) {
    case notif_dm_instance_added: /* no break */
    case notif_dm_instance_removed:
        snprintf(path, 128, "%s.ani.1.transceiver", obj->name);
        blobmsg_add_string(&b, "path", path);
        blobmsg_add_u32(&b, "index", 2);
        msg = b.head;
        break;

    case notif_dm_object_changed_transceiver_one:
        snprintf(path, 128, "%s.ani.1.transceiver.1", obj->name);
        blobmsg_add_string(&b, "path", path);
        msg = b.head;
        break;

    case notif_dm_object_changed_onu_activation:
        snprintf(path, 128, "%s.ani.1.tc.onu_activation", obj->name);
        blobmsg_add_string(&b, "path", path);
        msg = b.head;
        break;

    case notif_omci_reset_mib:
        /* Nothing to do */
        break;

    default:
        break;
    }

    const int rc = ubus_notify(s_ctx, &obj->ubus_obj, notification, msg, -1);
    if(rc) {
        SAH_TRACE_ERROR("ubus_notify() failed: rc=%d", rc);
    }

exit:
    return;
}

void notif_send_dm_instance_added_for_transceiver_two(void) {
    send_notif_common(notif_dm_instance_added);
}

void notif_send_dm_instance_removed_for_transceiver_two(void) {
    send_notif_common(notif_dm_instance_removed);
}

void notif_send_dm_object_changed_for_transceiver_one(void) {
    send_notif_common(notif_dm_object_changed_transceiver_one);
}

void notif_send_dm_object_changed_for_onu_activation(void) {
    send_notif_common(notif_dm_object_changed_onu_activation);
}

void notif_send_omci_reset_mib(void) {
    send_notif_common(notif_omci_reset_mib);
}

