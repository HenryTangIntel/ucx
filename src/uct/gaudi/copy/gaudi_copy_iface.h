#ifndef UCT_GAUDI_COPY_IFACE_H_
#define UCT_GAUDI_COPY_IFACE_H_

#include "../base/gaudi_iface.h"
#include <uct/base/uct_iface.h>
#include <ucs/memory/memory_type.h>
#include <ucs/async/async_fwd.h>

/* Gaudi interface address structure */
typedef struct uct_gaudi_copy_iface_addr {
    uint64_t    magic;      /* Magic number for validation */
    uint64_t    iface_id;   /* Interface unique ID */
} uct_gaudi_copy_iface_addr_t;


typedef struct uct_gaudi_copy_iface {
    uct_gaudi_iface_t           super;   // Must be first for UCX base API compatibility
    uct_gaudi_copy_iface_addr_t id;      // Unique interface ID or other Gaudi-specific fields
    uct_gaudi_iface_addr_t      addr;    // Use the correct type from base/gaudi_iface.h
    int                         eventfd; // Event file descriptor for async operations
    uint64_t                    id2;     // Unique interface ID or other Gaudi-specific fields (renamed to avoid duplicate member name)
    struct {
        unsigned                max_poll;
        unsigned                max_gaudi_events;
        double                  bandwidth;
        ucs_time_t              event_timeout; /* Event timeout for async operations */
    } config;
    ucs_mpool_t                 gaudi_event_desc; // Event descriptor for Gaudi events
    ucs_queue_head_t            active_events;    // Queue of active events
    ucs_queue_head_t            pending_requests; // Queue of pending async requests
    uint64_t                    event_sequence;   // Event sequence counter
    ucs_async_context_t         *async_context;   // Async context for event handling
} uct_gaudi_copy_iface_t;


/* Gaudi interface configuration */
typedef struct uct_gaudi_copy_iface_config {
    uct_iface_config_t    super;
    unsigned              max_poll;
    unsigned              max_gaudi_events;
    double                bandwidth;
    ucs_time_t            event_timeout;     /* Timeout for async events */
    unsigned              async_max_events;  /* Max async events in flight */
} uct_gaudi_copy_iface_config_t;



typedef struct uct_gaudi_copy_event_desc {
    int              event_id;          /* Gaudi event ID or handle */
    uct_completion_t *comp;             /* Completion callback */
    ucs_queue_elem_t queue;             /* Queue element for pending events */
    ucs_time_t       start_time;        /* Event start time for timeout */
    uint64_t         sequence;          /* Sequence number for tracking */
    void            *user_data;         /* User data for the event */
} uct_gaudi_copy_event_desc_t;

/* Function declarations for cross-file access */
ucs_status_t uct_gaudi_copy_create_event(uct_gaudi_copy_iface_t *iface,
                                        uct_completion_t *comp,
                                        uct_gaudi_copy_event_desc_t **event_desc_p);

void uct_gaudi_copy_signal_event(uct_gaudi_copy_iface_t *iface);

/* Async request structure for Gaudi operations */
typedef struct uct_gaudi_copy_request {
    uct_pending_req_t    super;         /* Base pending request */
    uct_completion_t     comp;          /* Completion for async ops */
    uct_gaudi_copy_event_desc_t *event; /* Associated event descriptor */
    ucs_status_t         status;        /* Operation status */
    size_t               length;        /* Transfer length */
    void                *local_addr;    /* Local buffer address */
    uint64_t             remote_addr;   /* Remote address */
} uct_gaudi_copy_request_t;


#endif
