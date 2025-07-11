/**
 * Copyright (c) 2024, Habana Labs. All rights reserved.
 *
 * See file LICENSE for terms.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gaudi_copy_md.h"
#include <uct/gaudi/base/gaudi_md.h>

static ucs_config_field_t uct_gaudi_copy_md_config_table[] = {
    {"", "", NULL,
     ucs_offsetof(uct_gaudi_copy_md_config_t, super), UCS_CONFIG_TYPE_TABLE(uct_md_config_table)},

    {"ASYNC_COPY", "y",
     "Enable asynchronous copy operations",
     ucs_offsetof(uct_gaudi_copy_md_config_t, enable_async_copy), UCS_CONFIG_TYPE_BOOL},

    {"MAX_COPY_SIZE", "1GB",
     "Maximum copy size in a single operation",
     ucs_offsetof(uct_gaudi_copy_md_config_t, max_copy_size), UCS_CONFIG_TYPE_MEMUNITS},

    {NULL}
};

static ucs_status_t uct_gaudi_copy_md_query(uct_md_h md, uct_md_attr_t *md_attr)
{
    uct_gaudi_copy_md_t *gaudi_md = ucs_derived_of(md, uct_gaudi_copy_md_t);

    md_attr->cap.flags            = UCT_MD_FLAG_REG;
    md_attr->cap.reg_mem_types    = UCS_BIT(UCS_MEMORY_TYPE_HOST) |
                                    UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->cap.alloc_mem_types  = 0;
    md_attr->cap.access_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST) |
                                    UCS_BIT(UCS_MEMORY_TYPE_GAUDI);
    md_attr->cap.detect_mem_types = 0;
    md_attr->cap.max_alloc        = 0;
    md_attr->cap.max_reg          = gaudi_md->config.max_copy_size;
    md_attr->cap.max_rkey_size    = 0;
    md_attr->rkey_packed_size     = 0;
    md_attr->reg_cost             = ucs_linear_func_make(1000e-9, 0.007e-9);
    md_attr->local_cpus           = UCS_CPU_SET_EMPTY;
    return UCS_OK;
}

static ucs_status_t uct_gaudi_copy_md_mem_reg(uct_md_h md, void *address,
                                              size_t length, unsigned flags,
                                              uct_mem_h *memh_p)
{
    *memh_p = address; /* Simple registration - just store the address */
    return UCS_OK;
}

static ucs_status_t uct_gaudi_copy_md_mem_dereg(uct_md_h md, uct_mem_h memh)
{
    return UCS_OK; /* Nothing to cleanup */
}

static uct_md_ops_t uct_gaudi_copy_md_ops = {
    .close         = uct_md_close_empty,
    .query         = uct_gaudi_copy_md_query,
    .mem_reg       = uct_gaudi_copy_md_mem_reg,
    .mem_dereg     = uct_gaudi_copy_md_mem_dereg,
};

static ucs_status_t uct_gaudi_copy_md_open(uct_component_t *component,
                                           const char *md_name,
                                           const uct_md_config_t *config,
                                           uct_md_h *md_p)
{
    const uct_gaudi_copy_md_config_t *gaudi_config = 
        ucs_derived_of(config, uct_gaudi_copy_md_config_t);
    uct_gaudi_copy_md_t *md;

    md = ucs_malloc(sizeof(*md), "gaudi_copy_md");
    if (md == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    md->super.ops       = &uct_gaudi_copy_md_ops;
    md->super.component = component;
    
    md->config.async_copy_enabled = gaudi_config->enable_async_copy;
    md->config.max_copy_size      = gaudi_config->max_copy_size;

    *md_p = &md->super;
    return UCS_OK;
}

typedef struct uct_gaudi_copy_component {
    uct_component_t super;
} uct_gaudi_copy_component_t;

static uct_gaudi_copy_component_t uct_gaudi_copy_component = {
    .super = {
        .query_md_resources = uct_gaudi_base_query_md_resources,
        .md_open            = uct_gaudi_copy_md_open,
        .cm_open            = ucs_empty_function_return_unsupported,
        .rkey_unpack        = ucs_empty_function_return_unsupported,
        .rkey_ptr           = ucs_empty_function_return_unsupported,
        .rkey_release       = ucs_empty_function_return_unsupported,
        .rkey_compare       = uct_base_rkey_compare,
        .name               = "gaudi_copy",
        .md_config          = {
            .name           = "Gaudi copy memory domain",
            .prefix         = "GAUDI_COPY_",
            .table          = uct_gaudi_copy_md_config_table,
            .size           = sizeof(uct_gaudi_copy_md_config_t),
        },
        .cm_config          = UCS_CONFIG_EMPTY_GLOBAL_LIST_ENTRY,
        .tl_list            = UCT_COMPONENT_TL_LIST_INITIALIZER(&uct_gaudi_copy_component.super),
        .flags              = 0
    }
};

UCT_COMPONENT_REGISTER(&uct_gaudi_copy_component.super);