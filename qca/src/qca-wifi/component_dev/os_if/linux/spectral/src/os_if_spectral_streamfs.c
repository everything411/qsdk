/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <os_if_spectral_streamfs.h>
#include <wlan_cfg80211_spectral.h>
#include <spectral_cmn_api_i.h>
#include <spectral_defs_i.h>
#include <qdf_module.h>
#include <wlan_cfg80211.h>
#include <wlan_objmgr_pdev_obj.h>
#include <qdf_net_if.h>
#include <wlan_osif_priv.h>
#include <wlan_spectral_ucfg_api.h>
#include <cfg_ucfg_api.h>
#include <cfg_spectral.h>

#define STREAMFS_DATA_CHANNEL_FILE                 "data_channel"
#define STREAMFS_DATA_SUB_BUFFER_SIZE_FILE         "data_sub_buffer_size"
#define STREAMFS_DATA_NUM_SUB_BUFFERS_FILE         "data_num_sub_buffers"

/**
 * spectral_get_dev_name() - Get net device name from pdev
 * @pdev: objmgr pdev
 *
 *  Return: netdev name
 */
static char *spectral_get_dev_name(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_osif_priv *pdev_ospriv;
	struct qdf_net_if *nif;

	pdev_ospriv = wlan_pdev_get_ospriv(pdev);
	if (!pdev_ospriv) {
		spectral_err("pdev_ospriv is NULL\n");
		return NULL;
	}

	nif = pdev_ospriv->nif;
	if (!nif) {
		spectral_err("pdev nif is NULL\n");
		return NULL;
	}

	return  qdf_net_if_get_devname(nif);
}

/**
 * os_if_spectral_streamfs_sub_buffer_debugfs_init() - Creates the debugfs
 * files for streamfs channel properties.
 * @pdev: objmgr pdev
 *
 *  Return: QDF_STATUS
 */
static QDF_STATUS
os_if_spectral_streamfs_sub_buffer_debugfs_init
			(struct wlan_objmgr_pdev *pdev)
{
	struct wlan_objmgr_psoc *psoc;
	struct pdev_spectral *ps;
	struct pdev_spectral_streamfs *pss;

	ps = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						   WLAN_UMAC_COMP_SPECTRAL);

	if (!ps) {
		spectral_err("PDEV SPECTRAL object is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	psoc = wlan_pdev_get_psoc(pdev);

	pss = &ps->streamfs_obj;

	pss->n_subbuf = cfg_get(psoc, CFG_SPECTRAL_STREAMFS_NUM_BUFFERS);
	qdf_debugfs_create_u32
			(STREAMFS_DATA_NUM_SUB_BUFFERS_FILE,
			 QDF_FILE_USR_READ,
			 pss->dir_ptr,
			 &pss->n_subbuf);


	pss->subbuf_size = cfg_get(psoc, CFG_SPECTRAL_STREAMFS_BUFFER_SIZE);
	qdf_debugfs_create_u32
			(STREAMFS_DATA_SUB_BUFFER_SIZE_FILE,
			 QDF_FILE_USR_READ,
			 pss->dir_ptr,
			 &pss->subbuf_size);

	return QDF_STATUS_SUCCESS;
}

/**
 * os_if_spectral_streamfs_sub_buffer_debugfs_deinit() - Destroys the debugfs
 * files for streamfs channel properties.
 * @pdev: objmgr pdev
 *
 *  Return: QDF_STATUS
 */
static void
os_if_spectral_streamfs_sub_buffer_debugfs_deinit
			(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_spectral *ps;
	struct pdev_spectral_streamfs *pss;

	ps = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						   WLAN_UMAC_COMP_SPECTRAL);

	if (!ps) {
		spectral_err("PDEV SPECTRAL object is NULL!");
		return;
	}

	pss = &ps->streamfs_obj;

	pss->n_subbuf = 0;
	pss->subbuf_size = 0;
}


/**
 * os_if_spectral_streamfs_get_buff_size() -Get the sample buffer
 * allocated size via streamfs
 * @pdev : Pointer to pdev
 * @buff_size: Pointer to store data
 *
 * Return: QDF_STATUS
 */

static QDF_STATUS
os_if_spectral_streamfs_get_buff_size(struct wlan_objmgr_pdev *pdev,
				      uint32_t *buff_size)
{
	struct pdev_spectral *ps;
	struct pdev_spectral_streamfs *pss;

	if (!buff_size) {
		osif_err("buff_size pointer is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	ps = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						   WLAN_UMAC_COMP_SPECTRAL);

	if (!ps) {
		spectral_err("PDEV SPECTRAL object is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	pss = &ps->streamfs_obj;

	*buff_size = pss->subbuf_size;

	return QDF_STATUS_SUCCESS;
}

/**
 * os_if_spectral_streamfs_init_channel() - Initialize streamfs channel for
 * spectral module for a given pdev
 * @pdev : Pointer to pdev
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
os_if_spectral_streamfs_init_channel(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_spectral *ps;
	struct pdev_spectral_streamfs *pss;
	char *devname;
	enum spectral_msg_type msg_type = SPECTRAL_MSG_NORMAL_MODE;
	qdf_dentry_t spectral_dir_ptr;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!pdev) {
		spectral_err("PDEV is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	ps = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						   WLAN_UMAC_COMP_SPECTRAL);

	if (!ps) {
		spectral_err("PDEV SPECTRAL object is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	ps->transport_mode = SPECTRAL_DATA_TRANSPORT_RELAY;

	devname = spectral_get_dev_name(pdev);
	if (!devname) {
		spectral_err("devname is NULL");
		return QDF_STATUS_E_NOENT;
	}

	/* Get parent directory for streamfs channel file */
	spectral_dir_ptr = ucfg_spectral_get_spectral_directory();

	if (!spectral_dir_ptr) {
		spectral_err("Spectral directory not found.");
		return QDF_STATUS_E_NOENT;
	}

	pss = &ps->streamfs_obj;

	pss->dir_ptr = qdf_streamfs_create_dir(devname, spectral_dir_ptr);

	if (!pss->dir_ptr) {
		spectral_err("Directory create failed");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = os_if_spectral_streamfs_sub_buffer_debugfs_init(pdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		spectral_err
			("Failed to initialise streamfs sub buffer debugfs files.");
		goto cleanup;
	}

	pss->chan_ptr = qdf_streamfs_open(STREAMFS_DATA_CHANNEL_FILE,
					  pss->dir_ptr,
					  pss->subbuf_size,
					  pss->n_subbuf, NULL);

	if (!pss->chan_ptr) {
		spectral_err("Chan create failed");
		status = QDF_STATUS_E_FAILURE;
		goto cleanup_debugfs;
	}

	for (; msg_type < SPECTRAL_MSG_TYPE_MAX; msg_type++)
		pss->streamfs_buf[msg_type] = NULL;

	return QDF_STATUS_SUCCESS;

cleanup_debugfs:
	os_if_spectral_streamfs_sub_buffer_debugfs_deinit(pdev);
cleanup:
	qdf_streamfs_remove_dir_recursive(pss->dir_ptr);
	pss->dir_ptr = NULL;

	return status;
}

/**
 * os_if_spectral_deinit_channel() - Destroy/close steamfs channel for
 * spectral module for a given pdev
 * @pdev : Pointer to pdev
 *
 * Return: Success/Failure
 */
static QDF_STATUS
os_if_spectral_deinit_channel(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_spectral *ps = NULL;
	struct pdev_spectral_streamfs *pss = NULL;
	enum spectral_msg_type msg_type = SPECTRAL_MSG_NORMAL_MODE;

	if (!pdev) {
		spectral_err("PDEV is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	ps = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						   WLAN_UMAC_COMP_SPECTRAL);

	if (!ps) {
		spectral_err("PDEV SPECTRAL object is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	pss = &ps->streamfs_obj;

	if (pss->chan_ptr) {
		qdf_streamfs_close(pss->chan_ptr);
		pss->chan_ptr = NULL;
	}

	os_if_spectral_streamfs_sub_buffer_debugfs_deinit(pdev);

	if (pss->dir_ptr) {
		qdf_streamfs_remove_dir_recursive(pss->dir_ptr);
		pss->dir_ptr = NULL;
	}

	for (; msg_type < SPECTRAL_MSG_TYPE_MAX; msg_type++)
		pss->streamfs_buf[msg_type] = NULL;

	return QDF_STATUS_SUCCESS;
}

/**
 * os_if_spectral_streamfs_alloc_buf() - Allocates spectral buffer in
 * streamfs channel for spectral module
 * @pdev : Pointer to pdev
 * @smsg_type: Spectral message type
 * @buf_type: Spectral buffer type
 *
 * Return: Pointer to the reserved memory
 */

static void *
os_if_spectral_streamfs_alloc_buf(struct wlan_objmgr_pdev *pdev,
				  enum spectral_msg_type smsg_type,
				  enum spectral_msg_buf_type buf_type)
{
	struct pdev_spectral *ps = NULL;
	struct pdev_spectral_streamfs *pss = NULL;
	void *buf = NULL;

	if (!pdev) {
		spectral_err("PDEV is NULL!");
		return NULL;
	}

	if (smsg_type >= SPECTRAL_MSG_TYPE_MAX) {
		spectral_err("Invalid Spectral message type %u", smsg_type);
		return NULL;
	}

	if (buf_type >= SPECTRAL_MSG_BUF_TYPE_MAX) {
		spectral_err("Invalid Spectral message buffer type %u",
			     buf_type);
		return NULL;
	}

	ps = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						   WLAN_UMAC_COMP_SPECTRAL);

	if (!ps) {
		spectral_err("PDEV SPECTRAL object is NULL!");
		return NULL;
	}

	pss = &ps->streamfs_obj;

	switch (buf_type) {
	case SPECTRAL_MSG_BUF_NEW:
		if (pss->streamfs_buf[smsg_type]) {
			spectral_err(
				"streamfs_buff is expected to be NULL");
			return NULL;
		}
		/* As the spectral data would be filled after the
		 * memory gets reserved.  Get the new sub-buffer's
		 * base address and then manually write the data into
		 * it. The offset would be moved once the data is
		 * written. If all sub-buffers are full, NULL would be
		 * returned in no-overwrite mode.
		 */
		pss->streamfs_buf[smsg_type] =
			qdf_streamfs_reserve(pss->chan_ptr, 0);
		if (!pss->streamfs_buf[smsg_type]) {
			spectral_err(
				"alloc sub-buffer(len=%u, msg_type=%u) failed",
				pss->subbuf_size, smsg_type);
			return NULL;
		}
		buf = pss->streamfs_buf[smsg_type];

		/* Set sub-buffer memory to zero after allocating */
		qdf_mem_zero(buf, pss->subbuf_size);
		break;
	case SPECTRAL_MSG_BUF_SAVED:
		if (!pss->streamfs_buf[smsg_type]) {
			spectral_err(
				"streamfs_buf is NULL, expected to have data");
			return NULL;
		}
		buf = pss->streamfs_buf[smsg_type];
		break;
	default:
		spectral_err("Failed to get spectral report buffer");
		buf = NULL;
	}

	return buf;
}

/**
 * os_if_spectral_streamfs_send_msg() - Sends spectral message to user
 * space
 * @pdev : Pointer to pdev
 * @smsg_type: Spectral message type
 *
 * Return: 0 on success else failure
 */
static int
os_if_spectral_streamfs_send_msg(struct wlan_objmgr_pdev *pdev,
				 enum spectral_msg_type smsg_type)
{
	struct pdev_spectral *ps = NULL;
	struct pdev_spectral_streamfs *pss = NULL;

	if (!pdev) {
		spectral_err("PDEV is NULL!");
		return -EINVAL;
	}

	if (smsg_type >= SPECTRAL_MSG_TYPE_MAX) {
		spectral_err("Invalid Spectral message type %u", smsg_type);
		return -EINVAL;
	}

	ps = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						   WLAN_UMAC_COMP_SPECTRAL);

	if (!ps) {
		spectral_err("PDEV SPECTRAL object is NULL!");
		return -EINVAL;
	}

	pss = &ps->streamfs_obj;

	 /* Move sub-buffer pointer, as the data has been written. */
	qdf_streamfs_reserve(pss->chan_ptr, pss->subbuf_size);

	/* Switch to next subbuffer */
	qdf_streamfs_flush(pss->chan_ptr);

	/* Clear the local copy */
	pss->streamfs_buf[smsg_type] = NULL;

	return 0;
}

/**
 * os_if_spectral_streamfs_free_msg() - Clears streamfs local buffer
 * spectral_buf_cb.free_sbuff API is called to free samp structure skb for
 * mode with channel width 160 or 80p80, for streamfs only local buffer
 * is cleared.
 * @pdev : Pointer to pdev
 * @smsg_type: Spectral message type
 *
 * Return: void
 */
static void
os_if_spectral_streamfs_free_msg(struct wlan_objmgr_pdev *pdev,
				 enum spectral_msg_type smsg_type)
{
	struct pdev_spectral *ps = NULL;
	struct pdev_spectral_streamfs *pss = NULL;

	if (smsg_type >= SPECTRAL_MSG_TYPE_MAX) {
		spectral_err("Invalid Spectral message type %u", smsg_type);
		return;
	}

	ps = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						   WLAN_UMAC_COMP_SPECTRAL);

	if (!ps) {
		spectral_err("PDEV SPECTRAL object is NULL!");
		return;
	}

	pss = &ps->streamfs_obj;

	/* Clear the local copy */
	pss->streamfs_buf[smsg_type] = NULL;
}

/**
 * os_if_spectral_streamfs_init() - Initialize streamfs channel and associated
 * callbacks for spectral module for a given pdev
 * @pdev : Pointer to pdev
 *
 * Return: QDF_STATUS
 */

QDF_STATUS
os_if_spectral_streamfs_init(struct wlan_objmgr_pdev *pdev)
{
	struct spectral_buffer_cb spectral_buf_cb = {0};
	struct spectral_context *sptrl_ctx;
	QDF_STATUS status;

	if (!pdev) {
		spectral_err("PDEV is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (wlan_spectral_is_feature_disabled_pdev(pdev)) {
		spectral_err("Spectral feature is disabled");
		return QDF_STATUS_COMP_DISABLED;
	}

	sptrl_ctx = spectral_get_spectral_ctx_from_pdev(pdev);

	if (!sptrl_ctx) {
		spectral_err("Spectral context is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	status = os_if_spectral_streamfs_init_channel(pdev);
	if (QDF_IS_STATUS_ERROR(status)) {
		spectral_err("Failed to initialise streamfs channel.");
		return status;
	}

	/* Register handlers */
	spectral_buf_cb.get_sbuff = os_if_spectral_streamfs_alloc_buf;
	spectral_buf_cb.send_bcast = NULL;
	spectral_buf_cb.send_unicast = os_if_spectral_streamfs_send_msg;
	spectral_buf_cb.free_sbuff = os_if_spectral_streamfs_free_msg;
	spectral_buf_cb.convert_to_phy_ch_width =
		wlan_spectral_get_phy_ch_width;
	spectral_buf_cb.convert_to_nl_ch_width =
		wlan_spectral_get_nl80211_chwidth;
	spectral_buf_cb.reset_transport_channel =
		os_if_spectral_streamfs_reset_channel;
	spectral_buf_cb.get_buff_size =
		os_if_spectral_streamfs_get_buff_size;

	if (sptrl_ctx->sptrlc_use_broadcast)
		sptrl_ctx->sptrlc_use_broadcast(pdev, false);

	if (sptrl_ctx->sptrlc_register_buffer_cb)
		sptrl_ctx->sptrlc_register_buffer_cb(pdev, &spectral_buf_cb);

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(os_if_spectral_streamfs_init);

/**
 * os_if_spectral_streamfs_deinit() - De-initialize streamfs channel and
 * associated callbacks for spectral module for a given pdev
 * @pdev : Pointer to pdev
 *
 * Return: QDF_STATUS
 */
QDF_STATUS os_if_spectral_streamfs_deinit(struct wlan_objmgr_pdev *pdev)
{
	struct spectral_context *sptrl_ctx;

	if (!pdev) {
		spectral_err("PDEV is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (wlan_spectral_is_feature_disabled_pdev(pdev)) {
		spectral_err("Spectral feature is disabled");
		return QDF_STATUS_COMP_DISABLED;
	}

	sptrl_ctx = spectral_get_spectral_ctx_from_pdev(pdev);

	if (!sptrl_ctx) {
		spectral_err("Spectral context is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (sptrl_ctx->sptrlc_deregister_buffer_cb)
		sptrl_ctx->sptrlc_deregister_buffer_cb(pdev);

	return os_if_spectral_deinit_channel(pdev);
}

qdf_export_symbol(os_if_spectral_streamfs_deinit);

/**
 * os_if_spectral_streamfs_reset_channel() - Reset spectral streamfs channel
 * @pdev: Pointer to pdev
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
os_if_spectral_streamfs_reset_channel(struct wlan_objmgr_pdev *pdev)
{
	struct pdev_spectral *ps = NULL;
	struct pdev_spectral_streamfs *pss = NULL;

	ps = wlan_objmgr_pdev_get_comp_private_obj(pdev,
						   WLAN_UMAC_COMP_SPECTRAL);

	if (!ps) {
		spectral_err("PDEV SPECTRAL object is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	pss = &ps->streamfs_obj;

	if (!pss->chan_ptr) {
		spectral_err("Streamfs channel pointer is NULL!");
		return QDF_STATUS_E_NULL_VALUE;
	}

	qdf_streamfs_reset(pss->chan_ptr);

	return QDF_STATUS_SUCCESS;
}

qdf_export_symbol(os_if_spectral_streamfs_reset_channel);
