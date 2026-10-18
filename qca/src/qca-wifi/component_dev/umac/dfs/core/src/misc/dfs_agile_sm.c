/*
 * Copyright (c) 2016-2020 The Linux Foundation. All rights reserved.
 * Copyright (c) 2007-2008 Sam Leffler, Errno Consulting
 * Copyright (c) 2023 Qualcomm Innovation Center, Inc. All rights reserved.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * DOC: This file has Agile State machine functions.
 *
 */

#include "../dfs_precac_forest.h"
#include <dfs_zero_cac.h>
#include <dfs.h>
#include <dfs_process_radar_found_ind.h>
#include "wlan_dfs_init_deinit_api.h"
#include "wlan_reg_services_api.h"
#ifdef QCA_SUPPORT_AGILE_DFS
#include <wlan_sm_engine.h> /* for struct wlan_sm */
#endif
#include <wlan_dfs_utils_api.h>

/* Number of 20MHz sub-channels in 160 MHz segment */
#define NUM_CHANNELS_160MHZ  8
/* Number of 20MHz sub-channels in 320 MHz segment */
#define NUM_CHANNELS_320MHZ 16
/* Number of 20MHz sub-channels in 240 MHz (320-80) segment */
#define NUM_CHANNELS_240MHZ 12

#ifdef WLAN_FEATURE_11BE
#define MAX_20MHZ_SUBCHANS NUM_CHANNELS_320MHZ
#else
#define MAX_20MHZ_SUBCHANS NUM_CHANNELS_160MHZ
#endif

#ifdef QCA_SUPPORT_AGILE_DFS
/* dfs_start_agile_engine() - Prepare ADFS params and program the agile
 *                            engine sending agile config cmd to FW.
 * @dfs: Pointer to struct wlan_dfs.
 */
void dfs_start_agile_engine(struct wlan_dfs *dfs)
{
	struct dfs_agile_cac_params adfs_param;
	struct wlan_lmac_if_dfs_tx_ops *dfs_tx_ops;
	struct dfs_soc_priv_obj *dfs_soc_obj = dfs->dfs_soc_obj;
	uint8_t n_sub_chans;
	uint8_t i;
	qdf_freq_t sub_chans[MAX_20MHZ_SUBCHANS];

	/* Fill the RCAC ADFS params and send it to FW.
	 * FW does not use RCAC timeout values for RCAC feature.
	 * FW runs an infinite timer.
	 */
	dfs_fill_adfs_chan_params(dfs, &adfs_param);
	adfs_param.min_precac_timeout = MIN_RCAC_DURATION;
	adfs_param.max_precac_timeout = MAX_RCAC_DURATION;
	adfs_param.ocac_mode = QUICK_RCAC_MODE;

	qdf_info("%s : %d RCAC channel request sent for pdev: %pK ch_freq: %d",
		 __func__, __LINE__, dfs->dfs_pdev_obj,
		 dfs->dfs_agile_precac_freq_mhz);

	dfs_tx_ops = wlan_psoc_get_dfs_txops(dfs_soc_obj->psoc);

	n_sub_chans =
	    dfs_find_subchannels_for_center_freq(
						 adfs_param.precac_center_freq_1,
						 adfs_param.precac_center_freq_2,
						 adfs_param.precac_chwidth,
						 sub_chans);
	for (i = 0; i < n_sub_chans; i++)
	    utils_dfs_deliver_event(dfs->dfs_pdev_obj,
				    sub_chans[i],
				    WLAN_EV_PCAC_STARTED);


	if (dfs_tx_ops && dfs_tx_ops->dfs_agile_ch_cfg_cmd)
		dfs_tx_ops->dfs_agile_ch_cfg_cmd(dfs->dfs_pdev_obj,
						 &adfs_param);
	else
		dfs_err(NULL, WLAN_DEBUG_DFS_ALWAYS,
			"dfs_tx_ops=%pK", dfs_tx_ops);
}

/**
 * --------------------- ROLLING CAC STATE MACHINE ----------------------
 *
 * Rolling CAC is a feature where in, a separate hardware (Agile detector)
 * will be brought up in a channel that is not the current operating channel
 * and will continue to monitor the channel non-stop, until the next
 * channel change or radar in this RCAC channel.
 *
 * Now if the Rolling CAC channel was radar free for a minimum duration
 * (1 min.) and the device is now switching to this channel, no CAC is required.
 *
 * I.e. let's say the current operating channel is 64 HT80 and we are starting
 * the agile detector in 100 HT80. After a minute of being up in 100 HT80, we
 * switch the radio to 100 HT80. This operating channel change will not
 * require CAC now since the channel was radar free for the last 1 minute,
 * as determined by the agile detector.
 *
 * Introduction of a rolling CAC state machine:
 *
 * To acheive the rolling CAC feature using the agile detector, a trivial
 * state machine is implemented, as represented below:
 *
 *                           _________________
 *                          |                 |
 *            |------------>|       INIT      |<-----------|
 *            |             |_________________|            |
 *            |                      |                     |
 *            |                      |                     |
 *            | [EV_RCAC_STOP]       | [EV_RCAC_START]     | [EV_RCAC_STOP]
 *            | [EV_ADFS_RADAR]      |                     | [EV_ADFS_RADAR]
 *            |                      |                     |
 *            |                      |                     |
 *    ________|________              |             ________|________
 *   |                 |             |----------->|                 |
 *   |    COMPLETE     |                          |     RUNNING     |
 *   |_________________|<-------------------------|_________________|
 *                             [EV_RCAC_DONE]
 *
 *
 *
 * Legend:
 *     _________________
 *    |                 |
 * 1. |   RCAC STATES   |
 *    |_________________|
 *
 * 2. [RCAC_EVENTS]
 *
 *
 * Event triggers and handlers description:
 *
 * EV_RCAC_START:
 *   Posted from vdev response and is handled by all three states.
 *   1. INIT handler:
 *        a. Check if RCAC is already running,
 *           - If yes, do not transition.
 *           - If no, go to step b.
 *        b. Check if a new RCAC channel can be found,
 *           - If no, do not transition.
 *           - If yes, transition to RUNNING.
 *
 * EV_RCAC_STOP:
 *   Posted from last vap down or config disable, handled by RUNNING
 *   and COMPLETE.
 *   1. RUNNING handler:
 *        a. Stop the HOST RCAC timer.
 *        b. Send wmi_adfs_abort_cmd to FW and transition to INIT.
 *   2. COMPLETE handler:
 *        a. Send wmi_adfs_abort_cmd to FW and transition to INIT.
 *
 * EV_ADFS_RADAR:
 *   Posted from radar detection and is handled in RUNNING and COMPLETE.
 *   1. RUNNING handler (same as EV_RCAC_START):
 *        a. Check if RCAC was running for this pdev,
 *           - If yes, transition to INIT and post EV_RCAC_START event.
 *           - If no, ignore.
 *   2. COMPLETE handler (same as EV_RCAC_START):
 *        a. Check if RCAC was running for this pdev,
 *           - If yes, transition to INIT and post EV_RCAC_START event.
 *           - If no, ignore.
 *
 *   Note: EV_ADFS_RADAR works same as EV_RCAC_START event right now, but
 *         will change in future, where, based on user preference, either
 *         a new RCAC channel will be picked (requiring the transition to
 *         INIT like present), or RCAC will be restarted on the same channel.
 *
 * EV_RCAC_DONE:
 *   Posted from host RCAC timer completion and is handled in RUNNING.
 *   1. RUNNING handler:
 *      a. mark RCAC done and transition to COMPLETE.
 *
 * Epilogue:
 *   Rolling CAC state machine is for the entire psoc and since the
 *   agile detector can run for one pdev at a time, sharing of resource is
 *   required.
 *   In case of ETSI preCAC, sharing was done in a round robin fashion where
 *   each pdev runs ADFS for it's channels alternatively. However, in RCAC, the
 *   CAC period is not defined is continuous till the next channel change.
 *
 *   Hence ADFS detector is shared as follows:
 *   1. First come first serve: the pdev that is brought up first, i.e, for
 *      the first vdev response, an RCAC_START is posted and this pdev will
 *      hold the agile detector and run RCAC till it is stopped.
 *   2. Stopping the RCAC can be either by disabling user config "rcac_en 0"
 *      or by bringing down all vaps, or if no channel is available.
 *   3. Once RCAC is stopped for a pdev, it can be started in the other pdev
 *      by restarting it's vap (i.e. a vdev response).
 *
 *   A working sequence of RCAC is as follows:
 *     - Consider that the channel configured during bring up is 52HT80.
 *       1. The First VAP's vdev_start_resp posts an event EV_RCAC_START to the
 *          RCAC state machine.
 *       2. The RCAC state machine which is in INIT state (default) receives the
 *          event, picks a channel to do rolling CAC on, e.g. channel 100HT80.
 *          The SM is then transitioned to RUNNING state.
 *       3. In the entry of RUNNING state, a host timer is started and agile
 *          cfg cmd to FW is sent.
 *       4. When the HOST timer expires, it posts the EV_RCAC_DONE event to
 *          the state machine.
 *       5. EV_RCAC_DONE event received in RUNNING state, transitions the SM
 *          to COMPLETE.
 *       6. In the entry of COMPLETE, the RCAC channel is marked as CAC done
 *          in the precac tree.
 *       7. If radar is detected on primary channel, the new channel is the
 *          RCAC channel (100HT80) which does not require CAC if the preCAC
 *          tree is marked as CAC done.
 *          Before sending vdev_start, an EV_RCAC_STOP is posted
 *          which moves the SM to INIT state clearing all the params and
 *          bringing down the agile detector.
 *          (CAC decisions are taken before).
 *       8. After vdev_resp, another EV_RCAC_START is sent to restart the
 *          RCAC SM with a new RCAC channel if available.
 *
 *   A future enhancement will be triggering RCAC_START at user level.
 */

/**
 * dfs_agile_set_curr_state() - API to set the current state of Agile SM.
 * @dfs_soc_obj: Pointer to DFS soc private object.
 * @state: value of current state.
 *
 * Return: void.
 */
static void dfs_agile_set_curr_state(struct dfs_soc_priv_obj *dfs_soc_obj,
				     enum dfs_agile_sm_state state)
{
	if (state < DFS_AGILE_S_MAX) {
		dfs_soc_obj->dfs_agile_sm_cur_state = state;
	} else {
		dfs_err(NULL, WLAN_DEBUG_DFS_ALWAYS,
			"DFS RCAC state (%d) is invalid", state);
		QDF_BUG(0);
	}
}

/**
 * dfs_agile_get_curr_state() - API to get current state of Agile SM.
 * @dfs_soc_obj: Pointer to DFS soc private object.
 *
 * Return: current state enum of type, dfs_rcac_sm_state.
 */
static enum dfs_agile_sm_state
dfs_agile_get_curr_state(struct dfs_soc_priv_obj *dfs_soc_obj)
{
	return dfs_soc_obj->dfs_agile_sm_cur_state;
}

/**
 * dfs_rcac_sm_transition_to() - Wrapper API to transition the Agile SM state.
 * @dfs_soc_obj: Pointer to dfs soc private object that hold the SM handle.
 * @state: State to which the SM is transitioning to.
 *
 * Return: void.
 */
static void dfs_agile_sm_transition_to(struct dfs_soc_priv_obj *dfs_soc_obj,
				       enum dfs_agile_sm_state state)
{
	wlan_sm_transition_to(dfs_soc_obj->dfs_agile_sm_hdl, state);
}

/**
 * dfs_agile_sm_deliver_event() - API to post events to Agile SM.
 * @dfs_soc_obj: Pointer to dfs soc private object.
 * @event: Event to be posted to the RCAC SM.
 * @event_data_len: Length of event data.
 * @event_data: Pointer to event data.
 *
 * Return: QDF_STATUS_SUCCESS on handling the event, else failure.
 *
 * Note: This version of event posting API is not under lock and hence
 * should only be called for posting events within the SM and not be
 * under a dispatcher API without a lock.
 */
static
QDF_STATUS dfs_agile_sm_deliver_event(struct dfs_soc_priv_obj *dfs_soc_obj,
				      enum dfs_agile_sm_evt event,
				      uint16_t event_data_len,
				      void *event_data)
{
	return wlan_sm_dispatch(dfs_soc_obj->dfs_agile_sm_hdl,
				event,
				event_data_len,
				event_data);
}

#ifdef QCA_SUPPORT_ADFS_RCAC
/* dfs_start_agile_rcac_timer() - Start host agile RCAC timer.
 *
 * @dfs: Pointer to struct wlan_dfs.
 */
void dfs_start_agile_rcac_timer(struct wlan_dfs *dfs)
{
	struct dfs_soc_priv_obj *dfs_soc_obj = dfs->dfs_soc_obj;
	uint32_t rcac_timeout = MIN_RCAC_DURATION;

	dfs_info(dfs, WLAN_DEBUG_DFS_ALWAYS,
		 "Host RCAC timeout = %d ms", rcac_timeout);

	qdf_hrtimer_start(&dfs_soc_obj->dfs_rcac_timer,
			  qdf_time_ms_to_ktime(rcac_timeout),
			  QDF_HRTIMER_MODE_REL);
}


/* dfs_stop_agile_rcac_timer() - Cancel the RCAC timer.
 *
 * @dfs: Pointer to struct wlan_dfs.
 */
void dfs_stop_agile_rcac_timer(struct wlan_dfs *dfs)
{
	struct dfs_soc_priv_obj *dfs_soc_obj;

	dfs_soc_obj = dfs->dfs_soc_obj;
	qdf_hrtimer_cancel(&dfs_soc_obj->dfs_rcac_timer);
}


/**
 * dfs_abort_agile_rcac() - Send abort Agile RCAC to F/W.
 * @dfs: Pointer to struct wlan_dfs.
 */
static void dfs_abort_agile_rcac(struct wlan_dfs *dfs)
{

	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_dfs_tx_ops *dfs_tx_ops;

	psoc = wlan_pdev_get_psoc(dfs->dfs_pdev_obj);
	dfs_tx_ops = wlan_psoc_get_dfs_txops(psoc);

	dfs_stop_agile_rcac_timer(dfs);
	if (dfs_tx_ops && dfs_tx_ops->dfs_ocac_abort_cmd)
		dfs_tx_ops->dfs_ocac_abort_cmd(dfs->dfs_pdev_obj);

	qdf_mem_zero(&dfs->dfs_rcac_param, sizeof(struct dfs_rcac_params));
	dfs->dfs_soc_obj->cur_agile_dfs_index = DFS_PSOC_NO_IDX;
	dfs_agile_cleanup_rcac(dfs);
}
#else
static inline void dfs_abort_agile_rcac(struct wlan_dfs *dfs)
{
}
#endif

/* dfs_abort_agile_precac() - Reset parameters of wlan_dfs and send abort
 * to F/W.
 * @dfs: Pointer to struct wlan_dfs.
 */
static void dfs_abort_agile_precac(struct wlan_dfs *dfs)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_lmac_if_dfs_tx_ops *dfs_tx_ops;

	psoc = wlan_pdev_get_psoc(dfs->dfs_pdev_obj);
	dfs_tx_ops = wlan_psoc_get_dfs_txops(psoc);

	dfs_cancel_precac_timer(dfs);
	dfs->dfs_soc_obj->cur_agile_dfs_index = DFS_PSOC_NO_IDX;
	dfs_agile_precac_cleanup(dfs);
	/*Send the abort to F/W as well */
	if (dfs_tx_ops && dfs_tx_ops->dfs_ocac_abort_cmd)
		dfs_tx_ops->dfs_ocac_abort_cmd(dfs->dfs_pdev_obj);
}

/**
 * dfs_agile_state_init_entry() - Entry API for INIT state
 * @ctx: DFS SoC private object
 *
 * API to perform operations on moving to INIT state
 *
 * Return: void
 */
static void dfs_agile_state_init_entry(void *ctx)
{
	struct dfs_soc_priv_obj *dfs_soc = (struct dfs_soc_priv_obj *)ctx;

	dfs_agile_set_curr_state(dfs_soc, DFS_AGILE_S_INIT);
}

/**
 * dfs_agile_state_init_exit() - Exit API for INIT state
 * @ctx: DFS SoC private object
 *
 * API to perform operations on moving out of INIT state
 *
 * Return: void
 */
static void dfs_agile_state_init_exit(void *ctx)
{
	/* NO OPS */
}

/**
 * dfs_init_agile_start_evt_handler() - Init state start event handler.
 * @dfs: Instance of wlan_dfs structure.
 * @dfs_soc: DFS SoC private object
 *
 * Return : True if PreCAC/RCAC chan is found.
 */
static bool  dfs_init_agile_start_evt_handler(struct wlan_dfs *dfs,
					      struct dfs_soc_priv_obj *dfs_soc)
{
	bool is_chan_found = false;

	/*For RCAC */
	if (dfs_is_agile_rcac_enabled(dfs)) {
		/* Check if feature is enabled for this DFS and if RCAC channel
		 * is valid, if those are true, send appropriate WMIs to FW
		 * and only then transition to the state as follows.
		 */
		dfs_soc->cur_agile_dfs_index = dfs->dfs_psoc_idx;
		dfs_prepare_agile_rcac_channel(dfs, &is_chan_found);
	}
	/*For PreCAC */
	else if (dfs_is_agile_precac_enabled(dfs)) {
		if (!dfs_soc->precac_state_started &&
		    !dfs_soc->dfs_precac_timer_running) {
			dfs_soc->precac_state_started = true;
			dfs_prepare_agile_precac_chan(dfs, &is_chan_found);
		}
	}

	return is_chan_found;
}

/**
 * dfs_agile_state_init_event() - INIT State event handler
 * @ctx: DFS SoC private object
 * @event: Event posted to the SM.
 * @event_data_len: Length of event data.
 * @event_data: Pointer to event data.
 *
 * API to handle events in INIT state
 *
 * Return: TRUE:  on handling event
 *         FALSE: on ignoring the event
 */
static bool dfs_agile_state_init_event(void *ctx,
				      uint16_t event,
				      uint16_t event_data_len,
				      void *event_data)
{
	struct dfs_soc_priv_obj *dfs_soc = (struct dfs_soc_priv_obj *)ctx;
	bool status;
	struct wlan_dfs *dfs;
	bool is_chan_found;

	if (!event_data)
		return false;

	dfs = (struct wlan_dfs *)event_data;

	switch (event) {
	case DFS_AGILE_SM_EV_AGILE_START:

		is_chan_found = dfs_init_agile_start_evt_handler(dfs,
								 dfs_soc);
		if (is_chan_found) {
			dfs_agile_sm_transition_to(dfs_soc,
						   DFS_AGILE_S_RUNNING);
		} else {
			/*
			 * This happens when there is no preCAC chan
			 * in any of the radios
			 */
			dfs_soc->cur_agile_dfs_index = DFS_PSOC_NO_IDX;
			dfs_agile_precac_cleanup(dfs);
			/* Cleanup and wait */
		}

		status = true;
		break;
	default:
		status = false;
		break;
	}

	return status;
}

/**
 * dfs_agile_state_running_entry() - Entry API for running state
 * @ctx: DFS SoC private object
 *
 * API to perform operations on moving to running state
 *
 * Return: void
 */
static void dfs_agile_state_running_entry(void *ctx)
{
	struct dfs_soc_priv_obj *dfs_soc = (struct dfs_soc_priv_obj *)ctx;
	struct wlan_dfs *dfs =
		dfs_soc->dfs_priv[dfs_soc->cur_agile_dfs_index].dfs;

	dfs_agile_set_curr_state(dfs_soc, DFS_AGILE_S_RUNNING);

	qdf_mem_zero(&dfs->adfs_completion_status, sizeof(struct adfs_completion_params));

	/* RCAC */
	if (dfs_is_agile_rcac_enabled(dfs)) {
		dfs_soc->ocac_status = OCAC_RESET;
		dfs_start_agile_rcac_timer(dfs);
		dfs_start_agile_engine(dfs);
	}
}

/**
 * dfs_agile_state_running_exit() - Exit API for RUNNING state
 * @ctx: DFS SoC private object
 *
 * API to perform operations on moving out of RUNNING state
 *
 * Return: void
 */
static void dfs_agile_state_running_exit(void *ctx)
{
	/* NO OPS */
}

/**
 * dfs_agile_state_running_event() - RUNNING State event handler
 * @ctx: DFS SoC private object
 * @event: Event posted to the SM.
 * @event_data_len: Length of event data.
 * @event_data: Pointer to event data.
 *
 * API to handle events in RUNNING state
 *
 * Return: TRUE:  on handling event
 *         FALSE: on ignoring the event
 */
static bool dfs_agile_state_running_event(void *ctx,
					 uint16_t event,
					 uint16_t event_data_len,
					 void *event_data)
{
	struct dfs_soc_priv_obj *dfs_soc = (struct dfs_soc_priv_obj *)ctx;
	bool status;
	struct wlan_dfs *dfs;
	bool is_cac_done_on_des_chan;

	if (!event_data)
		return false;

	dfs = (struct wlan_dfs *)event_data;

	if (dfs->dfs_psoc_idx != dfs_soc->cur_agile_dfs_index)
		return false;

	switch (event) {
	case DFS_AGILE_SM_EV_ADFS_RADAR:
		/* After radar is found on the Agile channel we need to find
		 * a new channel and then start Agile CAC on that.
		 * On receiving the "DFS_AGILE_SM_EV_ADFS_RADAR_FOUND" if
		 * we change the state from [RUNNING] -> [RUNNING] then
		 * [RUNNING] should handle case in which a channel is not found
		 * and bring the state machine back to INIT.
		 * Instead we move the state to INIT and post the event
		 * "DFS_AGILE_SM_EV_AGILE_START" so INIT handles the case of
		 * channel not found and stay in that state.
		 * Abort the existing RCAC and restart from INIT state.
		 */
		if (dfs_is_agile_rcac_enabled(dfs))
			dfs_abort_agile_rcac(dfs);
		else if (dfs_is_agile_precac_enabled(dfs))
			dfs_abort_agile_precac(dfs);

		dfs_agile_sm_transition_to(dfs_soc, DFS_AGILE_S_INIT);
		dfs_agile_sm_deliver_event(dfs_soc,
					   DFS_AGILE_SM_EV_AGILE_START,
					   event_data_len,
					   event_data);

		status = true;
		break;
	case DFS_AGILE_SM_EV_AGILE_STOP:
		if (dfs_is_rcac_domain(dfs))
			dfs_abort_agile_rcac(dfs);
		else if (dfs_is_precac_domain(dfs))
			dfs_abort_agile_precac(dfs);

		dfs_agile_sm_transition_to(dfs_soc, DFS_AGILE_S_INIT);
		status = true;
		break;
	case DFS_AGILE_SM_EV_AGILE_DONE:
		/* When the FW sends a delayed OCAC completion status, Host
		 * might have changed the precac channel already before an
		 * OCAC completion event is received. So the OCAC completion
		 * status should be validated if it is on the currently
		 * configured agile channel.
		 */

		/* Assume the previous agile channel was 64 (20Mhz) and
		 * current agile channel is 100(20Mhz), if the event from the
		 * FW is for previously configured agile channel 64(20Mhz)
		 * then Host ignores the event.
		 */
		if (!dfs_is_ocac_complete_event_for_cur_agile_chan(dfs)) {
			dfs_debug(NULL, WLAN_DEBUG_DFS_ALWAYS,
				  "OCAC completion event is received on a "
				  "different channel %d %d that is not the "
				  "current Agile channel %d",
				  dfs->adfs_completion_status.center_freq1,
				  dfs->adfs_completion_status.center_freq2,
				  dfs->dfs_agile_precac_freq_mhz);
			return true;
		}

		if (dfs_is_agile_precac_enabled(dfs)) {
			if (dfs_soc->ocac_status == OCAC_SUCCESS) {
				dfs_soc->ocac_status = OCAC_RESET;
				dfs_mark_adfs_chan_as_cac_done(dfs);
			}
			dfs_agile_sm_transition_to(dfs_soc, DFS_AGILE_S_INIT);
			/*
			 *  When preCAC is completed on a channel. The event
			 *  DFS_AGILE_SM_EV_AGILE_DONE is received and AGILE SM
			 *  moved to INIT STATE. Next it will try to continue
			 *  preCAC on other channels.
			 *
			 *  If BW_EXPAND feature is enabled, Check if there is
			 *  a possibility of BW Expansion, by calling API
			 *  dfs_bwexpand_try_jumping_to_target_subchan.
			 *
			 *  Example: User configured Channel and BW is stored
			 *  in DFS structure.
			 *  User configured channel - dfs_bw_expand_target_freq
			 *  User configured mode    - dfs_bw_expand_des_mode
			 *
			 *  If Bandwidth Expansion is possible, then AP will
			 *  move to the Channel with Expanded Bandwidth using
			 *  mlme_precac_chan_change_csa_for_freq.
			 *
			 *  The API will send WLAN_VDEV_SM_EV_CSA_RESTART event
			 *  So there is no need to send
			 *  DFS_AGILE_SM_EV_AGILE_START.
			 *
			 *  Therefore, Once Bandwidth Expansion is completed.
			 *  Return the status - True.
			 */
			if (dfs_bwexpand_try_jumping_to_target_subchan(dfs)) {
				dfs_agile_precac_cleanup(dfs);
				status = true;
				break;
			}
			dfs_agile_precac_cleanup(dfs);
			is_cac_done_on_des_chan =
				dfs_precac_check_home_chan_change(dfs);
			if (!is_cac_done_on_des_chan) {
				dfs_agile_sm_deliver_event(dfs_soc,
					DFS_AGILE_SM_EV_AGILE_START,
					event_data_len,
					event_data);
			}
		} else if (dfs_is_agile_rcac_enabled(dfs)) {
			if (dfs_soc->ocac_status == OCAC_SUCCESS) {
				dfs_agile_sm_transition_to(dfs_soc,
						DFS_AGILE_S_COMPLETE);
			} else if (dfs_soc->ocac_status == OCAC_CANCEL) {
				/* OCAC completion with status OCAC_CANCEL
				 * will be received from FW when rCAC channel
				 * config fails. Handle this case by clearing
				 * the status and restarting rCAC
				 */
				dfs_soc->ocac_status = OCAC_RESET;
				dfs_stop_agile_rcac_timer(dfs);
				dfs_agile_cleanup_rcac(dfs);
				dfs_agile_sm_transition_to(dfs_soc,
					DFS_AGILE_S_INIT);
				dfs_agile_sm_deliver_event(dfs_soc,
					DFS_AGILE_SM_EV_AGILE_START,
					event_data_len,
					event_data);
			} else {
				dfs_debug(NULL, WLAN_DEBUG_DFS_ALWAYS,
					  "Invalid OCAC completion status %d",
					  dfs_soc->ocac_status);
			}
		}
		status = true;
		break;
	default:
		status = false;
		break;
	}

	return status;
}

/**
 * dfs_agile_state_complete_entry() - Entry API for complete state
 * @ctx: DFS SoC private object
 *
 * API to perform operations on moving to complete state
 *
 * Return: void
 */
static void dfs_agile_state_complete_entry(void *ctx)
{
	struct dfs_soc_priv_obj *dfs_soc_obj = (struct dfs_soc_priv_obj *)ctx;
	struct wlan_dfs *dfs;

	dfs_agile_set_curr_state(dfs_soc_obj, DFS_AGILE_S_COMPLETE);

	if (!(dfs_soc_obj->cur_agile_dfs_index < WLAN_UMAC_MAX_PDEVS))
		return;

	dfs = dfs_soc_obj->dfs_priv[dfs_soc_obj->cur_agile_dfs_index].dfs;

	/* Mark the RCAC channel as CAC done. */
	dfs_mark_adfs_chan_as_cac_done(dfs);

	/* Check if BW_Expansion is already done and CSA sent internally.
	 * If Yes, then Agile is considered to completed.
	 */
	if (dfs_bwexpand_try_jumping_to_target_subchan(dfs))
		return;
	/*
	 * Check if rcac is done on preffered channel.
	 * If so, change channel from intermediate channel to preffered chan.
	 */
	dfs_precac_check_home_chan_change(dfs);
	/*
	 * In case of preCAC interCAC, if the above home channel change fails,
	 * we explicitly do agile start (DFS_AGILE_SM_EV_AGILE_START) as we need
	 * to pick the next agile channel. However, in case of RCAC, if the
	 * above home channel change fails, the agile continues in the current
	 * RCAC channel therefore explicit agile start
	 * (DFS_AGILE_SM_EV_AGILE_START) is not required. Refer to function
	 * "dfs_agile_state_running_event"
	 */
}

/**
 * dfs_agile_state_complete_exit() - Exit API for complete state
 * @ctx: DFS SoC private object
 *
 * API to perform operations on moving out of complete state
 *
 * Return: void
 */
static void dfs_agile_state_complete_exit(void *ctx)
{
	/* NO OPs. */
}

/**
 * dfs_agile_state_complete_event() - COMPLETE State event handler
 * @ctx: DFS SoC private object
 * @event: Event posted to the SM.
 * @event_data_len: Length of event data.
 * @event_data: Pointer to event data.
 *
 * API to handle events in COMPLETE state
 *
 * Return: TRUE:  on handling event
 *         FALSE: on ignoring the event
 */
static bool dfs_agile_state_complete_event(void *ctx,
					  uint16_t event,
					  uint16_t event_data_len,
					  void *event_data)
{
	struct dfs_soc_priv_obj *dfs_soc = (struct dfs_soc_priv_obj *)ctx;
	bool status;
	struct wlan_dfs *dfs;

	if (!event_data)
		return false;

	dfs = (struct wlan_dfs *)event_data;

	if (dfs->dfs_psoc_idx != dfs_soc->cur_agile_dfs_index)
		return false;

	switch (event) {
	case DFS_AGILE_SM_EV_ADFS_RADAR:
		/* Reset the RCAC done state for this RCAC chan of this dfs.
		 * Unmark the channels for RCAC done before calling abort API as
		 * the abort API invalidates the cur_agile_dfs_index.
		 */
		dfs_unmark_rcac_done(dfs);
		/* Abort the existing RCAC and restart from INIT state. */
		dfs_abort_agile_rcac(dfs);
		dfs_agile_sm_transition_to(dfs_soc, DFS_AGILE_S_INIT);
		dfs_agile_sm_deliver_event(dfs_soc,
					  DFS_AGILE_SM_EV_AGILE_START,
					  event_data_len,
					  event_data);
		status = true;
		break;
	case DFS_AGILE_SM_EV_AGILE_STOP:
		/* Reset the RCAC done state for this RCAC chan of this dfs.
		 * Unmark the channels for RCAC done before calling abort API as
		 * the abort API invalidates the cur_agile_dfs_index.
		 */
		dfs_unmark_rcac_done(dfs);
		dfs_abort_agile_rcac(dfs);
		dfs_agile_sm_transition_to(dfs_soc, DFS_AGILE_S_INIT);
		status = true;
		break;
	default:
		status = false;
		break;
	}

	return status;
}

static struct wlan_sm_state_info dfs_agile_sm_info[] = {
	{
		(uint8_t)DFS_AGILE_S_INIT,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		false,
		"INIT",
		dfs_agile_state_init_entry,
		dfs_agile_state_init_exit,
		dfs_agile_state_init_event
	},
	{
		(uint8_t)DFS_AGILE_S_RUNNING,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		false,
		"RUNNING",
		dfs_agile_state_running_entry,
		dfs_agile_state_running_exit,
		dfs_agile_state_running_event
	},
	{
		(uint8_t)DFS_AGILE_S_COMPLETE,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		false,
		"COMPLETE",
		dfs_agile_state_complete_entry,
		dfs_agile_state_complete_exit,
		dfs_agile_state_complete_event
	},
};

static const char *dfs_agile_sm_event_names[] = {
	"EV_AGILE_START",
	"EV_AGILE_STOP",
	"EV_AGILE_DONE",
	"EV_ADFS_RADAR_FOUND",
};

/**
 * dfs_agile_sm_print_state() - API to log the current state.
 * @dfs_soc_obj: Pointer to dfs soc private object.
 *
 * Return: void.
 */
static void dfs_agile_sm_print_state(struct dfs_soc_priv_obj *dfs_soc_obj)
{
	enum dfs_agile_sm_state state;

	state = dfs_agile_get_curr_state(dfs_soc_obj);
	if (!(state < DFS_AGILE_S_MAX))
		return;

	dfs_debug(NULL, WLAN_DEBUG_DFS_AGILE, "->[%s] %s",
		  dfs_soc_obj->dfs_agile_sm_hdl->name,
		  dfs_agile_sm_info[state].name);
}

/**
 * dfs_agile_sm_print_state_event() - API to log the current state and event
 *                                   received.
 * @dfs_soc_obj: Pointer to dfs soc private object.
 * @event: Event posted to RCAC SM.
 *
 * Return: void.
 */
static void dfs_agile_sm_print_state_event(struct dfs_soc_priv_obj *dfs_soc_obj,
					  enum dfs_agile_sm_evt event)
{
	enum dfs_agile_sm_state state;

	state = dfs_agile_get_curr_state(dfs_soc_obj);
	if (!(state < DFS_AGILE_S_MAX))
		return;

	dfs_debug(NULL, WLAN_DEBUG_DFS_AGILE, "[%s]%s, %s",
		  dfs_soc_obj->dfs_agile_sm_hdl->name,
		  dfs_agile_sm_info[state].name,
		  dfs_agile_sm_event_names[event]);
}

QDF_STATUS dfs_agile_sm_deliver_evt(struct dfs_soc_priv_obj *dfs_soc_obj,
				    enum dfs_agile_sm_evt event,
				    uint16_t event_data_len,
				    void *event_data)
{
	enum dfs_agile_sm_state old_state, new_state;
	QDF_STATUS status;

	DFS_AGILE_SM_SPIN_LOCK(dfs_soc_obj);
	old_state = dfs_agile_get_curr_state(dfs_soc_obj);

	/* Print current state and event received */
	dfs_agile_sm_print_state_event(dfs_soc_obj, event);

	status = dfs_agile_sm_deliver_event(dfs_soc_obj, event,
					   event_data_len, event_data);

	new_state = dfs_agile_get_curr_state(dfs_soc_obj);

	/* Print new state after event if transition happens */
	if (old_state != new_state)
		dfs_agile_sm_print_state(dfs_soc_obj);
	DFS_AGILE_SM_SPIN_UNLOCK(dfs_soc_obj);

	return status;
}

QDF_STATUS dfs_agile_sm_create(struct dfs_soc_priv_obj *dfs_soc_obj)
{
	struct wlan_sm *sm;

	sm = wlan_sm_create("DFS_AGILE", dfs_soc_obj,
			    DFS_AGILE_S_INIT,
			    dfs_agile_sm_info,
			    QDF_ARRAY_SIZE(dfs_agile_sm_info),
			    dfs_agile_sm_event_names,
			    QDF_ARRAY_SIZE(dfs_agile_sm_event_names));
	if (!sm) {
		qdf_err("DFS AGILE SM allocation failed");
		return QDF_STATUS_E_FAILURE;
	}
	dfs_soc_obj->dfs_agile_sm_hdl = sm;

	qdf_spinlock_create(&dfs_soc_obj->dfs_agile_sm_lock);

	/* Initialize the RCAC DFS index to default (no index). */
	dfs_soc_obj->cur_agile_dfs_index = DFS_PSOC_NO_IDX;
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dfs_agile_sm_destroy(struct dfs_soc_priv_obj *dfs_soc_obj)
{
	wlan_sm_delete(dfs_soc_obj->dfs_agile_sm_hdl);
	qdf_spinlock_destroy(&dfs_soc_obj->dfs_agile_sm_lock);

	return QDF_STATUS_SUCCESS;
}

#ifdef QCA_SUPPORT_ADFS_RCAC
QDF_STATUS dfs_set_rcac_enable(struct wlan_dfs *dfs, bool rcac_en)
{
	if (rcac_en == dfs->dfs_agile_rcac_ucfg) {
		dfs_info(dfs, WLAN_DEBUG_DFS_ALWAYS,
			 "Rolling CAC: %d is already configured", rcac_en);
		return QDF_STATUS_SUCCESS;
	}
	dfs->dfs_agile_rcac_ucfg = rcac_en;

	/* RCAC config is changed. Reset the preCAC tree. */
	dfs_reset_precac_lists(dfs);

	if (!rcac_en) {
		dfs_agile_sm_deliver_evt(dfs->dfs_soc_obj,
					DFS_AGILE_SM_EV_AGILE_STOP,
					0,
					(void *)dfs);
	}
	dfs_info(dfs, WLAN_DEBUG_DFS_ALWAYS, "rolling cac is %d", rcac_en);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dfs_get_rcac_enable(struct wlan_dfs *dfs, bool *rcacen)
{
	*rcacen = dfs->dfs_agile_rcac_ucfg;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dfs_set_rcac_freq(struct wlan_dfs *dfs, qdf_freq_t rcac_freq)
{
	if (wlan_reg_is_5ghz_ch_freq(rcac_freq))
		dfs->dfs_agile_rcac_freq_ucfg = rcac_freq;
	else
		dfs->dfs_agile_rcac_freq_ucfg = 0;

	dfs_info(dfs, WLAN_DEBUG_DFS_ALWAYS,  "rolling cac freq %d",
		 dfs->dfs_agile_rcac_freq_ucfg);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dfs_get_rcac_freq(struct wlan_dfs *dfs, qdf_freq_t *rcac_freq)
{
	*rcac_freq = dfs->dfs_agile_rcac_freq_ucfg;

	return QDF_STATUS_SUCCESS;
}

/*
 * Rolling CAC Timer timeout function. Following actions are done
 * on timer expiry:
 * Timer running flag is cleared.
 * If the rolling CAC state is completed, the RCAC freq and its sub-channels
 * are marked as 'CAC Done' in the preCAC tree.
 */
static enum qdf_hrtimer_restart_status
dfs_rcac_timeout(qdf_hrtimer_data_t *arg)
{
	struct wlan_dfs *dfs;
	struct dfs_soc_priv_obj *dfs_soc_obj;

	dfs_soc_obj = container_of(arg,
				   struct dfs_soc_priv_obj,
				   dfs_rcac_timer);
	dfs = dfs_soc_obj->dfs_priv[dfs_soc_obj->cur_agile_dfs_index].dfs;

	dfs_soc_obj->ocac_status = OCAC_SUCCESS;
	dfs_fill_adfs_completion_params(dfs, OCAC_SUCCESS);
	dfs_agile_sm_deliver_evt(dfs_soc_obj,
				DFS_AGILE_SM_EV_AGILE_DONE,
				0,
				(void *)dfs);

	return QDF_HRTIMER_NORESTART;
}

void dfs_rcac_timer_init(struct dfs_soc_priv_obj *dfs_soc_obj)
{

	qdf_hrtimer_init(&dfs_soc_obj->dfs_rcac_timer,
			 dfs_rcac_timeout,
			 QDF_CLOCK_MONOTONIC,
			 QDF_HRTIMER_MODE_REL,
			 QDF_CONTEXT_TASKLET);
}

void dfs_rcac_timer_deinit(struct dfs_soc_priv_obj *dfs_soc_obj)
{
	qdf_hrtimer_kill(&dfs_soc_obj->dfs_rcac_timer);
}

/* dfs_prepare_agile_rcac_channel() - Find a valid Rolling CAC channel if
 *                                    available.
 *
 * @dfs: Pointer to struct wlan_dfs.
 * @is_rcac_chan_available: Flag to indicate if a valid RCAC channel is
 *                          available.
 */
void dfs_prepare_agile_rcac_channel(struct wlan_dfs *dfs,
				    bool *is_rcac_chan_available)
{
	qdf_freq_t rcac_ch_freq = 0;

	/* Find out a valid rcac_ch_freq */
	dfs_set_agilecac_chan_for_freq(dfs, &rcac_ch_freq, 0, 0);

	/* If RCAC channel is available, the caller will start the timer and
	 * send RCAC config to FW. If channel not available, the caller takes
	 * care of sending RCAC abort and moving SM to INIT, resetting the RCAC
	 * variables.
	 */
	*is_rcac_chan_available = rcac_ch_freq ? true : false;
	dfs_debug(dfs, WLAN_DEBUG_DFS_AGILE, "Chosen rcac channel: %d",
		  rcac_ch_freq);
}
#endif

/*------------------- PUNCTURING SM ----------------------------------*/
#if defined(QCA_DFS_BW_PUNCTURE)
#define N_MAX_PUNC_SM 2

#define DFS_IS_PUNC_CHAN_WEATHER_RADAR(_n) (((_n) >= 5590) && ((_n) <= 5650))
#define DFS_IS_PUNC_CHAN_OVERLAP_WEATHER_RADAR(_n) (((_n) >= 5570) && ((_n) <= 5670))

void dfs_create_punc_sm(struct wlan_dfs *dfs)
{
	uint8_t i;

	for (i = 0; i < N_MAX_PUNC_SM; i++) {
		struct dfs_punc_obj *dfs_punc_obj = &dfs->dfs_punc_lst.dfs_punc_arr[i];

		dfs_punc_obj->dfs = dfs;
		dfs_punc_sm_create(dfs_punc_obj);
	}
}

void dfs_destroy_punc_sm(struct wlan_dfs *dfs)
{
	uint8_t i;

	for (i = 0; i < N_MAX_PUNC_SM; i++) {
		struct dfs_punc_obj *dfs_punc_obj = &dfs->dfs_punc_lst.dfs_punc_arr[i];

		dfs_punc_sm_destroy(dfs_punc_obj);
	}
}

void dfs_punc_sm_stop(struct wlan_dfs *dfs,
		      uint8_t indx,
		      struct dfs_punc_obj *dfs_punc_arr)
{
	dfs_punc_arr->punc_low_freq = 0;
	dfs_punc_arr->punc_high_freq = 0;
	dfs_cancel_punc_cac_timer(dfs_punc_arr);
	dfs_punc_arr->dfs_is_unpunctured = true;
	utils_dfs_puncturing_sm_deliver_evt(dfs->dfs_pdev_obj, indx, DFS_PUNC_SM_EV_STOP);
}

static inline
bool is_punc_slot_empty(uint8_t indx, struct dfs_punc_unpunc *active_dfs_sm)
{
	qdf_freq_t punc_low_freq =
	    active_dfs_sm->dfs_punc_arr[indx].punc_low_freq;
	qdf_freq_t punc_high_freq =
	    active_dfs_sm->dfs_punc_arr[indx].punc_high_freq;

	return (!punc_low_freq && !punc_high_freq);
}

static void
dfs_process_punc_bitmap(struct wlan_dfs *dfs, uint16_t *punc_list, uint8_t n_punc_chans)
{
	uint8_t i;
	uint8_t indx;
	struct dfs_punc_unpunc *active_dfs_sm = &dfs->dfs_punc_lst;
	qdf_freq_t punc_low_freq = 0;
	qdf_freq_t punc_high_freq = 0;

	indx = 0;
	for (i = 0; i < N_MAX_PUNC_SM; i++) {
		if (is_punc_slot_empty(i, active_dfs_sm)) {
			indx = i;
			break;
		}
	}

	if (n_punc_chans == 1) {
		punc_low_freq = punc_list[0] - 10;
		punc_high_freq = punc_list[0] + 10;
	} else if (n_punc_chans == 2) {
		punc_low_freq = punc_list[0] - 10;
		punc_high_freq = punc_list[1] + 10;
	}
	active_dfs_sm->dfs_punc_arr[indx].punc_low_freq = punc_low_freq;
	active_dfs_sm->dfs_punc_arr[indx].punc_high_freq = punc_high_freq;
	active_dfs_sm->dfs_punc_arr[indx].dfs_is_unpunctured = false;
	dfs_debug(dfs, WLAN_DEBUG_DFS_PUNCTURING,
		  "Lower frequency of SM %d = %d\n Higher frequency of SM %d = %d\n",
		  indx, active_dfs_sm->dfs_punc_arr[indx].punc_low_freq, indx,
		  active_dfs_sm->dfs_punc_arr[indx].punc_high_freq);
}

/**
 * dfs_remove_static_punc_bitmap_for_320bw - Remove the 80BW puncture bitmap
 * for 320 BW case. This is used to generate puncture channel list after
 * radar event is received.
 * @dfs_curr_radar_bitmap: Puncture bitmap generated by radar.
 * @ch_width:              Current channel width.
 *
 * Return: Puncture radar bitmap.
 */
static uint16_t
dfs_remove_static_punc_bitmap_for_320bw(uint16_t dfs_curr_radar_bitmap,
					enum phy_ch_width ch_width)
{
	uint16_t left_punc = 0x000F;
	uint16_t right_punc = 0xF000;
	uint8_t no_of_bits = 16;
	uint16_t msb = 1 << (no_of_bits - 1);
	uint16_t temp;

	if (ch_width == CH_WIDTH_320MHZ) {
		temp = dfs_curr_radar_bitmap & msb;
		/* Left side punctured */
		if (temp == 0)
			dfs_curr_radar_bitmap ^= left_punc;
		/* Right side punctured */
		else if (temp == 0x8000)
			dfs_curr_radar_bitmap ^= right_punc;
	}

	return dfs_curr_radar_bitmap;
}

/**
 * dfs_generate_punc_chan_list - Generate puncture channel list based
 * on current radar bitmap.
 * @dfs:                   structure to wlan_dfs.
 * @ch_width:              Current channel width.
 * @dfs_curr_radar_bitmap: Puncture bitmap generated by radar.
 * @punc_list:             Pointer to puncture channel list.
 *
 * Return: No of punctured channels.
 */
static uint8_t
dfs_generate_punc_chan_list(struct wlan_dfs *dfs,
			    enum phy_ch_width ch_width,
			    uint16_t dfs_curr_radar_bitmap,
			    uint16_t *punc_list)
{
	uint8_t n_cur_channels;
	uint16_t cur_freq_list[MAX_20MHZ_SUBCHANS] = {0};
	uint8_t i = 0;
	uint8_t punc_indx = 0;

	n_cur_channels =
	    dfs_get_bonding_channel_without_seg_info_for_freq(dfs->dfs_curchan,
							      cur_freq_list);

	while (dfs_curr_radar_bitmap) {
		if (dfs_curr_radar_bitmap & 1) {
			punc_list[punc_indx] = cur_freq_list[i];
			punc_indx++;
		}
		i++;
		dfs_curr_radar_bitmap >>= 1;
	}

	if (!punc_indx)
		dfs_debug(dfs, WLAN_DEBUG_DFS_ALWAYS, "No puncture channels");

	return punc_indx;
}

/**
 * dfs_generate_punc_chan_list - Generate puncture channel list based
 * on current radar bitmap during VDEV restart.
 * @dfs:                   structure to wlan_dfs.
 * @ch_width:              Current channel width.
 * @dfs_curr_radar_bitmap: Puncture bitmap generated by radar.
 * @punc_list:             Pointer to puncture channel list.
 *
 * Return: No of punctured channels.
 */
static uint8_t
dfs_generate_punc_chan_list_on_radar_event(struct wlan_dfs *dfs,
					   enum phy_ch_width ch_width,
					   uint16_t dfs_curr_radar_bitmap,
					   uint16_t *punc_list)
{
	uint8_t punc_indx;

	dfs_curr_radar_bitmap =
		dfs_remove_static_punc_bitmap_for_320bw(dfs_curr_radar_bitmap,
							ch_width);

	punc_indx = dfs_generate_punc_chan_list(dfs,
						ch_width,
						dfs_curr_radar_bitmap,
						punc_list);
	return punc_indx;
}

void dfs_punc_sm_stop_all(struct wlan_dfs *dfs)
{
	uint8_t i;

	for (i = 0 ; i < N_MAX_PUNC_SM; i++) {
		struct dfs_punc_obj *dfs_punc_obj =
		    &dfs->dfs_punc_lst.dfs_punc_arr[i];

		dfs_punc_sm_stop(dfs, i, dfs_punc_obj);
	}
}

void dfs_handle_radar_puncturing(struct wlan_dfs *dfs,
				 uint16_t *dfs_radar_bitmap,
				 uint16_t *freq_list,
				 uint8_t num_channels,
				 bool *is_ignore_radar_puncture)
{
	uint16_t dfs_curr_radar_bitmap, dfs_total_radar_bitmap;
	uint16_t bw;
	enum phy_ch_width ch_width;

	dfs_curr_radar_bitmap = dfs_generate_radar_bitmap(dfs,
							  freq_list,
							  num_channels);

	if (!dfs_curr_radar_bitmap)
		return;

	/* Check if the radar infected channels are already punctured by the
	 * HOST. If already punctured by host then no need to add them to NOL
	 * and no need to puncture them again.
	 */
	*is_ignore_radar_puncture =
	    dfs_is_ignore_radar_for_punctured_chans(dfs, dfs_curr_radar_bitmap);
	if (*is_ignore_radar_puncture)
		return;

	dfs_total_radar_bitmap = dfs_curr_radar_bitmap |
					dfs->dfs_curchan->dfs_ch_punc_pattern;

	/* Check if user and dfs combined puncture is valid */
	bw = dfs_chan_to_ch_width(dfs->dfs_curchan);
	ch_width = wlan_reg_find_chwidth_from_bw(bw);
	*dfs_radar_bitmap =
	    wlan_reg_find_nearest_puncture_pattern(ch_width,
						   dfs_total_radar_bitmap);

	if (*dfs_radar_bitmap) {
		uint8_t n_punc_chans;
		uint16_t punc_list[N_MAX_PUNC_SM] = {0};
		/* We get the nearest pattern because, in case of 240 MHz, even
		 * for a single 20 MHz  NOL we want to add a 40 MHz punct object
		 */
		dfs_curr_radar_bitmap =
		    wlan_reg_find_nearest_puncture_pattern(ch_width,
							   dfs_curr_radar_bitmap);
		n_punc_chans =
		    dfs_generate_punc_chan_list(dfs,
						ch_width,
						dfs_curr_radar_bitmap,
						punc_list);

		if (!n_punc_chans)
			return;

		/* Init the puncture objects according to dfs_cur_radar_bitmap */
		dfs_process_punc_bitmap(dfs, punc_list, n_punc_chans);
	}
}

void dfs_handle_nol_puncture(struct wlan_dfs *dfs, qdf_freq_t nolfreq)
{
	uint8_t i;
	int8_t indx = -1;

	for (i = 0; i < N_MAX_PUNC_SM; i++) {
		if (is_punc_slot_empty(i, &dfs->dfs_punc_lst))
			continue;

		if ((dfs->dfs_punc_lst.dfs_punc_arr[i].punc_low_freq < nolfreq) &&
		    (nolfreq  < dfs->dfs_punc_lst.dfs_punc_arr[i].punc_high_freq)) {
			indx = i;
			break;
		}
	}

	if (indx != -1) {
		utils_dfs_puncturing_sm_deliver_evt(dfs->dfs_pdev_obj,
						    indx,
						    DFS_PUNC_SM_EV_NOL_EXPIRY);
	}
}

static inline
bool is_punc_obj_empty(struct dfs_punc_obj *dfs_punc_arr)
{
	if (!dfs_punc_arr)
		return true;
	return false;
}

static inline
bool is_found_in_cur_chan_punc_list(struct wlan_dfs *dfs,
				    struct dfs_punc_obj *dfs_punc_arr,
				    uint8_t n_punc_chans,
				    uint16_t *dfs_curchan_punc_list)
{
	bool is_found = false;
	uint8_t i;

	for (i = 0; i < n_punc_chans; i++) {
		qdf_freq_t punc_low_freq = dfs_punc_arr->punc_low_freq;
		qdf_freq_t punc_high_freq = dfs_punc_arr->punc_high_freq;

		if (!punc_low_freq || !punc_high_freq) {
		    dfs_info(dfs, WLAN_DEBUG_DFS_PUNCTURING,
			     "low: %d, high: %d punc freqs", punc_low_freq, punc_high_freq);
		    return false;
		}

		/* If the puncture channel is non-dfs , Skip the channel. */
		if (!wlan_reg_is_dfs_for_freq(dfs->dfs_pdev_obj, dfs_curchan_punc_list[i])) {
			dfs_info(dfs, WLAN_DEBUG_DFS, "ch=%d is not dfs, skip",
				 dfs_curchan_punc_list[i]);
			continue;
		}

		/* If current dfs channel is part of puncture channel list,
		 * Send the chan is found in current puncture channel list.
		 */
		if (dfs_curchan_punc_list[i] > punc_low_freq &&
		    dfs_curchan_punc_list[i] < punc_high_freq) {
			is_found = true;
			break;
		}
	}

	return is_found;
}

void dfs_handle_dfs_puncture_unpuncture(struct wlan_dfs *dfs)
{
	uint16_t dfs_punc_radar_bitmap = dfs->dfs_curchan->dfs_ch_punc_pattern;
	uint8_t n_punc_chans;
	uint16_t dfs_curchan_punc_list[MAX_20MHZ_SUBCHANS] = {0};
	enum phy_ch_width ch_width;
	uint16_t bw;
	uint8_t i = 0;

	bw = dfs_chan_to_ch_width(dfs->dfs_curchan);
	ch_width = wlan_reg_find_chwidth_from_bw(bw);

	/* dfs_generate_punc_chan_list_on_radar_event is called to generate
	 * the punctured channel list during VDEV restart. This is different
	 * from dfs_generate_punc_chan_list because for 320BW case, the
	 * radar bitmap is not yet calculated and we have to take care of the
	 * one 80 BW segment which will be always punctured.
	 */
	n_punc_chans = dfs_generate_punc_chan_list_on_radar_event(dfs,
								  ch_width,
								  dfs_punc_radar_bitmap,
								  dfs_curchan_punc_list);
	/* if there is no puncture bitmap or number of punctured channel is
	 * zero, then stop the puncture SM. This case happens when the current
	 * channel receives an invalid puncture pattern, (eg three radar case)
	 * AP moves to another channel. But the SMs already started running,
	 * so in VDEV start, we check if there is no puncture bitmap or
	 * punctured channels for the current channel, then send STOP event
	 * for all the puncture SM.
	 */
	if (!dfs_punc_radar_bitmap || !n_punc_chans) {
		struct dfs_punc_obj *dfs_punc_obj = &dfs->dfs_punc_lst.dfs_punc_arr[i];

		for (i = 0 ; i < N_MAX_PUNC_SM; i++) {
		    dfs_punc_sm_stop(dfs, i, dfs_punc_obj);
		}
		dfs_debug(dfs, WLAN_DEBUG_DFS_ALWAYS, "No radar puncturing found");
		return;
	}

	if (n_punc_chans > N_MAX_PUNC_SM) {
		dfs_err(dfs, WLAN_DEBUG_DFS_ALWAYS,  "Invalid punc pattern");
		return;
	}

	for (i = 0 ; i < N_MAX_PUNC_SM; i++) {
		bool is_found = false;
		struct dfs_punc_obj *dfs_punc_obj = &dfs->dfs_punc_lst.dfs_punc_arr[i];

		if (is_punc_obj_empty(dfs_punc_obj))
			continue;

		/* dfs_is_unpunctured should be checked before slot empty check.
		 * In two radar puncture case, after unpuncturing the first
		 * puncture channel, the first puncture SM becomes empty.
		 * So if we check the slot empty first, it skips the first
		 * punc SM and checks if the second SM is unpunctured or not.
		 * since the second SM is punctured, the dfs_is_unpunctured
		 * check fails, we unnecessarily send a radar event to second
		 * puncture SM.
		 */
		if (dfs_punc_obj->dfs_is_unpunctured)
			return;

		if (is_punc_slot_empty(i, &dfs->dfs_punc_lst)) {
			continue;
		}

		is_found = is_found_in_cur_chan_punc_list(dfs,
							  dfs_punc_obj,
							  n_punc_chans,
							  dfs_curchan_punc_list);

		if (is_found) {
			utils_dfs_puncturing_sm_deliver_evt(dfs->dfs_pdev_obj,
							    i,
							    DFS_PUNC_SM_EV_RADAR);
		} else {
		    dfs_punc_sm_stop(dfs, i, dfs_punc_obj);
		}
	}
}

static uint8_t dfs_generate_punc_list_from_sm(struct dfs_punc_obj *dfs_punc,
					      uint16_t *punc_lst_freq_list)
{
	qdf_freq_t punc_low_freq = dfs_punc->punc_low_freq;
	qdf_freq_t punc_high_freq = dfs_punc->punc_high_freq;
	qdf_freq_t punc_nol_freq = (punc_low_freq + punc_high_freq) / 2;
	uint8_t n_punc_channels = 0;

	if (punc_high_freq - punc_low_freq == 20) {
		punc_lst_freq_list[0] = punc_nol_freq;
		n_punc_channels = 1;
	} else if (punc_high_freq - punc_low_freq == 40) {
		punc_lst_freq_list[0] = punc_nol_freq - 10;
		punc_lst_freq_list[1] = punc_nol_freq + 10;
		n_punc_channels = 2;
	}

	return n_punc_channels;
}

static uint16_t dfs_generate_internal_radar_pattern(struct wlan_dfs *dfs)
{
	uint8_t i;
	uint16_t punc_lst_freq_list[N_MAX_PUNC_SM] = {0};
	uint16_t dfs_internal_radar_bitmap = 0;

	for (i = 0; i < N_MAX_PUNC_SM; i++) {
	    struct dfs_punc_obj *dfs_punc_obj = &dfs->dfs_punc_lst.dfs_punc_arr[i];
	    uint16_t dfs_temp_radar_bitmap;
	    uint8_t n_punc_channels;

	    if (is_punc_obj_empty(dfs_punc_obj) ||
		is_punc_slot_empty(i, &dfs->dfs_punc_lst))
		continue;

	    n_punc_channels =
		dfs_generate_punc_list_from_sm(dfs_punc_obj, punc_lst_freq_list);
	    dfs_temp_radar_bitmap = dfs_generate_radar_bitmap(dfs,
							      punc_lst_freq_list,
							      n_punc_channels);
	    dfs_internal_radar_bitmap |= dfs_temp_radar_bitmap;
	}

	return dfs_internal_radar_bitmap;
}

bool dfs_is_ignore_radar_for_punctured_chans(struct wlan_dfs *dfs,
					     uint16_t dfs_curr_radar_bitmap)
{
	uint16_t existing_pattern = dfs->dfs_curchan->dfs_ch_punc_pattern;
	uint16_t radar_punc_pattern = 0;
	uint16_t bw;
	enum phy_ch_width ch_width;
	uint8_t n_punc_chans;
	uint16_t internal_pattern = dfs_generate_internal_radar_pattern(dfs);
	uint16_t punc_list[MAX_20MHZ_SUBCHANS] = {0};

	if (internal_pattern)
		radar_punc_pattern = internal_pattern;

	if (radar_punc_pattern) {
		uint16_t intersect_radar_pattern;

		intersect_radar_pattern = radar_punc_pattern & dfs_curr_radar_bitmap;
		bw = dfs_chan_to_ch_width(dfs->dfs_curchan);
		ch_width = wlan_reg_find_chwidth_from_bw(bw);

		if (intersect_radar_pattern) {
			uint8_t i;

			n_punc_chans =
			    dfs_generate_punc_chan_list(dfs,
							ch_width,
							intersect_radar_pattern,
							punc_list);

			if (!n_punc_chans)
				return false;

			for (i = 0; i < N_MAX_PUNC_SM; i++) {
				bool is_found = false;
				struct dfs_punc_obj *dfs_punc_obj = &dfs->dfs_punc_lst.dfs_punc_arr[i];

				if (is_punc_obj_empty(dfs_punc_obj))
					continue;
				if (is_punc_slot_empty(i, &dfs->dfs_punc_lst))
				    continue;

				is_found =
				    is_found_in_cur_chan_punc_list(dfs,
								   dfs_punc_obj,
								   n_punc_chans,
								   punc_list);

				if (is_found) {
					utils_dfs_puncturing_sm_deliver_evt(dfs->dfs_pdev_obj,
									    i,
									    DFS_PUNC_SM_EV_RADAR);
				}
			}
		}
	}

	/* If new pattern is a subset of the existing pattern then ignore
	 * the radar. No need to puncture
	 */
	if ((existing_pattern | dfs_curr_radar_bitmap) == existing_pattern) {
		dfs_err(dfs, WLAN_DEBUG_DFS, "radar event received on invalid channel");
		return true;
	}

	return false;
}

static enum qdf_hrtimer_restart_status
dfs_punc_cac_timeout(qdf_hrtimer_data_t *arg)
{
	struct dfs_punc_obj *dfs_punc;
	struct wlan_dfs *dfs;

	dfs_punc = container_of(arg, struct dfs_punc_obj, dfs_punc_cac_timer);

	dfs = dfs_punc->dfs;
	dfs_puncturing_sm_deliver_evt(dfs,
				      DFS_PUNC_SM_EV_CAC_EXPIRY,
				      0,
				      (void *)dfs_punc);

	return QDF_HRTIMER_NORESTART;
}

void dfs_punc_cac_timer_attach(struct wlan_dfs *dfs, struct dfs_punc_obj *dfs_punc_arr)
{
	qdf_hrtimer_init(&dfs_punc_arr->dfs_punc_cac_timer,
			 dfs_punc_cac_timeout,
			 QDF_CLOCK_MONOTONIC,
			 QDF_HRTIMER_MODE_REL,
			 QDF_CONTEXT_TASKLET);
}

void dfs_punc_cac_timer_reset(struct dfs_punc_obj *dfs_punc_arr)
{
	qdf_hrtimer_cancel(&dfs_punc_arr->dfs_punc_cac_timer);
}

void dfs_punc_cac_timer_detach(struct dfs_punc_obj *dfs_punc_arr)
{
	qdf_hrtimer_kill(&dfs_punc_arr->dfs_punc_cac_timer);
}

void dfs_start_punc_cac_timer(struct dfs_punc_obj *dfs_punc_arr,
			      bool is_weather_chan)
{
	uint32_t punc_cac_timeout;

	if (is_weather_chan)
		punc_cac_timeout = MIN_WEATHER_PRECAC_DURATION;
	else
		punc_cac_timeout = MIN_PRECAC_DURATION;

	qdf_hrtimer_start(&dfs_punc_arr->dfs_punc_cac_timer,
			  qdf_time_ms_to_ktime(punc_cac_timeout),
			  QDF_HRTIMER_MODE_REL);
}

void dfs_cancel_punc_cac_timer(struct dfs_punc_obj *dfs_punc_arr)
{
	qdf_hrtimer_cancel(&dfs_punc_arr->dfs_punc_cac_timer);
}

/**
 * dfs_puncturing_set_curr_state() - API to set current state of Puncturing SM.
 * @dfs_punc: Pointer to struct dfs_punc_obj that indicates the active SM.
 * @state:    value of current state.
 *
 * Return: void.
 */
static void dfs_puncturing_set_curr_state(struct dfs_punc_obj *dfs_punc,
					  enum dfs_punc_sm_state state)
{
	if (state < DFS_PUNCTURING_S_MAX) {
		dfs_punc->dfs_punc_sm_cur_state = state;
	} else {
		dfs_err(NULL, WLAN_DEBUG_DFS_ALWAYS,
			"DFS Puncturing state (%d) is invalid", state);
		QDF_BUG(0);
	}
}

/**
 * dfs_puncturing_get_curr_state() - API to get current state of Puncturing SM.
 * @dfs_punc: Pointer to struct dfs_punc_obj that indicates the active SM.
 *
 * Return: current state enum of type, dfs_punc_sm_state.
 */
static enum dfs_punc_sm_state
dfs_puncturing_get_curr_state(struct dfs_punc_obj *dfs_punc)
{
	return dfs_punc->dfs_punc_sm_cur_state;
}

/**
 * dfs_puncturing_sm_transition_to() - Wrapper API to transition the Puncturing SM state.
 * @dfs_punc: Pointer to struct dfs_punc_obj that indicates the active SM.
 * @state:    State to which the SM is transitioning to.
 *
 * Return: void.
 */
static void dfs_puncturing_sm_transition_to(struct dfs_punc_obj *dfs_punc,
					    enum dfs_punc_sm_state state)
{
	wlan_sm_transition_to(dfs_punc->dfs_punc_sm_hdl, state);
}

/**
 * dfs_puncturing_sm_deliver_event() - API to post events to Puncturing SM.
 * @dfs_punc: Pointer to struct dfs_punc_obj that indicates the active SM.
 * @event: Event to be posted to the RCAC SM.
 * @event_data_len: Length of event data.
 * @event_data: Pointer to event data.
 *
 * Return: QDF_STATUS_SUCCESS on handling the event, else failure.
 *
 * Note: This version of event posting API is not under lock and hence
 * should only be called for posting events within the SM and not be
 * under a dispatcher API without a lock.
 */
static
QDF_STATUS dfs_puncturing_sm_deliver_event(struct wlan_dfs *dfs,
					   enum dfs_agile_sm_evt event,
					   uint16_t event_data_len,
					   void *event_data)
{
	struct dfs_punc_obj *dfs_punc;

	dfs_punc = (struct dfs_punc_obj *)event_data;
	return wlan_sm_dispatch(dfs_punc->dfs_punc_sm_hdl,
				event,
				event_data_len,
				event_data);
}

/**
 * dfs_puncturing_state_unpunctured_entry() - Entry API for UNPUNCTURED state
 * @ctx: DFS Puncture SM object
 *
 * API to perform operations on moving to UNPUNCTURED state
 *
 * Return: void
 */
static void dfs_puncturing_state_unpunctured_entry(void *ctx)
{
	struct dfs_punc_obj *dfs_punc = (struct dfs_punc_obj *)ctx;

	dfs_puncturing_set_curr_state(dfs_punc, DFS_S_UNPUNCTURED);
}

/**
 * dfs_puncturing_state_unpunctured_exit() - Exit API for UNPUNCTURED state
 * @ctx: DFS Puncture SM object
 *
 * API to perform operations on moving out of UNPUNCTURED state
 *
 * Return: void
 */
static void dfs_puncturing_state_unpunctured_exit(void *ctx)
{
	/* NO OPS */
}

/**
 * dfs_puncturing_state_unpunctured_event() - UNPUNCTURED State event handler
 * @ctx: DFS Puncture SM object.
 * @event: Event posted to the SM.
 * @event_data_len: Length of event data.
 * @event_data: Pointer to event data.
 *
 * API to handle events in UNPUNCTURED state
 *
 * Return: TRUE:  on handling event
 *         FALSE: on ignoring the event
 */
static bool dfs_puncturing_state_unpunctured_event(void *ctx,
						   uint16_t event,
						   uint16_t event_data_len,
						   void *event_data)
{
	struct wlan_dfs *dfs;
	bool status;
	struct dfs_punc_obj *dfs_punc;

	if (!event_data)
		return false;

	dfs_punc = (struct dfs_punc_obj *)event_data;
	dfs = dfs_punc->dfs;

	switch (event) {
	case DFS_PUNC_SM_EV_RADAR:
		dfs_debug(dfs, WLAN_DEBUG_DFS_PUNCTURING, "Radar event received. Moving state to punctured");
		dfs_puncturing_sm_transition_to(dfs_punc, DFS_S_PUNCTURED);
		status = true;
		break;
	case DFS_PUNC_SM_EV_STOP:
		dfs_debug(dfs, WLAN_DEBUG_DFS_PUNCTURING, "Punc sm stop triggered");
		status = true;
		break;
	default:
		status = false;
		break;
	}

	return status;
}

/**
 * dfs_puncturing_state_punctured_entry() - Entry API for punctured state
 * @ctx: DFS Puncture SM object.
 *
 * API to perform operations on moving to punctured state.
 *
 * Return: void
 */
static void dfs_puncturing_state_punctured_entry(void *ctx)
{
	struct dfs_punc_obj *dfs_punc = (struct dfs_punc_obj *)ctx;

	dfs_puncturing_set_curr_state(dfs_punc, DFS_S_PUNCTURED);
}

/**
 * dfs_puncturing_state_punctured_exit() - Exit API for punctured state
 * @ctx: DFS Puncture SM object.
 *
 * API to perform operations on moving out of punctured state.
 *
 * Return: void
 */
static void dfs_puncturing_state_punctured_exit(void *ctx)
{
	/* NO OPS */
}

static bool dfs_is_weather_channel(struct dfs_punc_obj *dfs_punc_arr)
{
	qdf_freq_t punc_low_freq = dfs_punc_arr->punc_low_freq;
	qdf_freq_t punc_high_freq = dfs_punc_arr->punc_high_freq;
	qdf_freq_t punc_nol_freq = (punc_low_freq + punc_high_freq) / 2;

	if (punc_high_freq - punc_low_freq == DFS_CHWIDTH_20_VAL) {
		if (DFS_IS_PUNC_CHAN_WEATHER_RADAR(punc_nol_freq))
			return true;
	} else if (punc_high_freq - punc_low_freq == DFS_CHWIDTH_40_VAL) {
		if (DFS_IS_PUNC_CHAN_OVERLAP_WEATHER_RADAR(punc_nol_freq))
			return true;
	}
	return false;
}

/**
 * dfs_agile_state_running_event() - RUNNING State event handler
 * @ctx: DFS SoC private object
 * @event: Event posted to the SM.
 * @event_data_len: Length of event data.
 * @event_data: Pointer to event data.
 *
 * API to handle events in RUNNING state
 *
 * Return: TRUE:  on handling event
 *         FALSE: on ignoring the event
 */
static bool dfs_puncturing_state_punctured_event(void *ctx,
						 uint16_t event,
						 uint16_t event_data_len,
						 void *event_data)
{
	struct wlan_dfs *dfs;
	bool status;
	struct dfs_punc_obj *dfs_punc;
	bool is_weather_chan = false;

	if (!event_data)
		return false;

	dfs_punc = (struct dfs_punc_obj *)event_data;
	dfs = dfs_punc->dfs;

	switch (event) {
	case DFS_PUNC_SM_EV_RADAR:
		dfs_debug(dfs, WLAN_DEBUG_DFS_PUNCTURING, "Ignore radar when channel is already punctured and in punctured state");
		status = true;
		break;
	case DFS_PUNC_SM_EV_NOL_EXPIRY:
		dfs_debug(dfs, WLAN_DEBUG_DFS_PUNCTURING, "Nol expired, move the punc channel to CAC wait and start timer");
		is_weather_chan = dfs_is_weather_channel(dfs_punc);
		dfs_start_punc_cac_timer(dfs_punc, is_weather_chan);
		dfs_puncturing_sm_transition_to(dfs_punc, DFS_S_CAC_WAIT);
		status = true;
		break;
	case DFS_PUNC_SM_EV_STOP:
		dfs_debug(dfs, WLAN_DEBUG_DFS_PUNCTURING, "Punc sm stop triggered");
		dfs_puncturing_sm_transition_to(dfs_punc, DFS_S_UNPUNCTURED);
		status = true;
		break;
	default:
		status = false;
		break;
	}

	return status;
}

/**
 * dfs_puncturing_state_cac_wait_entry() - Entry API for cac wait state.
 * @ctx: DFS Puncture SM object.
 *
 * API to perform operations on moving to cac wait state
 *
 * Return: void
 */
static void dfs_puncturing_state_cac_wait_entry(void *ctx)
{
	struct dfs_punc_obj *dfs_punc = (struct dfs_punc_obj *)ctx;

	dfs_puncturing_set_curr_state(dfs_punc, DFS_S_CAC_WAIT);
}

/**
 * dfs_puncturing_state_cac_wait_exit() - Exit API for cac wait state
 * @ctx: DFS Puncture SM object.
 *
 * API to perform operations on moving out of cac wait state
 *
 * Return: void
 */
static void dfs_puncturing_state_cac_wait_exit(void *ctx)
{
	/* NO OPs. */
}

static
uint16_t dfs_unpuncture_radar_bitmap(struct wlan_dfs *dfs,
				     struct dfs_punc_obj *dfs_punc)
{
	uint8_t n_cur_channels;
	uint16_t dfs_radar_bitmap = dfs_generate_internal_radar_pattern(dfs);
	uint16_t bits = 0x1;
	uint8_t i, j;
	uint16_t cur_freq_list[MAX_20MHZ_SUBCHANS] = {0};
	uint16_t punc_lst_freq_list[N_MAX_PUNC_SM] = {0};
	uint8_t n_punc_channels;

	n_punc_channels = dfs_generate_punc_list_from_sm(dfs_punc, punc_lst_freq_list);

	n_cur_channels =
		dfs_get_bonding_channel_without_seg_info_for_freq(dfs->dfs_curchan,
								  cur_freq_list);

	for (i = 0; i < N_MAX_PUNC_SM; i++) {
		bits = 0x1;
		if (punc_lst_freq_list[i] == 0)
			continue;
		for (j = 0; j < n_cur_channels; j++) {
			if (punc_lst_freq_list[i] == cur_freq_list[j]) {
				dfs_radar_bitmap ^= bits;
				break;
			}
		bits <<= 1;
		}
	}

	return dfs_radar_bitmap;
}

/**
 * dfs_puncturing_state_cac_wait_event() -CAC wait State event handler
 * @ctx: DFS Puncture SM object.
 * @event: Event posted to the SM.
 * @event_data_len: Length of event data.
 * @event_data: Pointer to event data.
 *
 * API to handle events in cac wait state
 *
 * Return: TRUE:  on handling event
 *         FALSE: on ignoring the event
 */
static bool dfs_puncturing_state_cac_wait_event(void *ctx,
						uint16_t event,
						uint16_t event_data_len,
						void *event_data)
{
	struct wlan_dfs *dfs;
	bool status;
	struct dfs_punc_obj *dfs_punc;
	bool is_weather_chan = false;
	uint16_t new_punc_pattern = 0x0;
	uint16_t dfs_useronly_pattern;
	uint16_t dfs_internal_pattern;

	if (!event_data)
		return false;

	dfs_punc = (struct dfs_punc_obj *)event_data;
	dfs = dfs_punc->dfs;

	switch (event) {
	case DFS_PUNC_SM_EV_RADAR:
		is_weather_chan = dfs_is_weather_channel(dfs_punc);
		dfs_punc_cac_timer_reset(dfs_punc);
		dfs_start_punc_cac_timer(dfs_punc, is_weather_chan);
		dfs_debug(dfs, WLAN_DEBUG_DFS_PUNCTURING, "Reset CAC timer when radar seen in channel that is already punctured and in CAC WAIT state");
		status = true;
		break;
	case DFS_PUNC_SM_EV_CAC_EXPIRY:
		dfs_debug(dfs, WLAN_DEBUG_DFS_PUNCTURING, "CAC expiry received. Unpuncture the channe;");
		dfs_internal_pattern = dfs_generate_internal_radar_pattern(dfs);
		dfs_useronly_pattern =
		    dfs->dfs_curchan->dfs_ch_punc_pattern ^ dfs_internal_pattern;
		dfs_internal_pattern = dfs_unpuncture_radar_bitmap(dfs, dfs_punc);
		new_punc_pattern = dfs_useronly_pattern | dfs_internal_pattern;
		dfs_puncturing_sm_transition_to(dfs_punc, DFS_S_UNPUNCTURED);
		if (global_dfs_to_mlme.mlme_unpunc_chan_switch)
			global_dfs_to_mlme.mlme_unpunc_chan_switch(dfs->dfs_pdev_obj,
								   new_punc_pattern);
		dfs_punc->punc_low_freq = 0;
		dfs_punc->punc_high_freq = 0;
		dfs_punc->dfs_is_unpunctured = true;
		status = true;
		break;
	case DFS_PUNC_SM_EV_STOP:
		dfs_debug(dfs, WLAN_DEBUG_DFS_PUNCTURING, "Punc sm stop triggered");
		dfs_puncturing_sm_transition_to(dfs_punc, DFS_S_UNPUNCTURED);
		status = true;
		break;
	default:
		status = false;
		break;
	}

	return status;
}

static struct wlan_sm_state_info dfs_puncturing_sm_info[] = {
	{
		(uint8_t)DFS_S_UNPUNCTURED,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		false,
		"UNPUNCTURED",
		dfs_puncturing_state_unpunctured_entry,
		dfs_puncturing_state_unpunctured_exit,
		dfs_puncturing_state_unpunctured_event
	},
	{
		(uint8_t)DFS_S_PUNCTURED,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		false,
		"PUNCTURED",
		dfs_puncturing_state_punctured_entry,
		dfs_puncturing_state_punctured_exit,
		dfs_puncturing_state_punctured_event
	},
	{
		(uint8_t)DFS_S_CAC_WAIT,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		(uint8_t)WLAN_SM_ENGINE_STATE_NONE,
		false,
		"CAC_WAIT",
		dfs_puncturing_state_cac_wait_entry,
		dfs_puncturing_state_cac_wait_exit,
		dfs_puncturing_state_cac_wait_event
	},
};

static const char *dfs_punc_sm_evt_names[] = {
	"EV_PUNC_RADAR",
	"EV_PUNC_NOL_EXPIRY",
	"EV_PUNC_CAC_EXPIRY",
	"EV_PUNC_SM_STOP",
};

/**
 * dfs_puncturing_sm_print_state() - API to log the current state.
 * @dfs_punc: Pointer to dfs puncturing sm group.
 *
 * Return: void.
 */
static void dfs_puncturing_sm_print_state(struct dfs_punc_obj *dfs_punc)
{
	enum dfs_punc_sm_state state;

	state = dfs_puncturing_get_curr_state(dfs_punc);
	if (!(state < DFS_PUNCTURING_S_MAX))
		return;

	dfs_debug(NULL, WLAN_DEBUG_DFS_PUNCTURING, "->[%s] %s",
		  dfs_punc->dfs_punc_sm_hdl->name,
		  dfs_puncturing_sm_info[state].name);
}

/**
 * dfs_puncturing_sm_print_state_event() - API to log the current state and event
 *                                   received.
 * @dfs_soc_obj: Pointer to dfspuncturing sm group.
 * @event: Event posted to Puncturing SM.
 *
 * Return: void.
 */
static void dfs_puncturing_sm_print_state_event(struct dfs_punc_obj *dfs_punc,
						enum dfs_punc_sm_evt event)
{
	enum dfs_punc_sm_state state;

	state = dfs_puncturing_get_curr_state(dfs_punc);
	if (!(state < DFS_PUNCTURING_S_MAX))
		return;

	if (event >= QDF_ARRAY_SIZE(dfs_punc_sm_evt_names)) {
		dfs_debug(NULL, WLAN_DEBUG_DFS_PUNCTURING, "invalid event %u", event);
		return;
	}

	dfs_debug(NULL, WLAN_DEBUG_DFS_PUNCTURING, "[%s]%s, %s",
		  dfs_punc->dfs_punc_sm_hdl->name,
		  dfs_puncturing_sm_info[state].name,
		  dfs_punc_sm_evt_names[event]);
}

QDF_STATUS dfs_puncturing_sm_deliver_evt(struct wlan_dfs *dfs,
					 enum dfs_punc_sm_evt event,
					 uint16_t event_data_len,
					 void *event_data)
{
	enum dfs_punc_sm_state old_state, new_state;
	QDF_STATUS status;
	struct dfs_punc_obj *dfs_punc;

	dfs_punc = (struct dfs_punc_obj *)event_data;
	DFS_PUNC_SM_SPIN_LOCK(dfs_punc);
	old_state = dfs_puncturing_get_curr_state(dfs_punc);

	/* Print current state and event received */
	dfs_puncturing_sm_print_state_event(dfs_punc, event);

	status = dfs_puncturing_sm_deliver_event(dfs,
						 event,
						 event_data_len,
						 event_data);

	new_state = dfs_puncturing_get_curr_state(dfs_punc);

	/* Print new state after event if transition happens */
	if (old_state != new_state)
		dfs_puncturing_sm_print_state(dfs_punc);
	DFS_PUNC_SM_SPIN_UNLOCK(dfs_punc);

	return status;
}

QDF_STATUS dfs_punc_sm_create(struct dfs_punc_obj *dfs_punc)
{
	struct wlan_sm *sm;

	sm = wlan_sm_create("DFS_PUNCTURING", dfs_punc,
			    DFS_S_UNPUNCTURED,
			    dfs_puncturing_sm_info,
			    QDF_ARRAY_SIZE(dfs_puncturing_sm_info),
			    dfs_punc_sm_evt_names,
			    QDF_ARRAY_SIZE(dfs_punc_sm_evt_names));
	if (!sm) {
		qdf_err("DFS PUNCTURING SM allocation failed");
		return QDF_STATUS_E_FAILURE;
	}
	dfs_punc->dfs_punc_sm_hdl = sm;
	dfs_punc->punc_low_freq = 0;
	dfs_punc->punc_high_freq = 0;
	dfs_punc->dfs_is_unpunctured = true;

	qdf_spinlock_create(&dfs_punc->dfs_punc_sm_lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dfs_punc_sm_destroy(struct dfs_punc_obj *dfs_punc)
{
	wlan_sm_delete(dfs_punc->dfs_punc_sm_hdl);
	qdf_spinlock_destroy(&dfs_punc->dfs_punc_sm_lock);

	return QDF_STATUS_SUCCESS;
}
#endif /* QCA_DFS_BW_PUNCTURE */
#endif
