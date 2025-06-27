/**
* Copyright (c) 2023-2024 Intel Corporation
*
* See file LICENSE for terms.
*/

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif

#include "gaudi_copy_ep.h"
#include "gaudi_copy_iface.h" // For uct_gaudi_copy_iface_t
#include <uct/gaudi/gaudi_md.h>   // For UCT_MD_MEM_TYPE_GAUDI and uct_gaudi_md_t
#include <ucs/debug/log.h>
#include <ucs/debug/memtrack.h>
#include <uct/base/uct_iov.inl> // For uct_iov_get_buffer / uct_iov_length if using iov for pack_cb

// Placeholder hlthunk memcpy functions
// These would call actual hlthunk library functions for memory copy.
// Assuming they are synchronous for now.
static ucs_status_t hlthunk_memcpy_h2d(int fd, void *dst_gaudi, const void *src_host, size_t length) {
    ucs_debug("Placeholder: hlthunk_memcpy_h2d(dst=%p, src=%p, len=%zu)", dst_gaudi, src_host, length);
    // In a real implementation, this would copy and return status.
    // For now, simulate a copy by potentially touching memory if not too large or just return OK.
    if (length > 0 && dst_gaudi && src_host) {
         // To avoid unused variable warnings if not actually copying:
        (void)((char*)dst_gaudi)[0];
        (void)((const char*)src_host)[0];
    }
    return UCS_OK;
}

static ucs_status_t hlthunk_memcpy_d2h(int fd, void *dst_host, const void *src_gaudi, size_t length) {
    ucs_debug("Placeholder: hlthunk_memcpy_d2h(dst=%p, src=%p, len=%zu)", dst_host, src_gaudi, length);
    if (length > 0 && dst_host && src_gaudi) {
        (void)((char*)dst_host)[0];
        (void)((const char*)src_gaudi)[0];
    }
    return UCS_OK;
}

static ucs_status_t hlthunk_memcpy_d2d(int fd, void *dst_gaudi, const void *src_gaudi, size_t length) {
    ucs_debug("Placeholder: hlthunk_memcpy_d2d(dst=%p, src=%p, len=%zu)", dst_gaudi, src_gaudi, length);
    if (length > 0 && dst_gaudi && src_gaudi) {
        (void)((char*)dst_gaudi)[0];
        (void)((const char*)src_gaudi)[0];
    }
    return UCS_OK;
}


UCS_CLASS_INIT_FUNC(uct_gaudi_copy_ep_t, const uct_ep_params_t *params)
{
    uct_gaudi_copy_iface_t *iface = ucs_derived_of(params->iface, uct_gaudi_copy_iface_t);
    UCS_CLASS_CALL_SUPER_INIT(uct_base_ep_t, &iface->super);

    ucs_debug("Gaudi Copy EP created on iface %p", iface);
    return UCS_OK;
}

UCS_CLASS_CLEANUP_FUNC(uct_gaudi_copy_ep_t)
{
    ucs_debug("Gaudi Copy EP destroyed");
}

UCS_CLASS_DEFINE(uct_gaudi_copy_ep_t, uct_base_ep_t);
UCS_CLASS_DEFINE_NEW_FUNC(uct_gaudi_copy_ep_t, uct_ep_t, const uct_ep_params_t *);
UCS_CLASS_DEFINE_DELETE_FUNC(uct_gaudi_copy_ep_t, uct_ep_t);


static ucs_status_t uct_gaudi_copy_perform_copy(uct_gaudi_copy_iface_t *iface,
                                                void *dst, ucs_memory_type_t dst_mem_type,
                                                const void *src, ucs_memory_type_t src_mem_type,
                                                size_t length)
{
    uct_gaudi_md_t *gaudi_base_md = NULL;
    int hlfd = -1;

    // Get the hlthunk file descriptor from the main gaudi MD,
    // which should be referenced by the gaudi_copy_md.
    if (iface->md && iface->md->gaudi_md) {
         gaudi_base_md = ucs_derived_of(iface->md->gaudi_md, uct_gaudi_md_t);
         hlfd = gaudi_base_md->hlthunk_fd;
    }

    if (hlfd < 0) {
        ucs_error("Invalid hlthunk_fd, cannot perform copy for iface %p", iface);
        return UCS_ERR_NO_DEVICE;
    }

    if (dst_mem_type == UCT_MD_MEM_TYPE_GAUDI && src_mem_type == UCS_MEMORY_TYPE_HOST) {
        return hlthunk_memcpy_h2d(hlfd, dst, src, length);
    } else if (dst_mem_type == UCS_MEMORY_TYPE_HOST && src_mem_type == UCT_MD_MEM_TYPE_GAUDI) {
        return hlthunk_memcpy_d2h(hlfd, dst, src, length);
    } else if (dst_mem_type == UCT_MD_MEM_TYPE_GAUDI && src_mem_type == UCT_MD_MEM_TYPE_GAUDI) {
        return hlthunk_memcpy_d2d(hlfd, dst, src, length);
    } else if (dst_mem_type == UCS_MEMORY_TYPE_HOST && src_mem_type == UCS_MEMORY_TYPE_HOST) {
        // Host to Host copy: should ideally be handled by a different transport (e.g., scopy)
        // or direct memcpy if within the same process.
        // For now, let's allow it via memcpy for simplicity, though this isn't typical for a device copy TL.
        ucs_trace("Performing host-to-host memcpy via gaudi_copy (length %zu)", length);
        memcpy(dst, src, length);
        return UCS_OK;
    }

    ucs_error("Unsupported memory type combination for gaudi_copy: src_type=%d, dst_type=%d",
              src_mem_type, dst_mem_type);
    return UCS_ERR_UNSUPPORTED;
}


ucs_status_t uct_gaudi_copy_ep_put_bcopy(uct_ep_h tl_ep, uct_pack_callback_t pack_cb,
                                         void *arg, size_t length,
                                         uct_completion_t *comp)
{
    uct_gaudi_copy_ep_t *ep = ucs_derived_of(tl_ep, uct_gaudi_copy_ep_t);
    uct_gaudi_copy_iface_t *iface = ucs_derived_of(ep->super.super.iface, uct_gaudi_copy_iface_t);
    void *packed_buffer;
    ucs_status_t status;
    ucs_memory_type_t src_mem_type, dst_mem_type; // Need to determine these

    // For put_bcopy, the destination is "remote", which for gaudi_copy means Gaudi device memory.
    // The source is local, prepared by pack_cb. We assume pack_cb prepares it in host memory.
    // However, the 'arg' for pack_cb could itself be device memory. UCT needs a way to know this.
    // The pack_cb is expected to copy data from 'arg' into a contiguous buffer.
    // Let's assume 'pack_cb' produces data in a temporary host buffer if 'arg' is complex.
    // If 'arg' is already a simple pointer to contiguous data, pack_cb might be uct_pack_memcpy_single.

    // We need the destination address for Gaudi. This is implicit in `put_bcopy` for UCT.
    // For a copy transport, 'put' implies copying TO the "remote" side, which is the device.
    // The remote address is part of the EP's identity or connection params, but copy EPs are often simple.
    // Let's assume for now that the EP's remote address (ep->super.remote_address) is the Gaudi buffer.
    // This is a simplification; real transports often get remote address/rkey per operation.
    // The uct_ep_ops_t for put_bcopy does not take a remote_addr. This is a problem for copy transports.
    //
    // Looking at other copy transports (e.g. sysv_copy_ep_put_bcopy):
    // `sysv_copy_ep_put_bcopy` uses `ep->remote_seg->addr` as destination.
    // This `remote_seg` would be established during EP creation or connection.
    // For a simple local copy transport, we might assume the EP is "connected" to a specific device buffer.
    // This is not ideal. A better way for copy transports is often to use ZCOPY semantics
    // where addresses are explicit, or have custom functions.
    //
    // Given the standard put_bcopy signature, we have a problem: where is the destination Gaudi address?
    // Let's assume, for now, that the 'arg' for `pack_cb` is somehow the *destination* Gaudi address,
    // and `pack_cb` is a special one that gives us the *source* host buffer. This is not standard.
    //
    // A more standard interpretation for a *local* copy transport:
    // 'put' means copy from host to device. 'get' means copy from device to host.
    // The 'arg' to pack_cb is the source data.
    // The 'remote_address' associated with the endpoint (if any) is the destination base address on device.
    // This is still tricky with just `length`.
    //
    // Let's re-evaluate. `put_bcopy` is usually:
    //   - pack data from `arg` (source) into a buffer (often temp host buffer).
    //   - send this buffer to the remote side.
    // For `gaudi_copy`, "remote side" is the Gaudi device.
    // The destination address on Gaudi device is missing from the API call.
    // This implies `put_bcopy` might not be the right UCT primitive for direct H2D copy
    // unless the destination Gaudi address is embedded in the endpoint.
    //
    // For now, to make progress, let's assume `arg` for `pack_cb` is the *source host buffer*
    // and `ep->dest_ep->conn_priv` or similar (if we had connection phase) would hold the target Gaudi pointer.
    // Since we don't have that yet, this function is hard to implement correctly with current EP structure.
    //
    // A common pattern for copy-like transports is to use `uct_ep_am_bcopy` or `uct_ep_put_zcopy`
    // where addresses are more explicit.
    //
    // Let's assume `put_bcopy` means: copy `length` bytes from `arg` (source, host)
    // to a pre-defined Gaudi address associated with the endpoint. This is a MAJOR assumption.
    // If `pack_cb` is not `uct_pack_memcpy_single`, it will allocate a temp buffer.
    //
    // For a simple interpretation: `arg` is the source host buffer, `length` is its size.
    // Destination Gaudi address needs to be known.
    // Let's assume `tl_ep->remote_addr` (if it were a uct_base_ep_t field for remote address)
    // is the destination. This field does not exist in uct_base_ep_t.
    //
    // This highlights a structural issue with using generic `put_bcopy` for local device copies
    // without a clear way to specify the device address.
    //
    // Temporary workaround: Assume `arg` is source host buffer. `pack_cb` is `uct_pack_memcpy_single`.
    // Destination Gaudi address is needed.
    // Let's assume `comp` can be used to pass destination address for now (highly non-standard).
    // This is just to get something compilable.
    if (length == 0) {
        return UCS_OK;
    }

    if (comp == NULL || comp->count == 0) { // Using comp->count as a hack for destination address
        ucs_error("put_bcopy: Destination Gaudi address not provided (via comp hack)");
        return UCS_ERR_INVALID_PARAM;
    }
    void* dst_gaudi_addr = (void*)comp->count; // HACK: interpret comp->count as dst addr

    packed_buffer = (void*)arg; // Assuming pack_cb is trivial or arg is already packed buffer
    if (pack_cb != uct_pack_memcpy_single) {
        // If a real pack_cb is used, it would allocate a temp buffer.
        // We'd need to manage that buffer. This is too complex for this placeholder.
        // For now, only allow uct_pack_memcpy_single where arg is the host buffer.
        ucs_error("put_bcopy: pack_cb other than uct_pack_memcpy_single not supported in this placeholder");
        return UCS_ERR_UNSUPPORTED;
    }

    src_mem_type = UCS_MEMORY_TYPE_HOST; // Assuming source is host after pack_cb
    dst_mem_type = UCT_MD_MEM_TYPE_GAUDI;

    status = uct_gaudi_copy_perform_copy(iface, dst_gaudi_addr, dst_mem_type,
                                         packed_buffer, src_mem_type, length);

    if (status == UCS_OK && comp) {
        comp->status = UCS_OK; // Mark completion status
        // If it were async, we wouldn't call uct_invoke_completion here.
        // Since we assume sync, and if comp is to be used for actual notification:
        // uct_invoke_completion(comp, UCS_OK); // This is usually for async.
        // For bcopy, completion is typically handled by returning UCS_OK if sync.
    }
    // For synchronous bcopy, the completion is usually NULL or handled by returning UCS_OK.
    // If comp is non-NULL, it implies an async request was made, which we don't support yet.
    if (comp && status == UCS_INPROGRESS) return UCS_INPROGRESS; // If copy could be async
    if (comp && status == UCS_OK) { /* if sync and comp is for status reporting */ }


    return status; // UCS_OK if successful and synchronous
}

ucs_status_t uct_gaudi_copy_ep_get_bcopy(uct_ep_h tl_ep, void *buffer, size_t length,
                                         uint64_t remote_addr, uct_rkey_t rkey,
                                         uct_completion_t *comp)
{
    uct_gaudi_copy_ep_t *ep = ucs_derived_of(tl_ep, uct_gaudi_copy_ep_t);
    uct_gaudi_copy_iface_t *iface = ucs_derived_of(ep->super.super.iface, uct_gaudi_copy_iface_t);
    ucs_status_t status;
    ucs_memory_type_t src_mem_type, dst_mem_type;

    if (length == 0) {
        return UCS_OK;
    }

    // For get_bcopy, 'buffer' is the destination (typically host memory).
    // 'remote_addr' is the source address on the Gaudi device.
    // 'rkey' is usually for remote memory, might be unused or carry info for local device.

    dst_mem_type = UCS_MEMORY_TYPE_HOST;  // Assuming 'buffer' is host memory
    src_mem_type = UCT_MD_MEM_TYPE_GAUDI; // Assuming 'remote_addr' is Gaudi memory

    status = uct_gaudi_copy_perform_copy(iface, buffer, dst_mem_type,
                                         (void*)remote_addr, src_mem_type, length);

    if (status == UCS_OK && comp) {
        comp->status = UCS_OK;
        // uct_invoke_completion(comp, UCS_OK); // If async completion needed
    }
    if (comp && status == UCS_INPROGRESS) return UCS_INPROGRESS;

    return status;
}
