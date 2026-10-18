/*
*
* Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: ISC
*
*/

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>  /* atoi() */
#include <libubus.h> /* ubus_connect() */
#include <unistd.h>  /* access() */

#include "dbg_if.h"
#include "data_model.h"
#include "notif.h"
#include "onu_hal_macros.h"
#include "onu_hal_trace.h"

static const char* s_ubus_socket_path = "/var/run/ubus/ubus.sock";

static struct ubus_context* s_ctx = NULL;

static void usage(const char* prog) {
    printf("%s -h      : show this help and exit\n", prog);
    printf("%s         : start process with ONU nr = 1\n", prog);
    printf("%s onu_nr  : start process with ONU nr = onu_nr (must be 1 or 2)\n", prog);
    printf("\n");
    printf("Examples:\n");
    printf("%s 2       : advertise xpon_onu.2 on ubus\n", prog);
}

static struct ubus_context* connect_to_ubus(void) {
    struct ubus_context* ctx = NULL;
    if(access(s_ubus_socket_path, F_OK) != 0) {
        s_ubus_socket_path = "/var/run/ubus.sock";
        if(access(s_ubus_socket_path, F_OK) != 0) {
            SAH_TRACE_ERROR("No ubus socket path found");
            return NULL;
        }
    }

    ctx = ubus_connect(s_ubus_socket_path);
    if(!ctx) {
        SAH_TRACE_ERROR("Failed to connect to %s", s_ubus_socket_path);
        return NULL;
    }
    return ctx;
}

int main(int argc, char* argv[]) {

    int onu_nr = 1;
    if(argc == 2) {
        const char* option_one = argv[1];
        if(strncmp(option_one, "-h", 2) == 0) {
            usage(argv[0]);
            return 0;
        } else {
            onu_nr = atoi(option_one);
            if((onu_nr < 1) || (onu_nr > 2)) {
                printf("Error: invalid ONU nr: %d is not in [1, 2]\n", onu_nr);
                return 1;
            }
        }
    }

    SAH_TRACE_INFO("Starting %s with onu_nr=%d", argv[0], onu_nr);

    s_ctx = connect_to_ubus();
    if(!s_ctx) {
        return 1;
    }
    SAH_TRACE_INFO("Connected to %s", s_ubus_socket_path);

    dbg_if_init();

    if(dm_init(s_ctx, onu_nr)) {
        return 1;
    }

    notif_init(s_ctx);

    uloop_init();
    signal(SIGPIPE, SIG_IGN);

    ubus_add_uloop(s_ctx);

    uloop_run();

    SAH_TRACE_INFO("Stopping");
    dbg_if_cleanup();
    dm_cleanup();

    ubus_free(s_ctx);
    uloop_done();

    return 0;
}

