#include "gaudi_copy_md.h"
#include <ucs/sys/math.h>
#include <ucs/sys/string.h>
#include <ucs/debug/log.h>
#include <ucs/debug/memtrack_int.h>
#include <ucs/sys/compiler_def.h>
#include <uct/api/uct.h>

/* Fake Gaudi endpoint structure */
typedef struct uct_gaudi_ep {
    uct_ep_t super;
    /* Add any fake fields here if needed */
    int dummy;
} uct_gaudi_ep_t;

/* Create a fake endpoint */
ucs_status_t uct_gaudi_ep_create(uct_iface_h iface, const uct_ep_params_t *params, uct_ep_h *ep_p)
{
    uct_gaudi_ep_t *ep = ucs_malloc(sizeof(uct_gaudi_ep_t), "gaudi_ep");
    if (ep == NULL) {
        return UCS_ERR_NO_MEMORY;
    }
    ep->dummy = 0;
    *ep_p = &ep->super;
    return UCS_OK;
}

/* Destroy a fake endpoint */
void uct_gaudi_ep_destroy(uct_ep_h ep)
{
    uct_gaudi_ep_t *gaudi_ep = ucs_derived_of(ep, uct_gaudi_ep_t);
    ucs_free(gaudi_ep);
}

/* Fake put short operation */
ucs_status_t uct_gaudi_ep_put_short(uct_ep_h ep, const void *buffer, unsigned length,
                                    uint64_t remote_addr, uct_rkey_t rkey)
{
    ucs_trace("Fake Gaudi put_short: len=%u remote_addr=0x%lx", length, remote_addr);
    return UCS_OK;
}

/* Fake flush operation */
ucs_status_t uct_gaudi_ep_flush(uct_ep_h ep, unsigned flags, const uct_completion_t *comp)
{
    ucs_trace("Fake Gaudi flush");
    return UCS_OK;
}

ucs_status_t uct_gaudi_ep_get_short(uct_ep_h ep, void *buffer, unsigned length,
                                    uint64_t remote_addr, uct_rkey_t rkey, void *comp)
{
    ucs_trace("Fake Gaudi get_short: len=%u remote_addr=0x%lx", length, remote_addr);
    return UCS_ERR_UNSUPPORTED;
}

ssize_t uct_gaudi_ep_put_bcopy(uct_ep_h ep, uct_pack_callback_t pack_cb,
                               void *arg, uint64_t remote_addr, uct_rkey_t rkey)
{
    ucs_trace("Fake Gaudi put_bcopy: remote_addr=0x%lx", remote_addr);
    return UCS_ERR_UNSUPPORTED;
}

ssize_t uct_gaudi_ep_get_bcopy(uct_ep_h ep, uct_unpack_callback_t unpack_cb,
                               void *arg, uint64_t remote_addr, uct_rkey_t rkey, void *comp)
{
    ucs_trace("Fake Gaudi get_bcopy: remote_addr=0x%lx", remote_addr);
    return UCS_ERR_UNSUPPORTED;
}

ucs_status_t uct_gaudi_ep_am_short(uct_ep_h ep, uint8_t id, uint64_t header,
                                   const void *payload, unsigned length)
{
    ucs_trace("Fake Gaudi am_short: id=%u len=%u", id, length);
    return UCS_ERR_UNSUPPORTED;
}

ssize_t uct_gaudi_ep_am_bcopy(uct_ep_h ep, uint8_t id, uct_pack_callback_t pack_cb,
                              void *arg, unsigned flags)
{
    ucs_trace("Fake Gaudi am_bcopy: id=%u", id);
    return UCS_ERR_UNSUPPORTED;
}

ucs_status_t uct_gaudi_ep_am_zcopy(uct_ep_h ep, uint8_t id, const void *header,
                                   unsigned header_length, const uct_iov_t *iov,
                                   size_t iovcnt, unsigned flags, uct_completion_t *comp)
{
    ucs_trace("Fake Gaudi am_zcopy: id=%u iovcnt=%zu", id, iovcnt);
    return UCS_ERR_UNSUPPORTED;
}

ucs_status_t uct_gaudi_ep_pending_add(uct_ep_h ep, uct_pending_req_t *req, unsigned flags)
{
    ucs_trace("Fake Gaudi pending_add");
    return UCS_ERR_UNSUPPORTED;
}

void uct_gaudi_ep_pending_purge(uct_ep_h ep, uct_pending_purge_callback_t cb, void *arg)
{
    ucs_trace("Fake Gaudi pending_purge");
}