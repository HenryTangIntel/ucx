/**
 * Copyright (c) 2024, Habana Labs. All rights reserved.
 *
 * See file LICENSE for terms.
 */

#ifndef UCT_GAUDI_IPC_MD_H
#define UCT_GAUDI_IPC_MD_H

#include <uct/base/uct_md.h>
#include <uct/gaudi/base/gaudi_md.h>
#include <ucs/datastruct/khash.h>
#include <ucs/type/spinlock.h>
#include <ucs/config/types.h>
#include <hlthunk.h>


/**
 * @brief Gaudi IPC memory handle - uses dmabuf for cross-process sharing
 */
typedef struct uct_gaudi_ipc_md_handle {
    int      dmabuf_fd;    /* dmabuf file descriptor for sharing */
    uint64_t handle;       /* original gaudi memory handle */
    size_t   length;       /* memory size */
    pid_t    owner_pid;    /* process that created the memory */
} uct_gaudi_ipc_md_handle_t;

/**
 * @brief Gaudi IPC MD configuration
 */
typedef struct uct_gaudi_ipc_md_config {
    uct_md_config_t super;
} uct_gaudi_ipc_md_config_t;

/**
 * @brief Gaudi IPC MD descriptor
 */
typedef struct uct_gaudi_ipc_md {
    uct_md_t super;
} uct_gaudi_ipc_md_t;

/**
 * @brief Gaudi IPC memory handle for registration
 */
typedef struct uct_gaudi_ipc_memh {
    uct_gaudi_ipc_md_handle_t handle;
    void                     *mapped_addr;
    int                       gaudi_fd;
} uct_gaudi_ipc_memh_t;

/**
 * @brief Gaudi IPC component
 */
typedef struct uct_gaudi_ipc_component {
    uct_component_t super;
} uct_gaudi_ipc_component_t;

extern uct_component_t uct_gaudi_ipc_component;

ucs_status_t uct_gaudi_ipc_query_md_resources(uct_component_t *component,
                                              uct_md_resource_desc_t **resources_p,
                                              unsigned *num_resources_p);

#endif