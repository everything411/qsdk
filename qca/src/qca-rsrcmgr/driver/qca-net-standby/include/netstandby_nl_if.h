/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * @file netstandby_nl_if.h
 *	NSS Netlink common headers
 */
#ifndef __NETSTANDBY_NL_IF_H
#define __NETSTANDBY_NL_IF_H

#include <netstandby_msg_if.h>
#define NETSTANDBY_NL_CMN_CB_MAX_SZ 64 /* bytes */
#define NETSTANDBY_NL_VER 1

/*
 * We are reserving below netlink number to create a kernel netlink socket
 * These socket will be used for Kernel to APP and APP to APP communication
 */
#define NETLINK_USER 31
#define NETSTANDBY_NL_FAMILY "nl_netstandby"

/**
 * Real pointer size of the system.
 */
#ifdef __LP64__
typedef uint64_t netstandby_ptr_t;
#else
typedef uint32_t netstandby_ptr_t;
#endif

struct netstandby_rule;

/*
 * netstandby_nl_ret()
 *	NL return status
 */
enum netstandby_nl_ret {
	NETSTANDBY_NL_RET_SUCCESS = 0,                  /**< Success */
	NETSTANDBY_NL_RET_FAIL,                         /**< Message Failed */
};

/*
 * netstandby_nl_user_message_types
 */
enum netstandby_nl_msg_type {
	NETSTANDBY_NL_INIT_MSG = 0,		/**< INIT message */
	NETSTANDBY_NL_ENTER_MSG,	/**< Standby enter message */
	NETSTANDBY_NL_EXIT_MSG,		/**< Standby exit message */
	NETSTANDBY_NL_STATUS_MSG,	/**< Standby system stats and interface status */
	NETSTANDBY_NL_STOP_MSG,		/**< Standby system stop message */
	NETSTANDBY_NL_MAX_MSG,		/**< Standby max message */
};

/**
 * @brief Common message header for each NSS netlink message
 */
struct netstandby_nl_cmn {
	uint32_t version;			/**< message version */
	uint32_t pid;				/**< process ID for the message */
	netstandby_ptr_t sock_data;		/**< socket specific info, used by kernel */
	uint16_t cmd_len;			/**< command len */
	uint8_t cmd_type;			/**< command type */
	uint8_t res;				/**< reserve for future use */
	int32_t cb_owner;			/**< CB identifier */
	uint8_t cb_data[NETSTANDBY_NL_CMN_CB_MAX_SZ]; 	/**< user context buffer */
};

/*
 * netstandby_nl_event_param()
 */
struct netstandby_nl_event_param {
	enum netstandby_notif_type notif_type;		/**< Notification type */
	enum netstandby_subsystem_type system_type;	/**< Subsystem type */
};

/*
 * netstandby_nl_event_type
 */
enum netstandby_nl_event_type {
	NETSTANDBY_NL_CLI_EVENT_ENTER = 0,		/**< Enter Event type */
	NETSTANDBY_NL_CLI_EVENT_EXIT,			/**< Exit Event type */
	NETSTANDBY_NL_KERNEL_EVENT,			/**< Event from Kernel to Userspace */
	NETSTANDBY_NL_CLI_EVENT_STATUS_REQUEST,		/**< Request for system status */
	NETSTANDBY_NL_CLI_EVENT_STATUS_RESPONSE,	/**< Response for the status request */
#if defined(RM_QCA_PROP) && !defined(RM_QCA_256M_PROFILE)
	NETSTANDBY_NL_NSS_TELEMETRY,	/**< NSS telemetry stats to ES */
#endif
	NETSTANDBY_NL_EVENT_MAX,
};

/*
 * netstandby_nl_msg
 *	Netlink message between lib and driver
 */
struct netstandby_nl_msg {
	struct netstandby_nl_cmn cm;                    /**< Netlink common message */
	struct netstandby_rule rule;			/**< Rule message */
	int type;                                       /**< Subsystem type */

	int ret;
};

/*
 * netstandby_nl_system_state
 */
enum netstandby_nl_system_state {
	NETSTANDBY_NL_SYSTEM_INIT_STATE = 0,
	NETSTANDBY_NL_SYSTEM_ENTER_IN_PROGRESS,        /**< netstandby system enter in progress */
	NETSTANDBY_NL_SYSTEM_ENTER_COMPLETE,               /**< netstandby system in enter complete state */
	NETSTANDBY_NL_SYSTEM_EXIT_IN_PROGRESS,             /**< netstandby system exit in progress */
	NETSTANDBY_NL_SYSTEM_TRIGGER_IN_PROGRESS,          /**< netstandby system handling trigger from SS */
	NETSTANDBY_NL_SYSTEM_EXIT_COMPLETE,                /**< netstandby system in exit complete state */
	NETSTANDBY_NL_SYSTEM_MAX,
};

/*
 * netstandby_nl_avg_time
 */
struct netstandby_nl_status_info {
	int num_enter;                          /**< Number of times system entered netstandby mode */
	int num_exit;                           /**< Number of times system exited netstandby mode */
	int num_exit_retry;			/**< Number of times exit retried for SS */
	unsigned long avg_enter_time;           /**< Average enter time */
	unsigned long avg_exit_time;            /**< Average exit time */
	enum netstandby_nl_system_state state;     /**< State of the daemon */
};

/*
 * netstandby_avg_time_subsystem
 */
struct netstandby_nl_subsystem_info {
	struct netstandby_nl_status_info wifi_system;	/**< Stats structure for wifi SS */
	struct netstandby_nl_status_info nss_system;	/**< Stats structure for NSS SS */
	struct netstandby_nl_status_info bsp_system;	/**< Stats structure for BSP SS */
};

/*
 * netstandby_status
 */
struct netstandby_nl_system_info {
	struct netstandby_nl_status_info system_info;        /**< Status at system level */
	struct netstandby_nl_subsystem_info subsystem_info;      /**< Status at sub system level */
	pid_t status_process_pid;	/** < PID of the process which requested the status */

	/* TODO: Move this to 'netstandby_nl_msg_info' towards the end, marking it separately as a response section */
	enum netstandby_nl_ret status;	/** < PASS/FAIL status */
};

/*
 * netstandby_nl_msg_info
 *	common message information
 */
struct netstandby_nl_msg_info {
	enum netstandby_nl_event_type event_type;		/**< Event type */
	union {
		struct netstandby_rule rule;		/**< Rule structure to be used from CLI to daemon */
		struct netstandby_nl_event_param param;	/**< Param structure to be passed from kernel to daemon */
		struct netstandby_nl_system_info netstandby_status; /**< Status structure at daemon level */
#ifdef RM_QCA_PROP
		struct netstandby_erp_nss_telemetry nss_telemetry;	/**< Periodic NSS telemetry synced to user space */
		struct netstandby_lan_telemetry lan_telemetry[NETSTANDBY_LAN_CLIENT_TELEMETRY_MAX];	/**< LAN telemetry synced to user space */
#endif
	} ns_msg;
	pid_t resp_pid;	/** < PID of the process which started the call */
};

/**
 * netstandby_nl_cmn_init_cmd
 *	Initialize command.
 *
 * @datatypes
 * struct netstandby_nl_cmn
 *
 * @param cm[IN] common message
 * @param len[IN] command length
 * @param cmd[IN] command for the family
 *
 * return None
 */
static inline void netstandby_nl_cmn_init_cmd(struct netstandby_nl_cmn *cm, uint16_t len, uint8_t cmd)
{
	cm->cmd_type = cmd;
	cm->cmd_len = len;
}

/**
 * netstandby_nl_chk_ver
 *	check version.
 *
 * @datatypes
 * struct netstandby_nl_cmn
 *
 * @param cm[IN] common message
 * @param ver[IN] command version
 *
 * return true or falde
 */
static inline bool netstandby_nl_cmn_chk_ver(struct netstandby_nl_cmn *cm, uint32_t ver)
{
	return cm->version == ver;
}

/**
 * netstandby_nl_cmn_set_ver
 *	set version.
 *
 * @datatypes
 * struct netstandby_nl_cmn
 *
 * @param cm[IN] common message
 * @param ver[IN] command version
 *
 * return None
 */
static inline void netstandby_nl_cmn_set_ver(struct netstandby_nl_cmn *cm, uint32_t ver)
{
	cm->version = ver;
}

/**
 * netstandby_nl_cmn_get_ver
 *	get version.
 *
 * @datatypes
 * struct netstandby_nl_cmn
 *
 * @param cm[IN] common message
 *
 * return Version
 */
static inline uint32_t netstandby_nl_cmn_get_ver(struct netstandby_nl_cmn *cm)
{
	return cm->version;
}

/**
 * netstandby_nl_cmn_get_cmd_type
 *	get type
 *
 * @datatypes
 * struct netstandby_nl_cmn
 *
 * @param cm[IN] common message
 *
 * return type
 */
static inline uint8_t netstandby_nl_cmn_get_cmd_type(struct netstandby_nl_cmn *cm)
{
	return cm->cmd_type;
}

/**
 * netstandby_nl_cmn_get_cmd_len
 *	get command len
 *
 * @datatypes
 * struct netstandby_nl_cmn
 *
 * @param cm[IN] common message
 *
 * return len
 */
static inline uint16_t netstandby_nl_cmn_get_cmd_len(struct netstandby_nl_cmn *cm)
{
	return cm->cmd_len;
}

/**
 * netstandby_nl_cmn_get_cb_data
 *	get data
 *
 * @datatypes
 * struct netstandby_nl_cmn
 *
 * @param cm[IN] common message
 * @param cb_owner[IN] owner
 *
 * return NULL
 */
static inline void *netstandby_nl_cmn_get_cb_data(struct netstandby_nl_cmn *cm, int32_t cb_owner)
{
	/*
	 * If owner doesn't match then the caller is not the owner
	 */
	if (cm->cb_owner != cb_owner) {
		return NULL;
	}

	return cm->cb_data;
}

/**
 * netstandby_nl_cmn_set_cb_owner
 *	set owner
 *
 * @datatypes
 * struct netstandby_nl_cmn
 *
 * @param cm[IN] common message
 * @param cb_owner[IN] owner
 *
 * return NULL
 */
static inline void netstandby_nl_cmn_set_cb_owner(struct netstandby_nl_cmn *cm, int32_t cb_owner)
{
	cm->cb_owner = cb_owner;
}

/**
 * netstandby_nl_cmn_clr_cb_owner
 *	clear owner
 *
 * @datatypes
 * struct netstandby_nl_cmn
 *
 * @param cm[IN] common message
 *
 * return NULL
 */
static inline void netstandby_nl_cmn_clr_cb_owner(struct netstandby_nl_cmn *cm)
{
	netstandby_nl_cmn_set_cb_owner(cm, -1);
}

/**
 * netstandby_nl_rule_init
 *	Rule init
 *
 * @datatypes
 * struct netstandby_rule
 *
 * @param rule[IN] rule message
 * @param type[IN] message type
 *
 * return NONE
 */
static inline void netstandby_nl_rule_init(struct netstandby_nl_msg *msg, enum netstandby_nl_msg_type type)
{
	netstandby_nl_cmn_set_ver(&msg->cm, NETSTANDBY_NL_VER);
	netstandby_nl_cmn_init_cmd(&msg->cm, sizeof(struct netstandby_nl_msg), type);
}
#endif /* __NETSTANDBY_NL_CMN_IF_H */
