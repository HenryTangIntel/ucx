/**
 * Copyright (c) 2024, Habana Labs. All rights reserved.
 *
 * See file LICENSE for terms.
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gaudi_copy_ep.h"
#include "gaudi_copy_iface.h"

#include <ucs/type/class.h>
#include <ucs/arch/cpu.h>
#include <hlthunk.h>
#include <string.h>

static ucs_status_t uct_gaudi_copy_ep_put_short(uct_ep_h tl_ep, 
                                                const void *buffer,
                                                unsigned length,
                                                uint64_t remote_addr,
                                                uct_rkey_t rkey)
{
    /* Use optimized memory copy for short messages */
    if (length <= 64) {
        /* Fast path for very small copies */
        memcpy((void*)remote_addr, buffer, length);
    } else {
        /* Use optimized copy routines */
        ucs_arch_memcpy_relaxed((void*)remote_addr, buffer, length);
    }
    
    return UCS_OK;
}

static ucs_status_t uct_gaudi_copy_ep_put_bcopy(uct_ep_h tl_ep,
                                                uct_pack_callback_t pack_cb,
                                                void *arg, uint64_t remote_addr,
                                                uct_rkey_t rkey)
{
    uct_gaudi_copy_ep_t *ep = ucs_derived_of(tl_ep, uct_gaudi_copy_ep_t);
    size_t length;
    
    /* Pack data directly to destination */
    length = pack_cb((void*)remote_addr, arg);
    
    return length;
}

static ucs_status_t uct_gaudi_copy_ep_put_zcopy(uct_ep_h tl_ep,
                                                const uct_iov_t *iov,
                                                size_t iovcnt,
                                                uint64_t remote_addr,
                                                uct_rkey_t rkey,
                                                uct_completion_t *comp)
{
    uct_gaudi_copy_ep_t *ep = ucs_derived_of(tl_ep, uct_gaudi_copy_ep_t);
    uct_gaudi_copy_iface_t *iface = ucs_derived_of(ep->super.iface, uct_gaudi_copy_iface_t);
    void *dst_ptr = (void*)remote_addr;
    size_t total_length = 0;
    size_t i;

    /* Handle multiple IOV entries */
    for (i = 0; i < iovcnt; ++i) {
        if (iov[i].length > 0) {
            /* Use high-performance memory copy */
            if (iov[i].length >= iface->config.max_bcopy) {
                /* Large copy - use optimized routines */
                ucs_arch_memcpy_relaxed(dst_ptr, iov[i].buffer, iov[i].length);
            } else {
                /* Medium copy */
                memcpy(dst_ptr, iov[i].buffer, iov[i].length);
            }
            
            dst_ptr = (char*)dst_ptr + iov[i].length;
            total_length += iov[i].length;
        }
    }

    /* Complete immediately for synchronous copy */
    if (comp != NULL) {
        uct_invoke_completion(comp, UCS_OK);
    }

    return UCS_OK;
}

static ucs_status_t uct_gaudi_copy_ep_get_bcopy(uct_ep_h tl_ep,
                                                uct_unpack_callback_t unpack_cb,
                                                void *arg, size_t length,
                                                uint64_t remote_addr,
                                                uct_rkey_t rkey,
                                                uct_completion_t *comp)
{
    /* Unpack data directly from source */
    ucs_status_t status = unpack_cb(arg, (void*)remote_addr, length);
    
    if (comp != NULL) {
        uct_invoke_completion(comp, status);
    }
    
    return status;
}

static ucs_status_t uct_gaudi_copy_ep_get_zcopy(uct_ep_h tl_ep,
                                                const uct_iov_t *iov,
                                                size_t iovcnt,
                                                uint64_t remote_addr,
                                                uct_rkey_t rkey,
                                                uct_completion_t *comp)
{
    uct_gaudi_copy_ep_t *ep = ucs_derived_of(tl_ep, uct_gaudi_copy_ep_t);
    uct_gaudi_copy_iface_t *iface = ucs_derived_of(ep->super.iface, uct_gaudi_copy_iface_t);
    void *src_ptr = (void*)remote_addr;
    size_t total_length = 0;
    size_t i;

    /* Handle multiple IOV entries */
    for (i = 0; i < iovcnt; ++i) {
        if (iov[i].length > 0) {
            /* Use high-performance memory copy */
            if (iov[i].length >= iface->config.max_bcopy) {
                /* Large copy - use optimized routines */
                ucs_arch_memcpy_relaxed(iov[i].buffer, src_ptr, iov[i].length);
            } else {
                /* Medium copy */
                memcpy(iov[i].buffer, src_ptr, iov[i].length);
            }
            
            src_ptr = (char*)src_ptr + iov[i].length;
            total_length += iov[i].length;
        }
    }

    /* Complete immediately for synchronous copy */
    if (comp != NULL) {
        uct_invoke_completion(comp, UCS_OK);
    }

    return UCS_OK;
}

static UCS_CLASS_CLEANUP_FUNC(uct_gaudi_copy_ep_t)
{
    /* Nothing to cleanup */
}

uct_ep_ops_t uct_gaudi_copy_ep_ops = {
    .ep_put_short             = uct_gaudi_copy_ep_put_short,
    .ep_put_bcopy             = uct_gaudi_copy_ep_put_bcopy,
    .ep_put_zcopy             = uct_gaudi_copy_ep_put_zcopy,
    .ep_get_bcopy             = uct_gaudi_copy_ep_get_bcopy,
    .ep_get_zcopy             = uct_gaudi_copy_ep_get_zcopy,
    .ep_pending_add           = ucs_empty_function_return_unsupported,
    .ep_pending_purge         = ucs_empty_function_return_unsupported,
    .ep_flush                 = uct_base_ep_flush,
    .ep_fence                 = uct_base_ep_fence,
    .ep_create                = UCS_CLASS_NEW_FUNC_NAME(uct_gaudi_copy_ep_t),
    .ep_destroy               = UCS_CLASS_DELETE_FUNC_NAME(uct_gaudi_copy_ep_t),
};

UCS_CLASS_DEFINE(uct_gaudi_copy_ep_t, uct_base_ep_t)

ucs_status_t uct_gaudi_copy_ep_create_connected(const uct_ep_params_t *params,
                                                uct_ep_h *ep_p)
{
    uct_gaudi_copy_iface_t *iface = ucs_derived_of(params->iface, uct_gaudi_copy_iface_t);
    uct_gaudi_copy_ep_t *ep;

    ep = ucs_malloc(sizeof(*ep), "gaudi_copy_ep");
    if (ep == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    UCS_CLASS_CALL_SUPER_INIT(uct_base_ep_t, &ep->super, &iface->super);

    *ep_p = &ep->super.super;
    return UCS_OK;
}