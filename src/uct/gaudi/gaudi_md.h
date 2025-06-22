/*
 * Copyright (C) 2023, [Your Name/Organization Here]. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#ifndef UCT_GAUDI_MD_H
#define UCT_GAUDI_MD_H

#include <uct/base/uct_md.h>
#include <ucs/config/types.h> // For ucs_config_field_table_t if used in config

// Forward declarations
typedef struct uct_gaudi_md           uct_gaudi_md_t;
typedef struct uct_gaudi_md_config    uct_gaudi_md_config_t;
typedef struct uct_gaudi_mem_alloc_params uct_gaudi_mem_alloc_params_t;

struct uct_gaudi_md_config {
    uct_md_config_t super;
    // No Gaudi-specific MD config options for now
};

extern ucs_config_field_t uct_gaudi_md_config_table[]; // Configuration table

struct uct_gaudi_md {
    uct_md_t super;
    // Add Gaudi-specific MD members here
    // For example, a handle to the Gaudi device or context
    // For this placeholder, we can add a dummy device ID or similar
    int dummy_gaudi_device_id;
};

/* Defines parameters for uct_gaudi_mem_alloc */
struct uct_gaudi_mem_alloc_params {
    uct_mem_alloc_params_t super;
    // Add Gaudi-specific memory allocation parameters if any
    // For example, memory type (device, host-pinned), flags etc.
    // unsigned gaudi_mem_flags;
};


extern uct_component_t uct_gaudi_md_component;

#endif // UCT_GAUDI_MD_H
