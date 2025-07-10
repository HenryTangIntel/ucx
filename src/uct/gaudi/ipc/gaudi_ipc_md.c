/**
 * Copyright (c) 2024, Habana Labs Ltd. an Intel Company
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gaudi_ipc_md.h"
#include "gaudi_ipc_cache.h"
#include "gaudi_ipc.inl"
#include <uct/gaudi/base/gaudi_dma.h>
#include <string.h>
#include <limits.h>
#include <ucs/debug/log.h>
#include <ucs/sys/sys.h>
#include <ucs/debug/memtrack_int.h>

#include <ucs/type/class.h>
#include <ucs/profile/profile.h>
#include <ucs/sys/string.h>
#include <uct/api/v2/uct_v2.h>
#include <sys/types.h>
#include <unistd.h>

static ucs_config_field_t uct_gaudi_ipc_md_config_table[] = {
    {"", "", NULL,
     ucs_offsetof(uct_gaudi_ipc_md_config_t, super), UCS_CONFIG_TYPE_TABLE(uct_md_config_table)},

    {NULL}
};

static ucs_status_t
uct_gaudi_ipc_md_query(uct_md_h md, uct_md_attr_v2_t *md_attr)
{
    uct_md_base_md_query(md_attr);
    md_attr->flags            = UCT_MD_FLAG_REG |
                                UCT_MD_FLAG_NEED_RKEY;
    md_attr->reg_mem_types    = UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->cache_mem_types  = UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->access_mem_types = UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->rkey_packed_size = sizeof(uct_gaudi_ipc_rkey_t);
    return UCS_OK;
}

static ucs_status_t
uct_gaudi_ipc_mem_add_reg(void *addr, size_t length, uct_gaudi_ipc_memh_t *memh,
                         uct_gaudi_ipc_lkey_t **key_p)
{
    uct_gaudi_ipc_lkey_t *key;
    ucs_status_t status;
    uint64_t base_addr;
    uint32_t size;
    int dev_idx;

    key = ucs_calloc(1, sizeof(*key), "uct_gaudi_ipc_lkey_t");
    if (key == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    /* Stub implementation - assume contiguous memory */
    base_addr = (uintptr_t)addr;
    size = length; /* Use the provided length */
    dev_idx = 0;

    key->d_bptr = (void*)base_addr;
    key->b_len = size;

    key->ph.handle = (uint64_t)(uintptr_t)addr;
    status = UCS_OK;

    ucs_list_add_tail(&memh->list, &key->link);
    ucs_trace("registered addr:%p/%p length:%zd dev_num:%d",
              addr, (void*)key->d_bptr, key->b_len, dev_idx);

    memh->dev_num = dev_idx;
    *key_p        = key;
    status        = UCS_OK;

out:
    if (status != UCS_OK) {
        ucs_free(key);
    }

    return status;
}

static ucs_status_t
uct_gaudi_ipc_mkey_pack(uct_md_h md, uct_mem_h tl_memh, void *address,
                       size_t length, const uct_md_mkey_pack_params_t *params,
                       void *mkey_buffer)
{
    uct_gaudi_ipc_rkey_t *packed = mkey_buffer;
    uct_gaudi_ipc_memh_t *memh   = tl_memh;
    uct_gaudi_ipc_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_ipc_md_t);
    uct_gaudi_ipc_lkey_t *key;
    ucs_status_t status;

    ucs_list_for_each(key, &memh->list, link) {
        if (((uintptr_t)address >= (uintptr_t)key->d_bptr) &&
            ((uintptr_t)address < ((uintptr_t)key->d_bptr + key->b_len))) {
            goto found;
        }
    }

    status = uct_gaudi_ipc_mem_add_reg(address, length, memh, &key);
    if (status != UCS_OK) {
        return status;
    }

found:
    ucs_assertv(((uintptr_t)address + length) <= ((uintptr_t)key->d_bptr + key->b_len),
                "buffer 0x%lx..0x%lx region 0x%lx..0x%lx", (uintptr_t)address,
                (uintptr_t)address + length, (uintptr_t)key->d_bptr, (uintptr_t)key->d_bptr +
                key->b_len);

    packed->pid    = memh->pid;
    packed->ph     = key->ph;
    packed->d_bptr = key->d_bptr;
    packed->b_len  = key->b_len;
    
    /* Set channel information for node-local IPC */
    packed->src_device_id = memh->dev_num;
    packed->dst_device_id = 0; /* Will be determined by destination */
    packed->channel_id = memh->channel_id;

    /* Suppress unused variable warning */
    (void)gaudi_md;

    return UCS_OK;
}

UCS_PROFILE_FUNC(ucs_status_t, uct_gaudi_ipc_rkey_unpack,
                 (component, rkey_buffer, params, rkey_p, handle_p),
                 uct_component_t *component, const void *rkey_buffer,
                 const uct_rkey_unpack_params_t *params, uct_rkey_t *rkey_p,
                 void **handle_p)
{
    uct_gaudi_ipc_rkey_t *packed   = (uct_gaudi_ipc_rkey_t *)rkey_buffer;
    uct_gaudi_ipc_unpacked_rkey_t *unpacked;

    unpacked = ucs_malloc(sizeof(*unpacked), "uct_gaudi_ipc_unpacked_rkey_t");
    if (NULL == unpacked) {
        ucs_error("failed to allocate memory for uct_gaudi_ipc_unpacked_rkey_t");
        return UCS_ERR_NO_MEMORY;
    }

    unpacked->super = *packed;

    *handle_p = NULL;
    *rkey_p   = (uintptr_t) unpacked;
    return UCS_OK;
}

static ucs_status_t uct_gaudi_ipc_rkey_release(uct_component_t *component,
                                              uct_rkey_t rkey, void *handle)
{
    ucs_assert(NULL == handle);
    ucs_free((void *)rkey);
    return UCS_OK;
}

static ucs_status_t
uct_gaudi_ipc_mem_reg(uct_md_h md, void *address, size_t length,
                     const uct_md_mem_reg_params_t *params, uct_mem_h *memh_p)
{
    uct_gaudi_ipc_memh_t *memh;
    memh = ucs_malloc(sizeof(*memh), "uct_gaudi_ipc_memh_t");
    if (NULL == memh) {
        ucs_error("failed to allocate memory for uct_gaudi_ipc_memh_t");
        return UCS_ERR_NO_MEMORY;
    }

    // IPC MD should not open or allocate device memory. Just wrap the provided pointer/handle.
    memh->dev_num = -1;
    memh->pid     = getpid();
    memh->channel_id = 0; /* Will be set when channel is created */
    ucs_list_head_init(&memh->list);

    // Optionally, store address/length or any handle passed in params for later use
    // (e.g., memh->address = address; memh->length = length;)

    *memh_p = memh;
    return UCS_OK;
}

static ucs_status_t
uct_gaudi_ipc_mem_dereg(uct_md_h md, const uct_md_mem_dereg_params_t *params)
{
    uct_gaudi_ipc_memh_t *memh = params->memh;
    uct_gaudi_ipc_lkey_t *key, *tmp;

    UCT_MD_MEM_DEREG_CHECK_PARAMS(params, 0);

    ucs_list_for_each_safe(key, tmp, &memh->list, link) {
        /* No hlthunk_ipc_handle_close available; just free the key */
        ucs_free(key);
    }

    ucs_free(memh);
    return UCS_OK;
}

static void uct_gaudi_ipc_md_close(uct_md_h md)
{
    uct_gaudi_ipc_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_ipc_md_t);
    
    /* Cleanup device file descriptors */
    if (gaudi_md->device_fds) {
        ucs_free(gaudi_md->device_fds);
    }
    
    /* Cleanup channel map */
    if (gaudi_md->channel_map) {
        ucs_free(gaudi_md->channel_map);
    }
    
    pthread_mutex_destroy(&gaudi_md->channel_lock);
    ucs_free(gaudi_md);
}

ucs_status_t uct_gaudi_ipc_channel_create(uct_gaudi_ipc_md_t *md, 
                                          uint32_t src_device, uint32_t dst_device,
                                          uint32_t *channel_id)
{
    uint64_t channel_key;

    if (src_device >= md->device_count || dst_device >= md->device_count) {
        return UCS_ERR_INVALID_PARAM;
    }

    if (md->device_fds[src_device] < 0 || md->device_fds[dst_device] < 0) {
        return UCS_ERR_NO_DEVICE;
    }

    pthread_mutex_lock(&md->channel_lock);

    /* Check if channel already exists */
    channel_key = md->channel_map[src_device * md->device_count + dst_device];
    if (channel_key != 0) {
        *channel_id = (uint32_t)channel_key;
        pthread_mutex_unlock(&md->channel_lock);
        return UCS_OK;
    }

    /* No real channel creation needed; just synthesize a unique channel ID */
    *channel_id = (src_device << 16) | dst_device;
    md->channel_map[src_device * md->device_count + dst_device] = *channel_id;
    ucs_debug("Synthesized IPC channel %u between Gaudi devices %u -> %u", 
              *channel_id, src_device, dst_device);

    pthread_mutex_unlock(&md->channel_lock);

    return UCS_OK;
}

ucs_status_t uct_gaudi_ipc_channel_destroy(uct_gaudi_ipc_md_t *md, 
                                           uint32_t channel_id)
{
    int found = 0;
    int i, j;

    pthread_mutex_lock(&md->channel_lock);

    /* Remove the channel from the map if present */
    for (i = 0; i < md->device_count; i++) {
        for (j = 0; j < md->device_count; j++) {
            if (md->channel_map[i * md->device_count + j] == channel_id) {
                md->channel_map[i * md->device_count + j] = 0;
                found = 1;
            }
        }
    }

    pthread_mutex_unlock(&md->channel_lock);

    return found ? UCS_OK : UCS_ERR_NO_ELEM;
}

ucs_status_t uct_gaudi_ipc_channel_copy(uct_gaudi_ipc_md_t *md,
                                        uint32_t channel_id,
                                        void *dst, void *src, size_t length)
{
    /*
     * No real channel exists; this is a software-only channel ID.
     * For now, just perform a memcpy as a stub. In the future, if a device-specific
     * copy API is available, use it here.
     */

   
    if (dst == NULL || src == NULL || length == 0) {
        ucs_error("Invalid parameters for IPC channel copy: dst=%p, src=%p      length=%zu",
                  dst, src, length); 
        return UCS_ERR_INVALID_PARAM;
    }
     // please implement device to device memory ASYNC copy here

    return UCS_OK;
}


static uct_md_ops_t md_ops = {
    .close              = uct_gaudi_ipc_md_close,
    .query              = uct_gaudi_ipc_md_query,
    .mem_alloc          = (uct_md_mem_alloc_func_t)ucs_empty_function_return_unsupported,
    .mem_free           = (uct_md_mem_free_func_t)ucs_empty_function_return_unsupported,
    .mem_advise         = (uct_md_mem_advise_func_t)ucs_empty_function_return_unsupported,
    .mem_reg            = uct_gaudi_ipc_mem_reg,
    .mem_dereg          = uct_gaudi_ipc_mem_dereg,
    .mem_query          = (uct_md_mem_query_func_t)ucs_empty_function_return_unsupported,
    .mkey_pack          = uct_gaudi_ipc_mkey_pack,
    .mem_attach         = (uct_md_mem_attach_func_t)ucs_empty_function_return_unsupported,
    .detect_memory_type = (uct_md_detect_memory_type_func_t)ucs_empty_function_return_unsupported
};

static ucs_status_t
uct_gaudi_ipc_md_open(uct_component_t *component, const char *md_name,
                      const uct_md_config_t *config, uct_md_h *md_p)
{
    uct_gaudi_ipc_md_t *md;

    md = ucs_calloc(1, sizeof(*md), "uct_gaudi_ipc_md");
    if (md == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    md->super.ops       = &md_ops;
    md->super.component = &uct_gaudi_ipc_component.super;

    // Optionally: initialize device detection or IPC-specific state here
    // (e.g., uct_gaudi_ipc_detect_node_devices(md);)

    *md_p = &md->super;
    return UCS_OK;
}

uct_gaudi_ipc_component_t uct_gaudi_ipc_component = {
    .super = {
        .query_md_resources = uct_gaudi_base_query_md_resources,
        .md_open            = uct_gaudi_ipc_md_open,
        .cm_open            = (uct_component_cm_open_func_t)ucs_empty_function_return_unsupported,
        .rkey_unpack        = uct_gaudi_ipc_rkey_unpack,
        .rkey_ptr           = (uct_component_rkey_ptr_func_t)ucs_empty_function_return_unsupported,
        .rkey_release       = uct_gaudi_ipc_rkey_release,
        .rkey_compare       = uct_base_rkey_compare,
        .name               = "gaudi_ipc",
        .md_config          = {
            .name           = "Gaudi-IPC memory domain",
            .prefix         = "GAUDI_IPC_",
            .table          = uct_gaudi_ipc_md_config_table,
            .size           = sizeof(uct_gaudi_ipc_md_config_t),
        },
        .cm_config          = UCS_CONFIG_EMPTY_GLOBAL_LIST_ENTRY,
        .tl_list            = UCT_COMPONENT_TL_LIST_INITIALIZER(&uct_gaudi_ipc_component.super),
        .flags              = 0,
        .md_vfs_init        =
                (uct_component_md_vfs_init_func_t)ucs_empty_function
    },
    .lock                   = PTHREAD_MUTEX_INITIALIZER
};
UCT_COMPONENT_REGISTER(&uct_gaudi_ipc_component.super);

UCS_STATIC_CLEANUP {
    pthread_mutex_destroy(&uct_gaudi_ipc_component.lock);
}
