// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <dm.h>
#include <dma-bounce.h>
#include <lmb.h>
#include <malloc.h>
#include <memalign.h>
#include <mailbox-uclass.h>
#include <asm/io.h>
#include <asm/atomic.h>
#include <linux/tmelcom-qmp.h>
#include <linux/compat.h>
#include <linux/completion.h>
#include <linux/dma-mapping.h>
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <dm/device_compat.h>
#include <dm/devres.h>
#include <asm/gic.h>
#include <dt-bindings/interrupt-controller/arm-gic.h>

DECLARE_GLOBAL_DATA_PTR;

/* QMP protocol constants */
#define QMP_NUM_CHANS		0x1
#define QMP_TOUT_MS		1000
#define QMP_CTRL_DATA_SIZE	4
#define QMP_MAX_PKT_SIZE	0x18
#define QMP_UCORE_DESC_OFFSET	0x1000
#define QMP_SEND_TIMEOUT	30000

/* Mailbox size definitions */
#define QMP_HW_MBOX_SIZE		32
#define QMP_MBOX_RSV_SIZE		4
#define QMP_MBOX_IPC_PACKET_SIZE	(QMP_HW_MBOX_SIZE - QMP_CTRL_DATA_SIZE - QMP_MBOX_RSV_SIZE)
#define QMP_MBOX_IPC_MAX_PARAMS		5

/* SRAM buffer parameters */
#define QMP_MAX_PARAM_IN_PARAM_ID	14
#define QMP_PARAM_CNT_FOR_OUTBUF	3
#define QMP_SRAM_IPC_MAX_PARAMS		(QMP_MAX_PARAM_IN_PARAM_ID * QMP_PARAM_CNT_FOR_OUTBUF)
#define QMP_SRAM_IPC_MAX_BUF_SIZE	(QMP_SRAM_IPC_MAX_PARAMS * sizeof(u32))

/* TMEL error codes */
#define TMEL_ERROR_GENERIC		(0x1u)
#define TMEL_ERROR_NOT_SUPPORTED	(0x2u)
#define TMEL_ERROR_BAD_PARAMETER	(0x3u)
#define TMEL_ERROR_BAD_MESSAGE		(0x4u)
#define TMEL_ERROR_BAD_ADDRESS		(0x5u)
#define TMEL_ERROR_TMELCOM_FAILURE	(0x6u)
#define TMEL_ERROR_TMEL_BUSY		(0x7u)

/* PRNG parameters */
#define PRNG_MAX_LENGTH			0x200

/* IRQ check parameters */
#define IRQ_CHECK_TIMEOUT		100000
#define IRQ_CHECK_DELAY_US		10
#define MAX_IRQ_RETRY			30

/*
 * mbox data can be shared over mem or sram
 */
enum ipc_type {
	IPC_MBOX_MEM,
	IPC_MBOX_SRAM,
};

/*
 * mbox header indicates the type of payload and action required.
 */
struct ipc_header {
	u8 ipc_type:1;
	u8 msg_len:7;
	u8 msg_type;
	u8 action_id;
	s8 response;
};

struct mbox_payload {
	u32 param[QMP_MBOX_IPC_MAX_PARAMS];
};

struct sram_payload {
	u32 payload_ptr;
	u32 payload_len;
};

union ipc_payload {
	struct mbox_payload mbox_payload;
	struct sram_payload sram_payload;
};

struct tmel_ipc_pkt {
	struct ipc_header msg_hdr;
	union ipc_payload payload;
};

/**
 * enum qmp_local_state - definition of the local state machine
 * @LINK_DISCONNECTED: Init state, waiting for ucore to start
 * @LINK_NEGOTIATION: Set local link state to up, wait for ucore ack
 * @LINK_CONNECTED: Link state up, channel not connected
 * @LOCAL_CONNECTING: Channel opening locally, wait for ucore ack
 * @CHANNEL_CONNECTED: Channel fully opened
 * @LOCAL_DISCONNECTING: Channel disconnected locally, wait for ucore ack
 */
enum qmp_local_state {
	LINK_DISCONNECTED,
	LINK_NEGOTIATION,
	LINK_CONNECTED,
	LOCAL_CONNECTING,
	CHANNEL_CONNECTED,
	LOCAL_DISCONNECTING,
};

/**
 * struct qmp_channel_desc - IPC bits
 * @bits: Var to access each member
 * @val: u32 representation of above
 */
union qmp_channel_desc {
	struct {
		u32 link_state:1;
		u32 link_state_ack:1;
		u32 ch_state:1;
		u32 ch_state_ack:1;
		u32 tx:1;
		u32 tx_ack:1;
		u32 rx_done:1;
		u32 rx_done_ack:1;
		u32 reserved:8;
		u32 frag_size:8;
		u32 rem_frag_count:8;
	} bits;
	unsigned int val;
};

struct iovec_tmel {
	void *iov_base;	/* Pointer to data.  */
	size_t iov_len;	/* Length of data.  */
};

/**
 * struct qmp_device_cfg - QMP device configuration
 * @shared_irq: IRQ to notify tx completion from APSS to TME-L
 */
struct qmp_device_cfg {
	u32 shared_irq;
	phys_addr_t reg;
	u32 bit;
	bool issupport_check;
};

/**
 * struct qmp_device - local information for managing a single mailbox
 * @dev: The device that corresponds to this mailbox
 * @mcore_desc: Local core (APSS) mailbox descriptor
 * @ucore_desc: Remote core (TME-L) mailbox descriptor
 * @mcore: Local core (APSS) channel descriptor
 * @ucore: Remote core (TME-L) channel descriptor
 * @rx_pkt: Buffer to pass to client, holds received data from mailbox
 * @mbox_chan: Mailbox client chan for the IPC interrupt
 * @local_state: Current state of mailbox protocol
 * @link_complete: Use to block until link negotiation with remote proc
 * @ch_complete: Use to block until the channel is fully opened
 * @tx_sent: True if tx is sent and remote proc has not sent ack
 * @shared_irq: IRQ to notify tx completion from APSS to TME-L
 */
struct qmp_device {
	struct udevice *dev;

	void __iomem *mcore_desc;
	void __iomem *ucore_desc;
	union qmp_channel_desc mcore;
	union qmp_channel_desc ucore;

	struct iovec_tmel rx_pkt;

	struct mbox_chan *mbox_chan;

	enum qmp_local_state local_state;

	bool link_complete;
	bool ch_complete;
	bool is_enabled;

	atomic_t tx_sent;
	u32 shared_irq;
};

/**
 * struct tmel - tmel controller instance
 * @dev: The device that corresponds to this mailbox
 * @mdev: qmp_device associated with this tmel instance
 * @pkt: Buffer from client, to be sent over mailbox
 * @ipc_pkt: wrapper used for prepare/un_prepare
 * @sram_dma_addr: mailbox sram address to copy the data
 * @rx_done: Use to indicate receive completion from remote
 * @data: client data to be sent for the current request
 * @irq_base: Base address for interrupt controller
 * @irq_num: IRQ number
 * @irq_flags: IRQ flags
 * @sram_bounce: LMB address of lower-4-GB bounce buffer for SRAM path (0 if unused)
 * @sram_bounce_size: size of SRAM bounce buffer in bytes
 */
struct tmel {
	struct udevice *dev;
	struct qmp_device *mdev;
	struct iovec_tmel pkt;
	struct tmel_ipc_pkt *ipc_pkt;
	dma_addr_t sram_dma_addr;
	bool rx_done;
	void *data;
	void __iomem *irq_base;
	u32 irq_num;
	u32 irq_flags;
	phys_addr_t sram_bounce;	/* LMB addr of SRAM bounce buf, 0 if unused */
	size_t sram_bounce_size;	/* size of SRAM bounce buffer */
};

struct tmel_secboot_sec_auth_req {
	u32 sw_id;
	struct tmel_msg_param_type_buf_in elf_buf;
	struct tmel_msg_param_type_buf_in region_list;
	u32 relocate;
};

struct tmel_secboot_sec_auth_resp {
	u32 first_seg_addr;
	u32 first_seg_len;
	u32 entry_addr;
	u32 extended_error;
	u32 status;
};

struct tmel_secboot_sec_auth_v2_req {
	u32 sw_id;
	struct tmel_msg_param_type_buf_in elf_buf;
	struct tmel_msg_param_type_buf_in region_list;
	u32 relocate;
	u32 nsIntegrityCheck:1;
	u32 reservedBits:31;
	struct tmel_msg_param_type_buf_in reservedBuf;
};

struct tmel_secboot_sec_auth_v2_resp {
	u32 first_seg_addr;
	u32 first_seg_len;
	u32 entry_addr;
	u32 extended_error;
	u32 status;
	u32 keyHandle;
};

struct tmel_secboot_sec_auth {
	struct tmel_secboot_sec_auth_req req;
	struct tmel_secboot_sec_auth_resp resp;
};

struct tmel_secboot_sec_auth_v2 {
	struct tmel_secboot_sec_auth_v2_req req;
	struct tmel_secboot_sec_auth_v2_resp resp;
};

/* Forward declarations */
static void tmel_qmp_rx(struct tmel *tdev);
static int tmel_qmp_send_data(struct qmp_device *mdev, void *data);

/**
 * tmel_qmp_send_irq() - send an irq to a remote entity as an event signal.
 * @mdev: Which remote entity that should receive the irq.
 */
static inline void tmel_qmp_send_irq(struct qmp_device *mdev)
{
	if (!mdev || !mdev->dev)
		return;

	writel(mdev->mcore.val, mdev->mcore_desc);
	/* Ensure desc update is visible before IPC */
	wmb(); /* Flush write buffer before sending IPC interrupt */

	dev_dbg(mdev->dev, "%s: mcore 0x%x ucore 0x%x", __func__,
		mdev->mcore.val, mdev->ucore.val);

	writel(mdev->shared_irq, CONFIG_SHARED_IPC_INTERRUPT_REG);
}

/**
 * tmel_qmp_send_data() - Send the data to remote and notify.
 * @mdev: qmp_device to send the data to.
 * @data: Data to be sent to remote processor, should be in the format of
 *	  a iovec_tmel.
 *
 * Copy the data to the channel's mailbox and notify remote subsystem of new
 * data. This function will return an error if the previous message sent has
 * not been read.
 */
static int tmel_qmp_send_data(struct qmp_device *mdev, void *data)
{
	struct iovec_tmel *pkt = (struct iovec_tmel *)data;
	void __iomem *addr;

	if (!pkt || !pkt->iov_base)
		return -EINVAL;

	if (!mdev)
		return -EINVAL;

	if (pkt->iov_len > QMP_MAX_PKT_SIZE) {
		dev_err(mdev->dev, "Unsupported packet size\n");
		return -EINVAL;
	}

	if (atomic_read(&mdev->tx_sent)) {
		dev_err(mdev->dev, "Tx already sent\n");
		return -EAGAIN;
	}

	addr = mdev->mcore_desc + QMP_CTRL_DATA_SIZE;
	memcpy(addr, pkt->iov_base, pkt->iov_len);
	wmb();

	mdev->mcore.bits.frag_size = pkt->iov_len;
	mdev->mcore.bits.rem_frag_count = 0;

	dev_dbg(mdev->dev, "Copied buffer to mbox, sz: %d",
		mdev->mcore.bits.frag_size);

	atomic_set(&mdev->tx_sent, 1);

	mdev->mcore.bits.tx = !(mdev->mcore.bits.tx);

	/*
	 * Ensure all cached writes are visible to TMEL before invoking IRQ
	 */
	flush_dcache_all();

	tmel_qmp_send_irq(mdev);

	return 0;
}

/**
 * tmel_qmp_notify_client() - Notify the tmel client about remote data.
 * @tdev: tmel device to notify.
 * @message: response pkt from remote processor, should be in format of iovec_tmel.
 *
 * Wakeup the clients after receiving data from the remote.
 */
static void tmel_qmp_notify_client(struct tmel *tdev, void *message)
{
	struct iovec_tmel *pkt;

	if (!message) {
		dev_err(tdev->dev, "spurious message received\n");
		return;
	}

	if (tdev->rx_done) {
		dev_err(tdev->dev, "tmel response pending\n");
		return;
	}

	pkt = (struct iovec_tmel *)message;
	tdev->pkt.iov_len = pkt->iov_len;
	tdev->pkt.iov_base = pkt->iov_base;
	/* Ensure packet data is written before rx_done is set */
	wmb();
	tdev->rx_done = true;
}

/**
 * tmel_qmp_recv_data() - Receive data and send ack.
 * @tdev: tmel device that received the notification.
 * @mbox_of: offset of mailbox after QMP Control data.
 *
 * Copies data from mailbox and passes to the client upon receiving data
 * available notification. Also acknowledges the read completion.
 */
static void tmel_qmp_recv_data(struct tmel *tdev, u32 mbox_of)
{
	struct qmp_device *mdev = tdev->mdev;
	void __iomem *addr;
	struct iovec_tmel *pkt;

	if (!mdev)
		return;

	addr = mdev->ucore_desc + mbox_of;
	pkt = &mdev->rx_pkt;
	pkt->iov_len = mdev->ucore.bits.frag_size;

	memcpy(pkt->iov_base, addr, pkt->iov_len);
	wmb();
	mdev->mcore.bits.tx_ack = mdev->ucore.bits.tx;
	dev_dbg(mdev->dev, "%s: Send RX data to TMEL Client", __func__);
	tmel_qmp_notify_client(tdev, pkt);

	mdev->mcore.bits.rx_done = !(mdev->mcore.bits.rx_done);
	tmel_qmp_send_irq(mdev);
}

/**
 * tmel_qmp_clr_mcore_ch_state() - Clear the mcore state of a mailbox.
 * @mdev: mailbox device to be initialized.
 */
static inline void tmel_qmp_clr_mcore_ch_state(struct qmp_device *mdev)
{
	if (!mdev)
		return;

	/* Clear all fields except link_state */
	mdev->mcore.bits.ch_state = 0;
	mdev->mcore.bits.ch_state_ack = 0;
	mdev->mcore.bits.tx = 0;
	mdev->mcore.bits.tx_ack = 0;
	mdev->mcore.bits.rx_done = 0;
	mdev->mcore.bits.rx_done_ack = 0;
	mdev->mcore.bits.frag_size = 0;
	mdev->mcore.bits.rem_frag_count = 0;
}

/**
 * tmel_qmp_rx() - Handle incoming messages from remote processor.
 * @tdev: tmel device to send the event to.
 */
static void tmel_qmp_rx(struct tmel *tdev)
{
	struct qmp_device *mdev;

	if (!tdev)
		return;

	mdev = tdev->mdev;
	if (!mdev)
		return;

	/* read remote_desc from mailbox register */
	mdev->ucore.val = readl(mdev->ucore_desc);
	/* Make sure ucore descriptor is read before we proceed */
	rmb();

	/* Check if remote link down */
	if (mdev->local_state >= LINK_CONNECTED &&
	    !(mdev->ucore.bits.link_state)) {
		mdev->local_state = LINK_NEGOTIATION;
		mdev->mcore.bits.link_state_ack = mdev->ucore.bits.link_state;
		tmel_qmp_send_irq(mdev);
		return;
	}

	switch (mdev->local_state) {
	case LINK_NEGOTIATION:
		if (!(mdev->ucore.bits.link_state) ||
		    !(mdev->ucore.bits.link_state_ack)) {
			dev_err(mdev->dev, "rx irq:link down state\n");
			break;
		}
		tmel_qmp_clr_mcore_ch_state(mdev);
		mdev->mcore.bits.link_state_ack = mdev->ucore.bits.link_state;
		mdev->local_state = LINK_CONNECTED;
		/* Make sure state is updated before waking up the client */
		wmb();
		mdev->link_complete = true;
		dev_dbg(mdev->dev, "Set to link connected");
		break;
	case LINK_CONNECTED:
		/* No need to handle until local opens */
		break;
	case LOCAL_CONNECTING:
		if (!(mdev->ucore.bits.ch_state)) {
			dev_err(mdev->dev, "rx irq:channel not connected\n");
			break;
		}

		/* check remote ch_state change */
		if (mdev->mcore.bits.ch_state_ack != mdev->ucore.bits.ch_state) {
			/* Ack to remote ch_state change */
			mdev->mcore.bits.ch_state_ack = mdev->ucore.bits.ch_state;
			tmel_qmp_send_irq(mdev);
			break;
		}

		/* check remote ch_state change ack */
		if (mdev->mcore.bits.ch_state_ack &&
		    mdev->ucore.bits.ch_state_ack) {
			mdev->local_state = CHANNEL_CONNECTED;
			/* Make sure state is updated before waking up the client */
			wmb();
			mdev->ch_complete = true;
			dev_dbg(mdev->dev, "Set to channel connected");
			break;
		}

		break;
	case CHANNEL_CONNECTED:
		/* check if rx completed */
		if (tdev->rx_done)
			break;

		/* Check for remote channel down */
		if (!(mdev->ucore.bits.ch_state)) {
			mdev->local_state = LOCAL_CONNECTING;
			mdev->mcore.bits.ch_state_ack = mdev->ucore.bits.ch_state;
			dev_dbg(mdev->dev, "Remote Disconnect");
			tmel_qmp_send_irq(mdev);
		}

		/* Check TX done */
		if (atomic_read(&mdev->tx_sent) &&
		    mdev->ucore.bits.rx_done != mdev->mcore.bits.rx_done_ack) {
			/* Ack to remote */
			mdev->mcore.bits.rx_done_ack = mdev->ucore.bits.rx_done;
			atomic_set(&mdev->tx_sent, 0);
			dev_dbg(mdev->dev, "TX flag cleared");
		}

		/* Check if remote is Transmitting */
		if (!(mdev->ucore.bits.tx != mdev->mcore.bits.tx_ack))
			break;

		/* Check if RX failed */
		if (mdev->ucore.bits.frag_size == 0 ||
		    mdev->ucore.bits.frag_size > QMP_MAX_PKT_SIZE) {
			dev_err(mdev->dev, "Rx frag size error %d\n",
				mdev->ucore.bits.frag_size);
			break;
		}
		tmel_qmp_recv_data(tdev, QMP_CTRL_DATA_SIZE);
		break;
	case LOCAL_DISCONNECTING:
		/* check for ch_state change */
		if (!(mdev->ucore.bits.ch_state)) {
			tmel_qmp_clr_mcore_ch_state(mdev);
			mdev->local_state = LINK_CONNECTED;
			dev_dbg(mdev->dev, "Channel closed");
			/* Make sure the state is updated before signaling completion */
			wmb();
			mdev->ch_complete = false;
		}
		break;
	default:
		dev_err(mdev->dev, "Local Channel State corrupted\n");
	}
}

/**
 * tmel_prepare_msg() - copies the payload to the mbox destination
 * @tdev: the tmel device
 * @msg_uid: msg_type/action_id combo
 * @msg_buf: payload to be sent
 * @msg_size: size of the payload
 */
static int tmel_prepare_msg(struct tmel *tdev, u32 msg_uid, void *msg_buf,
			    size_t msg_size)
{
	struct tmel_ipc_pkt *ipc_pkt = tdev->ipc_pkt;
	struct ipc_header *msg_hdr = &ipc_pkt->msg_hdr;
	struct mbox_payload *mbox_payload = &ipc_pkt->payload.mbox_payload;
	struct sram_payload *sram_payload = &ipc_pkt->payload.sram_payload;

	if (!ipc_pkt || !msg_buf || !msg_size)
		return -EINVAL;

	memset(ipc_pkt, 0, sizeof(struct tmel_ipc_pkt));

	msg_hdr->msg_type = TMEL_MSG_UID_MSG_TYPE(msg_uid);
	msg_hdr->action_id = TMEL_MSG_UID_ACTION_ID(msg_uid);

	dev_dbg(tdev->dev, "uid: %d, msg_size: %zu msg_type:%d, action_id:%d\n",
		msg_uid, msg_size, msg_hdr->msg_type, msg_hdr->action_id);

	if (sizeof(struct ipc_header) + msg_size <= QMP_MBOX_IPC_PACKET_SIZE) {
		/* Mbox only */
		msg_hdr->ipc_type = IPC_MBOX_MEM;
		msg_hdr->msg_len = msg_size;
		memcpy((void *)mbox_payload, msg_buf, msg_size);
		wmb();

		flush_cache((ulong)mbox_payload, sizeof(struct mbox_payload));
	} else if (msg_size <= QMP_SRAM_IPC_MAX_BUF_SIZE) {
		/* SRAM path */
		void *sram_buf;

		msg_hdr->ipc_type = IPC_MBOX_SRAM;
		msg_hdr->msg_len = 8;

		/*
		 * TMEL DMA engine is 32-bit. If msg_buf is above 4 GB (stack/heap
		 * after U-Boot relocation to a high DDR bank), allocate a lower-4-GB
		 * bounce buffer so the DMA address fits in sram_payload->payload_ptr
		 * (a u32 field).
		 */
		tdev->sram_bounce = 0;
		tdev->sram_bounce_size = 0;

		sram_buf = dma32_bounce_start(msg_buf, msg_size,
					      &tdev->sram_bounce, DMA32_BB_RW);
		if (!sram_buf) {
			dev_err(tdev->dev,
				"Failed to alloc lower-4-GB SRAM bounce buffer (%zu bytes)\n",
				msg_size);
			return -ENOMEM;
		}

		tdev->sram_bounce_size = msg_size;

		tdev->sram_dma_addr = dma_map_single(sram_buf, msg_size,
						     DMA_BIDIRECTIONAL);
		if (!tdev->sram_dma_addr) {
			dma32_bounce_stop(msg_buf, tdev->sram_bounce,
					  msg_size, DMA32_BB_WRITE);
			tdev->sram_bounce = 0;
			tdev->sram_bounce_size = 0;
			return -ENOMEM;
		}
		sram_payload->payload_ptr = tdev->sram_dma_addr;
		sram_payload->payload_len = msg_size;

		flush_cache((ulong)sram_payload, sizeof(struct sram_payload));

	} else {
		dev_err(tdev->dev, "Invalid payload length: %zu\n", msg_size);
		return -EINVAL;
	}

	flush_cache((ulong)ipc_pkt, sizeof(struct tmel_ipc_pkt));
	return 0;
}

/**
 * tmel_unprepare_message() - Get the response data back for client
 * @tdev: the tmel device
 * @msg_buf: payload to be sent
 * @msg_size: size of the payload
 */
static void tmel_unprepare_message(struct tmel *tdev, void *msg_buf, size_t msg_size)
{
	struct tmel_ipc_pkt *ipc_pkt = (struct tmel_ipc_pkt *)tdev->pkt.iov_base;
	struct mbox_payload *mbox_payload = &ipc_pkt->payload.mbox_payload;

	if (ipc_pkt->msg_hdr.ipc_type == IPC_MBOX_MEM) {
		memcpy(msg_buf, mbox_payload, msg_size);
		wmb();
	} else if (ipc_pkt->msg_hdr.ipc_type == IPC_MBOX_SRAM) {
		dma_unmap_single(tdev->sram_dma_addr, msg_size, DMA_BIDIRECTIONAL);
		tdev->sram_dma_addr = 0;

		/*
		 * If a bounce buffer was used, copy the TMEL response back to
		 * the original msg_buf and release the LMB allocation.
		 */
		dma32_bounce_stop(msg_buf, tdev->sram_bounce,
				  tdev->sram_bounce_size, DMA32_BB_WRITE);
		tdev->sram_bounce = 0;
		tdev->sram_bounce_size = 0;
	}
}

/**
 * IRQ handling functions
 */
static inline u32 read_pending_interrupt(struct tmel *tdev)
{
	u32 shift = tdev->irq_num % 32;
	u32 offset = GICD_ISPENDRn + ((tdev->irq_num / 32) * 4);

	return ((readl(tdev->irq_base + offset) >> shift) & 1);
}

static inline void clear_pending_interrupt(struct tmel *tdev)
{
	u32 shift = tdev->irq_num % 32;
	u32 offset = GICD_ICPENDRn + ((tdev->irq_num / 32) * 4);
	u32 val = 1 << shift;

	writel(val, tdev->irq_base + offset);
}

static inline void set_interrupt_flags(struct tmel *tdev)
{
	u32 shift = (tdev->irq_num % 16) * 2;
	u32 offset = GICD_ICFGR + ((tdev->irq_num / 16) * 4);
	u32 val = readl(tdev->irq_base + offset);

	val &= ~(0x3 << shift);
	val |= (tdev->irq_flags == IRQ_TYPE_EDGE_RISING ? 0x2 : 0x0) << shift;
	writel(val, tdev->irq_base + offset);
}

static inline void enable_interrupt(struct tmel *tdev)
{
	u32 shift = tdev->irq_num % 32;
	u32 offset = GICD_ISENABLERn + ((tdev->irq_num / 32) * 4);
	u32 val = readl(tdev->irq_base + offset) | (1 << shift);

	writel(val, tdev->irq_base + offset);
}

/**
 * tmel_check_for_irq() - Check for interrupt from TMEL.
 * @tdev: tmel device to send the event to.
 */
static void tmel_check_for_irq(struct tmel *tdev)
{
	int status;
	int timeout = IRQ_CHECK_TIMEOUT;

	if (!tdev || !tdev->mdev)
		return;

	do {
		udelay(IRQ_CHECK_DELAY_US);
		status = read_pending_interrupt(tdev);
		if (status) {
			clear_pending_interrupt(tdev);
			break;
		}
		timeout--;
	} while (timeout);

	if (timeout == 0) {
		printf("IRQ check timeout reached, no interrupt received\n");
	}

	tmel_qmp_rx(tdev);
}

/**
 * tmel_process_request() - process client msg and wait for response
 * @tdev: the tmel device
 * @msg_uid: msg_type/action_id combo
 * @msg_buf: payload to be sent
 * @msg_size: size of the payload
 */
static int tmel_process_request(struct tmel *tdev, u32 msg_uid, void *msg_buf,
				size_t msg_size)
{
	struct qmp_device *mdev;
	struct tmel_ipc_pkt *resp_ipc_pkt;
	int ret = 0, retry;

	if (!tdev || !tdev->ipc_pkt) {
		dev_err(tdev->dev, "Invalid device or IPC packet\n");
		return -EINVAL;
	}

	if (!msg_buf || !msg_size) {
		dev_err(tdev->dev, "Invalid msg_buf or msg_size\n");
		return -EINVAL;
	}

	mdev = tdev->mdev;
	if (!mdev) {
		dev_err(tdev->dev, "Invalid mdev\n");
		return -EINVAL;
	}

	tdev->rx_done = false;

	ret = tmel_prepare_msg(tdev, msg_uid, msg_buf, msg_size);
	if (ret)
		return ret;

	tdev->pkt.iov_len = sizeof(struct tmel_ipc_pkt);
	tdev->pkt.iov_base = (void *)tdev->ipc_pkt;

	ret = tmel_qmp_send_data(mdev, &tdev->pkt);
	if (ret)
		return ret;

	/*
	 * After sending the data, IRQ will be received from TMEL
	 * to acknowledge it
	 */
	retry = MAX_IRQ_RETRY;
	while (retry--) {
		/*
		 * Make sure we see the updated value of rx_done
		 */
		rmb();
		tmel_check_for_irq(tdev);

		if (tdev->rx_done &&
		    mdev->mcore.bits.rx_done_ack == mdev->ucore.bits.rx_done_ack)
			break;
	}

	if (!tdev->rx_done) {
		dev_err(tdev->dev, "Timeout waiting for response\n");
		return -ETIMEDOUT;
	}

	if (tdev->pkt.iov_len != sizeof(struct tmel_ipc_pkt)) {
		tmel_unprepare_message(tdev, msg_buf, msg_size);
		return -EPROTO;
	}

	resp_ipc_pkt = (struct tmel_ipc_pkt *)tdev->pkt.iov_base;
	tmel_unprepare_message(tdev, msg_buf, msg_size);
	tdev->rx_done = false;

	return resp_ipc_pkt->msg_hdr.response;
}

/**
 * tmel_secboot_sec_auth() - authenticate the remote subsys image
 * @tdev: the tmel device
 * @msg: authentication message
 */
static int tmel_secboot_sec_auth(struct tmel *tdev, struct tmel_sec_auth *msg)
{
	struct tmel_secboot_sec_auth smsg __aligned(CONFIG_SYS_CACHELINE_SIZE) = {0};
	struct udevice *dev = tdev->dev;
	int ret;

	if (!tdev || !dev || !msg)
		return -EINVAL;

	smsg.req.sw_id = msg->sw_id;
	smsg.req.elf_buf = msg->elf_buf;
	smsg.req.region_list = msg->region_list;
	smsg.req.relocate = msg->relocate;

	ret = tmel_process_request(tdev, TMEL_MSG_UID_SECBOOT_SEC_AUTH,
				   &smsg,
				   sizeof(struct tmel_secboot_sec_auth));
	if (ret) {
		dev_err(dev, "Failed to send IPC: %d\n", ret);
		return ret;
	}

	if (smsg.resp.status) {
		dev_err(dev, "Failed with status: 0x%X\n", smsg.resp.status);
		ret = -EINVAL;
	}

	if (smsg.resp.extended_error) {
		dev_err(dev, "Failed with error: 0x%X", smsg.resp.extended_error);
		ret = -EINVAL;
	}

	return ret;
}

/**
 * tmel_secboot_sec_auth_v2() - authenticate the remote subsys image (v2)
 * @tdev: the tmel device
 * @msg: authentication message
 */
static int tmel_secboot_sec_auth_v2(struct tmel *tdev, struct tmel_sec_auth_v2 *msg)
{
	struct tmel_secboot_sec_auth_v2 smsg __aligned(CONFIG_SYS_CACHELINE_SIZE) = {0};
	struct udevice *dev = tdev->dev;
	int ret;

	if (!tdev || !dev || !msg)
		return -EINVAL;

	smsg.req.sw_id = msg->sw_id;
	smsg.req.elf_buf = msg->elf_buf;
	smsg.req.region_list = msg->region_list;
	smsg.req.relocate = msg->relocate;
	smsg.req.nsIntegrityCheck = msg->nsIntegrityCheck;

	ret = tmel_process_request(tdev, TMEL_MSG_UID_SECBOOT_SEC_AUTH_V2,
				   &smsg,
				   sizeof(struct tmel_secboot_sec_auth_v2));
	if (ret) {
		dev_err(dev, "Failed to send IPC: %d\n", ret);
		return ret;
	}

	if (smsg.resp.status) {
		dev_err(dev, "Failed with status: 0x%X\n", smsg.resp.status);
		ret = -EINVAL;
	}

	if (smsg.resp.extended_error) {
		dev_err(dev, "Failed with error: 0x%X", smsg.resp.extended_error);
		ret = -EINVAL;
	}

	msg->keyHandle = smsg.resp.keyHandle;

	return ret;
}

/**
 * tmelcom_fuse_list_read() - Read fuse list
 * @tdev: the tmel device
 * @fuse: fuse payload
 * @size: size of payload
 */
int tmelcom_fuse_list_read(struct tmel *tdev, struct tmel_fuse_payload *fuse, size_t size)
{
	int ret;
	struct tmel_fuse_read_multiple_msg msg = {0};
	struct udevice *dev = tdev->dev;
	phys_addr_t fuse_bounce = 0;
	void *fuse_buf;
	dma_addr_t dma_fuse;

	if (!tdev || !dev || !fuse || !size)
		return -EINVAL;

	if (ULONG_MAX - (unsigned long)fuse < size) {
		dev_err(dev, "Integer overflow in cache range calculation\n");
		return -EINVAL;
	}

	if (size > QMP_SRAM_IPC_MAX_BUF_SIZE) {
		dev_err(dev, "Fuse read size %zu exceeds maximum allowed\n", size);
		return -EINVAL;
	}

	/*
	 * msg.fuse_read_data.buf is a u32 field. TMEL reads this address and
	 * DMA-accesses the fuse data directly. The fuse buffer is bidirectional:
	 * the caller fills in fuse addresses (input) and TMEL writes back fuse
	 * values (output). Use DMA32_BB_RW so the fuse addresses are copied into
	 * the bounce buffer before TMEL reads them.
	 */
	fuse_buf = dma32_bounce_start(fuse, size, &fuse_bounce, DMA32_BB_RW);
	if (!fuse_buf) {
		dev_err(dev, "Failed to alloc lower-4-GB fuse bounce buffer\n");
		return -ENOMEM;
	}

	flush_cache((unsigned long)fuse_buf, size);
	dma_fuse = dma_map_single(fuse_buf, size, DMA_BIDIRECTIONAL);

	msg.status = TMEL_ERROR_GENERIC;
	msg.fuse_read_data.buf = (u32)dma_fuse;
	msg.fuse_read_data.buf_len = size;

	ret = tmel_process_request(tdev, TMEL_MSG_UID_FUSE_READ_MULTIPLE_ROW,
				   &msg, sizeof(msg));
	if (ret || msg.status)
		dev_err(dev, "%s: IPC failed, ret=%d status=0x%x\n",
			__func__, ret, msg.status);

	dma_unmap_single(dma_fuse, size, DMA_BIDIRECTIONAL);

	/* Copy fuse result back to original buffer if bounce was used */
	dma32_bounce_stop(fuse, fuse_bounce, size, DMA32_BB_WRITE);

	return ret ? ret : msg.status;
}

/**
 * tmelcom_get_tme_version() - Get TME version information
 * @tdev: the tmel device
 * @version_msg: message containing buffer pointer and length for version data
 */
int tmelcom_get_tme_version(struct tmel *tdev, struct tmel_get_tme_version *version_msg)
{
	int ret;
	struct tmel_get_state msg = {0};
	struct udevice *dev;
	dma_addr_t dma_version;
	void *version_buf;
	u32 length;

	if (!tdev || !version_msg)
		return -EINVAL;

	dev = tdev->dev;
	version_buf = (void *)(uintptr_t)version_msg->pdata;
	length = version_msg->length;

	if (!dev || !version_buf || !length)
		return -EINVAL;

	if (length != TME_PATCH_VERSION_LENGTH) {
		dev_err(dev, "Invalid buffer length %u (expected %d bytes)\n",
			length, TME_PATCH_VERSION_LENGTH);
		return -EINVAL;
	}

	/*
	 * msg.rsp.patch_version.pdata is a u32 field. TMEL writes version data
	 * to this address. Ensure version_buf is in lower 4 GB.
	 */
	{
		phys_addr_t ver_bounce = 0;
		void *ver_buf = dma32_bounce_start(version_buf, length,
						   &ver_bounce, DMA32_BB_WRITE);

		if (!ver_buf) {
			dev_err(dev, "Failed to alloc lower-4-GB version bounce buffer\n");
			return -ENOMEM;
		}

		dma_version = dma_map_single(ver_buf, length, DMA_BIDIRECTIONAL);

		msg.rsp.status = TMEL_ERROR_GENERIC;
		msg.rsp.patch_version.pdata = (u32)dma_version;
		msg.rsp.patch_version.length = length;
		msg.rsp.patch_version.length_used = 0;

		ret = tmel_process_request(tdev, TMEL_MSG_UID_SECBOOT_GET_STATE,
					   &msg, sizeof(msg));
		if (ret || msg.rsp.status)
			dev_err(dev, "%s: IPC failed, ret=%d status=0x%x\n",
				__func__, ret, msg.rsp.status);

		if (msg.rsp.patch_version.length_used > length) {
			dev_err(dev, "TME returned invalid length %u > buffer size %u\n",
				msg.rsp.patch_version.length_used, length);
			dma_unmap_single(dma_version, length, DMA_BIDIRECTIONAL);
			dma32_bounce_stop(NULL, ver_bounce, length, DMA32_BB_WRITE);
			return -EOVERFLOW;
		}

		dma_unmap_single(dma_version, length, DMA_BIDIRECTIONAL);
		dma32_bounce_stop(version_buf, ver_bounce, length, DMA32_BB_WRITE);
	}

	version_msg->length = msg.rsp.patch_version.length_used;

	return ret ? ret : msg.rsp.status;
}

#ifdef CONFIG_IPQ_TMEL_PRNG_IPC_SUPPORT
/**
 * tmelcom_prng_get() - Get random data from PRNG
 * @tdev: the tmel device
 * @prng_msg: message containing buffer pointer and length for PRNG data
 */
int tmelcom_prng_get(struct tmel *tdev, struct tmel_get_prng *prng_msg)
{
	int ret;
	struct tmel_prng_get_msg msg = {0};
	struct udevice *dev;
	dma_addr_t dma_prng;
	void *prng_buf;
	u32 length;

	if (!tdev || !prng_msg)
		return -EINVAL;

	dev = tdev->dev;
	prng_buf = (void *)(uintptr_t)prng_msg->pdata;
	length = prng_msg->length;

	if (!dev || !prng_buf || !length)
		return -EINVAL;

	/* Validate length does not exceed maximum allowed size */
	if (length > PRNG_MAX_LENGTH) {
		dev_err(dev, "PRNG length %u exceeds maximum allowed size 0x%x\n",
			length, PRNG_MAX_LENGTH);
		return -EINVAL;
	}

	/* Additional validation: ensure length doesn't cause overflow */
	if (length > (UINT32_MAX - ARCH_DMA_MINALIGN)) {
		dev_err(dev, "PRNG length %u could cause integer overflow\n", length);
		return -EINVAL;
	}

	/*
	 * msg.output.prng_buf.pdata is a u32 field. TMEL writes PRNG data to
	 * this address. Ensure prng_buf is in lower 4 GB.
	 */
	{
		phys_addr_t prng_bounce = 0;
		void *prng_dma_buf = dma32_bounce_start(prng_buf, length,
							&prng_bounce, DMA32_BB_WRITE);

		if (!prng_dma_buf) {
			dev_err(dev, "Failed to alloc lower-4-GB PRNG bounce buffer\n");
			return -ENOMEM;
		}

		dma_prng = dma_map_single(prng_dma_buf, length, DMA_BIDIRECTIONAL);

		msg.input.length = length;
		msg.output.status = TMEL_ERROR_GENERIC;
		msg.output.prng_buf.pdata = (u32)dma_prng;
		msg.output.prng_buf.length = length;
		msg.output.prng_buf.length_used = 0;

		ret = tmel_process_request(tdev, TMEL_MSG_UID_HCS_PRNG_GET,
					   &msg, sizeof(msg));
		if (ret || msg.output.status)
			dev_err(dev, "%s: IPC failed, ret=%d status=0x%x\n",
				__func__, ret, msg.output.status);

		if (msg.output.prng_buf.length_used > length) {
			dev_err(dev, "TME returned invalid length %u > buffer size %u\n",
				msg.output.prng_buf.length_used, length);
			dma_unmap_single(dma_prng, length, DMA_BIDIRECTIONAL);
			dma32_bounce_stop(NULL, prng_bounce, length, DMA32_BB_WRITE);
			return -EOVERFLOW;
		}

		dma_unmap_single(dma_prng, length, DMA_BIDIRECTIONAL);
		dma32_bounce_stop(prng_buf, prng_bounce, length, DMA32_BB_WRITE);
	}

	prng_msg->length = msg.output.prng_buf.length_used;

	return ret ? ret : msg.output.status;
}
#endif /* CONFIG_IPQ_TMEL_PRNG_IPC_SUPPORT */

/**
 * tmelcom_aes_encrypt() - Encrypt data using AES
 * @tdev: the tmel device
 * @aes_msg: AES encryption message with request/response
 * @size: size of the message
 */
static int tmelcom_aes_encrypt(struct tmel *tdev, void *aes_msg, size_t size)
{
	int ret;
	struct udevice *dev = tdev->dev;

	if (!tdev || !dev || !aes_msg || !size)
		return -EINVAL;

	ret = tmel_process_request(tdev, TMEL_MSG_UID_AES_ENCRYPT,
				   aes_msg, size);
	if (ret)
		dev_err(dev, "%s : IPC Failed. ret: %d\n", __func__, ret);

	return ret;
}

/**
 * tmelcom_aes_decrypt() - Decrypt data using AES
 * @tdev: the tmel device
 * @aes_msg: AES decryption message with request/response
 * @size: size of the message
 */
static int tmelcom_aes_decrypt(struct tmel *tdev, void *aes_msg, size_t size)
{
	int ret;
	struct udevice *dev = tdev->dev;

	if (!tdev || !dev || !aes_msg || !size)
		return -EINVAL;

	ret = tmel_process_request(tdev, TMEL_MSG_UID_AES_DECRYPT,
				   aes_msg, size);
	if (ret)
		dev_err(dev, "%s : IPC Failed. ret: %d\n", __func__, ret);

	return ret;
}

/**
 * tmelcom_aes_derive_key() - Derive AES key
 * @tdev: the tmel device
 * @derive_msg: AES key derivation message with request/response
 * @size: size of the message
 */
static int tmelcom_aes_derive_key(struct tmel *tdev, void *derive_msg, size_t size)
{
	int ret;
	struct udevice *dev = tdev->dev;

	if (!tdev || !dev || !derive_msg || !size)
		return -EINVAL;

	ret = tmel_process_request(tdev, TMEL_MSG_UID_AES_DERIVE_KEY,
				   derive_msg, size);
	if (ret)
		dev_err(dev, "%s : IPC Failed. ret: %d\n", __func__, ret);

	return ret;
}

/**
 * tmelcom_aes_clear_key() - Clear AES key
 * @tdev: the tmel device
 * @clear_msg: AES clear key message
 * @size: size of the message
 */
static int tmelcom_aes_clear_key(struct tmel *tdev, void *clear_msg, size_t size)
{
	int ret;
	struct udevice *dev = tdev->dev;

	if (!tdev || !dev || !clear_msg || !size)
		return -EINVAL;

	ret = tmel_process_request(tdev, TMEL_MSG_UID_AES_CLEAR_KEY,
				   clear_msg, size);
	if (ret)
		dev_err(dev, "%s : IPC Failed. ret: %d\n", __func__, ret);

	return ret;
}

/**
 * tmel_qmp_send() - Send message through mailbox
 * @chan: mailbox channel
 * @data: message data
 */
static int tmel_qmp_send(struct mbox_chan *chan, const void *data)
{
	struct tmel *tdev;
	int ret = -EINVAL;
	struct tmel_qmp_msg *tmsg;

	if (!chan || !data)
		return -EINVAL;

	tdev = dev_get_priv(chan->dev);
	tmsg = (struct tmel_qmp_msg *)data;

	if (!tdev || !tmsg->msg)
		return -EINVAL;

	switch (tmsg->msg_id) {
	case TMEL_MSG_UID_FUSE_READ_MULTIPLE_ROW:
		{
			struct tmel_fuse_payload *fuse = (struct tmel_fuse_payload *)tmsg->msg;

			ret = tmelcom_fuse_list_read(tdev, fuse, tmsg->size);
		}
		break;
	case TMEL_MSG_UID_SECBOOT_SEC_AUTH:
		{
			struct tmel_sec_auth *msg = (struct tmel_sec_auth *)tmsg->msg;

			ret = tmel_secboot_sec_auth(tdev, msg);
		}
		break;
	case TMEL_MSG_UID_SECBOOT_SEC_AUTH_V2:
		{
			struct tmel_sec_auth_v2 *sec_auth = (struct tmel_sec_auth_v2 *)tmsg->msg;

			ret = tmel_secboot_sec_auth_v2(tdev, sec_auth);
		}
		break;
	case TMEL_MSG_UID_SECBOOT_GET_STATE:
		{
			struct tmel_get_tme_version *version_msg =
				(struct tmel_get_tme_version *)tmsg->msg;

			ret = tmelcom_get_tme_version(tdev, version_msg);
		}
		break;
#ifdef CONFIG_IPQ_TMEL_PRNG_IPC_SUPPORT
	case TMEL_MSG_UID_HCS_PRNG_GET:
		{
			struct tmel_get_prng *prng_msg = (struct tmel_get_prng *)tmsg->msg;

			ret = tmelcom_prng_get(tdev, prng_msg);
		}
		break;
#endif
#ifdef CONFIG_IPQ_SOFTSKU_SUPPORT
	case TMEL_MSG_UID_LICENSE_INSTALL:
	case TMEL_MSG_UID_LICENSE_ENFORCE_HW:
		{
			/* License messages are handled via generic IPC mechanism */
			ret = tmel_process_request(tdev, tmsg->msg_id, tmsg->msg, tmsg->size);
		}
		break;
#endif
	case TMEL_MSG_UID_AES_ENCRYPT:
		ret = tmelcom_aes_encrypt(tdev, tmsg->msg, tmsg->size);
		break;
	case TMEL_MSG_UID_AES_DECRYPT:
		ret = tmelcom_aes_decrypt(tdev, tmsg->msg, tmsg->size);
		break;
	case TMEL_MSG_UID_AES_DERIVE_KEY:
		/* Handles both regular and max context derive key requests */
		ret = tmelcom_aes_derive_key(tdev, tmsg->msg, tmsg->size);
		break;
	case TMEL_MSG_UID_AES_CLEAR_KEY:
		ret = tmelcom_aes_clear_key(tdev, tmsg->msg, tmsg->size);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/**
 * tmel_qmp_startup() - Start qmp mailbox channel for communication.
 * @chan: mailbox channel that is being opened.
 * @args: phandle arguments
 * Waits for remote subsystem to open channel if link is not
 * initiated or until timeout.
 */
static int tmel_qmp_startup(struct mbox_chan *chan,
			    struct ofnode_phandle_args *args)
{
	struct tmel *tdev = dev_get_priv(chan->dev);
	struct qmp_device *mdev;
	void *rx_buf;
	int retry;
	int rx_buf_size;
	int ret = 0;

	if (!tdev)
		return -EINVAL;

	mdev = tdev->mdev;
	if (!mdev)
		return -EINVAL;
	/*
	 * Kick start the SM from the negotiation phase
	 * Rest of the link changes would follow when remote responds.
	 */
	mdev->mcore.bits.link_state = 1;
	mdev->local_state = LINK_NEGOTIATION;
	rx_buf_size = 1 * QMP_MAX_PKT_SIZE;

	rx_buf = malloc_cache_aligned(rx_buf_size);
	if (IS_ERR_OR_NULL(rx_buf))
		return -ENOMEM;

	memset(rx_buf, 0, rx_buf_size);
	mdev->rx_pkt.iov_base = rx_buf;
	tmel_qmp_send_irq(mdev);

	retry = MAX_IRQ_RETRY;
	while (!mdev->link_complete && retry--) {
		/*
		 * Make sure we see the updated value of link_complete
		 */
		rmb();
		tmel_check_for_irq(tdev);
		if (mdev->link_complete)
			break;
	}

	if (!mdev->link_complete) {
		dev_err(tdev->dev, "Timeout waiting for link completion\n");
		return -ETIMEDOUT;
	}

	if (mdev->local_state == LINK_CONNECTED) {
		mdev->mcore.bits.ch_state = 1;
		mdev->local_state = LOCAL_CONNECTING;
		dev_dbg(mdev->dev, "link complete, local connecting");
		tmel_qmp_send_irq(mdev);
	}

	retry = MAX_IRQ_RETRY;
	while (!mdev->ch_complete && retry--) {
		/*
		 * Make sure we see the updated value of ch_complete
		 */
		rmb();
		tmel_check_for_irq(tdev);
		if (mdev->ch_complete)
			break;
	}

	if (!mdev->ch_complete) {
		dev_err(tdev->dev, "Timeout waiting for channel completion\n");
		return -ETIMEDOUT;
	}

	return ret;
}

/**
 * tmel_qmp_shutdown() - Shutdown this mailbox channel.
 * @chan: mailbox channel to be shutdown.
 * Disconnect this mailbox channel so the client does not receive anymore
 * data and can reliquish control of the channel.
 */
static int tmel_qmp_shutdown(struct mbox_chan *chan)
{
	struct tmel *tdev = dev_get_priv(chan->dev);
	struct qmp_device *mdev = tdev->mdev;
	int retry;

	if (!mdev)
		return -EINVAL;

	if (mdev->local_state != LINK_DISCONNECTED) {
		mdev->local_state = LOCAL_DISCONNECTING;
		mdev->mcore.bits.ch_state = 0;
		tmel_qmp_send_irq(mdev);

		retry = MAX_IRQ_RETRY;
		while (mdev->local_state == LOCAL_DISCONNECTING && retry--) {
			/* Observe state updated by tmel_qmp_rx(). */
			rmb();
			tmel_check_for_irq(tdev);
		}

		if (mdev->local_state == LOCAL_DISCONNECTING) {
			dev_err(mdev->dev, "Timeout waiting for channel close\n");
			return -ETIMEDOUT;
		}
	}

	return 0;
}

static struct mbox_ops tmel_qmp_mbox_ops = {
	.of_xlate = tmel_qmp_startup,
	.rfree = tmel_qmp_shutdown,
	.send = tmel_qmp_send,
};

/**
 * tmel_init() - Initialize TMEL device
 * @dev: device to initialize
 */
static int tmel_init(struct udevice *dev)
{
	struct tmel *tdev = dev_get_priv(dev);

	if (!tdev)
		return -EINVAL;

	tdev->ipc_pkt = malloc_cache_aligned(sizeof(struct tmel_ipc_pkt));
	if (IS_ERR_OR_NULL(tdev->ipc_pkt))
		return -ENOMEM;

	memset(tdev->ipc_pkt, 0, sizeof(struct tmel_ipc_pkt));
	tdev->rx_done = false;
	tdev->dev = dev;

	return 0;
}

/**
 * qmp_init() - Initialize QMP device
 * @dev: device to initialize
 */
static struct qmp_device *qmp_init(struct udevice *dev)
{
	struct qmp_device *mdev;
	struct resource res;

	mdev = malloc_cache_aligned(sizeof(struct qmp_device));
	if (IS_ERR_OR_NULL(mdev))
		return ERR_PTR(-ENOMEM);

	memset(mdev, 0, sizeof(struct qmp_device));
	mdev->dev = dev;

	dev_read_resource(dev, 0, &res);
	mdev->mcore_desc = devm_ioremap(dev, res.start, resource_size(&res));
	if (IS_ERR(mdev->mcore_desc))
		return ERR_PTR(-EIO);

	dev_read_resource(dev, 1, &res);
	mdev->ucore_desc = devm_ioremap(dev, res.start, resource_size(&res));
	if (IS_ERR(mdev->ucore_desc))
		return ERR_PTR(-EIO);

	mdev->local_state = LINK_DISCONNECTED;
	mdev->link_complete = false;
	mdev->ch_complete = false;

	return mdev;
}

/**
 * tmel_qmp_parse_dt() - Parse device tree for TMEL device
 * @dev: device to parse DT for
 */
static int tmel_qmp_parse_dt(struct udevice *dev)
{
	struct tmel *tdev = dev_get_priv(dev);
	const void *fdt = gd->fdt_blob;
	int size, root, addr_cells, intc_node;
	const u32 *list;
	const fdt32_t *ph, *addr_cells_ptr, *reg, *intr_cells;
	u32 phandle, count, irq_type, irq_num;
	u64 addr;

	if (!tdev)
		return -EINVAL;

	root = fdt_path_offset(fdt, "/");
	addr_cells_ptr = fdt_getprop(fdt, root, "#address-cells", NULL);
	if (!addr_cells_ptr) {
		dev_err(dev, "#address-cells not found in root\n");
		return -ENODEV;
	}
	addr_cells = fdt32_to_cpu(*addr_cells_ptr);

	ph = fdt_getprop(fdt, root, "interrupt-parent", NULL);
	if (!ph) {
		dev_err(dev, "interrupt-parent not found in root\n");
		return -ENODEV;
	}

	phandle = fdt32_to_cpu(*ph);
	intc_node = fdt_node_offset_by_phandle(fdt, phandle);
	if (intc_node < 0) {
		dev_err(dev, "Interrupt controller node not found\n");
		return -ENODEV;
	}

	reg = fdt_getprop(fdt, intc_node, "reg", NULL);
	if (!reg) {
		dev_err(dev, "reg property not found in interrupt controller node\n");
		return -ENODEV;
	}

	if (addr_cells == 2)
		addr = ((u64)fdt32_to_cpu(reg[0]) << 32) | fdt32_to_cpu(reg[1]);
	else
		addr = fdt32_to_cpu(reg[0]);

	tdev->irq_base = (void __iomem *)(uintptr_t)addr;
	list = dev_read_prop(dev, "interrupts", &size);
	if (!list) {
		dev_err(dev, "interrupts property not found\n");
		return -ENOENT;
	}

	intr_cells = fdt_getprop(fdt, intc_node, "#interrupt-cells", NULL);
	if (!intr_cells) {
		dev_err(dev, "interrupt-cells property not found\n");
		return -ENOENT;
	}

	count = fdt32_to_cpu(*intr_cells);
	if (count != 3) {
		dev_err(dev, "unexpected interrupt-cells count: %d\n", count);
		return -ENOENT;
	}

	irq_type = fdt32_to_cpu(list[0]);
	irq_num = fdt32_to_cpu(list[1]);
	tdev->irq_flags = fdt32_to_cpu(list[2]);

	if (irq_type == GIC_SPI)
		tdev->irq_num = irq_num + 32;
	else
		tdev->irq_num = irq_num + 16;

	return 0;
}

/**
 * tmel_qmp_mbox_probe() - Probe TMEL QMP mailbox device
 * @dev: device to probe
 *
 * Return: 0 on success, negative error code on failure
 */
static int tmel_qmp_mbox_probe(struct udevice *dev)
{
	struct tmel *tdev;
	struct qmp_device *mdev;
	struct qmp_device_cfg *mdev_cfg;
	bool enable;
	int ret;

	/* Validate device private data */
	tdev = dev_get_priv(dev);
	if (!tdev)
		return -EINVAL;

	/* Get device configuration and determine enable state */
	mdev_cfg = (struct qmp_device_cfg *)dev_get_driver_data(dev);
	enable = true;
	if (mdev_cfg && mdev_cfg->issupport_check) {
		/* Check hardware support bit - if not set, disable */
		enable = !!(readl(mdev_cfg->reg) & mdev_cfg->bit);
		if (!enable) {
			dev_dbg(dev, "Device not supported by hardware\n");
			return -ENODEV;
		}
	}

	/* Parse device tree properties */
	ret = tmel_qmp_parse_dt(dev);
	if (ret) {
		dev_err(dev, "Failed to parse device tree: %d\n", ret);
		return ret;
	}

	/* Initialize TMEL device */
	ret = tmel_init(dev);
	if (ret) {
		dev_err(dev, "Failed to initialize TMEL: %d\n", ret);
		return ret;
	}

	/* Initialize QMP device */
	mdev = qmp_init(dev);
	if (IS_ERR(mdev)) {
		ret = PTR_ERR(mdev);
		dev_err(dev, "Failed to initialize QMP: %d\n", ret);
		goto err_cleanup_tmel;
	}

	/* Link QMP device to TMEL */
	tdev->mdev = mdev;
	mdev->is_enabled = enable;

	/* Configure device-specific settings */
	if (mdev_cfg)
		mdev->shared_irq = mdev_cfg->shared_irq;

	/* Configure and enable interrupts */
	set_interrupt_flags(tdev);
	enable_interrupt(tdev);

	/* Clear pending interrupts */
	clear_pending_interrupt(tdev);

	return 0;

err_cleanup_tmel:
	/* Cleanup TMEL resources on error */
	if (tdev->ipc_pkt) {
		free(tdev->ipc_pkt);
		tdev->ipc_pkt = NULL;
	}
	return ret;
}

static const struct qmp_device_cfg config_ipq_sec = {
	.shared_irq		= BIT(21),
	.issupport_check	= false,
};

static const struct qmp_device_cfg config_ipq5210_nsec = {
	.shared_irq		= BIT(20),
	.reg			= 0xA4020,
	.bit			= BIT(8),
	.issupport_check	= true,
};

static const struct udevice_id tmel_qmp_mbox_of_match[] = {
	{
		.compatible = "qcom,tmel-qmp-mbox-secure",
		.data = (ulong)&config_ipq_sec
	},
	{
		.compatible = "qcom,tmel-qmp-mbox",
		.data = (ulong)&config_ipq5210_nsec
	},
	{}
};

U_BOOT_DRIVER(tmel_qmp_mbox) = {
	.name = "tmel-qmp-mbox",
	.id = UCLASS_MAILBOX,
	.of_match = tmel_qmp_mbox_of_match,
	.probe = tmel_qmp_mbox_probe,
	.priv_auto = sizeof(struct tmel),
	.ops = &tmel_qmp_mbox_ops,
};
