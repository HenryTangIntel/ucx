#include "gaudi_iface.h"

#include "gaudi_md.h"

#include "gaudi_ep.h"


#include <ucs/type/class.h>
#include <ucs/debug/log.h>


static void uct_gaudi_iface_close(uct_iface_h tl_iface)
{
    uct_gaudi_iface_t *iface = (uct_gaudi_iface_t*)tl_iface;
    ucs_debug("Gaudi iface %p closed", iface);
    ucs_free(iface);
}


ucs_status_t uct_gaudi_iface_query(uct_iface_h tl_iface, uct_iface_attr_t *iface_attr)
{
    memset(iface_attr, 0, sizeof(*iface_attr));
    iface_attr->iface_addr_len = 0;
    iface_attr->ep_addr_len = 0;
    iface_attr->cap.flags = 0;
    iface_attr->latency = ucs_linear_func_make(1e-6, 0); // use correct type
    iface_attr->bandwidth.dedicated = 1e9;
    iface_attr->bandwidth.shared = 0;
    iface_attr->overhead = 0;
    iface_attr->priority = 0;
    return UCS_OK;
}







static uct_iface_ops_t uct_gaudi_iface_ops = {
    .ep_create        = (uct_ep_create_func_t)uct_gaudi_ep_create,
    .ep_destroy       = uct_gaudi_ep_destroy,
    .ep_put_short     = uct_gaudi_ep_put_short,
    .ep_get_short     = (uct_ep_get_short_func_t)uct_gaudi_ep_get_short,
    .ep_put_bcopy     = uct_gaudi_ep_put_bcopy,
    .ep_get_bcopy     = (uct_ep_get_bcopy_func_t)uct_gaudi_ep_get_bcopy,
    .ep_am_short      = uct_gaudi_ep_am_short,
    .ep_am_bcopy      = uct_gaudi_ep_am_bcopy,
    .ep_am_zcopy      = uct_gaudi_ep_am_zcopy,
    .ep_pending_add   = uct_gaudi_ep_pending_add,
    .ep_pending_purge = uct_gaudi_ep_pending_purge,
    .ep_flush         = (uct_ep_flush_func_t)uct_gaudi_ep_flush,
    .iface_close      = uct_gaudi_iface_close,
    .iface_query      = uct_gaudi_iface_query,
    // Other ops can be added as needed
};


UCS_CLASS_INIT_FUNC(uct_gaudi_iface_t, uct_iface_ops_t *ops, uct_iface_internal_ops_t *internal_ops,
                    uct_md_h md, uct_worker_h worker, const uct_iface_params_t *params,
                    uct_iface_config_t *config)
{
    UCS_CLASS_CALL_SUPER_INIT(uct_base_iface_t, ops, internal_ops, md, worker, params, config);
    return UCS_OK;
}

UCS_CLASS_CLEANUP_FUNC(uct_gaudi_iface_t)
{
    // Nothing to cleanup for now
}

UCS_CLASS_DEFINE(uct_gaudi_iface_t, uct_base_iface_t);

