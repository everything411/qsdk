/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * SPDX-License-Identifier: ISC
 */

#ifndef RPTR_MGR_UCODE__H
#define RPTR_MGR_UCODE__H

#include <ucode/lib.h>
#include <ucode/vm.h>

typedef void ifacemgr_hdl_t;
#define IFACE_MGR_UC_PATH "/usr/share/rptr_mgr/"

extern uc_value_t *uc_registry;

/*
 * Create embedded ucode VM for repeater manager
 */
uc_vm_t *wpa_ucode_create_vm(void);

/*
 * Load and execute ucode script from filesystem
 */
int wpa_ucode_run(const char *script);

/*
 * Prepare a call into a named ucode function (push callee on stack)
 */
int wpa_ucode_call_prepare(const char *fname);

/*
 * Invoke previously prepared ucode call with nargs arguments
 */
uc_value_t *wpa_ucode_call(size_t nargs);

/*
 * Free ucode VM and related resources
 */
void wpa_ucode_free_vm(void);

uc_value_t *wpa_ucode_global_init(const char *name, uc_resource_type_t *global_type);

int wpa_ucode_registry_add(uc_value_t *reg, uc_value_t *val);
uc_value_t *wpa_ucode_registry_get(uc_value_t *reg, int idx);
uc_value_t *wpa_ucode_registry_remove(uc_value_t *reg, int idx);

uc_value_t *uc_wpa_udebug_set(uc_vm_t *vm, size_t nargs);
uc_value_t *uc_wpa_printf(uc_vm_t *vm, size_t nargs);
uc_value_t *uc_wpa_getpid(uc_vm_t *vm, size_t nargs);

/*
 * Initialize ucode bridge and run top-level script
 */
int rptr_mgr_ucode_init(ifacemgr_hdl_t *ifacemgr_handle);

/*
 * Gracefully shutdown ucode bridge and VM
 */
void rptr_mgr_ucode_free(void);
#endif
