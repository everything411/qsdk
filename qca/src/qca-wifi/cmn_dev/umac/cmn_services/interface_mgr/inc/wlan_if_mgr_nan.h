/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

/*
 * DOC: contains interface manager public file containing NAN event handlers
 */
#ifndef _WLAN_IF_MGR_NAN_H_
#define _WLAN_IF_MGR_NAN_H_
#include "wlan_if_mgr_public_struct.h"

#if defined(WLAN_FEATURE_NAN)
/**
 * if_mgr_nan_post_enable() - NAN post enable event handler
 * @vdev: vdev object
 * @event_data: Interface mgr event data
 *
 * Interface manager nan post enable event handler
 *
 * Context: It should run in thread context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
if_mgr_nan_post_enable(struct wlan_objmgr_vdev *vdev,
		       struct if_mgr_event_data *event_data);
/**
 * if_mgr_nan_pre_enable() - NAN pre enable event handler
 * @vdev: vdev object
 * @event_data: Interface mgr event data
 *
 * Interface manager nan pre enable event handler
 *
 * Context: It should run in thread context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
if_mgr_nan_pre_enable(struct wlan_objmgr_vdev *vdev,
		      struct if_mgr_event_data *event_data);
/**
 * if_mgr_nan_post_disable() - NAN post disable event handler
 * @vdev: vdev object
 * @event_data: Interface mgr event data
 *
 * Interface manager nan post disable event handler
 *
 * Context: It should run in thread context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
if_mgr_nan_post_disable(struct wlan_objmgr_vdev *vdev,
			struct if_mgr_event_data *event_data);
#else

static inline
QDF_STATUS if_mgr_nan_post_enable(struct wlan_objmgr_vdev *vdev,
				  struct if_mgr_event_data *event_data)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS if_mgr_nan_pre_enable(struct wlan_objmgr_vdev *vdev,
				 struct if_mgr_event_data *event_data)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS if_mgr_nan_post_disable(struct wlan_objmgr_vdev *vdev,
				   struct if_mgr_event_data *event_data)
{
	return QDF_STATUS_SUCCESS;
}

#endif
#endif
