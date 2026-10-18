/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef __NSS_PPENL_PORT_MGMT_API_H__
#define __NSS_PPENL_PORT_MGMT_API_H__

#include <ppe_port_mgmt.h>

/**
 * nss_ppenl_port_mgmt_port_isolation_set()
 *	Set the PORT_MGMT port isolation
 *
 * @param[in] port_name		Port name.
 * @param[in] omci_id		OMCI ID.
 * @param[out] isol_info	PPE port isolation information (return value).
 *
 * @return
 * 0 on success or negative on failure.
 */
int nss_ppenl_port_mgmt_port_isolation_set(struct nss_ppenl_port_mgmt_info *port_mgmt_info);

/**
 * nss_ppenl_port_mgmt_act_ctrl_set()
 *	Set the PORT_MGMT action control configs
 *
 * @param[in] isol_info		PPE port isolation information.
 *
 * @return
 * 0 on success or negative on failure.
 */
int nss_ppenl_port_mgmt_act_ctrl_set(struct nss_ppenl_port_mgmt_info *port_mgmt_info);

/**
 * nss_ppenl_port_mgmt_isol_default()
 *	Set the PORT_MGMT default isol configs
 *
 * @param[in] isol_info		PPE port isolation information.
 *
 * @return
 * 0 on success or negative on failure.
 */
int nss_ppenl_port_mgmt_isol_default(struct nss_ppenl_port_mgmt_info *port_mgmt_info);

/**
 * nss_ppenl_port_mgmt_isol_default()
 *      Set the PORT_MGMT port mac learn limit.
 *
 * @param[in] mac_learn info:  PPE Port mac learn limit info.
 *
 * @return
 * 0 on success or negative on failure.
 */
int nss_ppenl_port_mgmt_mac_lrn_limit_set(struct nss_ppenl_port_mgmt_info *port_mgmt_info) ;

/**
 * nss_ppenl_port_mgmt_mac_filter_set()
 *      Block the mac in FDB.
 *
 * @param[in] mac_filter:  PPE Port mac filter info.
 *
 * @return
 * 0 on success or negative on failure.
 */
int nss_ppenl_port_mgmt_mac_filter_set(struct nss_ppenl_port_mgmt_info *port_mgmt_info);

/**
 * nss_ppenl_port_mgmt_mac_filter_clear()
 *      Allow the mac in FDB.
 *
 * @param[in] mac_filter:  PPE Port mac filter info.
 *
 * @return
 * 0 on success or negative on failure.
 */
int nss_ppenl_port_mgmt_mac_filter_clear(struct nss_ppenl_port_mgmt_info *port_mgmt_info);

/**
 * nss_ppenl_port_mgmt_omci_port_add()
 *	Add a port to the OMCI-managed port list.
 *
 * @param[in] port_mgmt_info	PPE port management info with omci_port.port_name set.
 *
 * @return
 * 0 on success or negative on failure.
 */
int nss_ppenl_port_mgmt_omci_port_add(struct nss_ppenl_port_mgmt_info *port_mgmt_info);

/**
 * nss_ppenl_port_mgmt_omci_port_del()
 *	Remove a port from the OMCI-managed port list.
 *
 * @param[in] port_mgmt_info	PPE port management info with omci_port.port_name set.
 *
 * @return
 * 0 on success or negative on failure.
 */
int nss_ppenl_port_mgmt_omci_port_del(struct nss_ppenl_port_mgmt_info *port_mgmt_info);

/**
 * nss_ppenl_port_mgmt_omci_port_flush()
 *	Flush all ports from the OMCI-managed port list.
 *
 * @param[in] port_mgmt_info	PPE port management info (omci_port field used for response).
 *
 * @return
 * 0 on success or negative on failure.
 */
int nss_ppenl_port_mgmt_omci_port_flush(struct nss_ppenl_port_mgmt_info *port_mgmt_info);

#endif /* __NSS_PPENL_PORT_MGMT_API_H__ */
