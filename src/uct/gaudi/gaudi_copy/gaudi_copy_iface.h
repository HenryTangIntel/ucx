/**
 * Copyright (c) 2024, Habana Labs. All rights reserved.
 *
 * See file LICENSE for terms.
 */

#ifndef UCT_GAUDI_COPY_IFACE_H
#define UCT_GAUDI_COPY_IFACE_H

#include <uct/base/uct_iface.h>
#include <uct/gaudi/gaudi_copy/gaudi_copy_md.h>
#include <ucs/sys/event_set.h>

#define UCT_GAUDI_COPY_TL_NAME "gaudi_copy"

typedef struct uct_gaudi_copy_iface_config {
    uct_iface_config_t  super;
    unsigned            bandwidth;
    double              overhead;
    double              latency;
    size_t              max_short;
    size_t              max_bcopy;
    size_t              max_zcopy;
} uct_gaudi_copy_iface_config_t;

typedef struct uct_gaudi_copy_iface {
    uct_base_iface_t super;
    struct {
        unsigned        bandwidth;
        double          overhead; 
        double          latency;
        size_t          max_short;
        size_t          max_bcopy;
        size_t          max_zcopy;
    } config;
} uct_gaudi_copy_iface_t;

extern uct_component_t uct_gaudi_copy_component;

#endif