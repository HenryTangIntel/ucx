/**
 * Copyright (c) 2024, Habana Labs. All rights reserved.
 *
 * See file LICENSE for terms.
 */

#ifndef UCT_GAUDI_COPY_EP_H
#define UCT_GAUDI_COPY_EP_H

#include <uct/base/uct_iface.h>
#include <uct/gaudi/gaudi_copy/gaudi_copy_iface.h>

typedef struct uct_gaudi_copy_ep {
    uct_base_ep_t super;
} uct_gaudi_copy_ep_t;

ucs_status_t uct_gaudi_copy_ep_create_connected(const uct_ep_params_t *params,
                                                uct_ep_h *ep_p);

extern uct_ep_ops_t uct_gaudi_copy_ep_ops;

#endif