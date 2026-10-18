/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#ifndef __NETFN_DTLS_H
#define __NETFN_DTLS_H

#include <netfn_types.h>
#include <netfn_dtls_types.h>
#include <linux/netdevice.h>

typedef void (*netfn_dtls_rx_handler_t)(struct sk_buff *skb, void *data);

/*
 * A tunnel compose of two types of Session(s); one for encapsulation & one for decapsulation.
 * Each tunnel is represented using dev under which Session has to be inserted.
 * DEV --> ENC_SESSION(active, passive) & DEC_SESSION(active, active)
 */

/*
 * netfn_dtls_tun_alloc
 *	Create new DTLS device to handle specific sessions.
 *
 * @param[in] pvt_sz	Private memory size for caller use.
 *
 * @return
 * net_device
 */
struct net_device *netfn_dtls_tun_alloc(ssize_t pvt_sz);

/*
 * netfn_dtls_tun_free
 *	Destroy the DTLS device.
 *
 * @param[in] net_device
 */
void netfn_dtls_tun_free(struct net_device *dev);

/*
 * netfn_dtls_session_add
 *	Insert new session under the DTLS device. Session has to switched to active using @netfn_dtls_tun_session_switch
 *
 * @datatypes
 * netfn_dtls_cfg
 * netfn_tuple_t
 *
 * @param[in] dev	DTLS device under which session will be added.
 * @param[in] data	Session parameters.
 * @param[in] t		session outer tuple (identifier).
 *
 * @return
 * Zero when successful, otherwise error code
 */
int netfn_dtls_tun_session_add(struct net_device *dev, struct netfn_dtls_cfg *cfg, netfn_tuple_t *t);

/*
 * netfn_dtls_tun_session_switch
 *	Switch encapsulation or Decapsulation session to Recent.
 *
 * @param[in] dev	DTLS device under which encapsulation session will be switched
 * @param[in] encap	True for Encapsulation session otherwise False.
 */
void netfn_dtls_tun_session_switch(struct net_device *dev, bool encap);

/*
 * netfn_dtls_session_del
 *	Delete the session from device.
 *
 * @datatypes
 * netfn_dtls_tuple
 *
 * @param[in] dev	DTLS device under which session to be deleted.
 * @param[in] encap	True for Encapsulation session otherwise False.
 * @param[in] epoch	Epoch value used during session addition.
 */
void netfn_dtls_tun_session_del(struct net_device *dev, bool encap, __be16 epoch);

/*
 * netfn_dtls_register_data_cb
 *	Register for DTLS data packet.
 *
 * @datatypes
 * netfn_dtls_rx_handler_t
 *
 * @param[in] dev	DTLS device on which register.
 * @param[in] cb	Callback handler.
 * @param[in] cb_data	App data to pass during CB.
 */
bool netfn_dtls_register_data_cb(struct net_device *dev, netfn_dtls_rx_handler_t cb, void *cb_data);

/*
 * netfn_dtls_unregister_data_cb
 *	Unregister for DTLS data packet.
 *
 * @param[in] dev	DTLS device on which register.
 */
void netfn_dtls_unregister_data_cb(struct net_device *dev);

#endif /* __NETFN_DTLS_H */
