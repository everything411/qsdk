// SPDX-License-Identifier: BSD-3-Clause
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 *
 * hostapd_log_extn - SS handler connectivity failure trigger events.
 *
 * Implements hostapd_log_trigger_emit() / hostapd_log_trigger_clear()
 * declared in ap/hostapd_log.h under CONFIG_QCN_EXTN.
 */

#include "utils/includes.h"
#include "utils/common.h"
#include <sys/socket.h>
#include <sys/un.h>
#include "ap/hostapd.h"
#include "ap/hostapd_log.h"
#include "cmn.h"
#include "hostapd_log_extn.h"


struct log_trigger_entry {
	struct dl_list list;
	u8 addr[ETH_ALEN];
};


void hostapd_log_extn_init(struct hostapd_data *hapd)
{
	dl_list_init(&hapd->hapd_extn.log_trigger_sent);
}


static struct log_trigger_entry *
log_trigger_find(struct hostapd_data *hapd, const u8 *addr)
{
	struct log_trigger_entry *e;

	dl_list_for_each(e, &hapd->hapd_extn.log_trigger_sent,
			 struct log_trigger_entry, list) {
		if (os_memcmp(e->addr, addr, ETH_ALEN) == 0)
			return e;
	}
	return NULL;
}


static void hapd_ssh_notify(const u8 *addr, const char *event_type,
			    const char *iface)
{
	struct sockaddr_un dst;
	char msg[128];
	int fd, len;

	fd = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (fd < 0)
		return;

	os_memset(&dst, 0, sizeof(dst));
	dst.sun_family = AF_UNIX;
	os_strlcpy(dst.sun_path, HAPD_SSH_SOCK_PATH, sizeof(dst.sun_path));

	len = os_snprintf(msg, sizeof(msg), "%s " MACSTR " %s",
			  event_type, MAC2STR(addr), iface ? iface : "");
	if (!os_snprintf_error(sizeof(msg), len))
		sendto(fd, msg, len, MSG_DONTWAIT,
		       (struct sockaddr *)&dst, sizeof(dst));
	close(fd);
}


void hostapd_log_trigger_emit(struct hostapd_data *hapd, const u8 *addr,
			      const char *event_type)
{
	struct log_trigger_entry *e;
	const char *iface_name;
	const u8 *event_addr;

	if (!hapd || !event_type)
		return;

	event_addr = addr ? addr : hapd->own_addr;

	if (addr) {
		if (log_trigger_find(hapd, addr))
			return;

		e = os_zalloc(sizeof(*e));
		if (!e)
			return;
		os_memcpy(e->addr, addr, ETH_ALEN);
		dl_list_add_tail(&hapd->hapd_extn.log_trigger_sent, &e->list);
	}

	iface_name = hapd->ctrl_sock_iface[0] ? hapd->ctrl_sock_iface :
		(hapd->conf ? hapd->conf->iface : "unknown");

	wpa_msg(hapd->msg_ctx, MSG_INFO,
		HOSTAPD_LOG_TRIGGER "addr=" MACSTR " iface=%s event=%s",
		MAC2STR(event_addr), iface_name, event_type);

	hapd_ssh_notify(event_addr, event_type, iface_name);
}


void hostapd_log_extn_deinit(struct hostapd_data *hapd)
{
	hostapd_log_trigger_clear(hapd, NULL);
}


void hostapd_log_trigger_clear(struct hostapd_data *hapd, const u8 *addr)
{
	struct log_trigger_entry *e, *tmp;

	if (!hapd)
		return;

	if (addr) {
		e = log_trigger_find(hapd, addr);
		if (e) {
			dl_list_del(&e->list);
			os_free(e);
		}
		return;
	}

	dl_list_for_each_safe(e, tmp, &hapd->hapd_extn.log_trigger_sent,
			      struct log_trigger_entry, list) {
		dl_list_del(&e->list);
		os_free(e);
	}
}
