/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <linux/debugfs.h>
#include "ppe_acl.h"
#include "ppe_acl_dump.h"

/*
 * ppe_acl_dump_ipo_type_str
 * 	ipo type string array
 */
static const char *ppe_acl_dump_ipo_type_str[] = {
	"IPO",			/* IPO rule. */
	"PRE_IPO",		/* PRE_IPO rule. */
};

/*
 * ppe_acl_dump_src_type_str
 * 	source type string array
 */
static const char *ppe_acl_dump_src_type_str[] = {
	"SRC_TYPE_INVALID",		/* Source type invalid. */
	"SRC_TYPE_DEV",			/* Source type device. */
	"SRC_TYPE_SC",			/* Source type service code. */
	"SRC_TYPE_FLOW",		/* Source type flow. */
	"DST_TYPE_L2",			/* L2 Destination type. */
	"DST_TYPE_L3",			/* L3 Destination type. */
};

/*
 * ppe_acl_dump_slice_type_str
 * 	Slice type string array
 */
static const char *ppe_acl_dump_slice_type_str[] = {
        "SLICE_DST_MAC",         /* ACL destination MAC slice type. */
        "SLICE_SRC_MAC",         /* ACL source MAC slice type. */
        "SLICE_VLAN",            /* ACL VLAN slice type. */
        "SLICE_L2_MISC",         /* ACL L2 miscellaneous slice type. */
        "SLICE_DST_IPV4",        /* ACL destination IPv4 slice type. */
        "SLICE_SRC_IPV4",        /* ACL source IPv4 slice type. */
        "SLICE_DST_IPV6_0",      /* ACL destination IPv6 0 slice type. */
        "SLICE_DST_IPV6_1",      /* ACL destination IPv6 1 slice type. */
        "SLICE_DST_IPV6_2",      /* ACL destination IPv6 2 slice type. */
        "SLICE_SRC_IPV6_0",      /* ACL source IPv6 0 slice type. */
        "SLICE_SRC_IPV6_1",      /* ACL source IPv6 1 slice type. */
        "SLICE_SRC_IPV6_2",      /* ACL source IPv6 2 slice type. */
        "SLICE_IP_MISC",         /* ACL IP miscellaneous slice type. */
	"SLICE_TYPE_UDF",			/* ACL UDF with offset */
	"SLICE_UDF_WITH_IP",			/* ACL UDF with L3 IP (IPv4/IPv6) slice type */
	"SLICE_EXT_VLAN",	 /* ACL ext VLAN slice type. */
};

/*
 * ppe_acl_dump_udf_type_str
 * 	UDF proto type string array
 */
static const char *ppe_acl_dump_udf_type_str[] = {
	"UDF_PROTO_NON_IP",	/* ACL UDF proto type non-ip */
	"UDF_PROTO_IPv4",	/* ACL UDF proto type ipv4 */
	"UDF_PROTO_IPv6",	/* ACL UDF proto type ip6 */
};

/*
 * ppe_acl_dump_udf_offset_from_str
 * 	UDF offset base string array
 */
static const char *ppe_acl_dump_udf_offset_from_str[] = {
	"UDF_OFFSET_BASE_L2",	/* ACL UDF offset base L2 */
	"UDF_OFFSET_BASE_L3",	/* ACL UDF offset base L3 */
	"UDF_OFFSET_BASE_L4",	/* ACL UDF offset base L4 */
};

/*
 * ppe_acl_dump_udf_op_from_str
 *	ACL UDF operation string arry.
 */
static const char *ppe_acl_dump_udf_op_from_str[] = {
	"UDF_OP_INVALID",        /* ACL UDF match operation is invalid */
	"UDF_OP_MATCH_MASK",     /* ACL UDF mask match operation */
	"UDF_OP_MATCH_RANGE",    /* ACL UDF range match operation */
	"UDF_OP_MATCH_LE",       /* ACL UDF 'less than' match operation */
	"UDF_OP_MATCH_GE",       /* ACL UDF 'greater than' match operation */
	"UDF_OP_MATCH_NE",       /* ACL UDF 'not equal to' match operation */
};

#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
static const char *ppe_acl_dump_counter_mode_type_str[] = {
	"VLAN_DEV",		/* ACL counter mode vlan dev type. */
	"PON_PM",		/* ACL counter mode PON pm type. */
};

static const char *ppe_acl_dump_src_info_type_str[] = {
	"VP",			/* ACL src info type is VP. */
	"L3_IF",		/* ACL src info is L3_IF type. */
};

static const char *ppe_acl_dump_mc_type_str[] = {
	"INVALID_MC_RULE"	/* ACL invalid mcast rule. */
	"NO_MC_RULE",		/* ACL no multicast rule. */
	"IP_MC_RULE",		/* ACL IP multicast rule. */
	"IP NO_MC RULE",	/* ACL IP_MC and NO_MC rule. */
	"NONIP_MC_RULE",	/* ACL NON IP multicast rule. */
	"NONIP NO_MC rule",	/* ACL NONIP and NO MC tyoe rule. */
	"IP NONIP rule",	/* ACL IPMC NONIP rule. */
	"NO_MC IP NONIP rule",	/* ACL all MC  rules. */
};

static const char *ppe_acl_dump_dhcp_type_str[] = {
	"INVALID_DHCP_RULE"	/* ACL invalid DHCP field. */
	"NO_DHCP_RULE",		/* ACL no DHCP rule. */
	"V4_DHCP_RULE",		/* ACL v4 DHCP rule. */
	"V4 NO_DHCP RULE",	/* ACL v4 No DHCP rule. */
	"V6_DHCP_RULE",		/* ACL v6 DHCP rule. */
	"V6 NO DHCP RULE",	/* ACL v6 No DHCP rule. */
	"V6 V4 DHCP RULE",	/* ACL v4 v6 DHCP rule. */
	"V4 V6 NON_DHCP RULE",	/* ACL all DHCP rule. */
};

static const char *ppe_acl_dump_pcp_cmd_type_str[] = {
	"UNCHANGED",		/* ACL pcp no action command. */
	"REPLACE",		/* ACL pcp replace action command. */
	"COPY_SPCP",		/* ACL copy SPCP action command. */
	"COPY_CPCP",		/* ACL copy CPCP action command. */
	"DSCP2PBIT",		/* ACL DSCP to PBIT map bit. */
	"TAG_REPLACE",		/* ACL replace action command in tag. */
	"TAG_COPY_SPCP",	/* ACL copy SPCP action command in tag. */
	"TAG_COPY_CPCP",	/* ACL copy CPCP action command in tag. */
	"TAG_DSCP2PBIT",	/* ACL DSCP to PBIT map bit in tag. */
};

static const char *ppe_acl_dump_dei_cmd_type_str[] = {
	"UNCHANGED",		/* ACL dei unchanged. */
	"REPLACE",		/* ACL dei replace command. */
	"COPY_SDEI",		/* ACL copy sdei action command. */
	"COPY_CDEI",		/* ACL copy cdei action command. */
};

static const char *ppe_acl_dump_pid_cmd_type_str[] = {
	"UNCHANGED",		/* ACL pid unchanged. */
	"REPLACE",		/* ACL pid replace command. */
	"COPY_STPID",		/* ACL copy stpid action command. */
	"COPY_CTPID",		/* ACL copy ctpid action command. */
};

static const char *ppe_acl_dump_vid_cmd_type_str[] = {
	"DELETE",		/* ACL vid unchanged. */
	"REPLACE",		/* ACL vid replace command. */
	"COPY_SVID",		/* ACL copy svid action command. */
	"COPY_CVID",		/* ACL copy cvid action command. */
};
#endif

/*
 * ppe_acl_dump_write_reset()
 *	Reset the msg buffer, specifying a new initial prefix
 */
int ppe_acl_dump_write_reset(struct ppe_acl_dump_instance *adi, char *base_prefix)
{
	int result;

	adi->msgp = adi->msg;
	adi->msg_len = 0;

	result = snprintf(adi->prefix, PPE_ACL_DUMP_PREFIX_SIZE, "%s", base_prefix);
	if ((result < 0) || (result >= PPE_ACL_DUMP_PREFIX_SIZE)) {
		return -1;
	}

	adi->prefix_level = 0;
	adi->prefix_levels[adi->prefix_level] = result;

	return 0;
}

/*
 * ppe_acl_dump_prefix_add()
 *	Add another level to the prefix
 */
int ppe_acl_dump_prefix_add(struct ppe_acl_dump_instance *adi, char *prefix)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = adi->prefix_levels[adi->prefix_level];
	pxremain = PPE_ACL_DUMP_PREFIX_SIZE - pxsz;

	result = snprintf(adi->prefix + pxsz, pxremain, ".%s", prefix);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	adi->prefix_level++;
	adi->prefix_levels[adi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_acl_dump_prefix_index_add()
 *	Add another level (numeric) to the prefix
 */
int ppe_acl_dump_prefix_index_add(struct ppe_acl_dump_instance *adi, uint32_t index)
{
	int pxsz;
	int pxremain;
	int result;

	pxsz = adi->prefix_levels[adi->prefix_level];
	pxremain = PPE_ACL_DUMP_PREFIX_SIZE - pxsz;
	result = snprintf(adi->prefix + pxsz, pxremain, ".%u", index);
	if ((result < 0) || (result >= pxremain)) {
		return -1;
	}

	adi->prefix_level++;
	adi->prefix_levels[adi->prefix_level] = pxsz + result;

	return 0;
}

/*
 * ppe_acl_dump_prefix_remove()
 *	Remove level from the prefix
 */
int ppe_acl_dump_prefix_remove(struct ppe_acl_dump_instance *adi)
{
	int pxsz;

	adi->prefix_level--;
	pxsz = adi->prefix_levels[adi->prefix_level];
	adi->prefix[pxsz] = 0;

	return 0;
}

/*
 * ppe_acl_dump_write()
 *	Write out to the message buffer, prefix is added automatically.
 */
int ppe_acl_dump_write(struct ppe_acl_dump_instance *adi, char *name, char *fmt, ...)
{
	int remain;
	char *ptr;
	int result;
	va_list args;

	remain = PPE_ACL_DUMP_BUFFER_SIZE - adi->msg_len;
	ptr = adi->msg + adi->msg_len;
	result = snprintf(ptr, remain, "%s.%s=", adi->prefix, name);
	if ((result < 0) || (result >= remain)) {
		return -1;
	}

	adi->msg_len += result;
	remain -= result;
	ptr += result;

	va_start(args, fmt);
	result = vsnprintf(ptr, remain, fmt, args);
	va_end(args);
	if ((result < 0) || (result >= remain)) {
		return -2;
	}

	adi->msg_len += result;
	remain -= result;
	ptr += result;

	result = snprintf(ptr, remain, "\n");
	if ((result < 0) || (result >= remain)) {
		return -3;
	}

	adi->msg_len += result;
	return 0;
}

/*
 * ppe_acl_dump_one()
 *	Fill ACL dump information for one ACL rule.
 */
int ppe_acl_dump_one(struct ppe_acl_dump_instance *adi, struct ppe_acl *acl)
{
	int result;
	uint64_t pkts, bytes;
	ppe_acl_rule_dev_type_t dev_type;
	struct ppe_acl_rule_match_one *r;
	ppe_acl_rule_match_type_t rule_t;
	enum ppe_drv_acl_slice_type slice_t;
	struct ppe_acl_rule_action *r_action;
	struct ppe_drv_acl_hw_info hw_info = {0};

	/*
	 * Current ACL count.
	 */
	if ((result = ppe_acl_dump_prefix_index_add(adi, adi->acl_cnt))) {
		goto error;
	}

	/*
	 * Rule information
	 */
	if ((result = ppe_acl_dump_prefix_add(adi, "rule"))) {
		goto error;
	}

	if ((result = ppe_acl_dump_write(adi, "user_id", "%d", acl->rule_id))) {
		goto error;
	}

	ppe_drv_acl_hw_info_get(acl->ctx, &hw_info);
	if ((result = ppe_acl_dump_write(adi, "hw_rule_id", "%d", hw_info.hw_rule_id))) {
		goto error;
	}

	if ((result = ppe_acl_dump_write(adi, "hw_list_id", "%d", hw_info.hw_list_id))) {
		goto error;
	}

	if ((result = ppe_acl_dump_write(adi, "hw_num_slices", "%d", hw_info.hw_num_slices))) {
		goto error;
	}

	if (acl->rule.cmn.cmn_flags & PPE_ACL_RULE_CMN_FLAG_FLOW_QOS_OVERRIDE) {
		if ((result = ppe_acl_dump_write(adi, "flow_qos_override", "%s", "true"))) {
			goto error;
		}
	}

	if ((result = ppe_acl_dump_write(adi, "priority", "%d", acl->pri))) {
		goto error;
	}

	if ((result = ppe_acl_dump_write(adi, "ipo_type", "%s", ppe_acl_dump_ipo_type_str[acl->ipo]))) {
		goto error;
	}

	if ((result = ppe_acl_dump_write(adi, "src_type", "%s", ppe_acl_dump_src_type_str[acl->rule.dev_type]))) {
		goto error;
	}

	dev_type = acl->rule.dev_type;
	switch (dev_type) {
	case PPE_ACL_RULE_DEV_TYPE_SRC_DEV:
		if ((result = ppe_acl_dump_write(adi, "src_dev", "%s", acl->rule.dev.dev_name))) {
			goto error;
		}

		break;

	case PPE_ACL_RULE_DEV_TYPE_SC:
		if ((result = ppe_acl_dump_write(adi, "dev_sc", "%d", acl->rule.dev.sc))) {
			goto error;
		}

		break;

	case PPE_ACL_RULE_DEV_TYPE_FLOW:
		if ((result = ppe_acl_dump_write(adi, "dev_flow", "%d", "true"))) {
			goto error;
		}

		break;

	case PPE_ACL_RULE_DEV_TYPE_DEST_L2_PORT:
		if ((result = ppe_acl_dump_write(adi, "dst_dev", "%s", acl->rule.dev.dev_name))) {
			goto error;
		}

		break;

	case PPE_ACL_RULE_DEV_TYPE_DEST_L3_PORT:
		if ((result = ppe_acl_dump_write(adi, "l3 dst_dev", "%s", acl->rule.dev.dev_name))) {
			goto error;
		}

		break;
	}

	if ((result = ppe_acl_dump_write(adi, "group_id", "%d", acl->info.cmn.res_chain))) {
		goto error;
	}

	if ((result = ppe_acl_dump_write(adi, "slice_cnt", "%d", acl->slice_cnt))) {
		goto error;
	}

	for (slice_t = 0; slice_t < PPE_DRV_ACL_SLICE_TYPE_MAX; slice_t++) {
		if (!acl->slice_type[slice_t]) {
			continue;
		}

		if ((result = ppe_acl_dump_write(adi, "slice_type", "%s",
						ppe_acl_dump_slice_type_str[slice_t]))) {
			goto error;
		}
	}

	for (rule_t = 0; rule_t < PPE_ACL_RULE_MATCH_TYPE_MAX; rule_t++) {
		if (!(acl->rule.valid_flags & (1 << rule_t))) {
			continue;
		}

		r = &acl->rule.rules[rule_t];

		switch (rule_t) {
		case PPE_ACL_RULE_MATCH_TYPE_SMAC:
			if ((result = ppe_acl_dump_prefix_add(adi, "smac"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "addr", "%pM", r->rule.smac.mac))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_MAC_MASK) {
				if ((result = ppe_acl_dump_write(adi, "mask", "%pM",
								r->rule.smac.mac_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_DMAC:
			if ((result = ppe_acl_dump_prefix_add(adi, "dmac"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "addr", "%pM",
							r->rule.dmac.mac))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_MAC_MASK) {
				if ((result = ppe_acl_dump_write(adi, "mask", "%pM",
							r->rule.dmac.mac_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_SVID:
			if ((result = ppe_acl_dump_prefix_add(adi, "svid"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "vid", "%d",
							r->rule.svid.vid_min))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_STAG_FMT) {
				if ((result = ppe_acl_dump_write(adi, "tag fmt", "%d",
								r->rule.svid.tag_fmt))) {
					goto error;
				}
				if ((result = ppe_acl_dump_write(adi, "tag fmt mask", "%d",
								r->rule.svid.tag_fmt_mask))) {
					goto error;
				}
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_SVID_MASK) {
				if ((result = ppe_acl_dump_write(adi, "mask_max", "%d",
							r->rule.svid.vid_mask_max))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "range_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_FLAG_SVID_RANGE)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_CVID:
			if ((result = ppe_acl_dump_prefix_add(adi, "cvid"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "vid", "%d",
							r->rule.cvid.vid_min))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_CTAG_FMT) {
				if ((result = ppe_acl_dump_write(adi, "tag fmt", "%d",
								r->rule.cvid.tag_fmt))) {
					goto error;
				}
				if ((result = ppe_acl_dump_write(adi, "tag fmt mask", "%d",
								r->rule.cvid.tag_fmt_mask))) {
					goto error;
				}
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_CVID_MASK) {
				if ((result = ppe_acl_dump_write(adi, "mask_max", "%d",
							r->rule.cvid.vid_mask_max))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "range_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_FLAG_CVID_RANGE)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_SPCP:
			if ((result = ppe_acl_dump_prefix_add(adi, "spcp"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "pcp", "%d",
							r->rule.spcp.pcp_mask))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_SPCP_MASK) {
				if ((result = ppe_acl_dump_write(adi, "mask", "%d",
							r->rule.spcp.pcp_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_CPCP:
			if ((result = ppe_acl_dump_prefix_add(adi, "cpcp"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "pcp", "%d",
							r->rule.cpcp.pcp_mask))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_CPCP_MASK) {
				if ((result = ppe_acl_dump_write(adi, "mask", "%d",
							r->rule.cpcp.pcp_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_PPPOE_SESS:
			if ((result = ppe_acl_dump_prefix_add(adi, "pppoe"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "ssesion_id", "%d",
							r->rule.pppoe_sess.pppoe_session_id))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_PPPOE_MASK) {
				if ((result = ppe_acl_dump_write(adi, "mask", "%d",
							r->rule.pppoe_sess.pppoe_session_id_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_ETHER_TYPE:
			if ((result = ppe_acl_dump_prefix_add(adi, "ether_type"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "proto", "%d",
							r->rule.ether_type.l2_proto))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_ETHTYPE_MASK) {
				if ((result = ppe_acl_dump_write(adi, "mask", "%d",
							r->rule.ether_type.l2_proto_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_L3_1ST_FRAG:
			if ((result = ppe_acl_dump_prefix_add(adi, "1st_frag"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "1st_frag", "%s", "true"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_IP_LEN:
			if ((result = ppe_acl_dump_prefix_add(adi, "l3_len"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "len", "%d",
							r->rule.l3_len.l3_length_min))) {
				goto error;
			}

			if ((r->rule_flags & PPE_ACL_RULE_FLAG_IPLEN_MASK)
					|| (r->rule_flags & PPE_ACL_RULE_FLAG_IPLEN_RANGE)) {
				if ((result = ppe_acl_dump_write(adi, "mask_max", "%d",
								r->rule.l3_len.l3_length_mask_max))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "range_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_FLAG_IPLEN_RANGE)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_TTL_HOPLIMIT:
			if ((result = ppe_acl_dump_prefix_add(adi, "ttl_hl"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "val", "%d",
							r->rule.ttl_hop.hop_limit))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_TTL_HOPLIMIT_MASK) {
				if ((result = ppe_acl_dump_write(adi, "mask", "%d",
								r->rule.ttl_hop.hop_limit_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_TOS_TC:
			if ((result = ppe_acl_dump_prefix_add(adi, "tos_tc"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "val", "%d",
							r->rule.tos_tc.l3_tos_tc))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_TOS_TC_MASK) {
				if ((result = ppe_acl_dump_write(adi, "mask", "%d",
							r->rule.tos_tc.l3_tos_tc_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_PROTO_NEXTHDR:
			if ((result = ppe_acl_dump_prefix_add(adi, "l4_proto"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "proto_nh", "%d",
							r->rule.proto_nexthdr.l3_v4proto_v6nexthdr))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_PROTO_NEXTHDR_MASK) {
				if ((result = ppe_acl_dump_write(adi, "mask", "%d",
							r->rule.proto_nexthdr.l3_v4proto_v6nexthdr_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_IP_GEN:
			if ((result = ppe_acl_dump_prefix_add(adi, "ip_misc"))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_L3_FRAG) {
				if ((result = ppe_acl_dump_write(adi, "l3_frag", "%s", "true"))) {
					goto error;
				}
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_ESP_HDR) {
				if ((result = ppe_acl_dump_write(adi, "esp_hdr", "%s", "true"))) {
					goto error;
				}
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_AH_HDR) {
				if ((result = ppe_acl_dump_write(adi, "ah_hdr", "%s", "true"))) {
					goto error;
				}
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_MOBILITY_HDR) {
				if ((result = ppe_acl_dump_write(adi, "mobility_hdr", "%s", "true"))) {
					goto error;
				}
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_OTHER_EXT_HDR) {
				if ((result = ppe_acl_dump_write(adi, "other_ext_hdr", "%s", "true"))) {
					goto error;
				}
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_FRAG_HDR) {
				if ((result = ppe_acl_dump_write(adi, "frag_hdr", "%s", "true"))) {
					goto error;
				}
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_IPV4_OPTION) {
				if ((result = ppe_acl_dump_write(adi, "ip_option", "%s", "true"))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_TCP_FLAG:
			if ((result = ppe_acl_dump_prefix_add(adi, "tcp_flag"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "flags", "%d",
							r->rule.tcp_flag.tcp_flags))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_TCP_FLG_MASK) {
				if ((result = ppe_acl_dump_write(adi, "mask", "%d",
							r->rule.tcp_flag.tcp_flags_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
						(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
						? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_SIP:
			if ((result = ppe_acl_dump_prefix_add(adi, "sip"))) {
				goto error;
			}

			if (r->rule.sip.ip_type == PPE_ACL_IP_TYPE_V4) {
				if ((result = ppe_acl_dump_write(adi, "v4", "%pI4",
								&r->rule.sip.ip[0]))) {
					goto error;
				}

				if (r->rule_flags & PPE_ACL_RULE_FLAG_SIP_MASK) {
					if ((result = ppe_acl_dump_write(adi, "mask", "%pI4",
								&r->rule.sip.ip_mask[0]))) {
						goto error;
					}
				}

				if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
								(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
								? "true": "false"))) {
					goto error;
				}
			} else if (r->rule.sip.ip_type == PPE_ACL_IP_TYPE_V6) {
				if ((result = ppe_acl_dump_write(adi, "v6", "%pI6",
								&r->rule.sip.ip[0]))) {
					goto error;
				}

				if (r->rule_flags & PPE_ACL_RULE_FLAG_SIP_MASK) {
					if ((result = ppe_acl_dump_write(adi, "mask", "%pI6",
								&r->rule.sip.ip_mask[0]))) {
						goto error;
					}
				}

				if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
								(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
								? "true": "false"))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_DIP:
			if ((result = ppe_acl_dump_prefix_add(adi, "dip"))) {
				goto error;
			}

			if (r->rule.dip.ip_type == PPE_ACL_IP_TYPE_V4) {
				if ((result = ppe_acl_dump_write(adi, "v4", "%pI4",
								&r->rule.dip.ip[0]))) {
					goto error;
				}

				if (r->rule_flags & PPE_ACL_RULE_FLAG_DIP_MASK) {
					if ((result = ppe_acl_dump_write(adi, "mask", "%pI4",
									&r->rule.dip.ip_mask[0]))) {
						goto error;
					}
				}

				if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
								(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
								? "true": "false"))) {
					goto error;
				}
			} else if (r->rule.dip.ip_type == PPE_ACL_IP_TYPE_V6) {
				if ((result = ppe_acl_dump_write(adi, "v6", "%pI6",
								&r->rule.dip.ip[0]))) {
					goto error;
				}

				if (r->rule_flags & PPE_ACL_RULE_FLAG_DIP_MASK) {
					if ((result = ppe_acl_dump_write(adi, "mask", "%pI6",
									&r->rule.dip.ip_mask[0]))) {
						goto error;
					}
				}

				if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
								(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
								? "true": "false"))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_SPORT:
			if ((result = ppe_acl_dump_prefix_add(adi, "sport"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "port", "%d",
							r->rule.sport.l4_port_min))) {
				goto error;
			}

			if (r->rule_flags & (PPE_ACL_RULE_FLAG_SPORT_MASK | PPE_ACL_RULE_FLAG_SPORT_RANGE)) {
				if ((result = ppe_acl_dump_write(adi, "mask_max", "%d",
								r->rule.sport.l4_port_max_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
							(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
							? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "range_en", "%s",
							(r->rule_flags & PPE_ACL_RULE_FLAG_SPORT_RANGE)
							? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_DPORT:
			if ((result = ppe_acl_dump_prefix_add(adi, "dport"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "port", "%d",
							r->rule.dport.l4_port_min))) {
				goto error;
			}

			if (r->rule_flags & (PPE_ACL_RULE_FLAG_DPORT_MASK | PPE_ACL_RULE_FLAG_DPORT_RANGE)) {
				if ((result = ppe_acl_dump_write(adi, "mask_max", "%d",
								r->rule.dport.l4_port_max_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
							(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
							? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "range_en", "%s",
							(r->rule_flags & PPE_ACL_RULE_FLAG_DPORT_RANGE)
							? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_UDF:
			if ((result = ppe_acl_dump_prefix_add(adi, "UDF_with_offset"))) {
				goto error;
			}

			if (r->rule.udf.udf_a.valid_op) {
				if ((result = ppe_acl_dump_write(adi, "udf_0_op", "%s",
								ppe_acl_dump_udf_op_from_str[r->rule.udf.udf_a.valid_op]))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_0", "0x%x",
								r->rule.udf.udf_a.min))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_0_mask_max", "0x%x",
								r->rule.udf.udf_a.mask_max))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_0_offset", "%d",
								r->rule.udf.udf_a.offset))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_0_proto", "%s",
								ppe_acl_dump_udf_type_str[r->rule.udf.udf_a.proto]))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_0_offset_base", "%s",
								ppe_acl_dump_udf_offset_from_str[r->rule.udf.udf_a.offset_base]))) {
					goto error;
				}
			}

			if (r->rule.udf.udf_b.valid_op) {
				if ((result = ppe_acl_dump_write(adi, "udf_1_op", "%s",
								ppe_acl_dump_udf_op_from_str[r->rule.udf.udf_b.valid_op]))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_1", "0x%x",
								r->rule.udf.udf_b.min))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_1_mask_max", "0x%x",
								r->rule.udf.udf_b.mask_max))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_1_offset", "%d",
								r->rule.udf.udf_b.offset))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_1_proto", "%s",
								ppe_acl_dump_udf_type_str[r->rule.udf.udf_b.proto]))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_1_offset_base", "%s",
								ppe_acl_dump_udf_offset_from_str[r->rule.udf.udf_b.offset_base]))) {
					goto error;
				}
			}

			if (r->rule.udf.udf_c.valid_op) {
				if ((result = ppe_acl_dump_write(adi, "udf_2_op", "%s",
								ppe_acl_dump_udf_op_from_str[r->rule.udf.udf_c.valid_op]))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_2", "0x%x",
								r->rule.udf.udf_c.min))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_2_mask_max", "0x%x",
								r->rule.udf.udf_c.mask_max))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_2_offset", "%d",
								r->rule.udf.udf_c.offset))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_2_proto", "%s",
								ppe_acl_dump_udf_type_str[r->rule.udf.udf_c.proto]))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_2_offset_base", "%s",
								ppe_acl_dump_udf_offset_from_str[r->rule.udf.udf_c.offset_base]))) {
					goto error;
				}
			}

			if (r->rule.udf.udf_d.valid_op) {
				if ((result = ppe_acl_dump_write(adi, "udf_3_op", "%s",
								ppe_acl_dump_udf_op_from_str[r->rule.udf.udf_d.valid_op]))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_3", "0x%x",
								r->rule.udf.udf_d.min))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_3_mask_max", "0x%x",
								r->rule.udf.udf_d.mask_max))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_3_offset", "%d",
								r->rule.udf.udf_d.offset))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_3_proto", "%s",
								ppe_acl_dump_udf_type_str[r->rule.udf.udf_d.proto]))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_3_offset_base", "%s",
								ppe_acl_dump_udf_offset_from_str[r->rule.udf.udf_d.offset_base]))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_UDF_WITH_IP:
			if ((result = ppe_acl_dump_prefix_add(adi, "UDF_with_IP"))) {
				goto error;
			}

			if (r->rule.udf_with_ip.ip_misc.flags & PPE_ACL_IP_MISC_FLAG_L3_FRAG) {
				if ((result = ppe_acl_dump_write(adi, "ip_misc_l3_frag", "%s", "true"))) {
					goto error;
				}
			}

			if (r->rule.udf_with_ip.ip_misc.flags & PPE_ACL_IP_MISC_FLAG_ESP_HDR) {
				if ((result = ppe_acl_dump_write(adi, "ip_misc_esp_hdr", "%s", "true"))) {
					goto error;
				}
			}

			if (r->rule.udf_with_ip.ip_misc.flags & PPE_ACL_IP_MISC_FLAG_AH_HDR) {
				if ((result = ppe_acl_dump_write(adi, "ip_misc_ah_hdr", "%s", "true"))) {
					goto error;
				}
			}

			if (r->rule.udf_with_ip.ip_misc.flags & PPE_ACL_IP_MISC_FLAG_MOBILITY_HDR) {
				if ((result = ppe_acl_dump_write(adi, "ip_misc_mobility_hdr", "%s", "true"))) {
					goto error;
				}
			}

			if (r->rule.udf_with_ip.ip_misc.flags & PPE_ACL_IP_MISC_FLAG_OTHER_EXT_HDR) {
				if ((result = ppe_acl_dump_write(adi, "ip_misc_other_ext_hdr", "%s", "true"))) {
					goto error;
				}
			}

			if (r->rule.udf_with_ip.ip_misc.flags & PPE_ACL_IP_MISC_FLAG_FRAG_HDR) {
				if ((result = ppe_acl_dump_write(adi, "ip_misc_frag_hdr", "%s", "true"))) {
					goto error;
				}
			}

			if (r->rule.udf_with_ip.ip_misc.flags & PPE_ACL_IP_MISC_FLAG_IPV4_OPTION) {
				if ((result = ppe_acl_dump_write(adi, "ip_misc_ipv4_option", "%s", "true"))) {
					goto error;
				}
			}

			if (r->rule.udf_with_ip.udf_a.valid_op) {
				if ((result = ppe_acl_dump_write(adi, "udf_0_op", "%s",
								ppe_acl_dump_udf_op_from_str[r->rule.udf_with_ip.udf_a.valid_op]))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_0", "0x%x",
								r->rule.udf_with_ip.udf_a.min))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_0_mask_max", "0x%x",
								r->rule.udf_with_ip.udf_a.mask_max))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_0_offset", "%d",
								r->rule.udf_with_ip.udf_a.offset))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_0_proto", "%s",
								ppe_acl_dump_udf_type_str[r->rule.udf_with_ip.udf_a.proto]))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_0_offset_base", "%s",
								ppe_acl_dump_udf_offset_from_str[r->rule.udf_with_ip.udf_a.offset_base]))) {
					goto error;
				}
			}

			if (r->rule.udf_with_ip.udf_b.valid_op) {
				if ((result = ppe_acl_dump_write(adi, "udf_1_op", "%s",
								ppe_acl_dump_udf_op_from_str[r->rule.udf_with_ip.udf_b.valid_op]))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_1", "0x%x",
								r->rule.udf_with_ip.udf_b.min))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_1_mask_max", "0x%x",
								r->rule.udf_with_ip.udf_b.mask_max))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_1_offset", "%d",
								r->rule.udf_with_ip.udf_b.offset))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_1_proto", "%s",
								ppe_acl_dump_udf_type_str[r->rule.udf_with_ip.udf_b.proto]))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_1_offset_base", "%s",
								ppe_acl_dump_udf_offset_from_str[r->rule.udf_with_ip.udf_b.offset_base]))) {
					goto error;
				}
			}

			if (r->rule.udf_with_ip.udf_c.valid_op) {
				if ((result = ppe_acl_dump_write(adi, "udf_2_op", "%s",
								ppe_acl_dump_udf_op_from_str[r->rule.udf_with_ip.udf_c.valid_op]))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_2", "0x%x",
								r->rule.udf_with_ip.udf_c.min))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_2_mask_max", "0x%x",
								r->rule.udf_with_ip.udf_c.mask_max))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_2_offset", "%d",
								r->rule.udf_with_ip.udf_c.offset))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_2_proto", "%s",
								ppe_acl_dump_udf_type_str[r->rule.udf_with_ip.udf_c.proto]))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_2_offset_base", "%s",
								ppe_acl_dump_udf_offset_from_str[r->rule.udf_with_ip.udf_c.offset_base]))) {
					goto error;
				}
			}

			if (r->rule.udf_with_ip.udf_d.valid_op) {
				if ((result = ppe_acl_dump_write(adi, "udf_3_op", "%s",
								ppe_acl_dump_udf_op_from_str[r->rule.udf_with_ip.udf_d.valid_op]))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_3", "0x%x",
								r->rule.udf_with_ip.udf_d.min))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_3_mask_max", "0x%x",
								r->rule.udf_with_ip.udf_d.mask_max))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_3_offset", "%d",
								r->rule.udf_with_ip.udf_d.offset))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_3_proto", "%s",
								ppe_acl_dump_udf_type_str[r->rule.udf_with_ip.udf_d.proto]))) {
					goto error;
				}

				if ((result = ppe_acl_dump_write(adi, "udf_3_offset_base", "%s",
								ppe_acl_dump_udf_offset_from_str[r->rule.udf_with_ip.udf_d.offset_base]))) {
					goto error;
				}
			}

			/*
			 * Dump is_ipv6 at the end, matching the order in ppe_drv_acl.c
			 * where rule type is set after all field matching is configured.
			 */
			if ((result = ppe_acl_dump_write(adi, "is_ipv6", "%s",
							r->rule.udf_with_ip.is_ipv6 ? "true" : "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
		case PPE_ACL_RULE_MATCH_TYPE_CTPID:
			if ((result = ppe_acl_dump_prefix_add(adi, "CTPID"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "ctpid_val", "%d",
							r->rule.ctpid.tpid_val))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_CTPID_EN) {
				if ((result = ppe_acl_dump_write(adi, "ctpid_mask", "%d",
								r->rule.ctpid.tpid_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
							(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
							? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_STPID:
			if ((result = ppe_acl_dump_prefix_add(adi, "STPID"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "stpid_val", "%d",
							r->rule.stpid.tpid_val))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_STPID_EN) {
				if ((result = ppe_acl_dump_write(adi, "stpid_mask", "%d",
								r->rule.stpid.tpid_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
							(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
							? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_CDEI:
			if ((result = ppe_acl_dump_prefix_add(adi, "CDEI"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "cdei", "%d",
							r->rule.cdei.dei))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_CDEI_EN) {
				if ((result = ppe_acl_dump_write(adi, "cdei_mask", "%d",
								r->rule.cdei.dei_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
							(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
							? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_SDEI:
			if ((result = ppe_acl_dump_prefix_add(adi, "SDEI"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "sdei", "%d",
							r->rule.sdei.dei))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_SDEI_EN) {
				if ((result = ppe_acl_dump_write(adi, "sdei_mask", "%d",
								r->rule.sdei.dei_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
							(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
							? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_DHCP_TYPE:
			if ((result = ppe_acl_dump_prefix_add(adi, "DHCP_TYPE"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "dhcp_type", "%s",
						ppe_acl_dump_dhcp_type_str[r->rule.dhcp_type.dhcp_type]))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_DHCP_TYPE_EN) {
				if ((result = ppe_acl_dump_write(adi, "dhcp_mask", "%d",
								r->rule.dhcp_type.dhcp_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
							(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
							? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;

		case PPE_ACL_RULE_MATCH_TYPE_MC_TYPE:
			if ((result = ppe_acl_dump_prefix_add(adi, "MC_TYPE"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_write(adi, "mc_type", "%s",
						ppe_acl_dump_mc_type_str[r->rule.mc_type.mc_type]))) {
				goto error;
			}

			if (r->rule_flags & PPE_ACL_RULE_FLAG_MC_TYPE_EN) {
				if ((result = ppe_acl_dump_write(adi, "mc_mask", "%d",
								r->rule.mc_type.mc_mask))) {
					goto error;
				}
			}

			if ((result = ppe_acl_dump_write(adi, "inverse_en", "%s",
							(r->rule_flags & PPE_ACL_RULE_GEN_FLAG_INVERSE_EN)
							? "true": "false"))) {
				goto error;
			}

			if ((result = ppe_acl_dump_prefix_remove(adi))) {
				goto error;
			}

			break;
#endif
		case PPE_ACL_RULE_MATCH_TYPE_DEFAULT:
			if ((result = ppe_acl_dump_write(adi, "default_rule", "%s", "true"))) {
				goto error;
			}

			break;

		default:
			ppe_acl_warn("%p: invalid rule type: %d", r, rule_t);
			break;
		}
	}

	/*
	 * Remove 'rule' from action.
	 */
	if ((result = ppe_acl_dump_prefix_remove(adi))) {
		goto error;
	}

	/*
	 * Action information.
	 */
	r_action = &acl->rule.action;
	if ((result = ppe_acl_dump_prefix_add(adi, "action"))) {
		goto error;
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_SERVICE_CODE_EN) {
		if ((result = ppe_acl_dump_write(adi, "service_code", "%d", r_action->service_code))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_QID_EN) {
		if ((result = ppe_acl_dump_write(adi, "qid", "%d", r_action->qid))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_ENQUEUE_PRI_CHANGE_EN) {
		if ((result = ppe_acl_dump_write(adi, "enqueue_pri", "%d", r_action->enqueue_pri))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_CTAG_DEI_CHANGE_EN) {
		if ((result = ppe_acl_dump_write(adi, "ctag_dei", "%s", "true"))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_STAG_DEI_CHANGE_EN) {
		if ((result = ppe_acl_dump_write(adi, "stag_dei", "%s", "true"))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_STAG_PCP_CHANGE_EN) {
		if ((result = ppe_acl_dump_write(adi, "stag_pcp", "%d", r_action->stag_pcp))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_TOS_TC_CHANGE_EN) {
		if ((result = ppe_acl_dump_write(adi, "tos_tc", "%d", r_action->tos_tc))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_CVID_CHANGE_EN) {
		if ((result = ppe_acl_dump_write(adi, "cvid", "%d", r_action->cvid))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_SVID_CHANGE_EN) {
		if ((result = ppe_acl_dump_write(adi, "svid", "%d", r_action->svid))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_DEST_INFO_CHANGE_EN) {
		if ((result = ppe_acl_dump_write(adi, "dest_dev", "%s", r_action->dst.dev_name))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_MIRROR_EN) {
		if ((result = ppe_acl_dump_write(adi, "mirror", "%s", "true"))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_REDIR_TO_CORE_EN) {
		if ((result = ppe_acl_dump_write(adi, "redir_core", "%d", r_action->redir_core))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_POLICER_EN) {
		if ((result = ppe_acl_dump_write(adi, "policer_id", "%d", r_action->policer_id))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_FW_CMD) {
		if ((result = ppe_acl_dump_write(adi, "fwd_cmd", "%d", r_action->fwd_cmd))) {
			goto error;
		}
	}

#ifdef NSS_PPE_FEATURE_EXCEPTION_EDIT
	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_EXCEPTION_EDIT_EN) {
		if ((result = ppe_acl_dump_write(adi, "exception_edit_en", "%s", "true"))) {
			goto error;
		}

		if ((result = ppe_acl_dump_write(adi, "service_code", "%d", PPE_DRV_SC_PKT_EXCEPTION_EDIT_EN))) {
			goto error;
		}
	}
#endif

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_CTAG_PCP_CHANGE_EN) {
		if ((result = ppe_acl_dump_write(adi, "ctag_pcp", "%d", r_action->ctag_pcp))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_CTAG_PID_CHANGE_EN) {
		if ((result = ppe_acl_dump_write(adi, "ctag_pid", "%d", r_action->ctag_pid))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_STAG_PID_CHANGE_EN) {
		if ((result = ppe_acl_dump_write(adi, "stag_pid", "%d", r_action->stag_pid))) {
			goto error;
		}
	}

#ifdef NSS_PPE_EXT_VLAN_FEATURE_SUPPORT
	if (r_action->flags_ext & PPE_ACL_RULE_ACTION_FLAG_STAG_DEI_CMD) {
		if ((result = ppe_acl_dump_write(adi, "stag_dei_cmd", "%s",
						ppe_acl_dump_dei_cmd_type_str[r_action->stag_dei_cmd]))) {
			goto error;
		}
	}

	if (r_action->flags_ext & PPE_ACL_RULE_ACTION_FLAG_CTAG_DEI_CMD) {
		if ((result = ppe_acl_dump_write(adi, "ctag_dei_cmd", "%s",
						ppe_acl_dump_dei_cmd_type_str[r_action->ctag_dei_cmd]))) {
			goto error;
		}
	}

	if (r_action->flags_ext & PPE_ACL_RULE_ACTION_FLAG_STAG_PCP_CMD) {
		if ((result = ppe_acl_dump_write(adi, "stag_pcp_cmd", "%s",
						ppe_acl_dump_pcp_cmd_type_str[r_action->stag_pcp_cmd]))) {
			goto error;
		}
	}

	if (r_action->flags_ext & PPE_ACL_RULE_ACTION_FLAG_CTAG_PCP_CMD) {
		if ((result = ppe_acl_dump_write(adi, "ctag_pcp_cmd", "%s",
						ppe_acl_dump_pcp_cmd_type_str[r_action->ctag_pcp_cmd]))) {
			goto error;
		}
	}

	if (r_action->flags_ext & PPE_ACL_RULE_ACTION_FLAG_STAG_VID_CMD) {
		if ((result = ppe_acl_dump_write(adi, "stag_vid_cmd", "%s",
						ppe_acl_dump_vid_cmd_type_str[r_action->stag_vid_cmd]))) {
			goto error;
		}
	}

	if (r_action->flags_ext & PPE_ACL_RULE_ACTION_FLAG_CTAG_VID_CMD) {
		if ((result = ppe_acl_dump_write(adi, "ctag_vid_cmd", "%s",
						ppe_acl_dump_vid_cmd_type_str[r_action->ctag_vid_cmd]))) {
			goto error;
		}
	}

	if (r_action->flags_ext & PPE_ACL_RULE_ACTION_FLAG_STAG_PID_CMD) {
		if ((result = ppe_acl_dump_write(adi, "stag_pid_cmd", "%s",
						ppe_acl_dump_pid_cmd_type_str[r_action->stag_pid_cmd]))) {
			goto error;
		}
	}

	if (r_action->flags_ext & PPE_ACL_RULE_ACTION_FLAG_CTAG_PID_CMD) {
		if ((result = ppe_acl_dump_write(adi, "ctag_pid_cmd", "%s",
						ppe_acl_dump_pid_cmd_type_str[r_action->ctag_pid_cmd]))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_DSCP_PBIT_MAP_IDX) {
		if ((result = ppe_acl_dump_write(adi, "dscp_pbit_map_idx", "%d", r_action->dscp_pbit_map_idx))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_COUNTER_EN) {
		if ((result = ppe_acl_dump_write(adi, "counter id", "%d", r_action->counter_id))) {
			goto error;
		}
		if ((result = ppe_acl_dump_write(adi, "counter mode", "%s",
						ppe_acl_dump_counter_mode_type_str[r_action->counter_mode]))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_TAGS_TO_RMV_EN) {
		if ((result = ppe_acl_dump_write(adi, "Tags to remove", "%d", r_action->tags_to_rmv))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_INT_DP_CHANGE_EN) {
		if ((result = ppe_acl_dump_write(adi, "Internal drop precedence", "%d", r_action->int_dp))) {
			goto error;
		}
	}

	if (r_action->flags & PPE_ACL_RULE_ACTION_FLAG_SRC_INFO) {
		if ((result = ppe_acl_dump_write(adi, "Source info type", "%s",
						ppe_acl_dump_src_info_type_str[r_action->src_info_type]))) {
			goto error;
		}

		if ((result = ppe_acl_dump_write(adi, "Source info", "%d", r_action->src_info))) {
			goto error;
		}
	}
#endif
	/*
	 * Remove the 'action' prefix for next interation
	 */
	if ((result = ppe_acl_dump_prefix_remove(adi))) {
		goto error;
	}

	/*
	 * Match counters
	 */
	ppe_drv_acl_get_hw_stats(acl->ctx, &pkts, &bytes);

	if ((result = ppe_acl_dump_prefix_add(adi, "stats"))) {
		goto error;
	}

	if ((result = ppe_acl_dump_write(adi, "pkts", "%llu", pkts))) {
		goto error;
	}

	if ((result = ppe_acl_dump_write(adi, "bytes", "%llu", bytes))) {
		goto error;
	}

	/*
	 * Remove the 'stats' prefix for next interation
	 */
	if ((result = ppe_acl_dump_prefix_remove(adi))) {
		goto error;
	}

	/*
	 * Remove the index prefix for next interation
	 */
	if ((result = ppe_acl_dump_prefix_remove(adi))) {
		goto error;
	}

error:
	return result;
}

/*
 * ppe_acl_dump_all()
 *	Prepare acl dump information for all the active ACL rules.
 */
static bool ppe_acl_dump_all(struct ppe_acl_dump_instance *adi)
{
	struct ppe_acl_base *acl_g = &ppe_acl_gbl;
	struct ppe_acl *acl;
	int result;

	if ((result = ppe_acl_dump_write_reset(adi, "acl"))) {
		return result;
	}

	/*
	 * Get the first available acl rule ID.
	 */
	spin_lock_bh(&acl_g->lock);
	if (!list_empty(&acl_g->active_rules)) {
		adi->acl_cnt = 0;
		list_for_each_entry(acl, &acl_g->active_rules, list) {
			result = ppe_acl_dump_one(adi, acl);
			if (result < 0) {
				ppe_acl_warn("%p: failed to collect dump for acl: %p", acl_g, acl);
				spin_unlock_bh(&acl_g->lock);
				return result;
			}

			adi->acl_cnt++;
		}
	}

	spin_unlock_bh(&acl_g->lock);

	return 0;
}

/*
 * ppe_acl_dump_dev_open()
 *	Open the character device file for ACL dump.
 */
static int ppe_acl_dump_dev_open(struct inode *inode, struct file *file)
{
	struct ppe_acl_dump_instance *adi;
	struct ppe_acl_base *acl_g = &ppe_acl_gbl;

	/*
	 * Allocate state information for the reading
	 */
	ppe_acl_assert(file->private_data == NULL, "unexpected double open: %px?\n", file->private_data);

	adi = (struct ppe_acl_dump_instance *)kzalloc(sizeof(struct ppe_acl_dump_instance), GFP_ATOMIC | __GFP_NOWARN);
	if (!adi) {
		ppe_acl_warn("%p: unable to allocate memory for dump instance", acl_g);
		return -ENOMEM;
	}

	adi->dump_en = true;
	file->private_data = adi;

	return 0;
}

/*
 * ppe_acl_dump_dev_release()
 *	Close the character device file for ACL dump.
 */
static int ppe_acl_dump_dev_release(struct inode *inode, struct file *file)
{
	struct ppe_acl_dump_instance *adi = (struct ppe_acl_dump_instance *)file->private_data;
	if (adi) {
		kfree(adi);
	}

	return 0;
}

/*
 * ppe_acl_dump_dev_read()
 *	Read file operation handler
 */
static ssize_t ppe_acl_dump_dev_read(struct file *file, char *buffer, size_t length, loff_t *offset)
{
	int bytes_read = 0;
	struct ppe_acl_dump_instance *adi;
	struct ppe_acl_base *acl_g = &ppe_acl_gbl;

	adi = (struct ppe_acl_dump_instance *)file->private_data;
	if (!adi) {
		ppe_acl_warn("%p: unable to find dump instance", acl_g);
		return -ENOMEM;
	}

	/*
	 * If there is still some message remaining to be output then complete that first
	 */
	if (adi->msg_len) {
		goto read_output;
	}

	if (adi->dump_en) {
		if (ppe_acl_dump_all(adi)) {
			ppe_acl_warn("Failed to create acl dump\n");
			return -EIO;
		}

		adi->dump_en = false;
		goto read_output;
	}

	return 0;

read_output:
	/*
	 * If supplied buffer is small we limit what we output
	 */
	bytes_read = adi->msg_len;
	if (bytes_read > length) {
		bytes_read = length;
	}

	if (copy_to_user(buffer, adi->msgp, bytes_read)) {
		return -EIO;
	}

	adi->msg_len -= bytes_read;
	adi->msgp += bytes_read;

	ppe_acl_trace("%p: state read done, bytes_read: %d bytes, remaining msg_len: %d\n",
			adi, bytes_read, adi->msg_len);

	/*
	 * Most read functions return the number of bytes put into the buffer
	 */
	return bytes_read;
}

/*
 * ppe_acl_dump_dev_write()
 *	Write file operation handler
 */
static ssize_t ppe_acl_dump_dev_write(struct file *filp, const char *buff, size_t len, loff_t * off)
{
	/*
	 * Not supported.
	 */
	return -EINVAL;
}

/*
 * File operations associated with acl dump character device
 */
static struct file_operations ppe_acl_dump_fops = {
	.read = ppe_acl_dump_dev_read,
	.write = ppe_acl_dump_dev_write,
	.open = ppe_acl_dump_dev_open,
	.release = ppe_acl_dump_dev_release
};

/*
 * ppe_acl_dump_exit()
 *	Unregister character device for ACL dump
 */
void ppe_acl_dump_exit(void)
{
	struct ppe_acl_base *acl_g = &ppe_acl_gbl;

	unregister_chrdev(acl_g->acl_dump_major_id, "ppe_acl_dump_dev");
}

/*
 * ppe_acl_dump_init()
 *	Register a character device for ACL dump
 */
bool ppe_acl_dump_init(struct dentry *dentry)
{
	struct ppe_acl_base *acl_g = &ppe_acl_gbl;
	int dev_id = -1;

#if (LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0))
	if (!debugfs_create_u32("ppe_acl_dump", S_IRUGO, acl_g->dentry, (u32 *)&acl_g->acl_dump_major_id)) {
		ppe_acl_warn("%p: Failed to create ppe state dev major file in debugfs\n", acl_g);
		return false;
	}
#else
	debugfs_create_u32("ppe_acl_dump", S_IRUGO, acl_g->dentry, (u32 *)&acl_g->acl_dump_major_id);
#endif

	/*
	 * Register a character device to dump the output
	 */
	dev_id = register_chrdev(0, "ppe_acl_dump_dev", &ppe_acl_dump_fops);
	if (dev_id < 0) {
		ppe_acl_warn("%p: Failed to register chrdev %d\n", acl_g, dev_id);
		return false;
	}

	acl_g->acl_dump_major_id = dev_id;
	ppe_acl_trace("%p: acl dump dev major id %d\n", acl_g, acl_g->acl_dump_major_id);

	return true;
}
