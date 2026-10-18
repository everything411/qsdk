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
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/init.h>
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
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/miscdevice.h>

int (*swhook_net_recv) (struct sk_buff ** pskb);

int (*omciibc_net_recv)(struct sk_buff **pskb) __rcu __read_mostly;
EXPORT_SYMBOL_GPL(omciibc_net_recv);
int (*swacl_net_recv) (struct sk_buff ** pskb) __rcu __read_mostly;
EXPORT_SYMBOL_GPL(swacl_net_recv);
///////////////////////////////////////////////////////////
#if 0
static int swhook_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static int swhook_close(struct inode *inode, struct file *filp)
{

	return 0;
}
#endif
#if 0
static const struct file_operations swhook_fops = {
	.owner		= THIS_MODULE,
	.open   	= swhook_open,
	.release	= swhook_close,
};

static struct miscdevice swhook_miscdev = {
	.minor		= MISC_DYNAMIC_MINOR,
	.name		= "swhook",
	.fops		= &swhook_fops,
};
#endif

int
swhook_rx_handle_frame(struct sk_buff **pskb)
{
	int (*omciibc_handle_frame)(struct sk_buff **pskb);
	int (*swacl_handle_frame)(struct sk_buff **pskb);
	
	omciibc_handle_frame = rcu_dereference(omciibc_net_recv);
	if(omciibc_handle_frame){
		if(omciibc_handle_frame(pskb)==0)
			return RX_HANDLER_PASS;		
	}

	swacl_handle_frame = rcu_dereference(swacl_net_recv);
	if(swacl_handle_frame){
		return swacl_handle_frame(pskb);
				
	}
	
	return RX_HANDLER_PASS;	
	
}

static int __init
swhook_init(void)
{
#if 0
	int error;	

	error = misc_register(&swhook_miscdev);
	if (error) {
		pr_warn("Module registration error (name=%s, major=%d, minor=%d, err=%d)\n",
			swhook_miscdev.name,
			MISC_MAJOR, swhook_miscdev.minor,
			error);		
		return error;
	}

#endif	
	BUG_ON(swhook_net_recv);
	RCU_INIT_POINTER(swhook_net_recv, swhook_rx_handle_frame);
	
	pr_info("swhook device registered \n");
	return 0;
}
static void
swhook_cleanup(void)
{	
#if 0
	misc_deregister(&swhook_miscdev);
#endif
	RCU_INIT_POINTER(swhook_net_recv, NULL);
	pr_debug("swhook driver module unloaded\n");
}

module_init(swhook_init);
module_exit(swhook_cleanup);

MODULE_AUTHOR("TREX Software");
MODULE_LICENSE("Dual BSD/GPL");
MODULE_DESCRIPTION("sw hook sdriver");

