/*
 * Copyright (C) 2023, [Your Name/Organization Here]. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#ifndef UCT_GAUDI_IFACE_H
#define UCT_GAUDI_IFACE_H

#include "gaudi_md.h" // For uct_gaudi_md_t if needed by iface
#include <uct/base/uct_iface.h>

// Forward declarations
typedef struct uct_gaudi_iface          uct_gaudi_iface_t;
typedef struct uct_gaudi_iface_config   uct_gaudi_iface_config_t;
typedef struct uct_gaudi_ep             uct_gaudi_ep_t; // Forward declare EP type

struct uct_gaudi_iface_config {
    uct_iface_config_t super;
    // Add Gaudi-specific interface configuration options here if any
    // unsigned int max_bcopy; // Example
};

extern ucs_config_field_t uct_gaudi_iface_config_table[]; // Configuration table

struct uct_gaudi_iface {
    uct_iface_t super;
    // Add Gaudi-specific interface members here
    // For example, stream/queue handles for Gaudi operations
    // For this placeholder, we can add a dummy queue ID or similar
    int dummy_gaudi_queue_id;
    uct_gaudi_iface_config_t config; // Store parsed config
};

// Gaudi endpoint structure (minimal placeholder)
struct uct_gaudi_ep {
    uct_base_ep_t super;
    // Add Gaudi-specific endpoint members
    // int dummy_gaudi_conn_id;
};


#endif // UCT_GAUDI_IFACE_H
