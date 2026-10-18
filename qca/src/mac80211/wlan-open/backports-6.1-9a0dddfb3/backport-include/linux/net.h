#ifndef __BACKPORT_LINUX_NET_H
#define __BACKPORT_LINUX_NET_H
#include_next <linux/net.h>
#include <linux/static_key.h>

#if LINUX_VERSION_IS_LESS(6,8,0)
#ifndef SOCKWQ_ASYNC_NOSPACE
#define SOCKWQ_ASYNC_NOSPACE   SOCK_ASYNC_NOSPACE
#endif
#ifndef SOCKWQ_ASYNC_WAITDATA
#define SOCKWQ_ASYNC_WAITDATA   SOCK_ASYNC_WAITDATA
#endif
#endif

#endif /* __BACKPORT_LINUX_NET_H */
