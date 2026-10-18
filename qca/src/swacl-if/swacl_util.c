/******************************************************************
 *
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
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
 *
 ******************************************************************/

#include <linux/version.h>
#include <linux/init.h>
#include <linux/ctype.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/netfilter.h>
#include <linux/unistd.h>

#include <linux/delay.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/list.h>

#include <linux/if_ether.h>
#include <linux/if_vlan.h>
#include <linux/if_arp.h>

#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/random.h>
#include <linux/ipv6.h>
#include <linux/inet.h>

#include <net/switchdev.h>
#include <linux/miscdevice.h>

#include "swacl_if.h"
#include "swacl_util.h"

extern struct swacl_if_priv g_swacl_priv;
int
swacl_if_str2argv(char *str, char *argv[], int argv_size)
{
	static char buff[SWACL_STRING_BUFF_LEN];
	char *p = buff;
	int i = 0;
	memset(buff,0x0,SWACL_STRING_BUFF_LEN);
	strlcpy(buff, str, SWACL_STRING_BUFF_LEN);
	while (p != NULL) {
		argv[i] = strsep(&p, " \t");
		if (strlen(argv[i]) > 0) {
			i++;
			if (i == argv_size - 1)
				break;
		}
	}
	argv[i] = 0;
	return i;
}

int
swacl_if_str2val(char *s)
{
	int val;
	if (s[0] == 'x') {
		sscanf(s + 1, "%x", &val);
	} else if (s[0] == '0' && s[1] == 'x') {
		sscanf(s + 2, "%x", &val);
	} else {
		sscanf(s, "%d", &val);
	}
	return val;
}
/* util_trim : removes leading and trailing spaces of a string */
char *
swacl_util_trim(char *s)
{
	int i;

	if (s) {
		i = strlen(s);
		while ((--i) > 0 && isspace(s[i]))
			s[i] = 0;
	}
	if (s) {
		while (isspace(*s))
			s++;
	}
	return s;
}
int
swacl_util_string_is_xdigit(char *str)
{
	char *s = str;
	int i;

	if (s[0] == '+' || s[0] == '-')
		s++;

	for (i=0; i<strlen(s); i++) {

		if (s[i]<'0' || s[i]>'9') {
			if(!(s[i]>='a' && s[i]<='f') && !(s[i]>='A' && s[i]<='F'))
				return 0;
		}
	}
	return 1;
}

int
swacl_util_string_is_digit(char *str)
{
	char *s = str;
	int i;

	if (s[0] == '+' || s[0] == '-')
		s++;

	for (i=0; i<strlen(s); i++) {

		if (s[i]<'0' || s[i]>'9')
			return 0;
	}
	return 1;
}

int
swacl_if_OUI_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%2.2x:%2.2x:%2.2x",rule_entry->oui[0],rule_entry->oui[1],rule_entry->oui[2]);
	return 0;
}

int
swacl_if_parse_OUI(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	char s1[2]= {':','\0'};
	char * p_str=NULL;
	unsigned int pos=0,value=0;
	char * buf = param_str;

	if(strlen(param_str)==0) {
		dbprintk("OUI: null string\n");
		return SWACL_IF_ERROR;
	}

	if((p_str = strstr(buf, s1)) == NULL) {
		dbprintk("OUI: format error string\n");
		return SWACL_IF_ERROR;
	}
	
	while ((p_str = strsep(&buf, s1)) != NULL) {

		p_str = swacl_util_trim(p_str);

		if(!swacl_util_string_is_xdigit(p_str)) {
			dbprintk("OUI: isn't xdigit\n");
			memset(rule_entry->oui,0x0,OUI_LEN);
			rule_entry->filter_flags |= ~SWACL_IF_filter_OUI;
			return SWACL_IF_ERROR;
		}

		value = simple_strtoul(p_str,NULL,16);
		rule_entry->oui[pos] = (unsigned char)value;
		pos++;
	}
	if(pos != OUI_LEN) {
		dbprintk("OUI: format error\n");
		memset(rule_entry->oui,0x0,OUI_LEN);
		rule_entry->filter_flags |= ~SWACL_IF_filter_OUI;
		return SWACL_IF_ERROR;
	}
	if(rule_entry->oui[0]==0 && rule_entry->oui[1]==0&&rule_entry->oui[2]==0) {
		dbprintk("OUI: all zero\n");
		return SWACL_IF_ERROR;
	}
	rule_entry->filter_flags |= SWACL_IF_filter_OUI;
	return SWACL_IF_SUCCESS;

}
int
swacl_if_ethertype_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"0x%4.4x",rule_entry->ethertype);
	return 0;
}

int
swacl_if_parse_ethertype(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	unsigned int value=0;
	char*        tmp_str;

	if(strlen(param_str)==0) {
		dbprintk("ETHERTYPE: null string\n");
		return SWACL_IF_ERROR;
	}

	param_str = swacl_util_trim(param_str);
	if(strncmp(param_str,"0x",2) != 0 && strncmp(param_str,"0X",2) != 0) {
		dbprintk("ETHERTYPE: format error\n");
		return SWACL_IF_ERROR;
	}
	tmp_str=param_str+2;
	if(!swacl_util_string_is_xdigit(tmp_str)) {
		dbprintk("ETHERTYPE: isn't xdigit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(tmp_str,NULL,16);
	if(value == 0) {
		dbprintk("ETHERTYPE: zero value\n");
		return SWACL_IF_ERROR;
	}
	rule_entry->ethertype = (unsigned short)value;
	rule_entry->filter_flags |= SWACL_IF_filter_ETHERTYPE;

	return SWACL_IF_SUCCESS;
}

int
swacl_if_dmac_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",rule_entry->dmac[0],rule_entry->dmac[1],rule_entry->dmac[2]
	        ,rule_entry->dmac[3],rule_entry->dmac[4],rule_entry->dmac[5]);
	return 0;

}
int
swacl_if_parse_dmac(char * param_str,struct swacl_if_rule_entry * rule_entry)
{

	char s1[2]= {':','\0'};
	char * p_str=NULL;
	unsigned int pos=0,value=0;
	char * buf = param_str;

	if(strlen(param_str)==0) {
		dbprintk("dmac: null string\n");
		return SWACL_IF_ERROR;
	}

	if((p_str = strstr(buf, s1)) == NULL) {
		dbprintk("dmac: format error string\n");
		return SWACL_IF_ERROR;
	}
	
	while ((p_str = strsep(&buf, s1)) != NULL) {

		p_str = swacl_util_trim(p_str);

		if(!swacl_util_string_is_xdigit(p_str)) {
			dbprintk("dmac: isn't xdigit\n");
			memset(rule_entry->dmac,0x0,ETH_ALEN);
			rule_entry->filter_flags |= ~SWACL_IF_filter_DMAC;
			return SWACL_IF_ERROR;
		}

		value = simple_strtoul(p_str,NULL,16);
		rule_entry->dmac[pos] = (unsigned char)value;
		pos++;
	}
	if(pos != ETH_ALEN) {
		dbprintk("dmac: format error\n");
		memset(rule_entry->dmac,0x0,ETH_ALEN);
		rule_entry->filter_flags |= ~SWACL_IF_filter_DMAC;
		return SWACL_IF_ERROR;
	}

	if(rule_entry->dmac[0]==0 && rule_entry->dmac[1]==0&&rule_entry->dmac[2]==0
	                &&rule_entry->dmac[3]==0 && rule_entry->dmac[4]==0&&rule_entry->dmac[5]==0) {
		dbprintk("dmac: all zero\n");
		return SWACL_IF_ERROR;
	}
	rule_entry->filter_flags |= SWACL_IF_filter_DMAC;
	return SWACL_IF_SUCCESS;
}

int
swacl_if_dmac_prefix_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%d",rule_entry->dmac_prefix);
	return 0;
}

int
swacl_if_parse_dmac_prefix(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	unsigned int value=0;

	if(strlen(param_str)==0) {
		dbprintk("dmac_prefix: null string\n");
		return SWACL_IF_ERROR;
	}

	param_str = swacl_util_trim(param_str);
	if(!swacl_util_string_is_digit(param_str)) {
		dbprintk("dmac_prefix: isn't digit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(param_str,NULL,10);
	if(value > 48 || (value%8) !=0 || value ==0) {
		dbprintk("dmac_prefix: range err(%d)\n",value);
		return SWACL_IF_ERROR;
	}
	rule_entry->dmac_prefix = (unsigned short)value;
	rule_entry->filter_flags |= SWACL_IF_filter_DMAC_PRE;

	return SWACL_IF_SUCCESS;
}

int
swacl_if_smac_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x",rule_entry->smac[0],rule_entry->smac[1],rule_entry->smac[2]
	        ,rule_entry->smac[3],rule_entry->smac[4],rule_entry->smac[5]);
	return 0;
}

int
swacl_if_parse_smac(char * param_str,struct swacl_if_rule_entry * rule_entry)
{

	char s1[2]= {':','\0'};
	char * p_str=NULL;
	unsigned int pos=0,value=0;
	char * buf = param_str;

	if(strlen(param_str)==0) {
		dbprintk("smac: null string\n");
		return SWACL_IF_ERROR;
	}

	if((p_str = strstr(buf, s1)) == NULL) {
		dbprintk("smac: format error string\n");
		return SWACL_IF_ERROR;
	}
	
	while ((p_str = strsep(&buf, s1)) != NULL) {

		p_str = swacl_util_trim(p_str);

		if(!swacl_util_string_is_xdigit(p_str)) {
			dbprintk("smac: isn't xdigit\n");
			memset(rule_entry->dmac,0x0,ETH_ALEN);
			rule_entry->filter_flags |= ~SWACL_IF_filter_SMAC;
			return SWACL_IF_ERROR;
		}

		value = simple_strtoul(p_str,NULL,16);
		rule_entry->smac[pos] = (unsigned char)value;
		pos++;
	}



	if(pos != ETH_ALEN) {
		dbprintk("smac: format error\n");
		memset(rule_entry->dmac,0x0,ETH_ALEN);
		rule_entry->filter_flags |= ~SWACL_IF_filter_SMAC;
		return SWACL_IF_ERROR;
	}

	if(rule_entry->smac[0]==0 && rule_entry->smac[1]==0&&rule_entry->smac[2]==0
	                &&rule_entry->smac[3]==0 && rule_entry->smac[4]==0&&rule_entry->smac[5]==0) {
		dbprintk("smac: all zero\n");
		return SWACL_IF_ERROR;
	}
	rule_entry->filter_flags |= SWACL_IF_filter_SMAC;
	return SWACL_IF_SUCCESS;
}

int
swacl_if_smac_prefix_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%d",rule_entry->smac_prefix);
	return 0;
}

int
swacl_if_parse_smac_prefix(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	unsigned int value=0;

	if(strlen(param_str)==0) {
		dbprintk("smac_prefix: null string\n");
		return SWACL_IF_ERROR;
	}

	param_str = swacl_util_trim(param_str);
	if(!swacl_util_string_is_digit(param_str)) {
		dbprintk("smac_prefix: isn't digit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(param_str,NULL,10);

	if(value > 48 || (value%8) !=0 || value ==0) {
		dbprintk("smac_prefix: range err(%d)\n",value);
		return SWACL_IF_ERROR;
	}
	rule_entry->smac_prefix = (unsigned short)value;
	rule_entry->filter_flags |= SWACL_IF_filter_SMAC_PRE;

	return SWACL_IF_SUCCESS;
}
int
swacl_if_ifname_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	
	int num;
	for ( num = 0 ; num < SWACL_IF_NAME_MAX_NUM ; num ++){
		
		if(strlen (rule_entry->bind_ifname[num]) != 0){
			if(num == 0)
				snprintf(param_str,param_str_len,"%s",rule_entry->bind_ifname[num]);
			else{
				strlcat(param_str,",",param_str_len);
				strlcat(param_str,rule_entry->bind_ifname[num],param_str_len);
			}
		}

	}
	return 0;
	
}

int
swacl_if_parse_ifname(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	char s1[2]= {',','\0'};
	char * p_str=NULL;
	struct net_device *lowdev;
	int pos = 0,slen = 0;
	char * buf = param_str;
	
	if(strlen(param_str)==0) {
		dbprintk("ifname: null string\n");
		return SWACL_IF_ERROR;
	}
	
	memset(rule_entry->bind_ifname,0x0,sizeof(rule_entry->bind_ifname));

	if((p_str = strstr(buf, s1)) == NULL) {

		if(strlen(param_str)==0) {
			dbprintk("ifname: null string\n");
			return SWACL_IF_ERROR;
		}

		p_str = swacl_util_trim(buf);

		if((slen = strlen(p_str)) > IFNAMSIZ) {
			dbprintk("ifname: name too long error\n");
			return SWACL_IF_ERROR;
		}
		if ((lowdev = dev_get_byname(p_str)) == NULL) {
			dbprintk("ifname: no netdev exist(%s)\n",p_str);
			return SWACL_IF_ERROR;
		}
		dev_put(lowdev);
		strlcpy(rule_entry->bind_ifname[0],p_str,slen);
		
	} else{
		while ((p_str = strsep(&buf, s1)) != NULL) {

			p_str = swacl_util_trim(p_str);

			if((slen = strlen(p_str)) > IFNAMSIZ) {
				dbprintk("ifname: name too long error\n");
				return SWACL_IF_ERROR;
			}

			if(slen <=0) {
				dbprintk("ifname: name is none error\n");
				return SWACL_IF_ERROR;
			}
			if ((lowdev = dev_get_byname(p_str)) == NULL) {
				dbprintk("ifname: no netdev exist(%s)\n",p_str);
				return SWACL_IF_ERROR;
			}
			dev_put(lowdev);
			
			if( pos >= (SWACL_IF_NAME_MAX_NUM-1)){
				dbprintk("ifname: too many interface\n");
				return SWACL_IF_ERROR;
			}
			
			strlcpy(rule_entry->bind_ifname[pos],p_str,slen);		
			pos++;
		}
	}
	rule_entry->filter_flags |= SWACL_IF_filter_IFNAME;
	return SWACL_IF_SUCCESS;

}
int
swacl_if_v4proto_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%d",rule_entry->v4_protocol);
	return 0;
}

int
swacl_if_parse_v4proto(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	unsigned int value=0;

	if(strlen(param_str)==0) {
		dbprintk("v4proto: null string\n");
		return SWACL_IF_ERROR;
	}

	param_str = swacl_util_trim(param_str);
	if(!swacl_util_string_is_digit(param_str)) {
		dbprintk("v4proto: isn't digit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(param_str,NULL,10);
	if(value > 255 || value == 0) {
		dbprintk("v4proto: error range\n");
		return SWACL_IF_ERROR;
	}

	rule_entry->v4_protocol = (unsigned char)value;
	rule_entry->filter_flags |= SWACL_IF_filter_V4_PROTO;

	return SWACL_IF_SUCCESS;

}

int
swacl_if_v6_nxt_header_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%d",rule_entry->v6_next_header);
	return 0;
}

int
swacl_if_parse_v6_nxt_header(char * param_str,struct swacl_if_rule_entry * rule_entry)
{

	unsigned int value=0;

	if(strlen(param_str)==0) {
		dbprintk("v6_nxt_header: null string\n");
		return SWACL_IF_ERROR;
	}

	param_str = swacl_util_trim(param_str);
	if(!swacl_util_string_is_digit(param_str)) {
		dbprintk("v6_nxt_header: isn't digit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(param_str,NULL,10);
	if(value > 255 || value == 0) {
		dbprintk("v6_nxt_header: error range\n");
		return SWACL_IF_ERROR;
	}

	rule_entry->v6_next_header = (unsigned char)value;
	rule_entry->filter_flags |= SWACL_IF_filter_V6_NXT_HEAD;

	return SWACL_IF_SUCCESS;
}

int
swacl_if_dip_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%u.%u.%u.%u",(rule_entry->dip&0xff000000)>>24,(rule_entry->dip&0x00ff0000)>>16
	        ,(rule_entry->dip&0x0000ff00)>>8,(rule_entry->dip&0x000000ff));
	return 0;
}

int
swacl_if_parse_dip(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	char s1[2]= {'.','\0'};
	char * p_str=NULL;
	unsigned int pos=0,value=0;
	char * buf = param_str;

	if(strlen(param_str)==0) {
		dbprintk("dip: null string\n");
		return SWACL_IF_ERROR;
	}
	if((p_str = strstr(buf, s1)) == NULL) {

		dbprintk("dip: format error(%s)\n",buf);
		return SWACL_IF_ERROR;

	} else {
		rule_entry->dip = 0;
		while ((p_str = strsep(&buf, s1)) != NULL) {

			p_str = swacl_util_trim(p_str);

			if(!swacl_util_string_is_digit(p_str)) {
				dbprintk("dip: isn't digit\n");
				return SWACL_IF_ERROR;
			}

			value = simple_strtoul(p_str,NULL,10);
			rule_entry->dip |= value << (32 - (8*(pos+1)));
			pos++;
		}

	}
	if(pos!=4 || rule_entry->dip == 0) {
		dbprintk("dip: format error or zero\n");
		return SWACL_IF_ERROR;
	}
	rule_entry->filter_flags |= SWACL_IF_filter_DIP;
	return SWACL_IF_SUCCESS;

}
int
swacl_if_dip_prefix_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%d",rule_entry->dip_prefix);
	return 0;
}

int
swacl_if_parse_dip_prefix(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	unsigned int value=0;

	if(strlen(param_str)==0) {
		dbprintk("prefix: null string\n");
		return SWACL_IF_ERROR;
	}
	param_str = swacl_util_trim(param_str);
	if(!swacl_util_string_is_digit(param_str)) {
		dbprintk("dip prefix: isn't digit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(param_str,NULL,10);

	if(value>32 || (value%8)!=0 || value ==0) {
		dbprintk("dip prefix: range error(%u)\n",value);
		return SWACL_IF_ERROR;
	}
	rule_entry->dip_prefix = (unsigned short)value;
	rule_entry->filter_flags |= SWACL_IF_filter_DIP_PRE;

	return SWACL_IF_SUCCESS;

}
int
swacl_if_sip_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%u.%u.%u.%u",(rule_entry->sip&0xff000000)>>24,(rule_entry->sip&0x00ff0000)>>16
	        ,(rule_entry->sip&0x0000ff00)>>8,(rule_entry->sip&0x000000ff));
	return 0;
}

int
swacl_if_parse_sip(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	char s1[2]= {'.','\0'};
	char * p_str=NULL;
	unsigned int pos=0,value=0;
	char * buf = param_str;


	if(strlen(param_str)==0) {
		dbprintk("sip: null string\n");
		return SWACL_IF_ERROR;
	}

	if((p_str = strstr(buf, s1)) == NULL) {

		dbprintk("sip: format error(%s)\n",buf);
		return SWACL_IF_ERROR;

	} else {
		rule_entry->sip = 0;
		while ((p_str = strsep(&buf, s1)) != NULL) {

			p_str = swacl_util_trim(p_str);

			if(!swacl_util_string_is_digit(p_str)) {
				dbprintk("sip: isn't digit\n");
				return SWACL_IF_ERROR;
			}

			value = simple_strtoul(p_str,NULL,10);
			rule_entry->sip |= value << (32 - (8*(pos+1)));
			pos++;
		}

	}
	if(pos!=4 || rule_entry->sip == 0) {
		dbprintk("sip: format error\n");
		return SWACL_IF_ERROR;
	}

	rule_entry->filter_flags |= SWACL_IF_filter_SIP;

	return SWACL_IF_SUCCESS;

}
int
swacl_if_sip_prefix_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%d",rule_entry->sip_prefix);
	return 0;
}

int
swacl_if_parse_sip_prefix(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	unsigned int value=0;

	if(strlen(param_str)==0) {
		dbprintk("sip prefix: null string\n");
		return SWACL_IF_ERROR;
	}

	param_str = swacl_util_trim(param_str);
	if(!swacl_util_string_is_digit(param_str)) {
		dbprintk("sip prefix: isn't digit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(param_str,NULL,10);
	if(value>32 || (value%8)!=0 || value == 0) {
		dbprintk("sip prefix: range error(%u)\n",value);
		return SWACL_IF_ERROR;
	}
	rule_entry->sip_prefix = (unsigned short)value;
	rule_entry->filter_flags |= SWACL_IF_filter_SIP_PRE;
	//dbprintk("sip prefix: parse ok(0x%x)\n",rule_entry->filter_flags);
	return SWACL_IF_SUCCESS;

}
int
swacl_if_v6_dip_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str, param_str_len,"%4.4x:%4.4x:%4.4x:%4.4x:%4.4x:%4.4x:%4.4x:%4.4x",
	        ntohs(rule_entry->v6dip.s6_addr16[0]), ntohs(rule_entry->v6dip.s6_addr16[1]),
	        ntohs(rule_entry->v6dip.s6_addr16[2]), ntohs(rule_entry->v6dip.s6_addr16[3]),
	        ntohs(rule_entry->v6dip.s6_addr16[4]), ntohs(rule_entry->v6dip.s6_addr16[5]),
	        ntohs(rule_entry->v6dip.s6_addr16[6]), ntohs(rule_entry->v6dip.s6_addr16[7]));
	return 0;
}

int
swacl_if_parse_v6_dip(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	int ret;
	char s1[2]= {':','\0'};
	char * p_str=NULL;
	unsigned int len=0;
	char * buf = param_str;

	if(strlen(param_str)==0) {
		dbprintk("v6 dip: null string\n");
		return SWACL_IF_ERROR;
	}

	if((p_str = strstr(buf, s1)) == NULL) {

		dbprintk("v6 dip: format error(%s)\n",buf);
		return SWACL_IF_ERROR;

	} else {
		p_str = swacl_util_trim(buf);
		len = strlen(p_str);
		ret = in6_pton(buf,len,(void*)&rule_entry->v6dip,-1,NULL);
		if(ret != 1) {
			dbprintk("v6 dip: in6_pton error(%s)\n",p_str);
			return SWACL_IF_ERROR;
		}
	}

	rule_entry->filter_flags |= SWACL_IF_filter_V6_DIP;

	return SWACL_IF_SUCCESS;
}
int
swacl_if_v6_dip_prefix_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%u",rule_entry->v6dip_prefix);
	return 0;
}

int
swacl_if_parse_v6_dip_prefix(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	unsigned int value=0;

	if(strlen(param_str)==0) {
		dbprintk("v6 prefix: null string\n");
		return SWACL_IF_ERROR;
	}

	param_str = swacl_util_trim(param_str);
	if(!swacl_util_string_is_digit(param_str)) {
		dbprintk("ip6 prefix: isn't digit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(param_str,NULL,10);
	if(value>128 || (value%8)!=0 || value ==0) {
		dbprintk("ip6 prefix: range error\n");
		return SWACL_IF_ERROR;
	}

	rule_entry->v6dip_prefix = (unsigned short)value;
	rule_entry->filter_flags |= SWACL_IF_filter_V6_DIP_PRE;

	return SWACL_IF_SUCCESS;
}

int
swacl_if_v6_sip_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len, "%4.4x:%4.4x:%4.4x:%4.4x:%4.4x:%4.4x:%4.4x:%4.4x",
	        ntohs(rule_entry->v6sip.s6_addr16[0]), ntohs(rule_entry->v6sip.s6_addr16[1]),
	        ntohs(rule_entry->v6sip.s6_addr16[2]), ntohs(rule_entry->v6sip.s6_addr16[3]),
	        ntohs(rule_entry->v6sip.s6_addr16[4]), ntohs(rule_entry->v6sip.s6_addr16[5]),
	        ntohs(rule_entry->v6sip.s6_addr16[6]), ntohs(rule_entry->v6sip.s6_addr16[7]));
	return 0;
}

int
swacl_if_parse_v6_sip(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	int ret;
	char s1[2]= {':','\0'};
	char * p_str=NULL;
	unsigned int len=0;
	char * buf = param_str;

	if(strlen(param_str)==0) {
		dbprintk("v6 sip: null string\n");
		return SWACL_IF_ERROR;
	}
	if((p_str = strstr(buf, s1)) == NULL) {

		dbprintk("v6 sip: format error(%s)\n",buf);
		return SWACL_IF_ERROR;

	} else {
		p_str = swacl_util_trim(buf);
		len = strlen(p_str);
		ret = in6_pton(buf,len,(void*)&rule_entry->v6sip,-1,NULL);
		if(ret != 1) {
			dbprintk("v6 sip: in6_pton error(%s)\n",p_str);
			return SWACL_IF_ERROR;
		}
	}

	rule_entry->filter_flags |= SWACL_IF_filter_V6_SIP;

	return SWACL_IF_SUCCESS;
}
int
swacl_if_v6_sip_prefix_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%u",rule_entry->v6sip_prefix);
	return 0;
}

int
swacl_if_parse_v6_sip_prefix(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	unsigned int value=0;

	if(strlen(param_str)==0) {
		dbprintk("v6 sip_pre: null string\n");
		return SWACL_IF_ERROR;
	}
	param_str = swacl_util_trim(param_str);
	if(!swacl_util_string_is_digit(param_str)) {
		dbprintk("v6_sip prefix: isn't digit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(param_str,NULL,10);
	if(value>128 || (value%8)!=0 || value == 0) {
		dbprintk("ip6 prefix: range error\n");
		return SWACL_IF_ERROR;
	}
	rule_entry->v6sip_prefix = (unsigned short)value;
	rule_entry->filter_flags |= SWACL_IF_filter_V6_SIP_PRE;

	return SWACL_IF_SUCCESS;
}
int
swacl_if_dport_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%u",rule_entry->dport_start);
	return 0;
}

int
swacl_if_parse_dport(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	unsigned int value=0;

	if(strlen(param_str)==0) {
		dbprintk("dport: null string\n");
		return SWACL_IF_ERROR;
	}
	param_str = swacl_util_trim(param_str);
	if(!swacl_util_string_is_digit(param_str)) {
		dbprintk("dport: isn't digit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(param_str,NULL,10);
	if(value == 0) {
		dbprintk("dport: zero\n");
		return SWACL_IF_ERROR;
	}
	rule_entry->dport_start = (unsigned short)value;
	rule_entry->filter_flags |= SWACL_IF_filter_DPORT;

	return SWACL_IF_SUCCESS;

}
int
swacl_if_dport_end_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%u",rule_entry->dport_end);
	return 0;
}

int
swacl_if_parse_dport_end(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	unsigned int value=0;

	if(strlen(param_str)==0) {
		dbprintk("dport_end: null string\n");
		return SWACL_IF_ERROR;
	}
	param_str = swacl_util_trim(param_str);
	if(!swacl_util_string_is_digit(param_str)) {
		dbprintk("dport_end: isn't digit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(param_str,NULL,10);
	if(value == 0) {
		dbprintk("dport end: zero\n");
		return SWACL_IF_ERROR;
	}
	rule_entry->dport_end = (unsigned short)value;
	rule_entry->filter_flags |= SWACL_IF_filter_DPORT_END;

	return SWACL_IF_SUCCESS;

}
int
swacl_if_sport_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%u",rule_entry->sport_start);
	return 0;
}

int
swacl_if_parse_sport(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	unsigned int value=0;

	if(strlen(param_str)==0) {
		dbprintk("sport: null string\n");
		return SWACL_IF_ERROR;
	}
	param_str = swacl_util_trim(param_str);
	if(!swacl_util_string_is_digit(param_str)) {
		dbprintk("sport: isn't digit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(param_str,NULL,10);
	if(value == 0) {
		dbprintk("sport: zero\n");
		return SWACL_IF_ERROR;
	}
	rule_entry->sport_start = (unsigned short)value;
	rule_entry->filter_flags |= SWACL_IF_filter_SPORT;

	return SWACL_IF_SUCCESS;

}
int
swacl_if_sport_end_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%u",rule_entry->sport_end);
	return 0;
}

int
swacl_if_parse_sport_end(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	unsigned int value=0;

	if(strlen(param_str)==0) {
		dbprintk("sport_end: null string\n");
		return SWACL_IF_ERROR;
	}

	param_str = swacl_util_trim(param_str);
	if(!swacl_util_string_is_digit(param_str)) {
		dbprintk("sport_end: isn't digit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(param_str,NULL,10);

	if(value == 0) {
		dbprintk("sport_end: zero\n");
		return SWACL_IF_ERROR;
	}

	rule_entry->sport_end = (unsigned short)value;
	rule_entry->filter_flags |= SWACL_IF_filter_SPORT_END;

	return SWACL_IF_SUCCESS;

}
int
swacl_if_vlan_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%u",rule_entry->vlantag_id);
	return 0;
}

int
swacl_if_parse_filter_vlan(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	unsigned int value=0;

	if(strlen(param_str)==0) {
		dbprintk("vlan filter: null string\n");
		return SWACL_IF_ERROR;
	}
	param_str = swacl_util_trim(param_str);
	if(!swacl_util_string_is_digit(param_str)) {
		dbprintk("vlan filter: isn't digit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(param_str,NULL,10);

	if(value <=0 || value >4095) {
		dbprintk("vlan filter: value illegel(%d)\n",value);
		return SWACL_IF_ERROR;
	}
	rule_entry->vlantag_id = (unsigned short)value;
	rule_entry->filter_flags |= SWACL_IF_filter_VLAN;

	return SWACL_IF_SUCCESS;
}
int
swacl_if_vlan_pbit_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%u",rule_entry->vlan_tag_pbit);
	return 0;
}

int
swacl_if_parse_filter_vlan_pbit(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	unsigned int value=0;

	if(strlen(param_str)==0) {
		dbprintk("pbit filter: null string\n");
		return SWACL_IF_ERROR;
	}

	param_str = swacl_util_trim(param_str);
	if(!swacl_util_string_is_digit(param_str)) {
		dbprintk("pbit: isn't digit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(param_str,NULL,10);
	if(value <0 || value >7) {
		dbprintk("pbit filter: value illegel(%d)\n",value);
		return SWACL_IF_ERROR;
	}
	rule_entry->vlan_tag_pbit = (unsigned short)value;
	rule_entry->filter_flags |= SWACL_IF_filter_VLAN_PRI;

	return SWACL_IF_SUCCESS;
}
int
swacl_if_act_vlan_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%u",rule_entry->vlan_mark);
	return 0;
}

int
swacl_if_parse_act_vlan(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	unsigned int value=0;

	if(strlen(param_str)==0) {
		dbprintk("act vlan: null string\n");
		return SWACL_IF_ERROR;
	}
	param_str = swacl_util_trim(param_str);
	if(!swacl_util_string_is_digit(param_str)) {
		dbprintk("act vlan: isn't digit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(param_str,NULL,10);
	if(value <=0 || value >4095) {
		dbprintk("act vlan: value illegel(%d)\n",value);
		return SWACL_IF_ERROR;
	}
	rule_entry->vlan_mark = (unsigned short)value;
	rule_entry->action_flags |= SWACL_IF_ACTION_VLANTAG_CHG;

	return SWACL_IF_SUCCESS;

}
int
swacl_if_act_pbit_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%u",rule_entry->pbit_mark);
	return 0;
}

int
swacl_if_parse_act_pbit(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	unsigned int value=0;

	if(strlen(param_str)==0) {
		dbprintk("act pbit: null string\n");
		return SWACL_IF_ERROR;
	}

	param_str = swacl_util_trim(param_str);
	if(!swacl_util_string_is_digit(param_str)) {
		dbprintk("act pbit: isn't digit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(param_str,NULL,10);
	if(value <0 || value >7) {
		dbprintk("act pbit: value illegel(%d)\n",value);
		return SWACL_IF_ERROR;
	}
	rule_entry->pbit_mark = (unsigned short)value;
	rule_entry->action_flags |= SWACL_IF_ACTION_PBIT_CHG;

	return SWACL_IF_SUCCESS;

}
int
swacl_if_rule_priority_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	snprintf(param_str,param_str_len,"%u",rule_entry->priority);
	return 0;
}

int
swacl_if_parse_rule_priority(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	unsigned int value=0;

	if(strlen(param_str)==0) {
		dbprintk("rule pri: null string\n");
		return SWACL_IF_ERROR;
	}

	param_str = swacl_util_trim(param_str);
	if(!swacl_util_string_is_digit(param_str)) {
		dbprintk("rule pri: isn't digit\n");
		return SWACL_IF_ERROR;
	}

	value = simple_strtoul(param_str,NULL,10);
	rule_entry->priority = (unsigned short)value;
	rule_entry->action_flags |= SWACL_IF_ACTION_RULE_PRIORITY;

	return SWACL_IF_SUCCESS;
}

unsigned int
swacl_if_filter_cmp(struct swacl_if_rule_entry * src_entry,struct swacl_if_rule_entry * dst_entry)
{
	int i,j,cmp_b,cmp_prex,num,num1;
	unsigned int match_filter=0;
	

	if(src_entry->filter_flags != dst_entry->filter_flags) {
		if(g_swacl_priv.debug_cmp >= SWACL_IF_DEBUG_LVL_1)
			dbprintk("src(0x%x,dst(0x%x) \n",src_entry->filter_flags,dst_entry->filter_flags);
		return SWACL_IF_MATCH_NO_FILTER;
	}

	for(i =0; i<SWACL_IF_FILTER_BIT_LEN; i++) {
		if((src_entry->filter_flags & (1<<i))) {
			switch((1<<i)) {
			case SWACL_IF_filter_ALL_PACKET:
				if(dst_entry->filter_flags&SWACL_IF_filter_ALL_PACKET)
					match_filter |= SWACL_IF_filter_ALL_PACKET;
				break;
			case SWACL_IF_filter_OUI:
				if(memcmp(src_entry->oui,dst_entry->oui,OUI_LEN)==0) {
					match_filter |= SWACL_IF_filter_OUI;
				} else
					goto not_match_filter;
				break;
			case SWACL_IF_filter_ETHERTYPE:
				if(src_entry->ethertype == dst_entry->ethertype) {
					match_filter |= SWACL_IF_filter_ETHERTYPE;
				} else
					goto not_match_filter;
				break;
			case SWACL_IF_filter_DMAC:
				if(src_entry->filter_flags & SWACL_IF_filter_DMAC_PRE) {
					if(src_entry->dmac_prefix<=dst_entry->dmac_prefix)
						cmp_b = (src_entry->dmac_prefix+7)/8;
					else
						cmp_b = (dst_entry->dmac_prefix+7)/8;

					for(j=0; j<cmp_b; j++) {
						if(src_entry->dmac[j] != dst_entry->dmac[j]) {
							goto not_match_filter;
						}
					}


					match_filter |= SWACL_IF_filter_DMAC | SWACL_IF_filter_DMAC_PRE;

				} else {
					if(memcmp(src_entry->dmac,dst_entry->dmac,ETH_ALEN)!=0) {
						goto not_match_filter;
					}
					match_filter |= SWACL_IF_filter_DMAC;
				}
				break;
			case SWACL_IF_filter_DMAC_PRE:

				break;
			case SWACL_IF_filter_SMAC:
				if(src_entry->filter_flags & SWACL_IF_filter_SMAC_PRE) {
					if(src_entry->smac_prefix<=dst_entry->smac_prefix)
						cmp_b = (src_entry->smac_prefix+7)/8;
					else
						cmp_b = (dst_entry->smac_prefix+7)/8;

					for(j=0; j<cmp_b-1; j++) {
						if(src_entry->smac[j] != dst_entry->smac[j]) {
							goto not_match_filter;
						}
					}

					match_filter |= SWACL_IF_filter_SMAC | SWACL_IF_filter_SMAC_PRE;

				} else {
					if(memcmp(src_entry->dmac,dst_entry->dmac,ETH_ALEN)!=0) {
						goto not_match_filter;
					}
					match_filter |= SWACL_IF_filter_SMAC;
				}
				break;
			case SWACL_IF_filter_SMAC_PRE:

				break;
			case SWACL_IF_filter_V4_PROTO:
				if(src_entry->v4_protocol == dst_entry->v4_protocol) {
					match_filter |= SWACL_IF_filter_V4_PROTO;
				} else
					goto not_match_filter;
				break;
			case SWACL_IF_filter_V6_NXT_HEAD:
				if(src_entry->v6_next_header == dst_entry->v6_next_header) {
					match_filter |= SWACL_IF_filter_V6_NXT_HEAD;
				} else
					goto not_match_filter;
				break;
			case SWACL_IF_filter_DIP:
				if(src_entry->filter_flags & SWACL_IF_filter_DIP_PRE) {
					if(src_entry->dip_prefix<=dst_entry->dip_prefix)
						cmp_prex = SWCAL_IF_PARSE_4PREFIX << (32 -src_entry->dip_prefix);
					else
						cmp_prex = SWCAL_IF_PARSE_4PREFIX << (32 -dst_entry->dip_prefix);

					if ((src_entry->dip & cmp_prex) != (dst_entry->dip & cmp_prex)) {
						goto not_match_filter;
					}
					match_filter |= SWACL_IF_filter_DIP | SWACL_IF_filter_DIP_PRE;
				} else {
					if (src_entry->dip != dst_entry->dip) {
						goto not_match_filter;
					}
					match_filter |= SWACL_IF_filter_DIP;
				}

				break;
			case SWACL_IF_filter_DIP_PRE:

				break;
			case SWACL_IF_filter_SIP:
				if(src_entry->filter_flags & SWACL_IF_filter_SIP_PRE) {
					if(src_entry->sip_prefix<=dst_entry->sip_prefix)
						cmp_prex = SWCAL_IF_PARSE_4PREFIX << (32 -src_entry->sip_prefix);
					else
						cmp_prex = SWCAL_IF_PARSE_4PREFIX << (32 -dst_entry->sip_prefix);

					if ((src_entry->sip & cmp_prex) != (dst_entry->sip & cmp_prex)) {
						goto not_match_filter;
					}
					match_filter |= SWACL_IF_filter_SIP | SWACL_IF_filter_SIP_PRE;
				} else {
					if (src_entry->sip != dst_entry->sip) {
						goto not_match_filter;
					}
					match_filter |= SWACL_IF_filter_SIP;
				}
				break;
			case SWACL_IF_filter_SIP_PRE:

				break;
			case SWACL_IF_filter_V6_DIP:
				if(src_entry->filter_flags & SWACL_IF_filter_V6_DIP_PRE) {
					if(src_entry->v6dip_prefix<=dst_entry->v6dip_prefix)
						cmp_b = (src_entry->v6dip_prefix+7)/8;
					else
						cmp_b = (dst_entry->v6dip_prefix+7)/8;
					if(g_swacl_priv.debug_cmp >= SWACL_IF_DEBUG_LVL_2)
						dbprintk("v6dip: cmp_b=%d\n",cmp_b);
					for(j=0; j<cmp_b-1; j++) {
						if(g_swacl_priv.debug_cmp >= SWACL_IF_DEBUG_LVL_2)
							dbprintk("cmp v6dip[%d]:0x%x-0x%x\n",j,src_entry->v6dip.s6_addr[j]
							         ,dst_entry->v6dip.s6_addr[j]);
						if(src_entry->v6dip.s6_addr[j] != dst_entry->v6dip.s6_addr[j]) {
							goto not_match_filter;
						}
					}

					if(src_entry->v6dip_prefix<=dst_entry->v6dip_prefix)

						match_filter |= SWACL_IF_filter_V6_DIP | SWACL_IF_filter_V6_DIP_PRE;
				} else {
					if (memcmp(&src_entry->v6dip,&dst_entry->v6dip,sizeof(struct in6_addr))!=0 ) {
						goto not_match_filter;
					}
					match_filter |= SWACL_IF_filter_V6_DIP;
				}
				break;
			case SWACL_IF_filter_V6_DIP_PRE:

				break;
			case SWACL_IF_filter_V6_SIP:
				if(src_entry->filter_flags & SWACL_IF_filter_V6_SIP_PRE) {
					if(src_entry->v6sip_prefix<=dst_entry->v6sip_prefix)
						cmp_b = (src_entry->v6sip_prefix+7)/8;
					else
						cmp_b = (dst_entry->v6sip_prefix+7)/8;
					if(g_swacl_priv.debug_cmp >= SWACL_IF_DEBUG_LVL_2)
						dbprintk("v6sip: cmp_b=%d\n",cmp_b);

					for(j=0; j<cmp_b; j++) {
						if(g_swacl_priv.debug_cmp >= SWACL_IF_DEBUG_LVL_2)
							dbprintk("cmp v6sip[%d]:0x%x-0x%x\n",j,src_entry->v6sip.s6_addr[j]
							         ,dst_entry->v6sip.s6_addr[j]);
						if(src_entry->v6sip.s6_addr[j] != dst_entry->v6sip.s6_addr[j]) {
							goto not_match_filter;
						}
					}

					match_filter |= SWACL_IF_filter_V6_SIP | SWACL_IF_filter_V6_SIP_PRE;
				} else {
					if (memcmp(&src_entry->v6sip,&dst_entry->v6sip,sizeof(struct in6_addr))!=0 ) {
						goto not_match_filter;
					}
					match_filter |= SWACL_IF_filter_V6_SIP;
				}
				break;
			case SWACL_IF_filter_V6_SIP_PRE:

				break;
			case SWACL_IF_filter_DPORT:
				if(src_entry->dport_start == dst_entry->dport_start) {
					match_filter |= SWACL_IF_filter_DPORT;
				} else
					goto not_match_filter;
				break;
			case SWACL_IF_filter_DPORT_END:
				if(src_entry->dport_end == dst_entry->dport_end) {
					match_filter |= SWACL_IF_filter_DPORT_END;
				} else
					goto not_match_filter;
				break;
			case SWACL_IF_filter_SPORT:
				if(src_entry->sport_start == dst_entry->sport_start) {
					match_filter |= SWACL_IF_filter_SPORT;
				} else
					goto not_match_filter;
				break;
			case SWACL_IF_filter_SPORT_END:
				if(src_entry->sport_end == dst_entry->sport_end) {
					match_filter |= SWACL_IF_filter_SPORT_END;
				} else
					goto not_match_filter;
				break;
			case SWACL_IF_filter_VLAN:
				if(src_entry->vlantag_id == dst_entry->vlantag_id) {
					match_filter |= SWACL_IF_filter_VLAN;
				} else
					goto not_match_filter;
				break;
			case SWACL_IF_filter_VLAN_PRI:
				if(src_entry->vlan_tag_pbit == dst_entry->vlan_tag_pbit) {
					match_filter |= SWACL_IF_filter_VLAN_PRI;
				} else
					goto not_match_filter;
				break;
			case SWACL_IF_filter_IFNAME:
				for ( num = 0 ; num < SWACL_IF_NAME_MAX_NUM ; num ++){
					if(strlen (src_entry->bind_ifname[num]) != 0){

						for ( num1 = 0 ; num1 < SWACL_IF_NAME_MAX_NUM ; num1 ++){
							if(strlen (dst_entry->bind_ifname[num1]) != 0){
								if(strcmp(src_entry->bind_ifname[num],dst_entry->bind_ifname[num1])==0) {
									match_filter |= SWACL_IF_filter_IFNAME;
									break;
								}
						
							}

						}
						
						
					}
					if((match_filter & SWACL_IF_filter_IFNAME) == SWACL_IF_filter_IFNAME )
						break;

				}
				if((match_filter & SWACL_IF_filter_IFNAME) != SWACL_IF_filter_IFNAME )
					goto not_match_filter;
				
			default:
				break;

			}

		}

	}
	if(g_swacl_priv.debug_cmp >= SWACL_IF_DEBUG_LVL_1)
		dbprintk("cmp match before: 0x%x-0x%x\n",src_entry->filter_flags,match_filter);

	if((match_filter&(SWACL_IF_filter_IFNAME|SWACL_IF_filter_ALL_PACKET))==(SWACL_IF_filter_IFNAME|SWACL_IF_filter_ALL_PACKET)){
		if(g_swacl_priv.debug_cmp >= SWACL_IF_DEBUG_LVL_1)
			dbprintk("cmp match all packet: 0x%x-0x%x\n",src_entry->filter_flags,match_filter);
		return SWACL_IF_MATCH_FILTER_OK;
	}
	if((src_entry->filter_flags & match_filter) == src_entry->filter_flags) {
		if(g_swacl_priv.debug_cmp >= SWACL_IF_DEBUG_LVL_1)
			dbprintk("cmp match: 0x%x-0x%x\n",src_entry->filter_flags,match_filter);
		return SWACL_IF_MATCH_FILTER_OK;
	}
not_match_filter:
	return SWACL_IF_MATCH_NO_FILTER;
}

int
swacl_if_parse_act_redirect(char * param_str,struct swacl_if_rule_entry * rule_entry)
{
	char s1[2]= {',','\0'};
	char * p_str=NULL;
	struct net_device *lowdev;
	int pos = 0,slen = 0;
	char * buf = param_str;
	
	if(strlen(param_str)==0) {
		dbprintk("redirect: null string\n");
		return SWACL_IF_ERROR;
	}
	
	memset(rule_entry->redirect_ifname,0x0,sizeof(rule_entry->redirect_ifname));

	if((p_str = strstr(buf, s1)) == NULL) {

		if(strlen(param_str)==0) {
			dbprintk("redirect: null string\n");
			return SWACL_IF_ERROR;
		}

		p_str = swacl_util_trim(buf);

		if((slen = strlen(p_str)) > IFNAMSIZ) {
			dbprintk("redirect: name too long error\n");
			return SWACL_IF_ERROR;
		}
		if ((lowdev = dev_get_byname(p_str)) == NULL) {
			dbprintk("redirect: no netdev exist(%s)\n",p_str);
			return SWACL_IF_ERROR;
		}
		dev_put(lowdev);
		strlcpy(rule_entry->redirect_ifname[0],p_str,slen);
		
	} else{
		while ((p_str = strsep(&buf, s1)) != NULL) {

			p_str = swacl_util_trim(p_str);

			if((slen = strlen(p_str)) > IFNAMSIZ) {
				dbprintk("redirect: name too long error\n");
				return SWACL_IF_ERROR;
			}

			if(slen <=0) {
				dbprintk("redirect: name is none error\n");
				return SWACL_IF_ERROR;
			}
			if ((lowdev = dev_get_byname(p_str)) == NULL) {
				dbprintk("redirect: no netdev exist(%s)\n",p_str);
				return SWACL_IF_ERROR;
			}
			dev_put(lowdev);
			
			if( pos >= (SWACL_IF_NAME_MAX_NUM-1)){
				dbprintk("redirect: too many interface\n");
				return SWACL_IF_ERROR;
			}
			
			strlcpy(rule_entry->redirect_ifname[pos],p_str,slen);		
			pos++;
		}
	}
	rule_entry->action_flags |= SWACL_IF_ACTION_REDIRECT;
	return SWACL_IF_SUCCESS;

}

int
swacl_if_act_redirect_to_str(char * param_str,int param_str_len,struct swacl_if_rule_entry * rule_entry)
{
	int num;
	for ( num = 0 ; num < SWACL_IF_NAME_MAX_NUM ; num ++){
		
		if(strlen (rule_entry->redirect_ifname[num]) != 0){
			if(num == 0)
				snprintf(param_str,param_str_len,"%s",rule_entry->redirect_ifname[num]);
			else{
				strlcat(param_str,",",param_str_len);
				strlcat(param_str,rule_entry->redirect_ifname[num],param_str_len);
			}
		}

	}
	return 0;
}


