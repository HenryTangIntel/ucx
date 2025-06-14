#ifndef UCT_GAUDI_EP_H
#define UCT_GAUDI_EP_H

#include <uct/api/uct.h>

/* Gaudi endpoint structure */
typedef struct uct_gaudi_ep {
    uct_ep_t super;
    int dummy;
} uct_gaudi_ep_t;

ucs_status_t uct_gaudi_ep_create(uct_iface_h iface, const uct_ep_params_t *params, uct_ep_h *ep_p);
void uct_gaudi_ep_destroy(uct_ep_h ep);
ucs_status_t uct_gaudi_ep_put_short(uct_ep_h ep, const void *buffer, unsigned length,
                                    uint64_t remote_addr, uct_rkey_t rkey);
ucs_status_t uct_gaudi_ep_flush(uct_ep_h ep, unsigned flags, const uct_completion_t *comp);

/* Additional endpoint operations (stubs) */
ucs_status_t uct_gaudi_ep_get_short(uct_ep_h ep, void *buffer, unsigned length,
                                    uint64_t remote_addr, uct_rkey_t rkey, void *comp);
ssize_t uct_gaudi_ep_put_bcopy(uct_ep_h ep, uct_pack_callback_t pack_cb,
                               void *arg, uint64_t remote_addr, uct_rkey_t rkey);
ssize_t uct_gaudi_ep_get_bcopy(uct_ep_h ep, uct_unpack_callback_t unpack_cb,
                               void *arg, uint64_t remote_addr, uct_rkey_t rkey, void *comp);
ucs_status_t uct_gaudi_ep_am_short(uct_ep_h ep, uint8_t id, uint64_t header,
                                   const void *payload, unsigned length);
ssize_t uct_gaudi_ep_am_bcopy(uct_ep_h ep, uint8_t id, uct_pack_callback_t pack_cb,
                              void *arg, unsigned flags);
ucs_status_t uct_gaudi_ep_am_zcopy(uct_ep_h ep, uint8_t id, const void *header,
                                   unsigned header_length, const uct_iov_t *iov,
                                   size_t iovcnt, unsigned flags, uct_completion_t *comp);
ucs_status_t uct_gaudi_ep_pending_add(uct_ep_h ep, uct_pending_req_t *req, unsigned flags);
void uct_gaudi_ep_pending_purge(uct_ep_h ep, uct_pending_purge_callback_t cb, void *arg);

#endif // UCT_GAUDI_EP_H
