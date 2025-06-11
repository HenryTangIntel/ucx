#include "gaudi_iface.h"
#include "gaudi_ep.h"
#include <ucs/type/class.h>

static ucs_status_t uct_gaudi_iface_query(uct_iface_h tl_iface,
                                          uct_iface_attr_t *iface_attr)
{
    iface_attr->cap.flags = UCT_IFACE_FLAG_AM_SHORT | UCT_IFACE_FLAG_PUT_SHORT;
    iface_attr->iface_addr_len = 0;
    iface_attr->ep_addr_len = 0;
    iface_attr->cap.am.max_short = 64;
    iface_attr->cap.put.max_short = 64;
    return UCS_OK;
}

UCS_CLASS_INIT_FUNC(uct_gaudi_iface_t, uct_md_h md, uct_worker_h worker,
                    const uct_iface_params_t *params, const uct_iface_config_t *tl_config)
{
    UCS_CLASS_CALL_SUPER_INIT(uct_base_iface_t, &uct_gaudi_iface_ops, md, worker, params, tl_config);
    // Create CQ; for real use, set size, handler, etc.
    self->cq = ibv_create_cq(((uct_gaudi_md_t*)md)->ctx, 128, NULL, NULL, 0);
    if (!self->cq)
        return UCS_ERR_IO_ERROR;
    return UCS_OK;
}

UCS_CLASS_CLEANUP_FUNC(uct_gaudi_iface_t)
{
    if (self->cq)
        ibv_destroy_cq(self->cq);
}

UCT_TL_DEFINE(&uct_gaudi_md_component, gaudi, uct_gaudi_query_devices,
    uct_gaudi_iface_t, "GAUDI_", NULL, NULL);