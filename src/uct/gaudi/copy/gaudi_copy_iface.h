#ifndef UCT_GAUDI_COPY_IFACE_H_
#define UCT_GAUDI_COPY_IFACE_H_

#include "../base/gaudi_iface.h"
#include <uct/base/uct_iface.h>
#include <ucs/memory/memory_type.h>

/* Gaudi interface address structure */
typedef struct uct_gaudi_copy_iface_addr {
    uint64_t    magic;      /* Magic number for validation */
    uint64_t    iface_id;   /* Interface unique ID */
} uct_gaudi_copy_iface_addr_t;


typedef struct uct_gaudi_copy_iface {
    uct_gaudi_iface_t           super;   // Must be first for UCX base API compatibility
    uct_gaudi_copy_iface_addr_t id;      // Unique interface ID or other Gaudi-specific fields
    uct_gaudi_iface_addr_t      addr;    // Use the correct type from base/gaudi_iface.h
    int  eventfd; // Event file descriptor for async operations
    uint64_t                   id2;      // Unique interface ID or other Gaudi-specific fields (renamed to avoid duplicate member name)
    struct {
        unsigned                max_poll;
        unsigned                max_cuda_events;
        double                  bandwidth;
    } config;
    ucs_mpool_t gaudi_event_desc; // Event descriptor for Gaudi events
} uct_gaudi_copy_iface_t;


/* Gaudi interface configuration */
typedef struct uct_gaudi_copy_iface_config {
    uct_iface_config_t    super;
    unsigned                max_poll;
    unsigned                max_cuda_events;
    double                  bandwidth;
} uct_gaudi_copy_iface_config_t;



typedef struct uct_gaudi_copy_event_desc {
    int       event;
    uct_completion_t *comp;
    ucs_queue_elem_t queue;
} uct_gaudi_copy_event_desc_t;


#endif
