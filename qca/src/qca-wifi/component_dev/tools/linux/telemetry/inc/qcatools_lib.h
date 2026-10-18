/*
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.

 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Common headerfiles across multiple applications are sourced from here.
 */
#ifndef _QCATOOLS_LIB_H_
#define _QCATOOLS_LIB_H_
#ifdef BUILD_PROFILE_OPEN
#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/wireless.h>
#include <netinet/ether.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <err.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <endian.h>
#include <errno.h>
#include <dirent.h>
#include <ctype.h>
#include <fcntl.h>
#include <inttypes.h>
#if UMAC_SUPPORT_CFG80211
#include <netlink/attr.h>
#include <cfg80211_nlwrapper_api.h>
#endif
#include <sys/queue.h>

#ifndef _LITTLE_ENDIAN
#define _LITTLE_ENDIAN  1234
#endif
#ifndef _BIG_ENDIAN
#define _BIG_ENDIAN 4321
#endif

#if defined(__LITTLE_ENDIAN)
#define _BYTE_ORDER _LITTLE_ENDIAN
#elif defined(__BIG_ENDIAN)
#define _BYTE_ORDER _BIG_ENDIAN
#else
#error "Please fix asm/byteorder.h"
#endif

#if BUILD_X86
struct cfg80211_data {
	void *data; /* data pointer */
	unsigned int length; /* data length */
	unsigned int flags; /* flags for data */
	unsigned int parse_data; /* 1 - data parsed by caller
				  * 0 - data parsed by wrapper
				  */
	/* callback that needs to be called when data recevied from driver */
	void (*callback)(struct cfg80211_data *);
};
#endif

/*
 * default socket id
 */
#define FILE_NAME_LENGTH 64
#define MAX_WIPHY 3
#define MAC_STRING_LENGTH 17

#define QDF_MAC_ADDR_SIZE 6

/*
 * Case-sensitive full length string comparison
 */
#define streq(a, b) ((strlen(a) == strlen(b)) &&\
		(strncasecmp(a, b, strlen(b)) == 0))

typedef enum config_mode_type {
	CONFIG_IOCTL    = 0, /* driver config mode is WEXT */
	CONFIG_CFG80211 = 1, /* driver config mode is cfg80211 */
	CONFIG_INVALID  = 2, /* invalid driver config mode */
} config_mode_type;

struct socket_context {
	u_int8_t cfg80211; /* cfg80211 enable flag */
#if UMAC_SUPPORT_CFG80211
	wifi_cfg80211_context cfg80211_ctxt; /* cfg80211 context */
#endif
	int sock_fd; /* wext socket file descriptor */
};

/*
 * struct queue_entry - Queue Entry
 * @value: abstact object in the queue
 * @tailq: TAILQ Entry
 */
struct queue_entry {
	void *value;
	TAILQ_ENTRY(queue_entry) tailq;
};

TAILQ_HEAD(queue_head, queue_entry);

/*
 * struct queue - Queue
 * @mutex: Mutex for locking queue operation
 * @head: Head of the queue
 * @cnt: Number of objects in the queue
 */
struct queue {
	pthread_mutex_t mutex;
	struct queue_head head;
	int cnt;
};
#endif /* BUILD_PROFILE_OPEN */
#endif
