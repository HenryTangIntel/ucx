#include "gaudi_copy_iface.h"
#include "gaudi_copy_md.h"
#include "gaudi_copy_ep.h"
#include "../base/gaudi_md.h"

#include <uct/gaudi/base/gaudi_iface.h>
#include <uct/gaudi/base/gaudi_md.h>
#include <ucs/type/class.h>
#include <ucs/sys/string.h>
#include <ucs/async/eventfd.h>
#include <ucs/arch/cpu.h>

/* Forward declarations */
static UCS_F_ALWAYS_INLINE int
uct_gaudi_copy_event_is_ready(uct_gaudi_copy_event_desc_t *event_desc);

static unsigned uct_gaudi_copy_iface_progress(uct_iface_h tl_iface);

ucs_status_t uct_gaudi_copy_create_event(uct_gaudi_copy_iface_t *iface,
                                        uct_completion_t *comp,
                                        uct_gaudi_copy_event_desc_t **event_desc_p);

void uct_gaudi_copy_signal_event(uct_gaudi_copy_iface_t *iface);
#include <ucs/memory/memory_type.h>
#include <ucs/async/async.h>
#include <ucs/time/time.h>
#include <errno.h>
#include <unistd.h>

#ifndef UCS_MEMORY_TYPE_GAUDI
#define UCS_MEMORY_TYPE_GAUDI 100  // Use a value not conflicting with existing types
#endif

#define UCT_GAUDI_COPY_IFACE_OVERHEAD 0
#define UCT_GAUDI_COPY_IFACE_LATENCY  ucs_linear_func_make(8e-6, 0)
#define UCT_GAUDI_IFACE_ADDR_MAGIC   0xDEADBEEF12345678ULL
#define UCT_GAUDI_TL_NAME            "gaudi"


static ucs_config_field_t uct_gaudi_copy_iface_config_table[] = {

    {"", "", NULL,
     ucs_offsetof(uct_gaudi_copy_iface_config_t, super),
     UCS_CONFIG_TYPE_TABLE(uct_iface_config_table)},

    {"MAX_POLL", "16",
     "Max number of event completions to pick during Gaudi events polling",
     ucs_offsetof(uct_gaudi_copy_iface_config_t, max_poll), UCS_CONFIG_TYPE_UINT},

    {"MAX_EVENTS", "inf",
     "Max number of Gaudi events. -1 is infinite",
     ucs_offsetof(uct_gaudi_copy_iface_config_t, max_gaudi_events), UCS_CONFIG_TYPE_UINT},

    {"BW", "10000MBs",
     "Effective memory bandwidth",
     ucs_offsetof(uct_gaudi_copy_iface_config_t, bandwidth), UCS_CONFIG_TYPE_BW},

    {"EVENT_TIMEOUT", "5s",
     "Timeout for async events completion",
     ucs_offsetof(uct_gaudi_copy_iface_config_t, event_timeout), UCS_CONFIG_TYPE_TIME},

    {"ASYNC_MAX_EVENTS", "128",
     "Maximum number of async events in flight",
     ucs_offsetof(uct_gaudi_copy_iface_config_t, async_max_events), UCS_CONFIG_TYPE_UINT},

    {NULL}
};
/* Forward declaration for the delete function */
static void UCS_CLASS_DELETE_FUNC_NAME(uct_gaudi_copy_iface_t)(uct_iface_t*);

static ucs_status_t uct_gaudi_copy_iface_get_address(uct_iface_h tl_iface,
                                                    uct_iface_addr_t *iface_addr)
{
    uct_gaudi_copy_iface_t *iface = ucs_derived_of(tl_iface, uct_gaudi_copy_iface_t);

    *(uct_gaudi_copy_iface_addr_t*)iface_addr = iface->id;
    return UCS_OK;
}


static int uct_gaudi_copy_iface_is_reachable_v2(
        const uct_iface_h tl_iface,
        const uct_iface_is_reachable_params_t *params)
{
    //uct_gaudi_copy_iface_t *iface = ucs_derived_of(tl_iface,
                                                //  uct_gaudi_copy_iface_t);
    //uct_gaudi_copy_iface_addr_t *addr;

    //if (!uct_iface_is_reachable_params_addrs_valid(params)) {
      //  return 0;
    //}

    //addr = (uct_gaudi_copy_iface_addr_t*)params->iface_addr;

    //return (addr != NULL) && (iface->id == *addr) &&
    //       uct_iface_scope_is_reachable(tl_iface, params);
    return 1;
}





static ucs_status_t uct_gaudi_copy_iface_query(uct_iface_h tl_iface, 
                                         uct_iface_attr_t *iface_attr)
{
    uct_gaudi_copy_iface_t *iface = ucs_derived_of(tl_iface, uct_gaudi_copy_iface_t);

    uct_base_iface_query(&iface->super.super, iface_attr);

   iface_attr->iface_addr_len          = sizeof(uct_gaudi_copy_iface_addr_t);
    iface_attr->device_addr_len         = 0;
    iface_attr->ep_addr_len             = 0;
    iface_attr->cap.flags               = UCT_IFACE_FLAG_CONNECT_TO_IFACE |
                                          UCT_IFACE_FLAG_GET_SHORT |
                                          UCT_IFACE_FLAG_PUT_SHORT |
                                          UCT_IFACE_FLAG_GET_ZCOPY |
                                          UCT_IFACE_FLAG_PUT_ZCOPY |
                                          UCT_IFACE_FLAG_PENDING;

    iface_attr->cap.event_flags         = UCT_IFACE_FLAG_EVENT_SEND_COMP |
                                          UCT_IFACE_FLAG_EVENT_RECV      |
                                          UCT_IFACE_FLAG_EVENT_FD        |
                                          UCT_IFACE_FLAG_EVENT_ASYNC_CB     |
                                          UCT_IFACE_FLAG_EVENT_RECV_SIG;

    iface_attr->cap.put.max_short       = UINT_MAX;
    iface_attr->cap.put.max_bcopy       = 0;
    iface_attr->cap.put.min_zcopy       = 0;
    iface_attr->cap.put.max_zcopy       = SIZE_MAX;
    iface_attr->cap.put.opt_zcopy_align = 1;
    iface_attr->cap.put.align_mtu       = iface_attr->cap.put.opt_zcopy_align;
    iface_attr->cap.put.max_iov         = 1;

    iface_attr->cap.get.max_short       = UINT_MAX;
    iface_attr->cap.get.max_bcopy       = 0;
    iface_attr->cap.get.min_zcopy       = 0;
    iface_attr->cap.get.max_zcopy       = SIZE_MAX;
    iface_attr->cap.get.opt_zcopy_align = 1;
    iface_attr->cap.get.align_mtu       = iface_attr->cap.get.opt_zcopy_align;
    iface_attr->cap.get.max_iov         = 1;

    iface_attr->cap.am.max_short        = 0;
    iface_attr->cap.am.max_bcopy        = 0;
    iface_attr->cap.am.min_zcopy        = 0;
    iface_attr->cap.am.max_zcopy        = 0;
    iface_attr->cap.am.opt_zcopy_align  = 1;
    iface_attr->cap.am.align_mtu        = iface_attr->cap.am.opt_zcopy_align;
    iface_attr->cap.am.max_hdr          = 0;
    iface_attr->cap.am.max_iov          = 1;

    iface_attr->latency                 = UCT_GAUDI_COPY_IFACE_LATENCY;
    iface_attr->bandwidth.dedicated     = 0;
    iface_attr->bandwidth.shared        = 1000.0; //iface->config.bandwidth;
    iface_attr->overhead                = UCT_GAUDI_COPY_IFACE_OVERHEAD;
    iface_attr->priority                = 0;

    return UCS_OK;
}

static ucs_status_t uct_gaudi_copy_iface_flush(uct_iface_h tl_iface, unsigned flags,
                                              uct_completion_t *comp)
{
    uct_gaudi_copy_iface_t *iface = ucs_derived_of(tl_iface, uct_gaudi_copy_iface_t);
    uct_gaudi_copy_event_desc_t *event_desc;
    ucs_queue_iter_t iter;
    unsigned active_ops = 0;

    /* Count active operations */
    ucs_queue_for_each_safe(event_desc, iter, &iface->active_events, queue) {
        if (!uct_gaudi_copy_event_is_ready(event_desc)) {
            active_ops++;
        }
    }

    /* If completion callback is provided and there are active operations */
    if (comp != NULL && active_ops > 0) {
        if (flags & UCT_FLUSH_FLAG_CANCEL) {
            /* Cancel all active operations */
            ucs_queue_for_each_safe(event_desc, iter, &iface->active_events, queue) {
                ucs_queue_del_iter(&iface->active_events, iter);
                if (event_desc->comp != NULL) {
                    uct_invoke_completion(event_desc->comp, UCS_ERR_CANCELED);
                }
                ucs_mpool_put(event_desc);
            }
            UCT_TL_IFACE_STAT_FLUSH(ucs_derived_of(tl_iface, uct_base_iface_t));
            return UCS_OK;
        } else {
            /* Return INPROGRESS to indicate async flush */
            UCT_TL_IFACE_STAT_FLUSH_WAIT(ucs_derived_of(tl_iface, uct_base_iface_t));
            return UCS_INPROGRESS;
        }
    }

    /* No active operations or no completion callback */
    if (active_ops > 0) {
        UCT_TL_IFACE_STAT_FLUSH_WAIT(ucs_derived_of(tl_iface, uct_base_iface_t));
        return UCS_INPROGRESS;
    }

    UCT_TL_IFACE_STAT_FLUSH(ucs_derived_of(tl_iface, uct_base_iface_t));
    return UCS_OK;
}


/* Helper function to create a new Gaudi event for async operations */
ucs_status_t uct_gaudi_copy_create_event(uct_gaudi_copy_iface_t *iface,
                                               uct_completion_t *comp,
                                               uct_gaudi_copy_event_desc_t **event_desc_p)
{
    uct_gaudi_copy_event_desc_t *event_desc;
    int event_id = -1;
    
    /* Get event descriptor from pool */
    event_desc = ucs_mpool_get(&iface->gaudi_event_desc);
    if (event_desc == NULL) {
        return UCS_ERR_NO_MEMORY;
    }
    
#ifdef HAVE_HLTHUNK_H
    /* Create Gaudi event using hlthunk
     * In real implementation this would be something like:
     * event_id = hlthunk_create_event(device_fd);
     * For now, simulate event creation
     */
    event_id = (int)(event_desc->sequence % 1000); /* Simulate event ID */
#endif
    
    /* Initialize event descriptor */
    event_desc->event_id = event_id;
    event_desc->comp = comp;
    event_desc->start_time = ucs_get_time();
    event_desc->user_data = NULL;
    
    /* Add to active events queue */
    ucs_queue_push(&iface->active_events, &event_desc->queue);
    
    ucs_trace("Created Gaudi event %d for async operation, sequence %lu",
              event_id, event_desc->sequence);
    
    *event_desc_p = event_desc;
    return UCS_OK;
}

/* Helper function to signal async event completion */
void uct_gaudi_copy_signal_event(uct_gaudi_copy_iface_t *iface)
{
    uint64_t dummy = 1;
    ssize_t ret;
    
    /* Signal the eventfd to wake up async processing */
    ret = write(iface->eventfd, &dummy, sizeof(dummy));
    if (ret != sizeof(dummy)) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ucs_warn("Failed to signal Gaudi eventfd: %m");
        }
    } else {
        ucs_trace_poll("Signaled Gaudi eventfd for async processing");
    }
}

/* Helper function to submit async operation with event tracking */
static ucs_status_t __attribute__((unused))
uct_gaudi_copy_post_async_op(uct_gaudi_copy_iface_t *iface,
                            uct_completion_t *comp,
                            const char *op_name)
{
    uct_gaudi_copy_event_desc_t *event_desc;
    ucs_status_t status;
    
    /* Create event for tracking this async operation */
    status = uct_gaudi_copy_create_event(iface, comp, &event_desc);
    if (status != UCS_OK) {
        return status;
    }
    
    /* In real implementation, this would submit the operation to Gaudi
     * For example: hlthunk_submit_async_op(device_fd, event_desc->event_id, ...);
     */
    
    ucs_debug("Posted async %s operation with event %d", 
              op_name, event_desc->event_id);
    
    /* Signal that there's async work to be processed */
    uct_gaudi_copy_signal_event(iface);
    
    return UCS_INPROGRESS;
}

/* Async event handler function */
static void uct_gaudi_copy_async_event_handler(int fd, ucs_event_set_types_t events, void *arg)
{
    uct_gaudi_copy_iface_t *iface = (uct_gaudi_copy_iface_t *)arg;
    uint64_t dummy;
    
    /* Read from eventfd to clear the event */
    if (read(fd, &dummy, sizeof(dummy)) < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ucs_warn("Failed to read from Gaudi eventfd: %m");
        }
    }
    
    /* Trigger progress on the interface */
    uct_gaudi_copy_iface_progress(&iface->super.super.super);
}

/* Check if an event is ready for completion */
static UCS_F_ALWAYS_INLINE int
uct_gaudi_copy_event_is_ready(uct_gaudi_copy_event_desc_t *event_desc)
{
    ucs_time_t current_time;
    
    if (event_desc->event_id < 0) {
        return 0; /* Invalid event */
    }
    
#ifdef HAVE_HLTHUNK_H
    /* In real implementation, this would check hlthunk event status
     * For example: return hlthunk_wait_and_release_event(device_fd, event_desc->event_id);
     * For now, simulate completion based on time
     */
#endif
    
    /* Simulate event completion after some time */
    current_time = ucs_get_time();
    if (event_desc->start_time != 0 && 
        (current_time - event_desc->start_time) > ucs_time_from_usec(100)) {
        return 1; /* Event completed */
    }
    
    return 0;
}

/* Process completed events from the active queue */
static UCS_F_ALWAYS_INLINE unsigned
uct_gaudi_copy_progress_events(uct_gaudi_copy_iface_t *iface, unsigned max_events)
{
    uct_gaudi_copy_event_desc_t *event_desc;
    ucs_queue_iter_t iter;
    unsigned count = 0;
    
    ucs_queue_for_each_safe(event_desc, iter, &iface->active_events, queue) {
        if (!uct_gaudi_copy_event_is_ready(event_desc)) {
            continue; /* Event not ready yet */
        }
        
        /* Remove from active queue */
        ucs_queue_del_iter(&iface->active_events, iter);
        
        /* Complete the operation */
        if (event_desc->comp != NULL) {
            ucs_trace_data("Gaudi event %d completed for desc %p, sequence %lu",
                          event_desc->event_id, event_desc, event_desc->sequence);
            uct_invoke_completion(event_desc->comp, UCS_OK);
        }
        
        /* Return event descriptor to pool */
        ucs_mpool_put(event_desc);
        
        count++;
        if (count >= max_events) {
            break;
        }
    }
    
    return count;
}

static unsigned uct_gaudi_copy_iface_progress(uct_iface_h tl_iface)
{
    uct_gaudi_copy_iface_t *iface = ucs_derived_of(tl_iface, uct_gaudi_copy_iface_t);
    unsigned max_events = iface->config.max_poll;
    unsigned count = 0;
    
    /* Process completed events */
    count += uct_gaudi_copy_progress_events(iface, max_events);
    
    /* Process pending requests if any */
    if (!ucs_queue_is_empty(&iface->pending_requests)) {
        /* Dispatch pending async requests */
        ucs_trace_poll("Processing %zu pending Gaudi requests", 
                      ucs_queue_length(&iface->pending_requests));
        /* Implementation would process pending queue here */
    }
    
    return count;
}

static ucs_status_t uct_gaudi_copy_iface_event_fd_arm(uct_iface_h tl_iface,
                                                    unsigned events)
{
    uct_gaudi_copy_iface_t *iface = ucs_derived_of(tl_iface, uct_gaudi_copy_iface_t);
    ucs_status_t status;
    
    /* Check if there are any ready events that would make arming unnecessary */
    if (!ucs_queue_is_empty(&iface->active_events)) {
        uct_gaudi_copy_event_desc_t *event_desc;
        ucs_queue_iter_t iter;
        
        ucs_queue_for_each_safe(event_desc, iter, &iface->active_events, queue) {
            if (uct_gaudi_copy_event_is_ready(event_desc)) {
                return UCS_ERR_BUSY; /* Events are ready, no need to arm */
            }
        }
    }
    
    /* Poll the eventfd to see if there are already pending events */
    status = ucs_async_eventfd_poll(iface->eventfd);
    if (status == UCS_OK) {
        return UCS_ERR_BUSY; /* Events are already pending */
    } else if (status == UCS_ERR_IO_ERROR) {
        return status; /* Error occurred */
    }
    
    /* No events pending, it's safe to arm */
    ucs_assertv(status == UCS_ERR_NO_PROGRESS, "Unexpected status: %s", 
                ucs_status_string(status));
    
    /* Enable async event monitoring */
    if (iface->async_context != NULL) {
        status = ucs_async_set_event_handler(UCS_ASYNC_MODE_THREAD,
                                           iface->eventfd,
                                           UCS_EVENT_SET_EVREAD,
                                           uct_gaudi_copy_async_event_handler,
                                           iface,
                                           iface->async_context);
        if (status != UCS_OK) {
            ucs_error("Failed to set Gaudi async event handler: %s",
                     ucs_status_string(status));
            return status;
        }
    }
    
    ucs_trace("Armed Gaudi interface %p for async events", iface);
    return UCS_OK;
}

static uct_iface_ops_t uct_gaudi_copy_iface_ops = {
    .ep_get_short             = uct_gaudi_copy_ep_get_short,
    .ep_put_short             = uct_gaudi_copy_ep_put_short,
    .ep_get_zcopy             = uct_gaudi_copy_ep_get_zcopy,
    .ep_put_zcopy             = uct_gaudi_copy_ep_put_zcopy,
    .ep_pending_add           = ucs_empty_function_return_busy,
    .ep_pending_purge         = ucs_empty_function,
    .ep_flush                 = uct_base_ep_flush,
    .ep_fence                 = uct_base_ep_fence,
    .ep_create                = UCS_CLASS_NEW_FUNC_NAME(uct_gaudi_copy_ep_t),
    .ep_destroy               = UCS_CLASS_DELETE_FUNC_NAME(uct_gaudi_copy_ep_t),
    .iface_flush              = uct_gaudi_copy_iface_flush,
    .iface_fence              = uct_base_iface_fence,
    .iface_progress_enable    = uct_base_iface_progress_enable,
    .iface_progress_disable   = uct_base_iface_progress_disable,
    .iface_progress           = uct_gaudi_copy_iface_progress,
    .iface_event_fd_get       = uct_gaudi_base_iface_event_fd_get,
    .iface_event_arm          = uct_gaudi_copy_iface_event_fd_arm,
    .iface_close              = UCS_CLASS_DELETE_FUNC_NAME(uct_gaudi_copy_iface_t),
    .iface_query              = uct_gaudi_copy_iface_query,
    .iface_get_device_address = (uct_iface_get_device_address_func_t)ucs_empty_function_return_success,
    .iface_get_address        = uct_gaudi_copy_iface_get_address,
    .iface_is_reachable       = uct_base_iface_is_reachable
};

static void uct_gaudi_copy_event_desc_init(ucs_mpool_t *mp, void *obj, void *chunk)
{
    uct_gaudi_copy_event_desc_t *event_desc = (uct_gaudi_copy_event_desc_t *) obj;
    uct_gaudi_copy_iface_t *iface = ucs_container_of(mp, uct_gaudi_copy_iface_t, gaudi_event_desc);

    memset(event_desc, 0, sizeof(*event_desc));
    event_desc->event_id = -1;  /* Invalid event ID initially */
    event_desc->comp = NULL;
    event_desc->start_time = 0;
    event_desc->sequence = ++iface->event_sequence;
    event_desc->user_data = NULL;
    
    ucs_trace("Gaudi event desc initialized: %p, sequence: %lu", 
              event_desc, event_desc->sequence);
}

static void uct_gaudi_copy_event_desc_cleanup(ucs_mpool_t *mp, void *obj)
{
    uct_gaudi_copy_event_desc_t *event_desc = (uct_gaudi_copy_event_desc_t *) obj;

    /* Clean up any Gaudi-specific event resources */
    if (event_desc->event_id >= 0) {
#ifdef HAVE_HLTHUNK_H
        /* Placeholder for Gaudi event cleanup
         * In real implementation this would call hlthunk event cleanup functions
         * For example: hlthunk_wait_and_release_event(iface->device_fd, event_desc->event_id);
         */
#endif
        ucs_debug("Cleaning up Gaudi event %d for desc %p", 
                  event_desc->event_id, event_desc);
        event_desc->event_id = -1;
    }
    
    /* Event has been removed from queue during progress iteration */
    ucs_trace("Event descriptor %p cleaned up", event_desc);
}

static ucs_status_t
uct_gaudi_copy_estimate_perf(uct_iface_h tl_iface, uct_perf_attr_t *perf_attr)
{
    uct_gaudi_copy_iface_t *iface   = ucs_derived_of(tl_iface,
                                                    uct_gaudi_copy_iface_t);
    uct_ep_operation_t op          = UCT_ATTR_VALUE(PERF, perf_attr, operation,
                                                    OPERATION, UCT_EP_OP_LAST);
    ucs_memory_type_t src_mem_type = UCT_ATTR_VALUE(PERF, perf_attr,
                                                    local_memory_type,
                                                    LOCAL_MEMORY_TYPE,
                                                    UCS_MEMORY_TYPE_UNKNOWN);
    ucs_memory_type_t dst_mem_type = UCT_ATTR_VALUE(PERF, perf_attr,
                                                    remote_memory_type,
                                                    REMOTE_MEMORY_TYPE,
                                                    UCS_MEMORY_TYPE_UNKNOWN);
    int zcopy                      = uct_ep_op_is_zcopy(op);
    const double latency           = 1.8e-6;
    const double overhead          = 4.0e-6;

    if (perf_attr->field_mask & UCT_PERF_ATTR_FIELD_BANDWIDTH) {
        if (uct_ep_op_is_fetch(op)) {
            ucs_swap(&src_mem_type, &dst_mem_type);
        }

        perf_attr->bandwidth.dedicated = 0;
        if ((src_mem_type == UCS_MEMORY_TYPE_HOST) &&
            (dst_mem_type == UCS_MEMORY_TYPE_GAUDI)) {
            perf_attr->bandwidth.shared = (zcopy ? 8300.0 : 7900.0) * UCS_MBYTE;
        } else if ((src_mem_type == UCS_MEMORY_TYPE_GAUDI) &&
                   (dst_mem_type == UCS_MEMORY_TYPE_HOST)) {
            perf_attr->bandwidth.shared = (zcopy ? 11660.0 : 9320.0) *
                                          UCS_MBYTE;
        } else if ((src_mem_type == UCS_MEMORY_TYPE_GAUDI) &&
                   (dst_mem_type == UCS_MEMORY_TYPE_GAUDI)) {
            perf_attr->bandwidth.shared = 320.0 * UCS_GBYTE;
        } else {
            perf_attr->bandwidth.shared = iface->config.bandwidth;
        }
    }

    if (perf_attr->field_mask & UCT_PERF_ATTR_FIELD_SEND_PRE_OVERHEAD) {
        perf_attr->send_pre_overhead = overhead;
    }

    if (perf_attr->field_mask & UCT_PERF_ATTR_FIELD_SEND_POST_OVERHEAD) {
        /* In case of sync mem copy, the send operation CPU overhead includes
           the latency of waiting for the copy to complete */
        perf_attr->send_post_overhead = zcopy ? 0 : latency;
    }

    if (perf_attr->field_mask & UCT_PERF_ATTR_FIELD_RECV_OVERHEAD) {
        perf_attr->recv_overhead = 0;
    }

    if (perf_attr->field_mask & UCT_PERF_ATTR_FIELD_LATENCY) {
        /* In case of async mem copy, the latency is not part of the overhead
           and it's a standalone property */
        perf_attr->latency = ucs_linear_func_make(zcopy ? latency : 0.0, 0.0);
    }

    if (perf_attr->field_mask & UCT_PERF_ATTR_FIELD_MAX_INFLIGHT_EPS) {
        perf_attr->max_inflight_eps = SIZE_MAX;
    }

    return UCS_OK;
}



static ucs_mpool_ops_t uct_gaudi_copy_event_desc_mpool_ops = {
    .chunk_alloc   = ucs_mpool_chunk_malloc,
    .chunk_release = ucs_mpool_chunk_free,
    .obj_init      = uct_gaudi_copy_event_desc_init,
    .obj_cleanup   = uct_gaudi_copy_event_desc_cleanup,
    .obj_str       = NULL
};

static uct_iface_internal_ops_t uct_gaudi_copy_iface_internal_ops = {
    .iface_estimate_perf   = uct_gaudi_copy_estimate_perf,
    .iface_vfs_refresh     = (uct_iface_vfs_refresh_func_t)ucs_empty_function,
    .ep_query              = (uct_ep_query_func_t)ucs_empty_function_return_unsupported,
    .ep_invalidate         = (uct_ep_invalidate_func_t)ucs_empty_function_return_unsupported,
    .ep_connect_to_ep_v2   = ucs_empty_function_return_unsupported,
    .iface_is_reachable_v2 = uct_gaudi_copy_iface_is_reachable_v2,
    .ep_is_connected       = uct_base_ep_is_connected
};

static UCS_CLASS_INIT_FUNC(uct_gaudi_copy_iface_t, uct_md_h md, uct_worker_h worker,
                           const uct_iface_params_t *params,
                           const uct_iface_config_t *tl_config)
{

    
    uct_gaudi_copy_iface_config_t *config = ucs_derived_of(tl_config,
                                                          uct_gaudi_copy_iface_config_t);
    ucs_status_t status;
    ucs_mpool_params_t mp_params;

    UCS_CLASS_CALL_SUPER_INIT(uct_gaudi_iface_t, &uct_gaudi_copy_iface_ops,
                              &uct_gaudi_copy_iface_internal_ops, md, worker,
                              params, tl_config, "gaudi_copy");

    status = uct_gaudi_base_check_device_name(params);
    if (status != UCS_OK) {
        return status;
    }

    self->id.iface_id                     = ucs_generate_uuid((uintptr_t)self);
    self->id.magic = UCT_GAUDI_IFACE_ADDR_MAGIC;
    self->config.max_poll        = config->max_poll;
    self->config.max_gaudi_events = config->max_gaudi_events;
    self->config.bandwidth       = config->bandwidth;
    self->config.event_timeout   = config->event_timeout;
    
    /* Initialize event sequence counter */
    self->event_sequence = 0;
    
    /* Initialize queues for events and requests */
    ucs_queue_head_init(&self->active_events);
    ucs_queue_head_init(&self->pending_requests);
    
    /* Initialize async context from worker */
    self->async_context = ucs_derived_of(worker, uct_priv_worker_t)->async;
    self->eventfd = UCS_ASYNC_EVENTFD_INVALID_FD;
    
    /* Create eventfd for async operations */
    status = ucs_async_eventfd_create(&self->eventfd);
    if (status != UCS_OK) {
        ucs_error("Failed to create eventfd for Gaudi interface: %s",
                  ucs_status_string(status));
        goto err_mpool_cleanup;
    }

    ucs_mpool_params_reset(&mp_params);
    mp_params.elem_size       = sizeof(uct_gaudi_copy_event_desc_t);
    mp_params.elems_per_chunk = 128;
    mp_params.max_elems       = self->config.max_gaudi_events;
    mp_params.ops             = &uct_gaudi_copy_event_desc_mpool_ops;
    mp_params.name            = "GAUDI EVENT objects";
    status = ucs_mpool_init(&mp_params, &self->gaudi_event_desc);
    if (UCS_OK != status) {
        ucs_error("mpool creation failed");
        goto err_eventfd_destroy;
    }
    /*
    ucs_queue_head_init(&self->active_queue);

    ucs_memory_type_for_each(src) {
        ucs_memory_type_for_each(dst) {
            self->queue_desc[src][dst].stream = 0;
            ucs_queue_head_init(&self->queue_desc[src][dst].event_queue);
        }
    }

    self->short_stream = 0;
    self->cuda_context = 0;

    */

    ucs_debug("Gaudi copy interface initialized: eventfd=%d max_events=%u",
              self->eventfd, self->config.max_gaudi_events);
    return UCS_OK;

err_eventfd_destroy:
    ucs_async_eventfd_destroy(self->eventfd);
err_mpool_cleanup:
    ucs_mpool_cleanup(&self->gaudi_event_desc, 1);
    return status;
}

static UCS_CLASS_CLEANUP_FUNC(uct_gaudi_copy_iface_t)
{
    uct_gaudi_copy_event_desc_t *event_desc;
    ucs_queue_iter_t iter;

    /* Disable progress to prevent new events */
    uct_base_iface_progress_disable(&self->super.super.super,
                                    UCT_PROGRESS_SEND | UCT_PROGRESS_RECV);

    /* Clean up active events */
    ucs_queue_for_each_safe(event_desc, iter, &self->active_events, queue) {
        ucs_queue_del_iter(&self->active_events, iter);
        if (event_desc->comp != NULL) {
            ucs_warn("Gaudi event %d still active during cleanup, completing with error",
                     event_desc->event_id);
            uct_invoke_completion(event_desc->comp, UCS_ERR_CANCELED);
        }
        ucs_mpool_put(event_desc);
    }

    /* Clean up pending requests */
    if (!ucs_queue_is_empty(&self->pending_requests)) {
        ucs_warn("Gaudi interface has %zu pending requests during cleanup",
                 ucs_queue_length(&self->pending_requests));
        /* Individual requests should clean themselves up */
    }

    /* Remove async event handler if set */
    if (self->async_context != NULL && self->eventfd != UCS_ASYNC_EVENTFD_INVALID_FD) {
        ucs_async_remove_handler(self->eventfd, 1);
    }

    /* Destroy eventfd */
    if (self->eventfd != UCS_ASYNC_EVENTFD_INVALID_FD) {
        ucs_async_eventfd_destroy(self->eventfd);
    }

    /* Clean up memory pool */
    ucs_mpool_cleanup(&self->gaudi_event_desc, 1);
    
    ucs_debug("Gaudi copy interface cleaned up");
}

UCS_CLASS_DEFINE(uct_gaudi_copy_iface_t, uct_gaudi_iface_t);
UCS_CLASS_DEFINE_NEW_FUNC(uct_gaudi_copy_iface_t, uct_iface_t, uct_md_h, uct_worker_h,
                          const uct_iface_params_t*, const uct_iface_config_t*);
static UCS_CLASS_DEFINE_DELETE_FUNC(uct_gaudi_copy_iface_t, uct_iface_t);


UCT_TL_DEFINE(&uct_gaudi_copy_component, gaudi_cpy, uct_gaudi_base_query_devices,
              uct_gaudi_copy_iface_t, "GAUDI_COPY_",
              uct_gaudi_copy_iface_config_table, uct_gaudi_copy_iface_config_t);




#if 0

static ucs_status_t uct_gaudi_iface_get_address(uct_iface_h tl_iface, 
                                               uct_iface_addr_t *iface_addr)
{
    uct_gaudi_iface_t *iface = ucs_derived_of(tl_iface, uct_gaudi_iface_t);
    uct_gaudi_iface_addr_t *addr = (uct_gaudi_iface_addr_t *)iface_addr;
    
    addr->magic = UCT_GAUDI_IFACE_ADDR_MAGIC;
    addr->iface_id = iface->id;
    
    return UCS_OK;
}

static int uct_gaudi_iface_is_reachable_v2(const uct_iface_h tl_iface,
                                          const uct_iface_is_reachable_params_t *params)
{
    uct_gaudi_iface_t *iface = ucs_derived_of(tl_iface, uct_gaudi_iface_t);
    const uct_gaudi_iface_addr_t *addr;
    const uint64_t *dev_addr;
    
    if (!uct_iface_is_reachable_params_addrs_valid(params)) {
        return 0;
    }
    
    dev_addr = (const uint64_t*)params->device_addr;
    if (*dev_addr != ucs_get_system_id()) {
        return 0; /* Different systems */
    }
    
    addr = (const uct_gaudi_iface_addr_t*)params->iface_addr;
    if (addr->magic != UCT_GAUDI_IFACE_ADDR_MAGIC) {
        return 0; /* Invalid magic */
    }
    
    /* Same interface cannot reach itself */
    if (addr->iface_id == iface->id) {
        return 0;
    }
    
    return uct_iface_scope_is_reachable(tl_iface, params);
}

static ucs_status_t uct_gaudi_iface_flush(uct_iface_h tl_iface, unsigned flags,
                                         uct_completion_t *comp)
{
    if (comp != NULL) {
        return UCS_ERR_UNSUPPORTED;
    }
    
    UCT_TL_IFACE_STAT_FLUSH(ucs_derived_of(tl_iface, uct_base_iface_t));
    return UCS_OK;
}

static unsigned uct_gaudi_iface_progress(uct_iface_h tl_iface)
{
    /* TODO: Implement actual progress for pending operations */
    return 0;
}

/* Endpoint operations stubs - to be implemented */
static ucs_status_t uct_gaudi_ep_put_short(uct_ep_h tl_ep, const void *buffer,
                                          unsigned length, uint64_t remote_addr,
                                          uct_rkey_t rkey)
{
    /* TODO: Implement short put using Gaudi DMA */
    UCT_TL_EP_STAT_OP(ucs_derived_of(tl_ep, uct_base_ep_t), PUT, SHORT, length);
    return UCS_OK;
}

static ucs_status_t uct_gaudi_ep_get_short(uct_ep_h tl_ep, void *buffer,
                                          unsigned length, uint64_t remote_addr,
                                          uct_rkey_t rkey)
{
    /* TODO: Implement short get using Gaudi DMA */
    UCT_TL_EP_STAT_OP(ucs_derived_of(tl_ep, uct_base_ep_t), GET, SHORT, length);
    return UCS_OK;
}

static ucs_status_t uct_gaudi_ep_am_short(uct_ep_h tl_ep, uint8_t id, uint64_t header,
                                    const void *payload, unsigned length)
{
    /* TODO: Implement active messages */
    return UCS_OK;
}

static ucs_status_t uct_gaudi_ep_put_zcopy(uct_ep_h tl_ep, const uct_iov_t *iov,
                                          size_t iovcnt, uint64_t remote_addr,
                                          uct_rkey_t rkey, uct_completion_t *comp)
{
    /* TODO: Implement zero-copy put */
    return UCS_INPROGRESS;
}

static ucs_status_t uct_gaudi_ep_get_zcopy(uct_ep_h tl_ep, const uct_iov_t *iov,
                                          size_t iovcnt, uint64_t remote_addr,
                                          uct_rkey_t rkey, uct_completion_t *comp)
{
    /* TODO: Implement zero-copy get */
    return UCS_INPROGRESS;
}

/* Endpoint class */
typedef struct uct_gaudi_ep {
    uct_base_ep_t super;
    uint64_t remote_id;
} uct_gaudi_ep_t;

static UCS_CLASS_INIT_FUNC(uct_gaudi_ep_t, const uct_ep_params_t *params)
{
    uct_gaudi_iface_t *iface = ucs_derived_of(params->iface, uct_gaudi_iface_t);
    const uct_gaudi_iface_addr_t *addr = (const uct_gaudi_iface_addr_t*)params->iface_addr;
    
    UCS_CLASS_CALL_SUPER_INIT(uct_base_ep_t, &iface->super);
    
    self->remote_id = addr->iface_id;
    
    return UCS_OK;
}

static UCS_CLASS_CLEANUP_FUNC(uct_gaudi_ep_t)
{
}

UCS_CLASS_DEFINE(uct_gaudi_ep_t, uct_base_ep_t)
UCS_CLASS_DEFINE_NEW_FUNC(uct_gaudi_ep_t, uct_ep_t, const uct_ep_params_t *);
UCS_CLASS_DEFINE_DELETE_FUNC(uct_gaudi_ep_t, uct_ep_t);

/**/
static uct_iface_ops_t uct_gaudi_iface_ops = {
    .ep_put_short             = uct_gaudi_ep_put_short,
    .ep_get_short             = uct_gaudi_ep_get_short,
    .ep_am_short              = uct_gaudi_ep_am_short,
    .ep_put_zcopy             = uct_gaudi_ep_put_zcopy,
    .ep_get_zcopy             = uct_gaudi_ep_get_zcopy,
    .ep_pending_add           = ucs_empty_function_return_busy,
    .ep_pending_purge         = ucs_empty_function,
    .ep_flush                 = uct_base_ep_flush,
    .ep_fence                 = uct_base_ep_fence,
    .ep_create                = UCS_CLASS_NEW_FUNC_NAME(uct_gaudi_ep_t),
    .ep_destroy               = UCS_CLASS_DELETE_FUNC_NAME(uct_gaudi_ep_t),
    .iface_flush              = uct_gaudi_iface_flush,
    .iface_fence              = uct_base_iface_fence,
    .iface_progress_enable    = uct_base_iface_progress_enable,
    .iface_progress_disable   = uct_base_iface_progress_disable,
    .iface_progress           = uct_gaudi_iface_progress,
    .iface_close              = UCS_CLASS_DELETE_FUNC_NAME(uct_gaudi_iface_t),
    .iface_query              = uct_gaudi_iface_query,
    .iface_get_device_address = (uct_iface_get_device_address_func_t) ucs_empty_function_return_success,
    .iface_get_address        = uct_gaudi_iface_get_address,
    .iface_is_reachable       = uct_base_iface_is_reachable,
};

static uct_iface_internal_ops_t uct_gaudi_iface_internal_ops = {
    .iface_estimate_perf   = uct_base_iface_estimate_perf,
    .iface_vfs_refresh     = (uct_iface_vfs_refresh_func_t)ucs_empty_function,
    .ep_query              = (uct_ep_query_func_t)ucs_empty_function_return_unsupported,
    .ep_invalidate         = (uct_ep_invalidate_func_t)ucs_empty_function_return_unsupported,
    .ep_connect_to_ep_v2   = ucs_empty_function_return_unsupported,
    .iface_is_reachable_v2 = uct_gaudi_iface_is_reachable_v2,
    .ep_is_connected       = uct_base_ep_is_connected
};




static UCS_CLASS_INIT_FUNC(uct_gaudi_iface_t, uct_md_h md, uct_worker_h worker,
                          const uct_iface_params_t *params,
                          const uct_iface_config_t *tl_config)
{
    uct_gaudi_iface_config_t *config = ucs_derived_of(tl_config, 
                                                      uct_gaudi_iface_config_t);
    ucs_status_t status;

    UCS_CLASS_CALL_SUPER_INIT(uct_base_iface_t, &uct_gaudi_iface_ops,
                              &uct_gaudi_iface_internal_ops,
                              md, worker, params, tl_config);

    status = uct_gaudi_base_check_device_name(params);
    if (status != UCS_OK) {
        ucs_error("Failed to check device name for Gaudi interface: %s",
                  ucs_status_string(status));
        return status;
    }

    self->id.iface_id = ucs_generate_uuid((uintptr_t)self);
    self->id.magic = UCT_GAUDI_IFACE_ADDR_MAGIC;
    self->config.bandwidth = config->bandwidth;
    self->config.latency = config->latency;
    self->config.max_short = config->max_short;

    return UCS_OK;
}

static UCS_CLASS_CLEANUP_FUNC(uct_gaudi_iface_t)
{
      //ucs_async_eventfd_destroy(self->eventfd);
}

UCS_CLASS_DEFINE(uct_gaudi_iface_t, uct_base_iface_t);


#endif