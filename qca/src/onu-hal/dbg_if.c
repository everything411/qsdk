/*
*
* Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
* SPDX-License-Identifier: ISC
*
*/

#include "dbg_if.h" /* dbg_if_init() */
// System headers
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h> /* unlink() */

#include "libubus.h"

#include "data_model.h"   /* dm_register_transceiver_two() */
#include "dbg_commands.h" /* ADD_INSTANCE */
#include "notif.h"        /* notif_send_dm_instance_added_for_transceiver_two() */
#include "onu_hal_macros.h"
#include "onu_hal_trace.h"

#define DEBUG_SERVER "/var/run/onu_hal_dbg.sock"
#define MAX_MSG_SIZE 128

static struct uloop_fd s_uloop_fd;

typedef void (* handle_dbg_command_fn_t) (void);

static void handle_add_instance(void) {
    if(dm_register_transceiver_two()) {
        return;
    }

    notif_send_dm_instance_added_for_transceiver_two();
}

static void handle_remove_instance(void) {
    dm_unregister_transceiver_two();
    notif_send_dm_instance_removed_for_transceiver_two();
}

static void handle_change_transceiver(void) {
    dm_change_transceiver_one_vendor_rev();
    notif_send_dm_object_changed_for_transceiver_one();
}

static void handle_change_onu_activation(void) {
    dm_change_onu_activation_onu_state();
    notif_send_dm_object_changed_for_onu_activation();
}

static void handle_omci_mib_reset(void) {
    notif_send_omci_reset_mib();
}

typedef struct _dbg_function {
    const char* name;
    handle_dbg_command_fn_t handler;
} dbg_function_t;

static const dbg_function_t DBG_FUNCTIONS[] = {
    { .name = ADD_INSTANCE, .handler = handle_add_instance },
    { .name = REMOVE_INSTANCE, .handler = handle_remove_instance },
    { .name = CHANGE_TRANSCEIVER, .handler = handle_change_transceiver },
    { .name = CHANGE_ONU_ACTIVATION, .handler = handle_change_onu_activation },
    { .name = OMCI_RESET_MIB, .handler = handle_omci_mib_reset  }
};

static void dbg_if_handler(struct uloop_fd* u, UNUSED unsigned int events) {
    when_null_trace(u, exit, ERROR, "struct uloop_fd* param is NULL");

    struct sockaddr_un cliaddr;
    socklen_t len = sizeof(cliaddr);
    char msg[MAX_MSG_SIZE];
    memset(msg, 0, MAX_MSG_SIZE);

    if(recvfrom(u->fd, msg, MAX_MSG_SIZE, 0, (struct sockaddr*) &cliaddr, &len) == -1) {
        SAH_TRACE_ERROR("Failed to read data: %s", strerror(errno));
    } else {
        SAH_TRACE_INFO("msg='%s'", msg);
    }
    const size_t n_dbg_functions = ARRAY_SIZE(DBG_FUNCTIONS);
    size_t i;
    bool found = false;
    for(i = 0; i < n_dbg_functions; ++i) {
        if(strncmp(msg, DBG_FUNCTIONS[i].name, strlen(DBG_FUNCTIONS[i].name)) == 0) {
            DBG_FUNCTIONS[i].handler();
            found = true;
            break;
        }
    }

    if(!found) {
        SAH_TRACE_INFO("Unknown msg [%s]", msg);
    }

exit:
    return;
}

bool dbg_if_init(void) {
    const int fd = socket(AF_LOCAL, SOCK_DGRAM, 0);
    if(fd == -1) {
        SAH_TRACE_ERROR("Failed to open socket: %s", strerror(errno));
        return false;
    }

    unlink(DEBUG_SERVER);

    struct sockaddr_un servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sun_family = AF_LOCAL;
    strlcpy(servaddr.sun_path, DEBUG_SERVER, sizeof(servaddr.sun_path));

    if(bind(fd, (const struct sockaddr*) &servaddr, sizeof(servaddr)) == -1) {
        SAH_TRACE_ERROR("Failed to bind name to socket: %s", strerror(errno));
        goto error;
    }

    s_uloop_fd.cb = dbg_if_handler;
    s_uloop_fd.fd = fd;

    if(uloop_fd_add(&s_uloop_fd, ULOOP_READ)) {
        SAH_TRACE_ERROR("Failed to add dbg IF to uloop");
        goto error;
    }

    return true;

error:
    close(fd);
    s_uloop_fd.fd = 0;
    return false;
}

void dbg_if_cleanup(void) {
    if(s_uloop_fd.fd > 0) {
        uloop_fd_delete(&s_uloop_fd);
        close(s_uloop_fd.fd);
    }
    unlink(DEBUG_SERVER);
}

