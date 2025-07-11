/**
 * Copyright (c) 2024, Habana Labs. All rights reserved.
 *
 * See file LICENSE for terms.
 */

#ifndef UCT_GAUDI_IPC_IFACE_H
#define UCT_GAUDI_IPC_IFACE_H

#include <uct/base/uct_iface.h>
#include <uct/gaudi/gaudi_ipc/gaudi_ipc_md.h>

#define UCT_GAUDI_IPC_TL_NAME "gaudi_ipc"

typedef struct uct_gaudi_ipc_iface_config {
    uct_iface_config_t super;
} uct_gaudi_ipc_iface_config_t;

typedef struct uct_gaudi_ipc_iface {
    uct_base_iface_t super;
} uct_gaudi_ipc_iface_t;

typedef struct uct_gaudi_ipc_ep {
    uct_base_ep_t super;
    pid_t         remote_pid;
} uct_gaudi_ipc_ep_t;

extern uct_component_t uct_gaudi_ipc_component;

#endif