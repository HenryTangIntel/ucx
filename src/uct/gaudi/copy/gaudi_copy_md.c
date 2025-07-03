/**
 * Copyright (c) 2024, Habana Labs Ltd. an Intel Company
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gaudi_copy_md.h"

#include <string.h>
#include <limits.h>
#include <ucs/debug/log.h>
#include <ucs/sys/sys.h>
#include <ucs/debug/memtrack_int.h>
#include <ucs/memory/memtype_cache.h>
#include <ucs/profile/profile.h>
#include <ucs/type/class.h>
#include <ucs/sys/ptr_arith.h>
#include <uct/gaudi/base/gaudi_iface.h>


static ucs_config_field_t uct_gaudi_copy_md_config_table[] = {
    {"", "", NULL,
        ucs_offsetof(uct_gaudi_copy_md_config_t, super), UCS_CONFIG_TYPE_TABLE(uct_md_config_table)},

    {NULL}
};

static struct {} uct_gaudi_dummy_memh;

static ucs_status_t
uct_gaudi_copy_md_query(uct_md_h uct_md, uct_md_attr_v2_t *md_attr)
{
    uct_md_base_md_query(md_attr);
    md_attr->flags            = UCT_MD_FLAG_REG;
    md_attr->reg_mem_types    = UCS_BIT(UCS_MEMORY_TYPE_HOST) |
                                UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->cache_mem_types  = 0;
    md_attr->alloc_mem_types  = 0;
    md_attr->access_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST) |
                                UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->detect_mem_types = UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->dmabuf_mem_types = 0;
    md_attr->max_alloc        = 0;
    return UCS_OK;
}

UCS_PROFILE_FUNC(ucs_status_t, uct_gaudi_copy_mem_reg,
                 (md, address, length, params, memh_p),
                 uct_md_h md, void *address, size_t length,
                 const uct_md_mem_reg_params_t *params, uct_mem_h *memh_p)
{
    *memh_p = &uct_gaudi_dummy_memh;
    return UCS_OK;
}

UCS_PROFILE_FUNC(ucs_status_t, uct_gaudi_copy_mem_dereg,
                 (md, params),
                 uct_md_h md, const uct_md_mem_dereg_params_t *params)
{
    UCT_MD_MEM_DEREG_CHECK_PARAMS(params, 0);
    return UCS_OK;
}

static ucs_status_t
uct_gaudi_copy_md_query_attributes(const uct_gaudi_copy_md_t *md,
                                  const void *address, size_t length,
                                  ucs_memory_info_t *mem_info)
{
    struct hl_info_args args = {0};
    struct hl_info_hw_ip_info hw_ip;

    args.op = HL_INFO_HW_IP_INFO;
    args.return_pointer = (uintptr_t)&hw_ip;
    args.return_size = sizeof(hw_ip);

    /* TODO: find a way to get the device index */
    /* if (hlthunk_get_info(md->super.component->md_index, &args)) { */
    if (hlthunk_get_info(0, &args)) {
        return UCS_ERR_INVALID_ADDR;
    }

    mem_info->type         = UCS_MEMORY_TYPE_GAUDI;
    /* mem_info->sys_dev      = md->super.component->md_index; */
    mem_info->sys_dev      = 0;
    mem_info->base_address = (void*)hw_ip.dram_base_address;
    mem_info->alloc_length = hw_ip.dram_size;

    return UCS_OK;
}

ucs_status_t
uct_gaudi_copy_md_mem_query(uct_md_h tl_md, const void *address, size_t length,
                           uct_md_mem_attr_t *mem_attr)
{
    ucs_memory_info_t default_mem_info = {
        .type         = UCS_MEMORY_TYPE_HOST,
        .sys_dev      = UCS_SYS_DEVICE_ID_UNKNOWN,
        .base_address = (void*)address,
        .alloc_length = length
    };
    uct_gaudi_copy_md_t *md = ucs_derived_of(tl_md, uct_gaudi_copy_md_t);
    ucs_memory_info_t addr_mem_info;
    ucs_status_t status;

    if (!(mem_attr->field_mask &
          (UCT_MD_MEM_ATTR_FIELD_MEM_TYPE | UCT_MD_MEM_ATTR_FIELD_SYS_DEV |
           UCT_MD_MEM_ATTR_FIELD_BASE_ADDRESS |
           UCT_MD_MEM_ATTR_FIELD_ALLOC_LENGTH |
           UCT_MD_MEM_ATTR_FIELD_DMABUF_FD |
           UCT_MD_MEM_ATTR_FIELD_DMABUF_OFFSET))) {
        return UCS_OK;
    }

    if (address != NULL) {
        status = uct_gaudi_copy_md_query_attributes(md, address, length,
                                                   &addr_mem_info);
        if (status != UCS_OK) {
            return status;
        }

        ucs_memtype_cache_update(addr_mem_info.base_address,
                                 addr_mem_info.alloc_length, addr_mem_info.type,
                                 addr_mem_info.sys_dev);
    } else {
        addr_mem_info = default_mem_info;
    }

    if (mem_attr->field_mask & UCT_MD_MEM_ATTR_FIELD_MEM_TYPE) {
        mem_attr->mem_type = addr_mem_info.type;
    }

    if (mem_attr->field_mask & UCT_MD_MEM_ATTR_FIELD_SYS_DEV) {
        mem_attr->sys_dev = addr_mem_info.sys_dev;
    }

    if (mem_attr->field_mask & UCT_MD_MEM_ATTR_FIELD_BASE_ADDRESS) {
        mem_attr->base_address = addr_mem_info.base_address;
    }

    if (mem_attr->field_mask & UCT_MD_MEM_ATTR_FIELD_ALLOC_LENGTH) {
        mem_attr->alloc_length = addr_mem_info.alloc_length;
    }

    return UCS_OK;
}

UCS_PROFILE_FUNC(ucs_status_t, uct_gaudi_copy_md_detect_memory_type,
                 (md, address, length, mem_type_p), uct_md_h md,
                 const void *address, size_t length,
                 ucs_memory_type_t *mem_type_p)
{
    uct_md_mem_attr_t mem_attr;
    ucs_status_t status;

    mem_attr.field_mask = UCT_MD_MEM_ATTR_FIELD_MEM_TYPE;

    status = uct_gaudi_copy_md_mem_query(md, address, length, &mem_attr);
    if (status != UCS_OK) {
        return status;
    }

    *mem_type_p = mem_attr.mem_type;
    return UCS_OK;
}

static void uct_gaudi_copy_md_close(uct_md_h uct_md) {
    uct_gaudi_copy_md_t *md = ucs_derived_of(uct_md, uct_gaudi_copy_md_t);

    ucs_free(md);
}

static uct_md_ops_t md_ops = {
    .close              = uct_gaudi_copy_md_close,
    .query              = uct_gaudi_copy_md_query,
    .mem_alloc          = (uct_md_mem_alloc_func_t)ucs_empty_function_return_unsupported,
    .mem_free           = (uct_md_mem_free_func_t)ucs_empty_function_return_unsupported,
    .mem_advise         = (uct_md_mem_advise_func_t)ucs_empty_function_return_unsupported,
    .mem_reg            = uct_gaudi_copy_mem_reg,
    .mem_dereg          = uct_gaudi_copy_mem_dereg,
    .mem_query          = uct_gaudi_copy_md_mem_query,
    .mkey_pack          = (uct_md_mkey_pack_func_t)ucs_empty_function_return_success,
    .mem_attach         = (uct_md_mem_attach_func_t)ucs_empty_function_return_unsupported,
    .detect_memory_type = uct_gaudi_copy_md_detect_memory_type
};

static ucs_status_t
uct_gaudi_copy_md_open(uct_component_t *component, const char *md_name,
                      const uct_md_config_t *md_config, uct_md_h *md_p)
{
    uct_gaudi_copy_md_t *md;

    md = ucs_malloc(sizeof(uct_gaudi_copy_md_t), "uct_gaudi_copy_md_t");
    if (NULL == md) {
        ucs_error("failed to allocate memory for uct_gaudi_copy_md_t");
        return UCS_ERR_NO_MEMORY;
    }

    md->super.ops       = &md_ops;
    md->super.component = &uct_gaudi_copy_component;

    *md_p = (uct_md_h)md;

    return UCS_OK;
}

uct_component_t uct_gaudi_copy_component = {
    .query_md_resources = uct_gaudi_base_query_md_resources,
    .md_open            = uct_gaudi_copy_md_open,
    .cm_open            = (uct_component_cm_open_func_t)ucs_empty_function_return_unsupported,
    .rkey_unpack        = uct_md_stub_rkey_unpack,
    .rkey_ptr           = (uct_component_rkey_ptr_func_t)ucs_empty_function_return_unsupported,
    .rkey_release       = (uct_component_rkey_release_func_t)ucs_empty_function_return_success,
    .rkey_compare       = uct_base_rkey_compare,
    .name               = "gaudi_cpy",
    .md_config          = {
        .name           = "Gaudi-copy memory domain",
        .prefix         = "GAUDI_COPY_",
        .table          = uct_gaudi_copy_md_config_table,
        .size           = sizeof(uct_gaudi_copy_md_config_t),
    },
    .cm_config          = UCS_CONFIG_EMPTY_GLOBAL_LIST_ENTRY,
    .tl_list            = UCT_COMPONENT_TL_LIST_INITIALIZER(&uct_gaudi_copy_component),
    .flags              = 0,
    .md_vfs_init        = (uct_component_md_vfs_init_func_t)ucs_empty_function
};
UCT_COMPONENT_REGISTER(&uct_gaudi_copy_component);
