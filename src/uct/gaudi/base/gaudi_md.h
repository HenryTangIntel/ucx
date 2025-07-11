/**
 * Copyright (c) 2024, Habana Labs. All rights reserved.
 *
 * See file LICENSE for terms.
 */

#ifndef UCT_GAUDI_MD_H
#define UCT_GAUDI_MD_H

#include <uct/base/uct_md.h>
#include <hlthunk.h>

typedef struct uct_gaudi_md_config {
    uct_md_config_t super;
} uct_gaudi_md_config_t;

typedef struct uct_gaudi_md {
    uct_md_t super;
} uct_gaudi_md_t;

typedef struct uct_gaudi_mem {
    void    *address;
    size_t   length;
    int      fd;
    uint64_t handle;
    int      dmabuf_fd;
} uct_gaudi_mem_t;

ucs_status_t uct_gaudi_base_query_md_resources(uct_component_t *component,
                                               uct_md_resource_desc_t **resources_p,
                                               unsigned *num_resources_p);

ucs_status_t uct_gaudi_base_query_tl_devices(uct_md_h md,
                                             uct_tl_device_resource_t **tl_devices_p,
                                             unsigned *num_tl_devices_p);

extern uct_component_t uct_gaudi_md_component;

#endif