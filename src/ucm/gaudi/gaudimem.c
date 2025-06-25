/*
 * Copyright (C) 2023, [Your Name/Organization Here]. ALL RIGHTS RESERVED.
 * See file LICENSE for terms.
 */

#include "gaudimem.h"
#include <ucm/api/ucm.h>
#include <ucm/event/event.h>
#include <ucm/util/log.h>
#include <ucs/debug/debug.h> // For ucs_debug, ucs_error etc.

void ucm_gaudi_mmap_hook(ucm_event_type_t event_type, ucm_event_t *event, void *arg);
void ucm_gaudi_munmap_hook(ucm_event_type_t event_type, ucm_event_t *event, void *arg);

// Placeholder for Gaudi memory event handler or other UCM related logic

ucs_status_t ucm_gaudi_mem_init(void)
{
#if HAVE_GAUDI
    ucs_status_t status;
    ucm_info("Initializing UCM Gaudi memory module (fixed handler registration)");
    status = ucm_set_event_handler(UCM_EVENT_MMAP, 0, ucm_gaudi_mmap_hook, NULL);
    if (status != UCS_OK) {
        ucm_error("Failed to register mmap event handler for Gaudi: %s", ucs_status_string(status));
        return status;
    }
    status = ucm_set_event_handler(UCM_EVENT_MUNMAP, 0, ucm_gaudi_munmap_hook, NULL);
    if (status != UCS_OK) {
        ucm_error("Failed to register munmap event handler for Gaudi: %s", ucs_status_string(status));
        ucm_unset_event_handler(UCM_EVENT_MMAP, ucm_gaudi_mmap_hook, NULL);
        return status;
    }
    return UCS_OK;
#else
    ucm_info("Gaudi support is not enabled");
    return UCS_ERR_UNSUPPORTED;
#endif
}

void ucm_gaudi_mem_cleanup(void)
{
#if HAVE_GAUDI
    ucm_info("Cleaning up UCM Gaudi memory module");
    ucm_unset_event_handler(UCM_EVENT_MMAP, ucm_gaudi_mmap_hook, NULL);
    ucm_unset_event_handler(UCM_EVENT_MUNMAP, ucm_gaudi_munmap_hook, NULL);
#endif
}

// Implementations for UCM event handler callbacks
void ucm_gaudi_mmap_hook(ucm_event_type_t event_type, ucm_event_t *event, void *arg)
{
#if HAVE_GAUDI
    if (event_type == UCM_EVENT_MMAP) {
        ucm_debug("Gaudi mmap hook: addr=%p length=%zu", event->mmap.address, event->mmap.size);
        // Potentially interact with hlthunk API here if needed
    }
#endif
}

void ucm_gaudi_munmap_hook(ucm_event_type_t event_type, ucm_event_t *event, void *arg)
{
#if HAVE_GAUDI
    if (event_type == UCM_EVENT_MUNMAP) {
        ucm_debug("Gaudi munmap hook: addr=%p length=%zu", event->munmap.address, event->munmap.size);
        // Potentially interact with hlthunk API here if needed
    }
#endif
}
// Auto-initialize this UCM module if compiled in
#if HAVE_GAUDI
// UCM_MODULE_INIT(ucm_gaudi_mem_init, ucm_gaudi_mem_cleanup);
#endif
