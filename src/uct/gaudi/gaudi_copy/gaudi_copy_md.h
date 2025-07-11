/**
 * Copyright (c) 2024, Habana Labs. All rights reserved.
 *
 * See file LICENSE for terms.
 */

#ifndef UCT_GAUDI_COPY_MD_H
#define UCT_GAUDI_COPY_MD_H

#include <uct/base/uct_md.h>
#include <uct/gaudi/base/gaudi_md.h>

typedef struct uct_gaudi_copy_md_config {
    uct_md_config_t super;
    int             enable_async_copy;
    size_t          max_copy_size;
} uct_gaudi_copy_md_config_t;

typedef struct uct_gaudi_copy_md {
    uct_md_t super;
    struct {
        int             async_copy_enabled;
        size_t          max_copy_size;
    } config;
} uct_gaudi_copy_md_t;

extern uct_component_t uct_gaudi_copy_component;

#endif