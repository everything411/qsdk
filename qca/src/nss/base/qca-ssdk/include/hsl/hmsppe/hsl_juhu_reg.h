/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */


//appe_acl_reg.h
#undef PRE_IPO_CNT_TBL_NUM
#ifdef HMSPPE
#define PRE_IPO_CNT_TBL_NUM 128
#elif JHPPE
#define PRE_IPO_CNT_TBL_NUM 512
#endif

#undef PRE_IPO_RULE_EXT_1_NUM
#ifdef HMSPPE
#define PRE_IPO_RULE_EXT_1_NUM 16
#elif JHPPE
#define PRE_IPO_RULE_EXT_1_NUM 64
#endif

#undef PRE_IPO_RULE_EXT_2_NUM
#ifdef HMSPPE
#define PRE_IPO_RULE_EXT_2_NUM 16
#elif JHPPE
#define PRE_IPO_RULE_EXT_2_NUM 64
#endif

#undef PRE_IPO_RULE_EXT_4_NUM
#ifdef HMSPPE
#define PRE_IPO_RULE_EXT_4_NUM 16
#elif JHPPE
#define PRE_IPO_RULE_EXT_4_NUM 64
#endif

#undef PRE_IPO_ACTION_NUM
#ifdef HMSPPE
#define PRE_IPO_ACTION_NUM 128
#elif JHPPE
#define PRE_IPO_ACTION_NUM 512
#endif

#undef EG_IPO_EXT_TBL_NUM
#ifdef HMSPPE
#define EG_IPO_EXT_TBL_NUM 256
#elif JHPPE
#define EG_IPO_EXT_TBL_NUM 1024
#endif


//cppe_qos_reg.h
#undef QOS_MAPPING_TBL_NUM
#ifdef HMSPPE
#define QOS_MAPPING_TBL_NUM 4640
#elif JHPPE
#define QOS_MAPPING_TBL_NUM 16928
#endif


//hmsppe_prx_pon_reg.h
#undef GEM_PORT_MAPPING_CTRL_ADDRESS
#ifdef HMSPPE
#define GEM_PORT_MAPPING_CTRL_ADDRESS 0x130
#elif JHPPE
#define GEM_PORT_MAPPING_CTRL_ADDRESS 0xa600
#endif

#undef GEM_PORT_MAPPING_TBL_ADDRESS
#ifdef HMSPPE
#define GEM_PORT_MAPPING_TBL_ADDRESS 0x1800
#elif JHPPE
#define GEM_PORT_MAPPING_TBL_ADDRESS 0xa800
#endif

#undef PORT_MAPPING_TBL_ADDRESS
#ifdef HMSPPE
#define PORT_MAPPING_TBL_ADDRESS 0x2000
#elif JHPPE
#define PORT_MAPPING_TBL_ADDRESS 0xb000
#endif


//hppe_acl_reg.h
#undef IPO_CNT_TBL_NUM
#ifdef HMSPPE
#define IPO_CNT_TBL_NUM 128
#elif JHPPE
#define IPO_CNT_TBL_NUM 512
#endif

#undef IPO_ACTION_NUM
#ifdef HMSPPE
#define IPO_ACTION_NUM 128
#elif JHPPE
#define IPO_ACTION_NUM 512
#endif

#undef RULE_EXT_1_REG_NUM
#ifdef HMSPPE
#define RULE_EXT_1_REG_NUM 16
#elif JHPPE
#define RULE_EXT_1_REG_NUM 64
#endif

#undef RULE_EXT_2_REG_NUM
#ifdef HMSPPE
#define RULE_EXT_2_REG_NUM 16
#elif JHPPE
#define RULE_EXT_2_REG_NUM 64
#endif

#undef RULE_EXT_4_REG_NUM
#ifdef HMSPPE
#define RULE_EXT_4_REG_NUM 16
#elif JHPPE
#define RULE_EXT_4_REG_NUM 64
#endif


//hppe_flow_reg.h
#undef IN_FLOW_CNT_TBL_NUM
#ifdef HMSPPE
#define IN_FLOW_CNT_TBL_NUM 4096
#elif JHPPE
#define IN_FLOW_CNT_TBL_NUM 16384
#endif

#undef IN_FLOW_TBL_NUM
#ifdef HMSPPE
#define IN_FLOW_TBL_NUM 4096
#elif JHPPE
#define IN_FLOW_TBL_NUM 16384
#endif

#undef IN_FLOW_3TUPLE_TBL_NUM
#ifdef HMSPPE
#define IN_FLOW_3TUPLE_TBL_NUM 4096
#elif JHPPE
#define IN_FLOW_3TUPLE_TBL_NUM 16384
#endif

#undef IN_FLOW_IPV6_5TUPLE_TBL_NUM
#ifdef HMSPPE
#define IN_FLOW_IPV6_5TUPLE_TBL_NUM 2048
#elif JHPPE
#define IN_FLOW_IPV6_5TUPLE_TBL_NUM 8192
#endif

#undef IN_FLOW_IPV6_3TUPLE_TBL_NUM
#ifdef HMSPPE
#define IN_FLOW_IPV6_3TUPLE_TBL_NUM 2048
#elif JHPPE
#define IN_FLOW_IPV6_3TUPLE_TBL_NUM 8192
#endif

#undef EG_FLOW_IPV6_IID_TBL_NUM
#ifdef HMSPPE
#define EG_FLOW_IPV6_IID_TBL_NUM 2048
#elif JHPPE
#define EG_FLOW_IPV6_IID_TBL_NUM 8192
#endif

#undef EG_FLOW_TREE_MAP_TBL_NUM
#ifdef HMSPPE
#define EG_FLOW_TREE_MAP_TBL_NUM 4096
#elif JHPPE
#define EG_FLOW_TREE_MAP_TBL_NUM 16384
#endif


//hppe_ip_reg.h
#undef HOST_TBL_NUM
#ifdef HMSPPE
#define HOST_TBL_NUM 768
#elif JHPPE
#define HOST_TBL_NUM 6144
#endif

#undef HOST_IPV6_TBL_NUM
#ifdef HMSPPE
#define HOST_IPV6_TBL_NUM 384
#elif JHPPE
#define HOST_IPV6_TBL_NUM 3072
#endif

#undef HOST_IPV4_MCAST_TBL_NUM
#ifdef HMSPPE
#define HOST_IPV4_MCAST_TBL_NUM 384
#elif JHPPE
#define HOST_IPV4_MCAST_TBL_NUM 3072
#endif

#undef HOST_IPV6_MCAST_TBL_NUM
#ifdef HMSPPE
#define HOST_IPV6_MCAST_TBL_NUM 192
#elif JHPPE
#define HOST_IPV6_MCAST_TBL_NUM 1536
#endif

#undef IN_NEXTHOP_TBL_NUM
#ifdef HMSPPE
#define IN_NEXTHOP_TBL_NUM 768
#elif JHPPE
#define IN_NEXTHOP_TBL_NUM 2560
#endif


//hppe_policer_reg.h
#undef IN_ACL_METER_CFG_TBL_NUM
#ifdef HMSPPE
#define IN_ACL_METER_CFG_TBL_NUM 128
#elif JHPPE
#define IN_ACL_METER_CFG_TBL_NUM 512
#endif

#undef IN_ACL_METER_CRDT_TBL_NUM
#ifdef HMSPPE
#define IN_ACL_METER_CRDT_TBL_NUM 128
#elif JHPPE
#define IN_ACL_METER_CRDT_TBL_NUM 512
#endif

#undef IN_ACL_METER_CNT_TBL_NUM
#ifdef HMSPPE
#define IN_ACL_METER_CNT_TBL_NUM 384
#elif JHPPE
#define IN_ACL_METER_CNT_TBL_NUM 1536
#endif


//hppe_portvlan_reg.h
#undef XLT_RULE_TBL_NUM
#ifdef HMSPPE
#define XLT_RULE_TBL_NUM 256
#elif JHPPE
#define XLT_RULE_TBL_NUM 128
#endif

#undef XLT_ACTION_TBL_NUM
#ifdef HMSPPE
#define XLT_ACTION_TBL_NUM 256
#elif JHPPE
#define XLT_ACTION_TBL_NUM 128
#endif


//jhppe_acl_reg.h
#undef PRE_IPO_RULE_INNER_OUTER_NUM
#ifdef HMSPPE
#define PRE_IPO_RULE_INNER_OUTER_NUM 16
#elif JHPPE
#define PRE_IPO_RULE_INNER_OUTER_NUM 64
#endif


//jhppe_l3_flow_reg.h
#undef IN_FLOW_6TUPLE_TBL_NUM
#ifdef HMSPPE
#define IN_FLOW_6TUPLE_TBL_NUM 2048
#elif JHPPE
#define IN_FLOW_6TUPLE_TBL_NUM 8192
#endif


//jhppe_policer_reg.h
#undef SAWF_FLOW_BLOCK_MAP_NUM
#ifdef HMSPPE
#define SAWF_FLOW_BLOCK_MAP_NUM 16
#elif JHPPE
#define SAWF_FLOW_BLOCK_MAP_NUM 64
#endif


//jhppe_portvlan_reg.h
#undef VLAN_TO_PORT_MAPPING_CTRL_ADDRESS
#ifdef HMSPPE
#define VLAN_TO_PORT_MAPPING_CTRL_ADDRESS 0x104
#elif JHPPE
#define VLAN_TO_PORT_MAPPING_CTRL_ADDRESS 0xa100
#endif

#undef VLAN_TO_PORT_MAPPING_TBL_ADDRESS
#ifdef HMSPPE
#define VLAN_TO_PORT_MAPPING_TBL_ADDRESS 0x200
#elif JHPPE
#define VLAN_TO_PORT_MAPPING_TBL_ADDRESS 0xa200
#endif
