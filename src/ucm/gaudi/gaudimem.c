/*
 * Copyright (C) 2023, [Your Name/Organization Here]. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#include <dlfcn.h>
#include "gaudimem.h"
#include <ucm/event/event.h>
#include <dlfcn.h>
#include <ucm/util/log.h>
#include <ucm/util/reloc.h>
#include <ucm/util/replace.h>
#include <ucs/debug/assert.h>
#include <ucm/util/sys.h>
#include <ucs/sys/compiler.h>
#include <ucs/sys/preprocessor.h>
#include <sys/mman.h>
#include <unistd.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

#if HAVE_GAUDI

UCM_DEFINE_REPLACE_DLSYM_FUNC(hlthunk_allocate_device_memory, int, -1, int, void **, size_t)
UCM_DEFINE_REPLACE_DLSYM_FUNC(hlthunk_free_device_memory, int, -1, int, void *)

static UCS_F_ALWAYS_INLINE void
ucm_dispatch_gaudi_mem_type_alloc(void *addr, size_t length)
{
    ucm_event_t event;
    event.mem_type.address  = addr;
    event.mem_type.size     = length;
    event.mem_type.mem_type = UCS_MEMORY_TYPE_GAUDI_DEVICE;
    ucm_event_dispatch(UCM_EVENT_MEM_TYPE_ALLOC, &event);
}

static UCS_F_ALWAYS_INLINE void
ucm_dispatch_gaudi_mem_type_free(void *addr, size_t length)
{
    ucm_event_t event;
    event.mem_type.address  = addr;
    event.mem_type.size     = length;
    event.mem_type.mem_type = UCS_MEMORY_TYPE_GAUDI_DEVICE;
    ucm_event_dispatch(UCM_EVENT_MEM_TYPE_FREE, &event);
}

int ucm_hlthunk_allocate_device_memory(int device_id, void **dptr, size_t size)
{
    int ret;
    ucm_event_enter();
    ret = ucm_orig_hlthunk_allocate_device_memory(device_id, dptr, size);
    if (ret == 0) {
        ucm_trace("ucm_hlthunk_allocate_device_memory(ptr=%p size:%lu)", *dptr, size);
        ucm_dispatch_gaudi_mem_type_alloc(*dptr, size);
    }
    ucm_event_leave();
    return ret;
}

int ucm_hlthunk_free_device_memory(int device_id, void *dptr)
{
    int ret;
    ucm_event_enter();
    ucm_trace("ucm_hlthunk_free_device_memory(device_id=%d, ptr=%p)", device_id, dptr);
    // For now, we don't know the size, so pass 0
    ucm_dispatch_gaudi_mem_type_free(dptr, 0);
    ret = ucm_orig_hlthunk_free_device_memory(device_id, dptr);
    ucm_event_leave();
    return ret;
}

static ucm_reloc_patch_t patches[] = {
    {"hlthunk_allocate_device_memory", ucm_hlthunk_allocate_device_memory},
    {"hlthunk_free_device_memory", ucm_hlthunk_free_device_memory},
    {NULL, NULL}
};

static ucs_status_t ucm_gaudimem_install(int events)
{
    static int ucm_gaudimem_installed = 0;
    static pthread_mutex_t install_mutex = PTHREAD_MUTEX_INITIALIZER;
    ucm_reloc_patch_t *patch;
    ucs_status_t status = UCS_OK;

    if (!(events & (UCM_EVENT_MEM_TYPE_ALLOC | UCM_EVENT_MEM_TYPE_FREE))) {
        goto out;
    }

    pthread_mutex_lock(&install_mutex);

    if (ucm_gaudimem_installed) {
        goto out_unlock;
    }

    for (patch = patches; patch->symbol != NULL; ++patch) {
        status = ucm_reloc_modify(patch);
        if (status != UCS_OK) {
            ucm_warn("failed to install relocation table entry for '%s'", patch->symbol);
            goto out_unlock;
        }
    }

    ucm_info("Gaudi hooks are ready");
    ucm_gaudimem_installed = 1;

out_unlock:
    pthread_mutex_unlock(&install_mutex);
out:
    return status;
}

static void ucm_gaudimem_get_existing_alloc(ucm_event_handler_t *handler)
{
    // No-op for now, could scan /proc/self/maps for Gaudi regions if needed
}

static ucm_event_installer_t ucm_gaudi_initializer = {
    .install            = ucm_gaudimem_install,
    .get_existing_alloc = ucm_gaudimem_get_existing_alloc
};

UCS_STATIC_INIT
{
    ucs_list_add_tail(&ucm_event_installer_list, &ucm_gaudi_initializer.list);
}

UCS_STATIC_CLEANUP
{
    ucs_list_del(&ucm_gaudi_initializer.list);
}

#endif // HAVE_GAUDI
