/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#include <libubox/avl.h>
#include <libubus.h>
#include <ucode/compiler.h>
#include <ucode/lib.h>
#include <ucode/vm.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <linux/nl80211.h>
#include <unistd.h>
#include <udebug.h>
#include <stdio.h>
#include <syslog.h>
#include "ucode.h"

/* Opaque handle to higher-level daemon; not dereferenced here */
void *rptr_mgr_global;
uc_resource_type_t *global_type;
static uc_value_t *global, *iface_registry;
static uc_vm_t *vm;
static uc_vm_t rptr_mgr_vm;
static struct uloop_timeout gc_timer;
static uc_value_t *registry;

static void uc_gc_timer(struct uloop_timeout *timeout)
{
	ucv_gc(vm);
}

uc_value_t *uc_rptr_mgr_printf(uc_vm_t *vm, size_t nargs)
{
	uc_value_t *level = uc_fn_arg(0);
	uc_value_t *ret;
	uc_cfn_ptr_t _sprintf;
	int start = 0;

	_sprintf = uc_stdlib_function("sprintf");
	if (!_sprintf)
		return NULL;

	if (ucv_type(level) == UC_INTEGER)
		start++;

	if (nargs <= start)
		return NULL;

	ret = _sprintf(vm, nargs - start);

	if (!ret)
		return NULL;

	if (ucv_type(ret) != UC_STRING)
		return NULL;

	const char *str = ucv_string_get(ret);
	syslog(LOG_ERR, "%s", str);
	ucv_put(ret);
	return NULL;
}

uc_vm_t *rptr_mgr_ucode_create_vm(void)
{
	static uc_parse_config_t config = {
		.strict_declarations = true,
		.lstrip_blocks = true,
		.trim_blocks = true,
		.raw_mode = true
	};

	uc_search_path_init(&config.module_search_path);
	uc_search_path_add(&config.module_search_path, IFACE_MGR_UC_PATH "*.so");
	uc_search_path_add(&config.module_search_path, IFACE_MGR_UC_PATH "*.uc");

	uc_vm_init(&rptr_mgr_vm, &config);

	uc_stdlib_load(uc_vm_scope_get(&rptr_mgr_vm));
	gc_timer.cb = uc_gc_timer;

	return &rptr_mgr_vm;
}

int rptr_mgr_ucode_run(const char *script)
{
	uc_source_t *source;
	uc_program_t *prog;
	uc_value_t *ops;
	char *err;
	int ret;

	source = uc_source_new_file(script);
	if (!source) {
		fprintf(stderr, "Failed to load script file\n");
		return -1;
	}

	prog = uc_compile(rptr_mgr_vm.config, source, &err);
	uc_source_put(source);
	if (!prog) {
		fprintf(stderr, "Error loading ucode: %s\n", err);
		return -1;
	}

	ret = uc_vm_execute(&rptr_mgr_vm, prog, &ops);
	uc_program_put(prog);
	if (ret || !ops) {
		fprintf(stderr, "Script execution failed (ret=%d, ops=%p)\n", ret, ops);
		return -1;
	}

	fprintf(stderr, "%s script executed successfully\n", script);

	registry = ucv_array_new(&rptr_mgr_vm);
	uc_vm_registry_set(&rptr_mgr_vm, "rptr_mgr.registry", registry);
	ucv_array_set(registry, 0, ucv_get(ops));

	return 0;
}

int rptr_mgr_ucode_call_prepare(const char *fname)
{
	uc_value_t *obj, *func;

	if (!registry)
		return -1;

	obj = ucv_array_get(registry, 0);
	if (!obj)
		return -1;

	func = ucv_object_get(obj, fname, NULL);
	if (!ucv_is_callable(func))
		return -1;

	uc_vm_stack_push(vm, ucv_get(obj));
	uc_vm_stack_push(vm, ucv_get(func));

	return 0;
}

uc_value_t *rptr_mgr_ucode_global_init(const char *name, uc_resource_type_t *global_type)
{
	uc_value_t *global = uc_resource_new(global_type, NULL);
	uc_value_t *proto;

	uc_vm_registry_set(&rptr_mgr_vm, "rptr_mgr.global", global);
	proto = ucv_prototype_get(global);
	ucv_object_add(proto, "data", ucv_get(ucv_object_new(&rptr_mgr_vm)));

	ucv_object_add(uc_vm_scope_get(&rptr_mgr_vm), name, ucv_get(global));

	return global;
}

int rptr_mgr_ucode_registry_add(uc_value_t *reg, uc_value_t *val)
{
	int i = 0;

	while (ucv_array_get(reg, i))
		i++;

	ucv_array_set(reg, i, ucv_get(val));

	return i + 1;
}

uc_value_t *rptr_mgr_ucode_registry_get(uc_value_t *reg, int idx)
{
	if (!idx)
		return NULL;

	return ucv_array_get(reg, idx - 1);
}

uc_value_t *rptr_mgr_ucode_registry_remove(uc_value_t *reg, int idx)
{
	uc_value_t *val = rptr_mgr_ucode_registry_get(reg, idx);
	void **dataptr;

	if (!val)
		return NULL;

	ucv_array_set(reg, idx - 1, NULL);
	dataptr = ucv_resource_dataptr(val, NULL);
	if (dataptr)
		*dataptr = NULL;

	return val;
}

uc_value_t *rptr_mgr_ucode_call(size_t nargs)
{
	if (uc_vm_call(vm, true, nargs) != EXCEPTION_NONE)
		return NULL;

	if (!gc_timer.pending)
		uloop_timeout_set(&gc_timer, 10);

	return uc_vm_stack_pop(vm);
}

void rptr_mgr_ucode_free_vm(void)
{
	if (!vm || !vm->config)
		return;

	uc_search_path_free(&vm->config->module_search_path);
	uc_vm_free(vm);
	registry = NULL;
	vm = (uc_vm_t *){};
}

uc_value_t *uc_rptr_mgr_getpid(uc_vm_t *vm, size_t nargs)
{
	return ucv_int64_new(getpid());
}

int rptr_mgr_ucode_init(ifacemgr_hdl_t *ifacemgr_handle)
{
	/* Treat handle as opaque; not dereferenced in this module */
	static const uc_function_list_t global_fns[] = {
		{ "printf",     uc_rptr_mgr_printf },
		{ "getpid",     uc_rptr_mgr_getpid },
	};

	rptr_mgr_global = ifacemgr_handle;
	vm = rptr_mgr_ucode_create_vm();
	global_type = uc_type_declare(vm, "rptr_mgr.global", global_fns, NULL);

	iface_registry = ucv_array_new(vm);
	uc_vm_registry_set(vm, "rptr_mgr.iface_registry", iface_registry);
	global = rptr_mgr_ucode_global_init("rptr_mgr", global_type);


	if (rptr_mgr_ucode_run(IFACE_MGR_UC_PATH "rptr_mgr.uc"))
		goto free_vm;

	ucv_gc(vm);
	return 0;

free_vm:
	rptr_mgr_ucode_free_vm();
	return -1;
}

void rptr_mgr_ucode_free(void)
{
	if (rptr_mgr_ucode_call_prepare("shutdown") == 0)
		ucv_put(rptr_mgr_ucode_call(0));
	rptr_mgr_ucode_free_vm();
}
