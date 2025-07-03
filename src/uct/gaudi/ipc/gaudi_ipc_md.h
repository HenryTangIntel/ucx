/**
 * Copyright (c) 2024, Habana Labs Ltd. an Intel Company
 */

#ifndef UCT_GAUDI_IPC_MD_H
#define UCT_GAUDI_IPC_MD_H

#include <uct/base/uct_md.h>
#include <uct/gaudi/base/gaudi_md.h>
#include <uct/gaudi/base/gaudi_iface.h>
#include <ucs/datastruct/khash.h>
#include <ucs/type/spinlock.h>
#include <ucs/config/types.h>


typedef struct uct_gaudi_ipc_md_handle {
    uint64_t handle;
} uct_gaudi_ipc_md_handle_t;

/**
 * @brief gaudi ipc MD descriptor
 */
typedef struct uct_gaudi_ipc_md {
    uct_md_t                 super;
} uct_gaudi_ipc_md_t;


/**
 * @brief gaudi ipc component extension
 */
typedef struct {
    uct_component_t             super;
    pthread_mutex_t             lock;
} uct_gaudi_ipc_component_t;

extern uct_gaudi_ipc_component_t uct_gaudi_ipc_component;

/**
 * @brief gaudi ipc domain configuration.
 */
typedef struct uct_gaudi_ipc_md_config {
    uct_md_config_t          super;
} uct_gaudi_ipc_md_config_t;


/**
 * @brief list of gaudi ipc regions registered for memh
 */
typedef struct {
    pid_t           pid;
    int             dev_num;
    ucs_list_link_t list;
} uct_gaudi_ipc_memh_t;


/**
 * @brief gaudi ipc region registered for exposure
 */
typedef struct {
    uct_gaudi_ipc_md_handle_t  ph;
    void*                     d_bptr;
    size_t                    b_len;
    ucs_list_link_t           link;
} uct_gaudi_ipc_lkey_t;


/**
 * @brief gaudi ipc remote key for put/get
 */
typedef struct {
    uct_gaudi_ipc_md_handle_t  ph;
    pid_t                     pid;
    void*                     d_bptr;
    size_t                    b_len;
} uct_gaudi_ipc_rkey_t;


typedef struct {
    uct_gaudi_ipc_rkey_t       super;
} uct_gaudi_ipc_unpacked_rkey_t;

#endif
