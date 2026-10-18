/* 
Copyright (c) 2024, Qualcomm Innovation Center, Inc. All rights reserved.
SPDX-License-Identifier: ISC

 MIT License

Copyright (c) 2018 王劣劣

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */
#include <stdio.h>  
#include <unistd.h>  
#include <stdlib.h>  
#include <string.h>  
#include <signal.h>  
#include <errno.h>  
#include <net/if.h>
#include <sys/types.h>
#include <asm/types.h>  
#include <arpa/inet.h>  
#include <sys/socket.h>  
#include <linux/netlink.h>  
#include <linux/rtnetlink.h>  
#include <linux/route.h>
#include <ifaddrs.h>

#define BUFLEN 2048
#define MSGLEN 1024
#define dbprintf(fmt, args...)      do { if(dbg_level) fprintf(stderr,"%s:%d: %s(): " fmt, __FILE__, __LINE__, __FUNCTION__, ##args); } while (0)

#define t_assert(x) { \
    if(!(x))  {err = -__LINE__;goto error;} \
}  
int dbg_level = 0;
/*Ctrl + C exit*/
static volatile int Running = 1;

void intHandler(int dummy) {
	Running = 0;
}

char trigger_msg[MSGLEN] = { 0 };
void set_msg(char *str) {
	if (!str)
		return;
	memset(trigger_msg, 0, sizeof(trigger_msg));
	strlcpy(trigger_msg, str, sizeof(trigger_msg));
}

/** 
 * parser RTA to tb
 */
void parse_rtattr(struct rtattr **tb, int max, struct rtattr *attr, int len) {
	for (; RTA_OK(attr, len); attr = RTA_NEXT(attr, len)) {
		if (attr->rta_type <= max) {
			tb[attr->rta_type] = attr;
		}
	}
}

/**
 * display link status
 * Show message when network interface changed, example plug/unplug network, add/remove interface, enable/disable net ports
 */
void print_ifinfomsg(struct nlmsghdr *nh) {
	int len;
	struct rtattr *tb[IFLA_MAX + 1];
	struct ifinfomsg *ifinfo;
	char tmp_msg[MSGLEN];
	bzero(tb, sizeof(tb));
	ifinfo = NLMSG_DATA(nh);
	len = nh->nlmsg_len - NLMSG_SPACE(sizeof(*ifinfo));
	parse_rtattr(tb, IFLA_MAX, IFLA_RTA(ifinfo), len);

	len = snprintf(tmp_msg, sizeof(tmp_msg), "type=%s ifname=%s link=%s",
			(nh->nlmsg_type == RTM_NEWLINK) ? "NEWLINK" : "DELLINK",
			(tb[IFLA_IFNAME]) ? (char*) RTA_DATA(tb[IFLA_IFNAME]) : "",
			(ifinfo->ifi_flags & IFF_UP) ? "up" : "down");
	set_msg(tmp_msg);
}

/** 
 * Display address
 * Print ip address when ip address changed, for example get ip address from dhcp server 
 */
void print_ifaddrmsg(struct nlmsghdr *nh) {
	int len;
	struct rtattr *tb[IFA_MAX + 1];
	struct ifaddrmsg *ifaddr;
	char tmp[100]={0};
	char tmp_msg[MSGLEN]={0};
	char ifname[IFNAMSIZ]={0};

	bzero(tb, sizeof(tb));
	ifaddr = NLMSG_DATA(nh);
	len = nh->nlmsg_len - NLMSG_SPACE(sizeof(*ifaddr));
	parse_rtattr(tb, IFA_MAX, IFA_RTA(ifaddr), len);
	if_indextoname(ifaddr->ifa_index, ifname);
	if (nh->nlmsg_type == RTM_NEWADDR) {
		if (tb[IFA_LABEL] != NULL) { //IFA_LABEL attribute not aways support IPv6
			dbprintf("tb[IFA_LABEL]=%s\n", RTA_DATA(tb[IFA_LABEL]));
		}
		if (tb[IFA_ADDRESS] != NULL) {
			inet_ntop(ifaddr->ifa_family,
					RTA_DATA(tb[IFA_ADDRESS]), tmp,
					sizeof(tmp));
			//if(strcmp(eth, "eth0") == 0 ){
			snprintf(tmp_msg, sizeof(tmp_msg), "type=NEWADDR ifname=%s ipaddr=%s",
					ifname, tmp);
			set_msg(tmp_msg);
			//}
		}

	} else {
		snprintf(tmp_msg, sizeof(tmp_msg), "type=DELADDR ifname=%s", ifname);
		set_msg(tmp_msg);
	}
}

#if 0
void print_rtmsg(struct nlmsghdr *nh) {
	int len;
	struct rtattr *tb[RTA_MAX + 1];
	struct rtmsg *rt;
	char tmp[256];
	bzero(tb, sizeof(tb));
	rt = NLMSG_DATA(nh);
	len = nh->nlmsg_len - NLMSG_SPACE(sizeof(*rt));
	parse_rtattr(tb, RTA_MAX, RTM_RTA(rt), len);
	printf("%s: ",
			(nh->nlmsg_type == RTM_NEWROUTE) ?
					"NEWROUT" : "DELROUT");
	if (tb[RTA_DST] != NULL) {
		inet_ntop(rt->rtm_family, RTA_DATA(tb[RTA_DST]), tmp,
				sizeof(tmp));
		printf("RTA_DST %s ", tmp);
	}
	if (tb[RTA_SRC] != NULL) {
		inet_ntop(rt->rtm_family, RTA_DATA(tb[RTA_SRC]), tmp,
				sizeof(tmp));
		printf("RTA_SRC %s ", tmp);
	}
	if (tb[RTA_GATEWAY] != NULL) {
		inet_ntop(rt->rtm_family, RTA_DATA(tb[RTA_GATEWAY]), tmp,
				sizeof(tmp));
		printf("RTA_GATEWAY %s ", tmp);
	}

	printf("\n");
}
#endif

int main(int argc, char *argv[])  
{  
	int socket_fd;
	int i, err = 0;
	fd_set rd_set;
	struct timeval timeout;
	int select_r;
	int read_r;
	struct sockaddr_nl sa;
	struct nlmsghdr *nh;
	int len = BUFLEN;
	char buff[2048];
	char script_path[256] = "/etc/omci/script/network/net_monitor_cb.sh";
	const char *dbg_str = getenv("NET_MONITOR_DBG");
	char cmd[1024], care_protocol=0;

	if(dbg_str) {
    		dbg_level = atoi(dbg_str);
    		dbprintf("dbg_level=%d\n",dbg_level);
    	}

	/* Handling parameters */
	for (i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-f")) {
			strlcpy(script_path,argv[i+1],sizeof(script_path));
			dbprintf("script_path=%s\n",script_path);
			i++;
		} else if (!strcmp(argv[i], "-4")) {
			care_protocol=1;	//IPv4 only
			i++;
		} else if (!strcmp(argv[i], "-6")) {
			care_protocol=2;	//IPv6 only
			i++;
		} else {
			printf("usage: %s [OPTION]...\n", argv[0]);
			printf("-f	script path\n");
			printf("-4	IPv4 only\n");
			printf("-6	IPv6 only\n");
			return -1;
		}
	}

	signal(SIGINT, intHandler);

    socket_fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    t_assert(socket_fd > 0);  
    t_assert(!setsockopt(socket_fd, SOL_SOCKET, SO_RCVBUF, &len, sizeof(len)));  

    bzero(&sa, sizeof(sa));  
    sa.nl_family = AF_NETLINK;  
    //sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV4_ROUTE | RTMGRP_IPV6_IFADDR | RTMGRP_IPV6_ROUTE;
    if (care_protocol == 1) {	//IPv4 only
	    sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR;
    } else if ( care_protocol == 2) {	//IPv6 only
	    sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV6_IFADDR;
    } else {	//Both IPv4/IPv6
	    sa.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR;
    }

    t_assert(!bind(socket_fd, (struct sockaddr *) &sa, sizeof(sa)));  
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;  
    while (Running) {  
        FD_ZERO(&rd_set);  
        FD_SET(socket_fd, &rd_set);
        select_r = select(socket_fd + 1, &rd_set, NULL, NULL, &timeout);
        if (select_r < 0) {
            perror("select");
        } else if (select_r > 0) {
            if (FD_ISSET(socket_fd, &rd_set)) {
                if ((read_r = read(socket_fd, buff, sizeof(buff))) < 0 ) {
			continue;
		}
                timeout.tv_sec = 1;
        	timeout.tv_usec = 0;
		for (nh = (struct nlmsghdr *) buff; NLMSG_OK(nh, read_r); nh = NLMSG_NEXT(nh, read_r)) {
                    switch (nh->nlmsg_type) {
                    default:
                        /*other*/
                	    dbprintf("nh->nlmsg_type(other) = %d\n", nh->nlmsg_type);
                        break;  
                    case NLMSG_DONE:
                    case NLMSG_ERROR:
                	    dbprintf("nh->nlmsg_type(done/error) = %d\n", nh->nlmsg_type);
                        break;
                    case RTM_NEWLINK:
                    case RTM_DELLINK:
                    	dbprintf("nh->nlmsg_type = %s\n", (nh->nlmsg_type==RTM_NEWLINK)?"NEWLINK":"DELLINK");
                        print_ifinfomsg(nh);

			snprintf(cmd,sizeof(cmd),"%s %s",trigger_msg,script_path);
			dbprintf("cmd: %s\n",cmd);
			system(cmd);
                        break;
                    case RTM_NEWADDR:
                    case RTM_DELADDR:
                    	dbprintf("nh->nlmsg_type = %s\n", (nh->nlmsg_type==RTM_NEWADDR)?"NEWADDR":"DELADDR");
                        print_ifaddrmsg(nh);

			snprintf(cmd,sizeof(cmd),"%s %s",trigger_msg,script_path);
			dbprintf("cmd: %s\n",cmd);
			system(cmd);
                        break;
#if 0
                    case RTM_NEWROUTE:
                    case RTM_DELROUTE:
                        print_rtmsg(nh);
                        break;
#endif
                    }	//end switch
  
                }	//end for
            }
        }
    }
    close(socket_fd);  
  
error:  
    if (err < 0) {  
        printf("Error at line %d\nErrno=%d\n", -err, errno);  
    }  
    return err;  
} 
