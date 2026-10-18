/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
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

#ifndef __NSS_PPENL_SOCK_API_H__
#define __NSS_PPENL_SOCK_API_H__

/** @addtogroup chapter_nlsocket
 This chapter describes socket APIs for direct use.

 @note1hang
 Use these APIs(s) only if there are no available helpers for the specific family.
*/

/**
 * @ingroup nss_ppenl_socket_datatypes
 * 	NSS_PPE NL socket context.
 */
struct nss_ppenl_sock_ctx {
	/* Public, caller must populate using helpers */
	const char *family_name;		/**< Family name. */
	void *user_ctx;				/**< Socket user context. */

	/* Private, maintained by the library */
	pthread_t thread;			/**< Response sync. */
	pthread_spinlock_t lock;		/**< Context lock. */
	int ref_cnt;				/**< References to the socket. */

	struct nl_sock *nl_sk;			/**< Linux NL socket. */
	struct nl_cb *nl_cb;			/**< nss_ppe NL callback context. */

	pid_t pid;				/**< Process ID associated with the socket. */
	int family_id;				/**< Family identifier. */
	int grp_id;				/**< Group indentifier. */
	bool is_avail;				/**< Indicates if the socket is available to send or listen. */
};

/** @addtogroup nss_ppenl_socket_macros @{ */

/**
 * Prints error log.
 *
 * @param[in] arg Argument to be printed
 */
#define nss_ppenl_sock_log_error(arg, ...) printf("NSS_PPE_NLERROR(%s[%d]):"arg, __func__, __LINE__, ##__VA_ARGS__)

/**
 * Prints arguments
 *
 * @param[in] arg Argument to be printed
 */
#define nss_ppenl_sock_log_info(arg, ...) printf("NSS_PPE_NLINFO(%s[%d]):"arg, __func__, __LINE__, ##__VA_ARGS__)

/** @} *//* end_addtogroup nss_ppenl_socket_macros */

/** @addtogroup nss_ppenl_socket_functions @{ */

/**
 * Sets family name.
 *
 * @param[in] sock Socket context.
 * @param[in] name Family name.
 *
 * @return
 * None.
 */
static inline void nss_ppenl_sock_set_family(struct nss_ppenl_sock_ctx *sock, const char *name)
{
	sock->family_name = name;
}

/**
 * Sets user context.
 *
 * @param[in] sock Socket context.
 * @param[in] user User context.
 *
 * @return
 * None.
 */
static inline void nss_ppenl_sock_set_user_ctx(struct nss_ppenl_sock_ctx *sock, void *user)
{
	sock->user_ctx = user;
}

/**
 * Extracts NSS_PPE NL message data.
 *
 * @param[in] msg NL message.
 *
 * @return
 * Pointer to start of NSS_PPE NL message.
 */
static inline void *nss_ppenl_sock_get_data(struct nl_msg *msg)
{
	struct genlmsghdr *genl_hdr = nlmsg_data((nlmsg_hdr(msg)));

	return genlmsg_data(genl_hdr);
}

/**
 * Opens NSS_PPE NL family socket.
 *
 * @param[in] sock Socket context to be allocated by the caller.
 * @param[in] cb Callback function for response.
 *
 * @return
 * Status of the operation.
 *
 * @note The underlying entity should set the sock->family name for the socket to open.
 */
int nss_ppenl_sock_open(struct nss_ppenl_sock_ctx *sock, nl_recvmsg_msg_cb_t cb);

/**
 * Closes NSS_PPE NL family socket.
 *
 * @param[in] sock Socket context.
 *
 * @return
 * None.
 */
void nss_ppenl_sock_close(struct nss_ppenl_sock_ctx *sock);

/**
 * Sends NSS_PPE NL message synchronously.
 *
 * @param[in] sock Socket context.
 * @param[in] cm Common message header.
 * @param[in] data Message data.
 * @param[in] has_resp Determines if response is needed from kernel.
 *
 * @detdesc The function blocks until ack/error is received from the kernel
 *       and also blocks for the message response from the kernel if is_resp is TRUE

 * @return
 * Status of the send operation.
 */
int nss_ppenl_sock_send(struct nss_ppenl_sock_ctx *sock, struct nss_ppenl_cmn *cm, void *data, bool has_resp);

/**
 * Listens to asynchronous events from kernel.
 *
 * @param[in] sock Socket context.
 *
 * @return
 * Listen status.
 */
int nss_ppenl_sock_listen(struct nss_ppenl_sock_ctx *sock);

/**/
/** @} *//* end_addtogroup nss_ppenl_socket_functions */

#endif /* __NSS_PPENL_SOCK_API_H__ */
