#include "gaudi_ep.h"
#include <ucs/type/class.h>

UCS_CLASS_INIT_FUNC(uct_gaudi_ep_t, const uct_ep_params_t *params)
{
    UCS_CLASS_CALL_SUPER_INIT(uct_base_ep_t, params);
    // Create QP here with ibv_create_qp using iface's PD, CQ
    self->qp = NULL; // To be implemented
    return UCS_OK;
}

UCS_CLASS_CLEANUP_FUNC(uct_gaudi_ep_t)
{
    if (self->qp)
        ibv_destroy_qp(self->qp);
}

UCS_CLASS_DEFINE(uct_gaudi_ep_t, uct_base_ep_t);