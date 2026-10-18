/*
 **************************************************************************
 * Copyright (c) 2014, The Linux Foundation.  All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 **************************************************************************
 */

struct ecm_classifier_default_instance;

/*
 * Structure used to synchronise a classifier instance with the state as presented by the accel engine
 */
struct ecm_classifier_default_sync {
	uint8_t qos;
	uint32_t qws;
	uint32_t qwr;
	uint16_t qbc;
	uint32_t from_data_total;		/* Amount of bytes sent by 'from' */
	uint32_t to_data_total;			/* Amount of bytes sent by 'to' */
};

typedef void (*ecm_classifier_default_process_callback_t)(struct ecm_classifier_default_instance *dci, ecm_tracker_sender_type_t sender, struct iphdr *ip_hdr, int ip_hdr_len, int ip_total_len, struct sk_buff *skb);
typedef ecm_db_timer_group_t (*ecm_classifier_default_timer_group_change_callback_t)(struct ecm_classifier_default_instance *dci);
typedef struct ecm_tracker_instance *(*ecm_classifier_default_tracker_get_and_ref_callback_t)(struct ecm_classifier_default_instance *dci);

struct ecm_classifier_default_instance {
	struct ecm_classifier_instance base;	/* Base class implemented by this classifier */

	/*
	 * Functions specific to the default classifier.
	 */
	ecm_classifier_default_process_callback_t process;		/* Process new data for connection - Used ONLY by front ends */
	ecm_classifier_default_timer_group_change_callback_t timer_group_change;
									/* Utility to other classifiers: detect if update is needed to timer group */
	ecm_classifier_default_tracker_get_and_ref_callback_t tracker_get_and_ref;
									/* Utility to front end: Obtain default classifier tracker */
};

struct ecm_classifier_default_instance *ecm_classifier_default_instance_alloc(struct ecm_db_connection_instance *ci, int protocol, ecm_db_direction_t dir, int from_port, int to_port);

int ecm_classifier_default_init(struct dentry *dentry);
void ecm_classifier_default_exit(void);
