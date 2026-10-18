/*
 * Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
 * SPDX-License-Identifier: ISC
 */

#include <ppe_qos.h>
#include "ppe_qos.h"

/*
 * ppe_qos_get_int_pri_func()
 *	Get int_pri and other details of PPE queue corresponding to the handle_id/class_id of input dev.
 *
*/
ppe_qos_ret_t ppe_qos_get_int_pri_func(struct ppe_qos_req *req)
{
	char dev_name[IFNAMSIZ] = {0};
	struct ppe_drv_queue_info q_info = {0};
	struct net_device *dev;

        strlcpy(dev_name, req->dev, IFNAMSIZ);
        if (dev_name[strlen(dev_name) - 1] == '\n') {
                dev_name[strlen(dev_name) - 1] = '\0';
        }

	dev = dev_get_by_name(&init_net, dev_name);
	q_info.handle_id = req->handle_id;
	if(!dev){
		ppe_qos_warn("DEVICE NOT FOUND FOR dev= %s\n", dev_name);
		return PPE_QOS_INVALID_DEV;
	}

	if (ppe_drv_qos_queue_info_get(dev, q_info.handle_id, &q_info)) {
		if (!q_info.valid) {
			ppe_qos_warn("%px:PPE qdisc are attached to only leaf classes.\n"
					"Classid %d is not a leaf class on %s interface", dev, q_info.handle_id, dev_name);
			dev_put(dev);
			return PPE_QOS_CLASS_NON_LEAF;
		}

		req->int_pri = q_info.int_pri;
		req->port_id = q_info.port_id;
		req->ucast_qid = q_info.ucast_qid;
		ppe_qos_info("%px:PPE DRV API called for dev = %s and handle_id = %d and the returned values are:"
				"\n int_pri = %d \n ucast_qid = %d\n", dev, dev_name, req->handle_id, req->int_pri, req->ucast_qid);
		dev_put(dev);
		return PPE_QOS_SUCCESS;
	}

	dev_put(dev);
	return PPE_QOS_INVALID_HANDLE_ID;
}
EXPORT_SYMBOL(ppe_qos_get_int_pri_func);

/*
 * ppe_qos_deinit()
 *	Qos deinit API
 */
void ppe_qos_deinit(void)
{
	return;
}

/*
 * ppe_qos_init()
 *	qos init API
 */
void ppe_qos_init(struct dentry *d_rule)
{
	return;
}
