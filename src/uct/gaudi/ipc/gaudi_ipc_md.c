/**
 * Copyright (c) 2024, Habana Labs Ltd. an Intel Company
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gaudi_ipc_md.h"
#include "gaudi_ipc_cache.h"
#include "gaudi_ipc.inl"
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
uct_gaudi_ipc_mem_add_reg(void *addr, uct_gaudi_ipc_memh_t *memh,
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

    if (hlthunk_get_mem_info(addr, &base_addr, &size, &dev_idx)) {
        status = UCS_ERR_INVALID_ADDR;
        goto out;
    }

    key->d_bptr = (void*)base_addr;
    key->b_len = size;

    status = hlthunk_ipc_handle_create(addr, &key->ph.handle);
    if (status != UCS_OK) {
        goto out;
    }

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
    uct_gaudi_ipc_lkey_t *key;
    ucs_status_t status;

    ucs_list_for_each(key, &memh->list, link) {
        if (((uintptr_t)address >= (uintptr_t)key->d_bptr) &&
            ((uintptr_t)address < ((uintptr_t)key->d_bptr + key->b_len))) {
            goto found;
        }
    }

    status = uct_gaudi_ipc_mem_add_reg(address, memh, &key);
    if (status != UCS_OK) {
        return status;
    }

found:
    ucs_assertv(((uintptr_t)address + length) <= ((uintptr_t)key->d_bptr + key->b_len),
                "buffer 0x%lx..0x%lx region 0x%p..0x%p", (uintptr_t)address,
                (uintptr_t)address + length, key->d_bptr, (uintptr_t)key->d_bptr +
                key->b_len);

    packed->pid    = memh->pid;
    packed->ph     = key->ph;
    packed->d_bptr = key->d_bptr;
    packed->b_len  = key->b_len;

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

    memh->dev_num = -1;
    memh->pid     = getpid();
    ucs_list_head_init(&memh->list);

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
        hlthunk_ipc_handle_close(key->ph.handle);
        ucs_free(key);
    }

    ucs_free(memh);
    return UCS_OK;
}

static void uct_gaudi_ipc_md_close(uct_md_h md)
{
    ucs_free(md);
}

static ucs_status_t
uct_gaudi_ipc_md_open(uct_component_t *component, const char *md_name,
                     const uct_md_config_t *config, uct_md_h *md_p)
{
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
    uct_gaudi_ipc_md_t* md;

    md = ucs_calloc(1, sizeof(*md), "uct_gaudi_ipc_md");
    if (md == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    md->super.ops       = &md_ops;
    md->super.component = &uct_gaudi_ipc_component.super;
    *md_p               = &md->super;

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
