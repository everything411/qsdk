/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#include <linux/types.h>
#include <linux/module.h>

void *athdbg_kmalloc(size_t size, gfp_t flags, const char *struct_name,
		     const char *module_name);
void *athdbg_kzalloc(size_t size, gfp_t flags, const char *struct_name,
		     const char *module_name);
void athdbg_kfree(const void *ptr);
void athdbg_kfree_rcu(const void *ptr);

#define kzalloc(__XX__, __YY__, ...) ({ \
	char *sname = #__VA_ARGS__;\
	void *allocaddr = athdbg_kzalloc(__XX__, __YY__, sname, THIS_MODULE->name); \
	allocaddr; \
})

#define kmalloc(__XX__, __YY__, ...) ({ \
	char *sname = #__VA_ARGS__;\
	void *allocaddr = athdbg_kmalloc(__XX__, __YY__, sname, THIS_MODULE->name); \
	allocaddr; \
})

#define kfree(__XX__)	athdbg_kfree(__XX__)

#define ath_kfree_rcu(ptr, rhf) \
do { \
	athdbg_kfree_rcu(ptr); \
	kfree_rcu((ptr), rhf); \
} while (0)

/*MINIDUMP_LOG(start_addr, size, struct_name)*/
#define MINIDUMP_LOG(__XX__, __YY__, __ZZ__) \
	athmem_add_entry_to_minidump(__XX__, __YY__, __ZZ__, THIS_MODULE->name)

#define MINIDUMP_REMOVE(__XX__)	athmem_free_entry_in_minidump(__XX__)
