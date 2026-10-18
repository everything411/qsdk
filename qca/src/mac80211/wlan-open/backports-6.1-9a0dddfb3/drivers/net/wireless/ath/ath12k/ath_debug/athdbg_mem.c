// SPDX-License-Identifier: BSD-3-Clause-Clear
/* Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.*/
#include <linux/export.h>
#include <linux/rbtree.h>
#include <linux/debugfs.h>
#include <linux/spinlock.h>
#include <linux/module.h>
#include <linux/skbuff.h>
#include <net/netlink.h>
#include "athdbg_minidump.h"
#include "athdbg_mem.h"

#if LINUX_VERSION_IS_LESS(6,8,0)
#if !defined(CONFIG_DEBUG_MEM_USAGE)
#undef kzalloc
#undef kmalloc
#undef kfree
#endif
#endif

unsigned int athmem_stats_num_nodes;
unsigned long long athmem_stats_dup_ptr;
atomic_t athmem_flag_init_done;

static struct rb_root athmem_obj_tree_root = RB_ROOT;
static DEFINE_SPINLOCK(athmem_spinlock);

/* Module-name get local table — pointers into ath_debug __rodata, valid for the
 * lifetime of this module.  Struct names are not duplicated here; they are
 * resolved dynamically from ath12k_dump_list (defined in athdbg_minidump.c)
 * so that any addition to that list is automatically reflected without
 * touching this file.
 */
static const char * const athmem_module_names[] = {
	"ath12k", "ath12k_wifi6", "ath12k_wifi7", "ath12k_wifi8",
	"cfg80211", "mac80211",
};

static const char *athmem_get_local_name(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(athmem_module_names); i++) {
		if (strcmp(name, athmem_module_names[i]) == 0)
			return athmem_module_names[i];
	}
#if !defined(CPTCFG_MAC80211_ATHMEMDEBUG) && defined(CONFIG_QCA_MINIDUMP)
	for (i = 0; i < ath12k_dump_list_size; i++) {
		if (strcmp(name, ath12k_dump_list[i]) == 0)
			return ath12k_dump_list[i];
	}
#endif
	/* Unknown name: fall back to a heap copy so the node still owns it. */
	return kstrdup(name, GFP_ATOMIC);
}

struct athmem_debug_object {
	struct rb_node rb_node;
	unsigned long long pointer;
	size_t size;
	const char *struct_name;
	const char *module_name;
};

struct athmem_node {
	const char *struct_name;
	void *start_addr;
	const char *module_name;
	size_t size;
	int count;
	struct list_head dump_list;
	struct list_head dup_list;
};

static struct rb_root athmem_static_struct_root = RB_ROOT;

struct list_head athmem_visited_list = LIST_HEAD_INIT(athmem_visited_list);

static struct athmem_static_struct_node *athmem_find_static_name_node
						(const char *struct_name)
{
	struct rb_node *node = athmem_static_struct_root.rb_node;
	struct athmem_static_struct_node *data;

	while (node) {
		data = rb_entry(node, struct athmem_static_struct_node, rb);
		int ret = strcmp(struct_name, data->name);

		if (ret < 0)
			node = node->rb_left;
		else if (ret > 0)
			node = node->rb_right;
		else
			return data;
	}
	return NULL;
}

static int athmem_insert_static_struct(struct athmem_static_struct_node *newnode)
{
	struct rb_node **link = &athmem_static_struct_root.rb_node;
	struct rb_node *parent = NULL;
	struct athmem_static_struct_node *cur;
	int ret;

	while (*link) {
		cur = rb_entry(*link, struct athmem_static_struct_node, rb);
		ret = strcmp(newnode->name, cur->name);

		parent = *link;
		if (ret < 0)
			link = &(*link)->rb_left;
		else if (ret > 0)
			link = &(*link)->rb_right;
		else
			return -EEXIST; /* name already exists */
	}

	rb_link_node(&newnode->rb, parent, link);
	rb_insert_color(&newnode->rb, &athmem_static_struct_root);
	return 0;
}

int athmem_add_struct_info(void *addr, uint64_t size, const char *name)
{
	struct athmem_static_struct_node *nnode;
	struct athmem_addr_info *ainfo;
	int ret;

	ainfo = kmalloc(sizeof(*ainfo), GFP_KERNEL);
	if (!ainfo)
		return -ENOMEM;

	nnode = athmem_find_static_name_node(name);
	if (!nnode) {
		nnode = kmalloc(sizeof(*nnode), GFP_KERNEL);
		if (!nnode) {
			kfree(ainfo);
			return -ENOMEM;
		}

		strscpy(nnode->name, name, MAX_NAME_LEN);
		INIT_LIST_HEAD(&nnode->addr_list);

		ret = athmem_insert_static_struct(nnode);
		if (ret) {
			kfree(ainfo);
			kfree(nnode);
			return ret;
		}
	}

	ainfo->vaddr = addr;
	ainfo->size = size;
	list_add(&ainfo->list, &nnode->addr_list);

	return 0;
}

struct list_head *athmem_get_addr_list(const char *name)
{
	struct athmem_static_struct_node *nnode;

	nnode = athmem_find_static_name_node(name);
	return nnode ? &nnode->addr_list : NULL;
}

static struct athmem_debug_object *lookup_object(unsigned long ptr)
{
	struct rb_node *rb = athmem_obj_tree_root.rb_node;
	struct athmem_debug_object *object;

	while (rb) {
		object = rb_entry(rb, struct athmem_debug_object, rb_node);
		if (ptr < object->pointer)
			rb = object->rb_node.rb_left;
		else if (ptr > object->pointer)
			rb = object->rb_node.rb_right;
		else if (object->pointer == ptr)
			return object;
	}
	return NULL;
}

static void athmem_remove_object(struct athmem_debug_object *object)
{
	rb_erase(&object->rb_node, &athmem_obj_tree_root);
}

static struct athmem_debug_object *athmem_find_and_remove_obj(unsigned long ptr)
{
	unsigned long flags;
	struct athmem_debug_object *object;

	spin_lock_irqsave(&athmem_spinlock, flags);
	object = lookup_object(ptr);
	if (object) {
		athmem_stats_num_nodes--;
		athmem_remove_object(object);
	}
	spin_unlock_irqrestore(&athmem_spinlock, flags);

	return object;
}

static void athmem_free_name(const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(athmem_module_names); i++) {
		if (name == athmem_module_names[i])
			return;
	}
#if !defined(CPTCFG_MAC80211_ATHMEMDEBUG) && defined(CONFIG_QCA_MINIDUMP)
	for (i = 0; i < ath12k_dump_list_size; i++) {
		if (name == ath12k_dump_list[i])
			return;
	}
#endif
	kfree(name);
}

static char *athmem_delete_obj_full(unsigned long ptr)
{
	struct athmem_debug_object *object;
	char *struct_name;

	object = athmem_find_and_remove_obj(ptr);
	if (!object)
		return NULL;
	struct_name  = object->struct_name ? kstrdup(object->struct_name, GFP_ATOMIC) : NULL;
	athmem_free_name(object->struct_name);
	athmem_free_name(object->module_name);
	object->struct_name = NULL;
	object->module_name = NULL;
	kfree(object);
	return struct_name;
}

static struct athmem_node *athmem_find_dump_node(const char *struct_name)
{
	struct athmem_node *visited_node;

	list_for_each_entry(visited_node, &athmem_visited_list, dump_list) {
		if (visited_node->struct_name) {
			if (strcmp(struct_name, visited_node->struct_name) == 0)
				return visited_node;
		}
	}
	return NULL;
}

static void athmem_add_node_to_minidump(struct athmem_node *visited_node)
{
	struct athmem_node *duplicate_node;

	if (visited_node) {
		athdbg_add_to_minidump_log(visited_node->start_addr,
					   visited_node->size,
					   visited_node->struct_name,
					   visited_node->module_name);
		list_for_each_entry(duplicate_node, &visited_node->dup_list,
				    dup_list) {
			athdbg_add_to_minidump_log(duplicate_node->start_addr,
						   duplicate_node->size,
						   duplicate_node->struct_name,
						   duplicate_node->module_name);
		}
	}
}

static void athmem_find_duplicate(const char *struct_name,
				  struct athmem_debug_object *obj)
{
	struct athmem_node *visited_node, *duplicate_node;

	visited_node = athmem_find_dump_node(obj->struct_name);
	if (visited_node) {
		duplicate_node = kzalloc(sizeof(*duplicate_node), GFP_ATOMIC);
		if (duplicate_node) {
			duplicate_node->struct_name = obj->struct_name;
			duplicate_node->start_addr = (void *)(uintptr_t)obj->pointer;
			duplicate_node->module_name = obj->module_name;
			duplicate_node->size = obj->size;

			INIT_LIST_HEAD(&duplicate_node->dump_list);
			INIT_LIST_HEAD(&duplicate_node->dup_list);
			list_add_tail(&duplicate_node->dup_list,
				      &visited_node->dup_list);
		}
	} else {
		visited_node = kzalloc(sizeof(*visited_node), GFP_ATOMIC);
		if (visited_node) {
			visited_node->struct_name = obj->struct_name;
			visited_node->start_addr = (void *)(uintptr_t)obj->pointer;
			visited_node->module_name = obj->module_name;
			visited_node->size = obj->size;

			INIT_LIST_HEAD(&visited_node->dump_list);
			INIT_LIST_HEAD(&visited_node->dup_list);
			list_add_tail(&visited_node->dump_list,
				      &athmem_visited_list);
		} else {
			return;
		}
	}
	visited_node->count++;
}

static void athmem_free_minidump_list(void)
{
	struct athmem_node *visited_node, *duplicate_node, *tmp_visited_node, *tmp_dup_node;

	list_for_each_entry_safe(visited_node, tmp_visited_node,
				 &athmem_visited_list, dump_list) {
		list_for_each_entry_safe(duplicate_node, tmp_dup_node,
					 &visited_node->dup_list, dup_list) {
			list_del(&duplicate_node->dup_list);
			kfree(duplicate_node);
		}
		list_del(&visited_node->dump_list);
		kfree(visited_node);
	}
}

static void athmem_create_minidump_list(void)
{
	struct rb_node *node;
	unsigned long flags;
	struct athmem_debug_object *obj;
	u32 count = 0;

	spin_lock_irqsave(&athmem_spinlock, flags);
	for (node = rb_first(&athmem_obj_tree_root); node; node = rb_next(node)) {
		obj = rb_entry(node, struct athmem_debug_object, rb_node);
		if (!obj->struct_name)
			continue;
		athmem_find_duplicate((char *)obj->struct_name, obj);
		count++;
	}
	spin_unlock_irqrestore(&athmem_spinlock, flags);
}

void athmem_find_and_add_entry_in_minidump(const char *struct_name)
{
	struct athmem_node *minidump_node;

	athmem_create_minidump_list();
	minidump_node = athmem_find_dump_node(struct_name);
	if (minidump_node) {
		pr_info("\nAdding struct %s to Minidump", struct_name);
		athmem_add_node_to_minidump(minidump_node);
	} else {
		pr_err("No entry found\n");
	}
	athmem_free_minidump_list();
}

void athmem_dump_data_local(uint64_t *vaddr, uint64_t size, const char *name)
{
	struct file *filp;
	char filename[256];
	loff_t pos_write = 0;
	ssize_t written;

	if (!vaddr || !size || name == NULL) {
		pr_err("Invalid vaddr[%p]/size[%llu]/name[%s]", vaddr, size, name);
		return;
	}

	snprintf(filename, sizeof(filename), "/tmp/%s.BIN", name);

	filp = filp_open(filename, O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (IS_ERR(filp)) {
		pr_err("Single TLV Dump: Failed to create file %s, err=%ld\n",
					filename, PTR_ERR(filp));
	} else {
		written = kernel_write(filp, vaddr, size, &pos_write);
		if (written < 0) {
			pr_err("Single TLV Dump: Failed to write to file %s, err=%zd\n",
					filename, written);
		}

		filp_close(filp, NULL);
	}
}

void athmem_free_static_struct_list(void)
{
	struct rb_node *node;
	struct athmem_static_struct_node *nnode;
	struct athmem_addr_info *ainfo, *tmp;

	for (node = rb_first(&athmem_static_struct_root);
		node;) {
		nnode = rb_entry(node, struct athmem_static_struct_node, rb);
		node = rb_next(node);

		list_for_each_entry_safe(ainfo, tmp, &nnode->addr_list, list) {
			list_del(&ainfo->list);
			kfree(ainfo);
		}

		rb_erase(&nnode->rb, &athmem_static_struct_root);
		kfree(nnode);
	}
}

void athmem_collect_struct(const char *struct_name)
{
	struct athmem_node *struct_node, *dup_node;
	struct list_head *list_info;
	struct athmem_addr_info *tmp_info;
	char tmp_structname[MAX_NAME_LEN];
	int ins_cnt = 0;

	athmem_create_minidump_list();
	struct_node = athmem_find_dump_node(struct_name);

	if (struct_node) {
		if (struct_node->count > 1) {
			snprintf(tmp_structname, MAX_NAME_LEN, "%s%d",
					struct_node->struct_name,
					ins_cnt);
			ins_cnt++;
			athmem_dump_data_local((uint64_t *)struct_node->start_addr,
						struct_node->size,
						struct_node->struct_name);
			list_for_each_entry(dup_node, &struct_node->dup_list, dup_list) {
				snprintf(tmp_structname, MAX_NAME_LEN, "%s%d",
					dup_node->struct_name,
					ins_cnt);
				ins_cnt++;
				athmem_dump_data_local((uint64_t *)dup_node->start_addr,
							dup_node->size,
							tmp_structname);
			}
		} else {
			athmem_dump_data_local((uint64_t *)struct_node->start_addr,
						struct_node->size,
						struct_node->struct_name);
		}
	} else {
		list_info = athmem_get_addr_list(struct_name);

		if (!list_info) {
			pr_info("\nstruct %s not present in minidump", struct_name);
			goto out;
		}

		list_for_each_entry(tmp_info, list_info, list) {
			snprintf(tmp_structname, MAX_NAME_LEN, "%s%d",
					struct_name,
					ins_cnt);
			athmem_dump_data_local((uint64_t *)tmp_info->vaddr,
						tmp_info->size,
						struct_name);
			ins_cnt++;
		}
	}

out:
	athmem_free_minidump_list();
}


void athmem_print_all_allocated_list(void)
{
	struct athmem_node *visited_node;

	athmem_create_minidump_list();
	list_for_each_entry(visited_node, &athmem_visited_list, dump_list) {
		pr_info("Alloc list : Struct name: %-25s Count: %d", visited_node->struct_name,
			visited_node->count);
	}

	athmem_free_minidump_list();
}

void athmem_find_and_print_minidump_entry(const char *struct_name)
{
	struct athmem_node *visited_node;
	int count = 0;

	visited_node = athmem_find_dump_node(struct_name);
	if (visited_node) {
		pr_info("Dump list : Struct name: %-25s Count: %d",
			visited_node->struct_name, visited_node->count);
	} else {
		pr_info("Dump list : Struct name: %-25s Count: %d",
			struct_name, count);
	}
}

void athmem_print_minidump_list(void)
{
	athmem_create_minidump_list();
#if !defined(CPTCFG_MAC80211_ATHMEMDEBUG) && defined(CONFIG_QCA_MINIDUMP)
	athdbg_iterate_minidump_list();
#endif
	athmem_free_minidump_list();
}

void athmem_clear_minidump_and_rb_tree(void)
{
	struct rb_node *node, *next;
	struct athmem_debug_object *obj;
	unsigned long flags;

	pr_info("Deleting minidump entries\n");

	if (RB_EMPTY_ROOT(&athmem_obj_tree_root))
		return;

	spin_lock_irqsave(&athmem_spinlock, flags);

	for (node = rb_first(&athmem_obj_tree_root); node; node = next) {
		next = rb_next(node);
		obj = rb_entry(node, struct athmem_debug_object, rb_node);
		if ((obj->struct_name) && (strlen(obj->struct_name) > 0))
			athdbg_remove_minidump_segment((void *)(uintptr_t)obj->pointer);
		athmem_free_name(obj->struct_name);
		athmem_free_name(obj->module_name);
		rb_erase(node, &athmem_obj_tree_root);
		kfree(obj);
	}
	athmem_stats_num_nodes = 0;
	spin_unlock_irqrestore(&athmem_spinlock, flags);
}

static void athmem_create_object(unsigned long long ptr, size_t size,
				 gfp_t gfp, const char *struct_name,
				 const char *module_name)
{
	unsigned long flags;
	struct athmem_debug_object *object, *parent;
	struct rb_node **link, *rb_parent;

	atomic_cmpxchg(&athmem_flag_init_done, 0, 1);

	object = kmalloc(sizeof(*object), (gfp));
	if (!object) {
		pr_warn("Allocation of athmem_debug_object failed\n");
		return;
	}

	object->pointer = ptr;
	object->size = size;
	object->struct_name = athmem_get_local_name(struct_name);
	object->module_name = athmem_get_local_name(module_name);

	spin_lock_irqsave(&athmem_spinlock, flags);

	link = &athmem_obj_tree_root.rb_node;
	rb_parent = NULL;
	while (*link) {
		rb_parent = *link;
		parent = rb_entry(rb_parent, struct athmem_debug_object, rb_node);
		if (ptr < parent->pointer) {
			link = &parent->rb_node.rb_left;
		} else if (ptr > parent->pointer) {
			link = &parent->rb_node.rb_right;
		} else {
			athmem_stats_dup_ptr++;
			spin_unlock_irqrestore(&athmem_spinlock, flags);
			pr_debug("dup ptr incrementing struct name %s ptr = %p",
				 struct_name, (void *)(uintptr_t)ptr);
			athmem_free_name(object->struct_name);
			athmem_free_name(object->module_name);
			kfree(object);
			return;
		}
	}
	athmem_stats_num_nodes++;
	rb_link_node(&object->rb_node, rb_parent, link);
	rb_insert_color(&object->rb_node, &athmem_obj_tree_root);
	spin_unlock_irqrestore(&athmem_spinlock, flags);
}

char *ath_minidump_update_free(void *ptr)
{
	if (!atomic_read(&athmem_flag_init_done) || RB_EMPTY_ROOT(&athmem_obj_tree_root))
		return NULL;

	return athmem_delete_obj_full((unsigned long)ptr);
}

void ath_minidump_update_alloc(void *ptr, size_t len, const char *struct_name,
			       const char *module_name)
{
	athmem_create_object((unsigned long)ptr, len, GFP_ATOMIC, struct_name,
			     module_name);
}
EXPORT_SYMBOL(ath_minidump_update_alloc);

void athmem_add_entry_to_minidump(void *start_addr, size_t size,
				  const char *struct_name,
				  const char *module_name)
{
	ath_minidump_update_alloc(start_addr, size, struct_name, module_name);
	athdbg_add_to_minidump_log(start_addr, size, struct_name, module_name);
}
EXPORT_SYMBOL(athmem_add_entry_to_minidump);

void athmem_free_entry_in_minidump(const void *start_addr)
{
	char *struct_name;

	struct_name = ath_minidump_update_free((void *)start_addr);
	if ((struct_name) && (strlen(struct_name) > 0))
		athdbg_remove_minidump_segment((void *)start_addr);

	kfree(struct_name);
}
EXPORT_SYMBOL(athmem_free_entry_in_minidump);

void *athdbg_kzalloc(size_t size, gfp_t flags, const char *struct_name,
		     const char *module_name)
{
	void *va_addr = kzalloc(size, flags);

	if (va_addr && struct_name)
		athdbg_minidump_log(va_addr, size, struct_name, module_name);

	return va_addr;
}
EXPORT_SYMBOL(athdbg_kzalloc);

void *athdbg_kmalloc(size_t size, gfp_t flags, const char *struct_name,
		     const char *module_name)
{
	void *va_addr = kmalloc(size, flags);

	if (va_addr && struct_name)
		athdbg_minidump_log(va_addr, size, struct_name, module_name);

	return va_addr;
}
EXPORT_SYMBOL(athdbg_kmalloc);

void athdbg_kfree(const void *ptr)
{
	char *struct_name;

	struct_name = ath_minidump_update_free((void *)ptr);

	if ((struct_name) && (strlen(struct_name) > 0))
		athdbg_remove_minidump_segment((void *)ptr);
	kfree(struct_name);
	kfree(ptr);
}
EXPORT_SYMBOL(athdbg_kfree);

void athdbg_kfree_rcu(const void *ptr)
{
	char *struct_name;

	struct_name = ath_minidump_update_free((void *)ptr);

	if ((struct_name) && (strlen(struct_name) > 0))
		athdbg_remove_minidump_segment((void *)ptr);
	kfree(struct_name);
}
EXPORT_SYMBOL(athdbg_kfree_rcu);
