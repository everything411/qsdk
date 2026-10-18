// SPDX-License-Identifier: BSD-3-Clause-Clear
/*
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/device.h>
#include <linux/debugfs.h>
#include <linux/vmalloc.h>
#include "core.h"
#include "wmi.h"
#include "debug.h"
#include "debugfs.h"
#include "dp_mon.h"
#include <linux/inet.h>
#include <net/sock.h>

/* Convert a kernel virtual address to a kernel logical address */
static void ath12k_pktlog_release(struct ath12k_pktlog *pktlog)
{
	uintptr_t vaddr, vaddr_start, vaddr_end;
	unsigned long page_cnt;
	struct page *page;

	vaddr_start = (uintptr_t)pktlog->buf;
	page_cnt = DIV_ROUND_UP(sizeof(*pktlog->buf) + pktlog->buf_size,
				PAGE_SIZE);
	vaddr_end = vaddr_start + (page_cnt * PAGE_SIZE);

	for (vaddr = vaddr_start; vaddr < vaddr_end; vaddr += PAGE_SIZE) {
		page = vmalloc_to_page((const void *)vaddr);
		if (page)
#if LINUX_VERSION_IS_LESS(6,8,0)
			clear_bit(PG_reserved, &page->flags);
#else
			ClearPageReserved(page);
#endif
	}

	vfree(pktlog->buf);
	pktlog->buf = NULL;
}

static int ath12k_alloc_pktlog_buf(struct ath12k *ar)
{
	u32 page_cnt;
	uintptr_t vaddr, vaddr_start, vaddr_end;
	struct page *page;
	void *buf;
	struct ath12k_pktlog *pktlog = &ar->debug.pktlog;

	if (pktlog->buf_size == 0)
		return -EINVAL;

	page_cnt = DIV_ROUND_UP(sizeof(*pktlog->buf) + pktlog->buf_size,
				PAGE_SIZE);
	buf =  vmalloc((page_cnt + PKTLOG_EXTRA_PAGES) * PAGE_SIZE);
	if (!buf)
		return -ENOMEM;

	pktlog->buf = (struct ath12k_pktlog_buf *)ALIGN((uintptr_t)(buf),
							PAGE_SIZE);
	pktlog->buf_alloc_size = pktlog->buf_size;
	vaddr_start = (uintptr_t)pktlog->buf;
	vaddr_end = vaddr_start + (page_cnt * PAGE_SIZE);

	for (vaddr = vaddr_start; vaddr < vaddr_end; vaddr += PAGE_SIZE) {
		page = vmalloc_to_page((const void *)vaddr);
		if (page)
#if LINUX_VERSION_IS_LESS(6,8,0)
			set_bit(PG_reserved, &page->flags);
#else
			SetPageReserved(page);
#endif
	}

	return 0;
}

/**
 * ath12k_pktlog_remote_service_send - Send data over remote pktlog socket
 * @service: Pointer to remote pktlog service structure
 * @buf: Buffer containing data to send
 * @len: Length of data to send
 *
 * Wrapper function for sock_sendmsg() to send pktlog data over TCP socket.
 * The function validates the service state and socket availability before
 * attempting to send data.
 *
 * Return: Number of bytes sent on success, negative error code on failure
 */
int ath12k_pktlog_remote_service_send(struct ath12k_pktlog_remote_service *service,
				      char *buf, int len)
{
	struct msghdr msg = {0};
	struct kvec iov = {0};
	int size = 0;

	if (!service || !buf)
		return 0;

	if (!service->connect_done)
		return -ENOTCONN;

	if (len <= 0 || len > ATH12K_MAX_SEND_SIZE)
		return -EINVAL;

	if (!service->send_socket) {
		service->missed_records++;
		return -ENOTCONN;
	}

	iov.iov_base = buf;
	iov.iov_len = len;

	iov_iter_kvec(&msg.msg_iter, ITER_SOURCE, &iov, 1, iov.iov_len);

	size = sock_sendmsg(service->send_socket, &msg);
	if (size < 0) {
		service->missed_records++;
		return size;
	}

	return size;
}

/**
 * ath12k_pktlog_stop_service - Stop remote pktlog service
 * @ar: ath12k radio pointer
 *
 * Stops the remote pktlog service and releases all allocated resources
 * including sockets and work queues.
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_pktlog_stop_service(struct ath12k *ar)
{
	struct ath12k_pktlog *pl_info;
	struct ath12k_pktlog_remote_service *service;

	if (!ar)
		return -EINVAL;

	pl_info = &ar->debug.pktlog;
	if (!pl_info->rpktlog_svc)
		return -EINVAL;

	service = pl_info->rpktlog_svc;
	service->running = 0;
	service->connect_done = 0;

	if (service->send_socket) {
		sock_release(service->send_socket);
		service->send_socket = NULL;
	}
	if (service->listen_socket) {
		sock_release(service->listen_socket);
		service->listen_socket = NULL;
	}

	service->missed_records = 0;
	service->fend_counts = 0;
	service->port = 0;

	return 0;
}

/**
 * ath12k_pktlog_start_accept_service - Accept incoming connections (server mode)
 * @work: Work structure
 *
 * Work queue function that accepts incoming connections on the listen socket
 * and schedules the send service for data transmission.
 */
static void ath12k_pktlog_start_accept_service(struct work_struct *work)
{
	struct ath12k_pktlog_remote_service *service;
	struct ath12k *ar;
	struct socket *accept_socket = NULL;
	int ret;

	service = container_of(work, struct ath12k_pktlog_remote_service,
			       accept_service);
	ar = service->ar;
	if (!ar || !ar->ab) {
		ath12k_warn(ar->ab, "Remote pktlog: Invalid ar pointer");
		return;
	}

	while (service->running && service->listen_socket) {
		ret = kernel_accept(service->listen_socket, &accept_socket,
				    O_NONBLOCK);
		if (ret == 0)
			break;

		if (ret == -EAGAIN || ret == -EWOULDBLOCK) {
			msleep(100);
			continue;
		}

		ath12k_dbg(ar->ab, ATH12K_DBG_DATA,
			   "Remote pktlog: accept failed: %d\n", ret);
		return;
	}

	if (!service->running || !service->listen_socket) {
		if (accept_socket)
			sock_release(accept_socket);
		return;
	}

	if (service->send_socket) {
		ath12k_warn(ar->ab,
			    "Remote pktlog: Closing previous connection\n");
		sock_release(service->send_socket);
		service->connect_done = 0;
	}

	service->send_socket = accept_socket;
	service->connect_done = 1;

	ath12k_info(ar->ab, "Remote pktlog: Connection accepted\n");
	schedule_work(&service->send_service);

	if (service->running)
		schedule_work(&service->accept_service);
}

/**
 * ath12k_pktlog_start_remote_service - Start remote pktlog server
 * @work: Work structure
 *
 * Work queue function that creates a listen socket, binds to the configured
 * port, and starts listening for incoming connections. Schedules the accept
 * service to handle connections.
 */
static void ath12k_pktlog_start_remote_service(struct work_struct *work)
{
	struct ath12k_pktlog_remote_service *service;
	struct ath12k *ar;
	struct socket *sock = NULL;
	struct sockaddr_in server_addr;
	int ret;
	int opt = 1;

	service = container_of(work, struct ath12k_pktlog_remote_service,
			       connection_service);

	if (!service || !service->running)
		return;

	ar = service->ar;
	if (!ar || !ar->ab) {
		ath12k_warn(ar->ab, "Remote pktlog: Invalid ar pointer");
		return;
	}

	ret = sock_create(AF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
	if (ret < 0) {
		ath12k_err(ar->ab, "Remote pktlog: Failed to create socket: %d\n",
			   ret);
		return;
	}

	ret = sock_setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
			      KERNEL_SOCKPTR(&opt), sizeof(opt));
	if (ret < 0) {
		ath12k_err(ar->ab, "Remote pktlog: Failed to set SO_REUSEADDR: %d\n",
			   ret);
		sock_release(sock);
		return;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	server_addr.sin_port = htons(service->port);

	ret = kernel_bind(sock, (struct sockaddr *)&server_addr,
			  sizeof(server_addr));
	if (ret < 0) {
		ath12k_err(ar->ab, "Remote pktlog: Failed to bind to port %u: %d\n",
			   service->port, ret);
		sock_release(sock);
		return;
	}

	ret = kernel_listen(sock, ATH12K_MAX_LISTEN_CONNECTIONS);
	if (ret < 0) {
		ath12k_err(ar->ab, "Remote pktlog: Failed to listen: %d\n", ret);
		sock_release(sock);
		return;
	}

	service->listen_socket = sock;

	ath12k_info(ar->ab, "Remote pktlog: Server started on port %u\n",
		    service->port);
	schedule_work(&service->accept_service);
}

/**
 * ath12k_pktlog_start_remote_service_client - Start remote pktlog client
 * @work: Work structure
 *
 * Work queue function that creates a client socket and connects to the
 * configured remote server IP address. Schedules the send service for
 * data transmission after successful connection.
 */
static void ath12k_pktlog_start_remote_service_client(struct work_struct *work)
{
	struct ath12k_pktlog_remote_service *service;
	struct ath12k_pktlog *pl_info;
	struct ath12k *ar;
	struct socket *sock = NULL;
	struct sockaddr_in server_addr;
	struct __kernel_sock_timeval tv = {.tv_sec = 5, .tv_usec = 0};
	int ret;
	u8 ip[4];

	service = container_of(work, struct ath12k_pktlog_remote_service,
			       client_service);
	if (!service || !service->running)
		return;

	ar = service->ar;
	if (!ar || !ar->ab) {
		ath12k_warn(ar->ab, "Remote pktlog client: Invalid ar pointer");
		return;
	}

	pl_info = &ar->debug.pktlog;
	if (!pl_info->pktlog_remote_client || !pl_info->ipaddr[0]) {
		ath12k_warn(ar->ab, "No remote IP address configured\n");
		return;
	}

	ret = sscanf(pl_info->ipaddr, "%hhu.%hhu.%hhu.%hhu",
		     &ip[0], &ip[1], &ip[2], &ip[3]);
	if (ret != 4) {
		ath12k_err(ar->ab, "Remote pktlog: Invalid IP format: %s\n",
			   pl_info->ipaddr);
		return;
	}

	ret = sock_create(AF_INET, SOCK_STREAM, IPPROTO_TCP, &sock);
	if (ret < 0) {
		ath12k_err(ar->ab,
			   "Remote pktlog: Failed to create socket: %d\n", ret);
		return;
	}

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = htonl((ip[0] << 24) | (ip[1] << 16) |
					    (ip[2] << 8) | ip[3]);
	server_addr.sin_port = htons(service->port);

	sock_setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO_NEW,
			KERNEL_SOCKPTR(&tv), sizeof(tv));

	ret = kernel_connect(sock, (struct sockaddr *)&server_addr,
			     sizeof(server_addr), 0);
	if (ret < 0) {
		ath12k_err(ar->ab, "Remote pktlog: Failed to connect to %s:%u: %d\n",
			   pl_info->ipaddr, service->port, ret);
		sock_release(sock);
		return;
	}

	if (service->send_socket) {
		ath12k_warn(ar->ab, "Remote pktlog: Closing previous connection\n");
		sock_release(service->send_socket);
		service->connect_done = 0;
	}

	service->send_socket = sock;
	service->connect_done = 1;

	ath12k_info(ar->ab, "Remote pktlog: Connected to %s:%u\n",
		    pl_info->ipaddr, service->port);

	/* Schedule send service */
	schedule_work(&service->send_service);
}

/**
 * ath12k_pktlog_run_send_service - Main data transmission service
 * @work: Work structure
 *
 * Work queue function that handles the main pktlog data transmission.
 * Performs initial handshake (device_id, radio_name, header) and then
 * continuously streams pktlog data from the circular buffer in chunks.
 * Handles buffer wrap-around with two-part sends.
 */
static void ath12k_pktlog_run_send_service(struct work_struct *work)
{
	struct ath12k_pktlog_remote_service *service;
	struct ath12k_pktlog *pl_info;
	struct ath12k *ar;
	struct ath12k_pktlog_buf *log_buf;
	char *buf_ptr;
	u32 available, send_size, part1, part2;
	int ret;
	u32 read_idx;
	bool handshake_done = false;
	u64 total_bytes_sent = 0;

	service = container_of(work, struct ath12k_pktlog_remote_service,
			       send_service);
	if (!service)
		return;

	ar = service->ar;
	if (!ar || !ar->ab)
		return;

	if (!service->running || !service->connect_done) {
		ath12k_warn(ar->ab,
			    "Send service: not running or not connected\n");
		return;
	}

	pl_info = &ar->debug.pktlog;
	log_buf = pl_info->buf;
	if (!log_buf) {
		ath12k_warn(ar->ab, "Pktlog buffer not allocated\n");
		return;
	}

	ath12k_dbg(ar->ab, ATH12K_DBG_DATA,
		   "Remote pktlog: send_service start, write=%u read=%u max=%u wrap=%d\n",
		   pl_info->rlog_write_index, pl_info->rlog_read_index,
		   pl_info->rlog_max_size, pl_info->is_wrap);

	while (service->running && service->connect_done) {
		if (!handshake_done) {
			struct ath12k_pktlog_remote_id_hdr id_hdr;
			struct sockaddr_in local_addr;

			memset(&id_hdr, 0, sizeof(id_hdr));
			memcpy(id_hdr.mac_addr, ar->ab->mac_addr, ETH_ALEN);

			if (kernel_getsockname(service->send_socket,
					       (struct sockaddr *)&local_addr) == 0)
				memcpy(id_hdr.ip_addr, &local_addr.sin_addr.s_addr, 4);
			else
				ath12k_dbg(ar->ab, ATH12K_DBG_DATA,
					   "Remote pktlog: could not get local IP\n");

			if (ar->pdev && ar->pdev->phy_name)
				strscpy(id_hdr.radio_name, ar->pdev->phy_name,
					sizeof(id_hdr.radio_name));

			ret = ath12k_pktlog_remote_service_send(service,
								(char *)&id_hdr,
								sizeof(id_hdr));
			if (ret < 0) {
				ath12k_warn(ar->ab,
					    "Failed to send ID header: %d\n",
					    ret);
				goto reconnect;
			}

			ret = ath12k_pktlog_remote_service_send(service,
							(char *)&log_buf->bufhdr,
							ATH12K_PKTLOG_HEADER_SIZE);
			if (ret < 0) {
				ath12k_warn(ar->ab,
					    "Failed to send buffer header: %d\n",
					    ret);
				goto reconnect;
			}

			handshake_done = true;
			ath12k_dbg(ar->ab, ATH12K_DBG_DATA,
				   "Remote pktlog: Handshake done: MAC=%pM, radio=%s\n",
				   ar->ab->mac_addr,
				   ar->pdev ? ar->pdev->phy_name : "unknown");
			ath12k_dbg(ar->ab, ATH12K_DBG_DATA,
				   "(buf_size=%u, write_idx=%u, read_idx=%u)\n",
				   pl_info->buf_size,
				   pl_info->rlog_write_index,
				   pl_info->rlog_read_index);
		}

		spin_lock_bh(&pl_info->lock);

		if (pl_info->rlog_read_index >= pl_info->rlog_max_size) {
			ath12k_warn(ar->ab, "Invalid read index, resetting\n");
			pl_info->rlog_read_index = 0;
		}

		if (pl_info->is_wrap) {
			available = (pl_info->rlog_max_size - pl_info->rlog_read_index) +
				pl_info->rlog_write_index;
		} else {
			if (pl_info->rlog_write_index >= pl_info->rlog_read_index)
				available = pl_info->rlog_write_index -
					pl_info->rlog_read_index;
			else
				available = 0;
		}
		spin_unlock_bh(&pl_info->lock);

		if (available == 0) {
			msleep(ATH12K_PKTLOG_SEND_SLEEP_MS);
			continue;
		}

		if (!service->running && available > 0 &&
		    available <= ATH12K_MAX_SEND_SIZE) {
			buf_ptr = (char *)log_buf->log_data;

			if (pl_info->rlog_read_index + available <=
			    pl_info->rlog_max_size) {
				ret = ath12k_pktlog_remote_service_send(service,
						buf_ptr + pl_info->rlog_read_index,
						available);
				if (ret > 0) {
					spin_lock_bh(&pl_info->lock);
					pl_info->rlog_read_index += available;
					spin_unlock_bh(&pl_info->lock);
				}
				total_bytes_sent += ret;
			} else {
				part1 = pl_info->rlog_max_size - pl_info->rlog_read_index;
				ret = ath12k_pktlog_remote_service_send(service,
					buf_ptr + pl_info->rlog_read_index, part1);
				if (ret > 0) {
					ret = ath12k_pktlog_remote_service_send(service,
							buf_ptr, available - part1);
					if (ret > 0) {
						spin_lock_bh(&pl_info->lock);
						pl_info->rlog_read_index =
							available - part1;
						spin_unlock_bh(&pl_info->lock);
					}
				}
				total_bytes_sent += ret;
			}

			if (ret > 0)
				ath12k_dbg(ar->ab, ATH12K_DBG_DATA,
					   "Remote pktlog: sent last %u bytes\n",
					   available);
			break;
		}

		send_size = min(available, (u32)ATH12K_MAX_SEND_SIZE);
		buf_ptr = (char *)log_buf->log_data;

		spin_lock_bh(&pl_info->lock);
		if (pl_info->rlog_read_index + send_size > pl_info->rlog_max_size) {
			part1 = pl_info->rlog_max_size - pl_info->rlog_read_index;
			part2 = send_size - part1;
			read_idx = pl_info->rlog_read_index;

			spin_unlock_bh(&pl_info->lock);

			ret = ath12k_pktlog_remote_service_send(service,
								buf_ptr + read_idx,
								part1);
			if (ret < 0) {
				ath12k_warn(ar->ab,
					    "Failed to send part1: %d\n", ret);
				goto reconnect;
			}
			total_bytes_sent += ret;

			ret = ath12k_pktlog_remote_service_send(service,
								buf_ptr, part2);
			if (ret < 0) {
				ath12k_warn(ar->ab,
					    "Failed to send part2: %d\n", ret);
				goto reconnect;
			}
			total_bytes_sent += ret;

			spin_lock_bh(&pl_info->lock);
			pl_info->rlog_read_index = part2;
		} else {
			read_idx = pl_info->rlog_read_index;
			spin_unlock_bh(&pl_info->lock);

			ret = ath12k_pktlog_remote_service_send(service,
								buf_ptr + read_idx,
								send_size);
			if (ret < 0) {
				ath12k_warn(ar->ab,
					    "Failed to send data: %d\n", ret);
				goto reconnect;
			}
			total_bytes_sent += ret;

			spin_lock_bh(&pl_info->lock);
			pl_info->rlog_read_index += send_size;
		}

		if (pl_info->rlog_read_index >= pl_info->rlog_max_size)
			pl_info->rlog_read_index = 0;

		if (pl_info->is_wrap &&
		    pl_info->rlog_read_index == pl_info->rlog_write_index)
			pl_info->is_wrap = 0;

		spin_unlock_bh(&pl_info->lock);

		cond_resched();
	}

	return;

reconnect:
	ath12k_warn(ar->ab,
		    "Remote pktlog: connection lost (err=%d, sent=%llu, missed=%u)\n",
		    ret, total_bytes_sent, service->missed_records);
	service->running = 0;
	service->connect_done = 0;
	if (service->send_socket) {
		sock_release(service->send_socket);
		service->send_socket = NULL;
	}
	pl_info->filter &= ~ATH12K_PKTLOG_REMOTE_ENABLE;
}

void ath12k_pktlog_init_remote_service_work(struct ath12k *ar)
{
	struct ath12k_pktlog *pl_info = &ar->debug.pktlog;
	struct ath12k_pktlog_remote_service *service;

	if (!pl_info->rpktlog_svc) {
		pl_info->rpktlog_svc = kzalloc(sizeof(*pl_info->rpktlog_svc),
					       GFP_KERNEL);
		if (!pl_info->rpktlog_svc)
			return;
	}

	service = pl_info->rpktlog_svc;
	service->port = ATH12K_DEFAULT_REMOTE_PKTLOG_PORT;
	service->ar = ar;

	INIT_WORK(&service->connection_service,
		  ath12k_pktlog_start_remote_service);

	INIT_WORK(&service->accept_service,
		  ath12k_pktlog_start_accept_service);

	INIT_WORK(&service->client_service,
		  ath12k_pktlog_start_remote_service_client);

	INIT_WORK(&service->send_service,
		  ath12k_pktlog_run_send_service);
}

static void ath12k_init_pktlog_buf(struct ath12k *ar, struct ath12k_pktlog
                                   *pktlog)
{
	if (!ar->ab->pktlog_defs_checksum) {
		pktlog->buf->bufhdr.magic_num = PKTLOG_MAGIC_NUM;
		pktlog->buf->bufhdr.version = CUR_PKTLOG_VER;
	} else {
		pktlog->buf->bufhdr.magic_num = PKTLOG_NEW_MAGIC_NUM;
		pktlog->buf->bufhdr.version = ar->ab->pktlog_defs_checksum;
	}

	if (!test_bit(WMI_TLV_SERVICE_PKTLOG_DECODE_INFO_SUPPORT, ar->ab->wmi_ab.svc_map)) {
                ath12k_warn(ar->ab, "firmware doesn't support pktlog decode info support\n");
                pktlog->invalid_decode_info = 1;
        }

	pktlog->buf->rd_offset = -1;
	pktlog->buf->wr_offset = 0;
}

/* Allocate and init pktlog buf if not already allocated.
 * Must be called with wiphy_lock held. Idempotent: no-op if buf exists.
 */
int ath12k_pktlog_buf_alloc_if_needed(struct ath12k *ar, bool *allocated)
{
	int ret;

	if (allocated)
		*allocated = false;

	if (ar->debug.pktlog.buf) {
		if (ar->debug.pktlog.buf_alloc_size == ar->debug.pktlog.buf_size)
			return 0;
		ath12k_pktlog_release(&ar->debug.pktlog);
	}

	ret = ath12k_alloc_pktlog_buf(ar);
	if (ret)
		return ret;

	ath12k_init_pktlog_buf(ar, &ar->debug.pktlog);
	if (allocated)
		*allocated = true;

	return 0;
}

void ath12k_pktlog_buf_release_if_allocated(struct ath12k *ar, bool allocated)
{
	if (allocated)
		ath12k_pktlog_release(&ar->debug.pktlog);
}

void ath12k_pktlog_buf_reset(struct ath12k *ar)
{
	if (!ar->debug.pktlog.buf)
		return;

	ath12k_init_pktlog_buf(ar, &ar->debug.pktlog);
}

static inline void ath12k_pktlog_mov_rd_idx(struct ath12k_pktlog *pl_info,
                                            int32_t *rd_offset)
{
	int32_t boundary;
	struct ath12k_pktlog_buf *log_buf = pl_info->buf;
	struct ath12k_pktlog_hdr *log_hdr;

	log_hdr = (struct ath12k_pktlog_hdr *)(log_buf->log_data + *rd_offset);
	boundary = *rd_offset;
	boundary += pl_info->hdr_size;
	boundary += log_hdr->size;

	if (boundary <= pl_info->buf_size)
		*rd_offset = boundary;
	else
		*rd_offset = log_hdr->size;

	if ((pl_info->buf_size - *rd_offset) < pl_info->hdr_size)
		*rd_offset = 0;
}

static char *ath12k_pktlog_getbuf(struct ath12k_pktlog *pl_info,
                                  struct ath12k_pktlog_hdr_arg *hdr_arg)
{
	struct ath12k_pktlog_buf *log_buf;
	int32_t cur_wr_offset, buf_size;
	u32 prev_wr;
	char *log_ptr;

	spin_lock_bh(&pl_info->lock);
	log_buf = pl_info->buf;
	buf_size = pl_info->buf_size;

	cur_wr_offset = log_buf->wr_offset;
	/* Move read offset to the next entry if there is a buffer overlap */
	if (log_buf->rd_offset >= 0) {
		if ((cur_wr_offset <= log_buf->rd_offset) &&
		    (cur_wr_offset + pl_info->hdr_size) >
		     log_buf->rd_offset)
			ath12k_pktlog_mov_rd_idx(pl_info, &log_buf->rd_offset);
	} else {
		log_buf->rd_offset = cur_wr_offset;
	}

	memcpy(&log_buf->log_data[cur_wr_offset],
	       hdr_arg->pktlog_hdr, pl_info->hdr_size);

	cur_wr_offset += pl_info->hdr_size;

	if ((buf_size - cur_wr_offset) < hdr_arg->payload_size) {
		while ((cur_wr_offset <= log_buf->rd_offset) &&
		       (log_buf->rd_offset < buf_size))
			  ath12k_pktlog_mov_rd_idx(pl_info, &log_buf->rd_offset);
		cur_wr_offset = 0;
	}

	while ((cur_wr_offset <= log_buf->rd_offset) &&
	       ((cur_wr_offset + hdr_arg->payload_size) > log_buf->rd_offset))
			  ath12k_pktlog_mov_rd_idx(pl_info, &log_buf->rd_offset);

	log_ptr = &log_buf->log_data[cur_wr_offset];
	cur_wr_offset += hdr_arg->payload_size;

	log_buf->wr_offset =
		((buf_size - cur_wr_offset) >=
		 pl_info->hdr_size) ? cur_wr_offset : 0;

	if (pl_info->filter & ATH12K_PKTLOG_REMOTE_ENABLE) {
		prev_wr = pl_info->rlog_write_index;

		pl_info->rlog_write_index = log_buf->wr_offset;
		if (log_buf->wr_offset < prev_wr)
			pl_info->is_wrap = 1;
	}
	spin_unlock_bh(&pl_info->lock);

	return log_ptr;
}

static vm_fault_t pktlog_pgfault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct ath12k *ar = vma->vm_file->private_data;
	struct ath12k_pktlog *pl_info = &ar->debug.pktlog;
	struct ath12k_pktlog_buf *buf = pl_info->buf;
	unsigned long page_cnt;
	struct page *page;
	void *kaddr;

	if (!buf)
		return VM_FAULT_SIGBUS;

	page_cnt = DIV_ROUND_UP(sizeof(*buf) + pl_info->buf_alloc_size,
			       PAGE_SIZE);
	if (vmf->pgoff >= page_cnt)
		return VM_FAULT_SIGBUS;

	kaddr = (char *)buf + (vmf->pgoff << PAGE_SHIFT);
	page = vmalloc_to_page(kaddr);
	if (!page)
		return VM_FAULT_SIGBUS;
	get_page(page);
	vmf->page = page;

	return 0;
}

static const struct vm_operations_struct pktlog_vmops = {
	.fault = pktlog_pgfault
};

static int ath12k_pktlog_mmap(struct file *file, struct vm_area_struct
                              *vma)
{
	struct ath12k *ar = file->private_data;

	/* entire buffer should be mapped */
	if (vma->vm_pgoff != 0)
		return -EINVAL;

	if (!ar->debug.pktlog.buf) {
		ath12k_err(ar->ab, "Can't allocate pktlog buf\n");
		return -ENOMEM;
	}
#if LINUX_VERSION_IS_LESS(6, 6, 3)
	vma->vm_flags |= VM_LOCKED;
#else
	vm_flags_set(vma, VM_LOCKED);
#endif
	vma->vm_ops = &pktlog_vmops;

	return 0;
}

static ssize_t ath12k_pktlog_read(struct file *file, char __user *userbuf,
                                  size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	struct ath12k_pktlog *info = &ar->debug.pktlog;
	struct ath12k_pktlog_buf *log_buf;
	size_t bufhdr_size, rem_len, nbytes = 0, ret_val = 0;
	size_t start_offset, end_offset = 0;
	size_t fold_offset = INVALID_OFFSET, ppos_data;
	int cur_rd_offset;
	char *buf;
	ssize_t final_ret = 0;

	if (count == 0 || !userbuf)
		return 0;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	if (!ath12k_ftm_mode && ar->ah->state != ATH12K_HW_STATE_ON) {
		final_ret = -ENETDOWN;
		goto unlock;
	}

	buf = vmalloc(count);
	if (!buf) {
		final_ret = -ENOMEM;
		goto unlock;
	}

	spin_lock_bh(&info->lock);
	log_buf = info->buf;
	if (!log_buf) {
		spin_unlock_bh(&info->lock);
		final_ret = 0;
		goto free;
	}

	bufhdr_size = sizeof(log_buf->bufhdr);
	if (!info->fw_version_record || info->invalid_decode_info)
		bufhdr_size -= sizeof(struct ath12k_pktlog_decode_info);

	/* copy valid log entries from circular buffer into user space */
	rem_len = count;

	if (*ppos < bufhdr_size) {
		nbytes = min_t(size_t, bufhdr_size - (size_t)*ppos, rem_len);
		if (*ppos + nbytes > sizeof(log_buf->bufhdr)) {
			final_ret = -EFAULT;
			spin_unlock_bh(&info->lock);
			goto free;
		}

		memcpy(buf, ((char *)&log_buf->bufhdr) + *ppos, nbytes);
		rem_len -= nbytes;
		ret_val += nbytes;
	}

	start_offset = log_buf->rd_offset;
	if (rem_len == 0 || start_offset == INVALID_OFFSET) {
		spin_unlock_bh(&info->lock);
		goto copy_to_user;
	}

	cur_rd_offset = start_offset;

	/* Find the last offset and fold-offset if the buffer is folded */
	do {
		int log_data_offset;
		struct ath12k_pktlog_hdr *log_hdr;

		log_hdr = (struct ath12k_pktlog_hdr *)(log_buf->log_data +
						       cur_rd_offset);
		log_data_offset = cur_rd_offset + info->hdr_size;

		if (fold_offset == INVALID_OFFSET &&
		    ((info->buf_size - log_data_offset) <= log_hdr->size))
			fold_offset = log_data_offset - 1;

		ath12k_pktlog_mov_rd_idx(info, &cur_rd_offset);

		if (fold_offset == INVALID_OFFSET &&
		    cur_rd_offset == 0 &&
		    cur_rd_offset != log_buf->wr_offset)
			fold_offset = log_data_offset + log_hdr->size - 1;

		end_offset = log_data_offset + log_hdr->size - 1;

	} while (cur_rd_offset != log_buf->wr_offset);

	spin_unlock_bh(&info->lock);

	ppos_data = *ppos + ret_val - bufhdr_size + start_offset;

	if (fold_offset == INVALID_OFFSET) {
		if (ppos_data > end_offset)
			goto copy_to_user;

		nbytes = min(rem_len, end_offset - ppos_data + 1);
		if (ppos_data < 0 || ppos_data + nbytes > info->buf_size) {
			final_ret = -EFAULT;
			goto free;
		}

		memcpy(buf + ret_val, log_buf->log_data + ppos_data, nbytes);
		ret_val += nbytes;
	} else {
		if (ppos_data <= fold_offset) {
			nbytes = min(rem_len, fold_offset - ppos_data + 1);
			if (ppos_data < 0 || ppos_data + nbytes >
			    info->buf_size) {
				final_ret = -EFAULT;
				goto free;
			}

			memcpy(buf + ret_val, log_buf->log_data + ppos_data,
			       nbytes);
			ret_val += nbytes;
			rem_len -= nbytes;
		}

		if (rem_len > 0) {
			ppos_data =
				*ppos + ret_val - (bufhdr_size +
						   (fold_offset - start_offset + 1));

			if (ppos_data <= end_offset) {
				nbytes = min(rem_len, end_offset - ppos_data + 1);
				if (ppos_data < 0 || ppos_data + nbytes >
				    info->buf_size) {
					final_ret = -EFAULT;
					goto free;
				}

				memcpy(buf + ret_val, log_buf->log_data + ppos_data,
				       nbytes);
				ret_val += nbytes;
			}
		}
	}

	if (ret_val == 0) {
		final_ret = 0;
		goto free;
	}

copy_to_user:
	if (ret_val < 0 || ret_val > count || *ppos + ret_val < *ppos) {
		final_ret = -EFAULT;
		goto free;
	}

	if (copy_to_user(userbuf, buf, ret_val)) {
		final_ret = -EFAULT;
		goto free;
	}

	*ppos += ret_val;
	final_ret = ret_val;

free:
	vfree(buf);
unlock:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return final_ret;
}

static const struct file_operations fops_pktlog_dump = {
	.read = ath12k_pktlog_read,
	.mmap = ath12k_pktlog_mmap,
	.open = ath12k_debugfs_open
};

/**
 * ath12k_write_pktlog_start() : This function is responsible for starting and
 * stopping pktlog data collection.
 *
 * Below command invokes this function:
 * 1. To start the dump collection:
 *   echo 1 > /sys/kernel/debug/ath12k/<HW>/mac0/pktlog/start
 *
 * 2. To stop the dump collection:
 *   echo 0 > /sys/kernel/debug/ath12k/<HW>/mac0/pktlog/start
 *
 */
static ssize_t ath12k_write_pktlog_start(struct file *file, const char __user *ubuf,
                                         size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	u32 start_pktlog;
	bool allocated = false;
	int err;

	err = kstrtou32_from_user(ubuf, count, 0, &start_pktlog);
	if (err)
		return err;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	if (!ath12k_ftm_mode && ar->ah->state != ATH12K_HW_STATE_ON) {
		err = -ENETDOWN;
		goto exit;
	}

	if (ar->debug.is_pkt_logging && start_pktlog) {
		ath12k_err(ar->ab, "packet logging is inprogress\n");
		err = -EINVAL;
		goto exit;
	}

	if (start_pktlog) {
		err = ath12k_pktlog_buf_alloc_if_needed(ar, &allocated);
		if (err)
			goto exit;

		err = ath12k_wmi_pdev_pktlog_enable(ar,
						    ar->debug.pktlog_filter);
		if (err) {
			ath12k_err(ar->ab,
				   "failed to enable pktlog filter 0%x: %d\n",
				   ar->debug.pktlog_filter, err);
			ath12k_pktlog_buf_release_if_allocated(ar, allocated);
			goto exit;
		}
		ath12k_pktlog_buf_reset(ar);
		ar->debug.is_pkt_logging = true;
	} else {
		ar->debug.is_pkt_logging = false;
		ath12k_dp_mon_pktlog_config(ar, false,
					    ar->debug.pktlog_mode,
					    ar->debug.pktlog_filter);
		err = ath12k_dp_mon_rx_update_filter(ar);
		if (err)
			ath12k_err(ar->ab,
				   "Failed to configure pktlog filters\n");

		err = ath12k_wmi_pdev_pktlog_disable(ar);
		if (err) {
			ath12k_err(ar->ab,
				   "failed to disable pktlog : %d\n", err);
			goto exit;
		}
	}

	err = count;

exit:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return err;
}

static ssize_t ath12k_read_pktlog_start(struct file *file, char __user *ubuf,
                                        size_t count, loff_t *ppos)
{
	char buf[32];
	struct ath12k *ar = file->private_data;
	int len = 0;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	len = scnprintf(buf, sizeof(buf), "%d\n",
					ar->debug.is_pkt_logging);
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_pktlog_start = {
	.read = ath12k_read_pktlog_start,
	.write = ath12k_write_pktlog_start,
	.open = ath12k_debugfs_open
};

static ssize_t ath12k_pktlog_size_write(struct file *file, const char __user *ubuf,
                                        size_t count, loff_t *ppos)
{
	struct ath12k *ar = file->private_data;
	u32 pktlog_size;
	int ret;

	if (kstrtou32_from_user(ubuf, count, 0, &pktlog_size))
		return -EINVAL;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	/* Validate size is within reasonable limits */
	if (pktlog_size < ATH12K_PKTLOG_SIZE_MIN ||
	    pktlog_size > ATH12K_PKTLOG_SIZE_MAX) {
		ath12k_err(ar->ab,
			   "Invalid size. Must be between 16KB and 50MB\n");
		ret = -EINVAL;
		goto exit;
	}

	if (pktlog_size == ar->debug.pktlog.buf_size) {
		ret = count;
		goto exit;
	}

	if (ar->debug.is_pkt_logging) {
		ath12k_err(ar->ab,
			   "Stop packet logging before changing the size\n");
		ret = -EINVAL;
		goto exit;
	}

	ar->debug.pktlog.buf_size = pktlog_size;
	ret = count;

exit:
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);
	return ret;
}

static ssize_t ath12k_pktlog_size_read(struct file *file, char __user *ubuf,
                                       size_t count, loff_t *ppos)
{
	char buf[32];
	struct ath12k *ar = file->private_data;
	int len = 0;

	wiphy_lock(ath12k_ar_to_hw(ar)->wiphy);
	len = scnprintf(buf, sizeof(buf), "%uL\n",
			ar->debug.pktlog.buf_size);
	wiphy_unlock(ath12k_ar_to_hw(ar)->wiphy);

	return simple_read_from_buffer(ubuf, count, ppos, buf, len);
}

static const struct file_operations fops_pktlog_size = {
	.read = ath12k_pktlog_size_read,
	.write = ath12k_pktlog_size_write,
	.open = ath12k_debugfs_open
};

/**
 * ath12k_pktlog_remote_enable - Enable/disable remote pktlog
 * @ar: ath12k radio pointer
 * @enable: 1 to enable, 0 to disable
 *
 * Enables or disables remote pktlog service. When enabling:
 * - Server mode: If no IP configured, creates listen socket
 * - Client mode: If IP configured, connects to remote server
 *
 * Return: 0 on success, negative error code on failure
 */
int ath12k_pktlog_remote_enable(struct ath12k *ar, u32 enable)
{
	struct ath12k_pktlog *pl_info;
	struct ath12k_pktlog_remote_service *service;

	if (!ar)
		return -EINVAL;

	pl_info = &ar->debug.pktlog;
	if (!pl_info->rpktlog_svc)
		return -EINVAL;

	service = pl_info->rpktlog_svc;

	if (enable) {
		if (pl_info->filter & ATH12K_PKTLOG_REMOTE_ENABLE) {
			if (service->running) {
				ath12k_warn(ar->ab,
					    "Remote pktlog already enabled\n");
				return 0;
			}
		}

		if (!pl_info->buf) {
			ath12k_warn(ar->ab, "Pktlog buffer not allocated\n");
			return -ENOMEM;
		}

		service->running = 1;
		service->connect_done = 0;
		service->missed_records = 0;
		service->fend_counts = 0;

		if (!service->port)
			service->port = ATH12K_DEFAULT_REMOTE_PKTLOG_PORT;

		pl_info->rlog_write_index = 0;
		pl_info->rlog_read_index = 0;
		pl_info->rlog_max_size = pl_info->buf_size;
		pl_info->is_wrap = 0;

		if (pl_info->pktlog_remote_client && pl_info->ipaddr[0]) {
			ath12k_dbg(ar->ab, ATH12K_DBG_DATA,
				   "Starting remote pktlog client mode\n");
			if (!schedule_work(&service->client_service))
				ath12k_warn(ar->ab, "Client service already scheduled\n");
		} else {
			ath12k_dbg(ar->ab, ATH12K_DBG_DATA,
				   "Starting remote pktlog server mode\n");
			if (!schedule_work(&service->connection_service))
				ath12k_warn(ar->ab, "Connection service already scheduled\n");
		}

		pl_info->filter |= ATH12K_PKTLOG_REMOTE_ENABLE;
	} else {
		ath12k_dbg(ar->ab, ATH12K_DBG_DATA,
			   "(missed=%u, fends=%u, write_idx=%u, read_idx=%u)\n",
			   service->missed_records, service->fend_counts,
			   pl_info->rlog_write_index,
			   pl_info->rlog_read_index);
		pl_info->filter &= ~ATH12K_PKTLOG_REMOTE_ENABLE;
		ath12k_pktlog_stop_service(ar);
	}

	return 0;
}

static void ath12k_pktlog_init(struct ath12k *ar)
{
	struct ath12k_pktlog *pktlog = &ar->debug.pktlog;

	spin_lock_init(&pktlog->lock);
	pktlog->buf_size = ATH12K_DEBUGFS_PKTLOG_SIZE_DEFAULT;
	pktlog->buf = NULL;

	pktlog->hdr_size = sizeof(struct ath12k_pktlog_hdr);
	pktlog->hdr_size_field_offset =
		offsetof(struct ath12k_pktlog_hdr, size);

	pktlog->rlog_write_index = 0;
	pktlog->rlog_read_index = 0;
	pktlog->rlog_max_size = 0;
	pktlog->is_wrap = 0;
	memset(pktlog->ipaddr, 0, sizeof(pktlog->ipaddr));
	pktlog->pktlog_remote_client = 0;

	ath12k_pktlog_init_remote_service_work(ar);
}

void ath12k_init_pktlog(struct ath12k *ar)
{
	ar->debug.debugfs_pktlog = debugfs_create_dir("pktlog",
						      ar->debug.debugfs_pdev);
	ath12k_debugfs_create_file("start", 0644, ar->debug.debugfs_pktlog,
				   ar->ab, ar, &fops_pktlog_start);
	ath12k_debugfs_create_file("size", 0644, ar->debug.debugfs_pktlog,
				   ar->ab, ar, &fops_pktlog_size);
	ath12k_debugfs_create_file("dump", 0444, ar->debug.debugfs_pktlog,
				   ar->ab, ar, &fops_pktlog_dump);

	ath12k_pktlog_init(ar);
}

void ath12k_deinit_pktlog(struct ath12k *ar)
{
	struct ath12k_pktlog *pktlog = &ar->debug.pktlog;

	if (pktlog->rpktlog_svc) {
		struct ath12k_pktlog_remote_service *service = pktlog->rpktlog_svc;

		if (pktlog->filter & ATH12K_PKTLOG_REMOTE_ENABLE)
			ath12k_pktlog_stop_service(ar);

		cancel_work_sync(&service->connection_service);
		cancel_work_sync(&service->accept_service);
		cancel_work_sync(&service->client_service);
		cancel_work_sync(&service->send_service);

		kfree(pktlog->rpktlog_svc);
		pktlog->rpktlog_svc = NULL;
	}

	if (pktlog->buf)
		ath12k_pktlog_release(pktlog);
}

static void ath12k_pktlog_pull_hdr(struct ath12k_pktlog_hdr_arg *arg,
				   struct ath12k *ar, u8 *data)
{
	struct ath12k_pktlog_hdr *hdr;

	if (!arg || !data) {
		ath12k_warn(ar->ab, "Invalid arguments to pktlog_pull_hdr\n");
		return;
	}

	hdr = (struct ath12k_pktlog_hdr *)data;

	hdr->flags = __le16_to_cpu(hdr->flags);
	hdr->missed_cnt = __le16_to_cpu(hdr->missed_cnt);
	hdr->log_type = __le16_to_cpu(hdr->log_type);
	hdr->size = __le16_to_cpu(hdr->size);
	hdr->timestamp = __le32_to_cpu(hdr->timestamp);
	hdr->type_specific_data = __le32_to_cpu(hdr->type_specific_data);

	arg->log_type = hdr->log_type;
	arg->payload = hdr->payload;
	arg->payload_size = hdr->size;
	arg->pktlog_hdr = data;
}

static void ath12k_pktlog_write_buf(struct ath12k *ar,
				    struct ath12k_pktlog *pl_info,
				    struct ath12k_pktlog_hdr_arg *hdr_arg)
{
	char *log_data;

	if (!pl_info || !pl_info->buf || pl_info->buf_size <= 0) {
		ath12k_warn(ar->ab, "Invalid pl_info or buffer\n");
		return;
	}

	if (!hdr_arg || !hdr_arg->payload || hdr_arg->payload_size <= 0) {
		ath12k_warn(ar->ab, "Invalid hdr_arg or payload\n");
		return;
	}

	log_data = ath12k_pktlog_getbuf(pl_info, hdr_arg);
	if (!log_data) {
		ath12k_warn(ar->ab, "pktlog data is NULL\n");
		return;
	}

	if (hdr_arg->payload_size > pl_info->buf_size) {
		ath12k_warn(ar->ab,
			    "Payload size is too large : %d > buf_size: %d\n",
			    hdr_arg->payload_size, pl_info->buf_size);
		return;
	}

	if (log_data < pl_info->buf->log_data ||
	    (log_data + hdr_arg->payload_size) >
	    (pl_info->buf->log_data + pl_info->buf_size)) {
		ath12k_warn(ar->ab,
			    "memcpy out of bounds : log_data: %p size: %d\n",
			    log_data, hdr_arg->payload_size);
		return;
	}

	memcpy(log_data, hdr_arg->payload, hdr_arg->payload_size);
}

void ath12k_htt_pktlog_process(struct ath12k *ar, u8 *data, u32 len)
{
	struct ath12k_pktlog *pl_info;
	struct ath12k_pktlog_hdr *hdr;
	struct ath12k_pktlog_hdr_arg hdr_arg;
	u16 payload_size;
	u32 hdr_size = struct_size(hdr, payload, 0);

	if (!ar || !data)
		return;

	if (len < hdr_size) {
		ath12k_warn(ar->ab, "HTT PKTLOG payload too short: %u\n", len);
		return;
	}

	hdr = (struct ath12k_pktlog_hdr *)data;
	payload_size = __le16_to_cpu(hdr->size);
	if (payload_size > len - hdr_size) {
		ath12k_warn(ar->ab,
			    "HTT PKTLOG invalid inner payload size: %u > %u\n",
			    payload_size, len - hdr_size);
		return;
	}

	pl_info = &ar->debug.pktlog;
	ath12k_pktlog_pull_hdr(&hdr_arg, ar, data);
	ath12k_pktlog_write_buf(ar, pl_info, &hdr_arg);
}

void ath12k_htt_ppdu_pktlog_process(struct ath12k *ar, u8 *data,
                                    u32 len)
{
	struct ath12k_pktlog *pl_info;
	struct ath12k_pktlog_hdr hdr;
	struct ath12k_pktlog_hdr_arg hdr_arg;

	if (!ar)
		return;

	pl_info = &ar->debug.pktlog;
	hdr.flags = (1 << PKTLOG_FLG_FRM_TYPE_REMOTE_S);
	hdr.missed_cnt = 0;
	hdr.log_type = ATH12K_PKTLOG_TYPE_PPDU_STATS;
	hdr.timestamp = 0;
	hdr.size = len;
	hdr.type_specific_data = 0;

	hdr_arg.log_type = hdr.log_type;
	hdr_arg.payload_size = hdr.size;
	hdr_arg.payload = (u8 *)data;
	hdr_arg.pktlog_hdr = (u8 *)&hdr;

	ath12k_pktlog_write_buf(ar, pl_info, &hdr_arg);
}

void ath12k_dp_txrx_stats_buf_pktlog_process(struct ath12k *ar, u8 *data,
					     u16 log_type, u32 len)
{
	struct ath12k_pktlog *pl_info;
	struct ath12k_pktlog_hdr hdr;
	struct ath12k_pktlog_hdr_arg hdr_arg;

	if (!ar)
		return;

	pl_info = &ar->debug.pktlog;

	hdr.flags = (1 << PKTLOG_FLG_FRM_TYPE_REMOTE_S);
	hdr.missed_cnt = 0;
	hdr.log_type = log_type;
	hdr.timestamp = 0;
	hdr.size = len;
	hdr.type_specific_data = 0;

	hdr_arg.log_type = log_type;
	hdr_arg.payload_size = len;
	hdr_arg.payload = data;
	hdr_arg.pktlog_hdr = (u8 *)&hdr;

	ath12k_pktlog_write_buf(ar, pl_info, &hdr_arg);
}
EXPORT_SYMBOL(ath12k_dp_txrx_stats_buf_pktlog_process);

void ath12k_cbf_pktlog_process(struct ath12k *ar, u8 *data, u32 len,
			       struct htt_t2h_ppdu_stats_ind_hdr *htt_hdr,
			       struct htt_ppdu_stats_rx_mgmtctrl_payload_tlv *cbf_tlv)
{
	struct ath12k_pktlog *pl_info;
	struct ath12k_pktlog_hdr hdr;
	struct ath12k_pktlog_hdr_arg hdr_arg;
	u32 total_len;
	char *log_data;

	if (!ar)
		return;

	pl_info = &ar->debug.pktlog;

	if (!pl_info || !pl_info->buf || pl_info->buf_size <= 0) {
		ath12k_warn(ar->ab, "Invalid pl_info or buffer for CBF\n");
		return;
	}

	total_len = sizeof(*htt_hdr) + sizeof(*cbf_tlv) + len;

	if (total_len > pl_info->buf_size) {
		ath12k_warn(ar->ab, "CBF frame too large: %u > %u\n",
			    total_len, pl_info->buf_size);
		return;
	}

	hdr.flags = (1 << PKTLOG_FLG_FRM_TYPE_REMOTE_S);
	hdr.missed_cnt = 0;
	hdr.log_type = ATH12K_PKTLOG_TYPE_PPDU_STATS;
	hdr.timestamp = 0;
	hdr.size = ALIGN(total_len, PKTLOG_ALIGN);

	hdr_arg.log_type = hdr.log_type;
	hdr_arg.payload_size = hdr.size;
	hdr_arg.payload = data;
	hdr_arg.pktlog_hdr = (u8 *)&hdr;

	log_data = ath12k_pktlog_getbuf(pl_info, &hdr_arg);
	if (!log_data) {
		ath12k_dbg(ar->ab, ATH12K_DBG_DATA,
			   "Failed to get pktlog buffer for CBF frame\n");
		return;
	}

	if (log_data < pl_info->buf->log_data ||
	    (log_data + sizeof(*htt_hdr)) >
	    (pl_info->buf->log_data + pl_info->buf_size)) {
		ath12k_warn(ar->ab, "CBF htt_hdr memcpy out of bounds\n");
		return;
	}
	memcpy(log_data, htt_hdr, sizeof(struct htt_t2h_ppdu_stats_ind_hdr));
	log_data += sizeof(struct htt_t2h_ppdu_stats_ind_hdr);

	if (log_data < pl_info->buf->log_data ||
	    (log_data + sizeof(*cbf_tlv)) >
	    (pl_info->buf->log_data + pl_info->buf_size)) {
		ath12k_warn(ar->ab, "CBF cbf_tlv memcpy out of bounds\n");
		return;
	}
	memcpy(log_data, cbf_tlv,
	       sizeof(struct htt_ppdu_stats_rx_mgmtctrl_payload_tlv));
	log_data += sizeof(struct htt_ppdu_stats_rx_mgmtctrl_payload_tlv);

	if (log_data < pl_info->buf->log_data ||
	    (log_data + len) > (pl_info->buf->log_data + pl_info->buf_size)) {
		ath12k_warn(ar->ab, "CBF data memcpy out of bounds\n");
		return;
	}
	memcpy(log_data, data, len);
}
EXPORT_SYMBOL(ath12k_cbf_pktlog_process);
