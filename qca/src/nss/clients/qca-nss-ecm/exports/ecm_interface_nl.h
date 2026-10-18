/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __ECM_INTERFACE_NL_H__
#define __ECM_INTERFACE_NL_H__

/*
 * ecm_interface_node_connections_defunct_by_type_sta_join()
 *	Defunct by the type -  station join
 */
void ecm_interface_node_connections_defunct_by_type_sta_join(uint8_t *mac);

/*
 * ecm_interface_defunct_qm_connections()
 *	Defunct the connections with qm type and qm id
 */
void ecm_interface_defunct_qm_connections(uint8_t *mac, uint8_t wifi_qm_type, uint8_t wifi_qm_id);

/*
 * ecm_interface_node_connections_defunct_by_mac_addr()
 *	Defunct the connections on this node by mac addr.
 */
void ecm_interface_node_connections_defunct_by_mac_addr(uint8_t *mac);
#endif

