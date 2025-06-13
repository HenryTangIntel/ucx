#include "gaudi_md.h"
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