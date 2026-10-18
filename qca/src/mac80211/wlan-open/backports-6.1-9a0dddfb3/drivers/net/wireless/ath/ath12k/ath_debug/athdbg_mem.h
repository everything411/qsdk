/* SPDX-License-Identifier: BSD-3-Clause-Clear*/
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#ifndef __ATHDBG_MEM_H
#define __ATHDBG_MEM_H

#define FUNC_OBJ_ARR_SIZE 500
#define BUF_SIZE 10192
#define MAX_NAME_LEN 64

struct sk_buff;
struct sk_buff_head;

struct athmem_addr_info {
	void *vaddr;
	size_t size;
	struct list_head list;
};

struct athmem_static_struct_node {
	char name[MAX_NAME_LEN];
	struct rb_node rb;
	struct list_head addr_list;
};

void ath_minidump_update_alloc(void *ptr, size_t len, const char *struct_name,
					const char *module_name);
void athmem_debug_print_rbtree(void);
void athmem_print_all_allocated_list(void);
void athmem_print_minidump_list(void);
void athmem_find_entry_in_rbtree(const char *struct_name);
void athmem_find_and_print_minidump_entry(const char *struct_name);
void athmem_enable_minidump_list(void);
void athmem_clear_memdebug_info(void);
char *ath_minidump_update_free(void *ptr);
void athmem_add_entry_to_minidump(void *start_addr, size_t size,
				  const char *struct_name,
				  const char *module_name);
void athmem_free_entry_in_minidump(const void *start_addr);
void athmem_clear_minidump_and_rb_tree(void);
int athmem_add_struct_info(void *addr, uint64_t size, const char *name);
void athmem_free_static_struct_list(void);
void athmem_dump_data_local(uint64_t *vaddr, uint64_t size, const char *name);
#endif
