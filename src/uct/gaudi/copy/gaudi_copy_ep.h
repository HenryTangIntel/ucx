#ifndef UCT_GAUDI_COPY_EP_H_
#define UCT_GAUDI_COPY_EP_H_

#include <uct/base/uct_ep.h>
#include <uct/api/uct.h> // For uct_completion_t, uct_pack_callback_t etc.
#include <ucs/datastruct/mpool.h> // For ucs_mpool_t if using pending reqs

/**
 * @brief Gaudi Copy endpoint
 */
typedef struct uct_gaudi_copy_ep {
    uct_base_ep_t super;
    /* Add Gaudi Copy EP specific fields here if any */
    /* For a simple copy transport, EP might not need many fields */
    /* If we were to support asynchronous copies, we might have a pending queue here */
} uct_gaudi_copy_ep_t;

UCS_CLASS_DECLARE_NEW_FUNC(uct_gaudi_copy_ep_t, uct_ep_t, const uct_ep_params_t *);
UCS_CLASS_DECLARE_DELETE_FUNC(uct_gaudi_copy_ep_t, uct_ep_t);

ucs_status_t uct_gaudi_copy_ep_put_bcopy(uct_ep_h tl_ep, uct_pack_callback_t pack_cb,
                                         void *arg, size_t length,
                                         uct_completion_t *comp);

ucs_status_t uct_gaudi_copy_ep_get_bcopy(uct_ep_h tl_ep, void *buffer, size_t length,
                                         uint64_t remote_addr, uct_rkey_t rkey,
                                         uct_completion_t *comp);

/* If Active Messages are to be supported via bcopy */
/*
ucs_status_t uct_gaudi_copy_ep_am_bcopy(uct_ep_h tl_ep, uint8_t id,
                                        uct_pack_callback_t pack_cb,
                                        void *arg, size_t length,
                                        unsigned flags, uct_completion_t *comp);
*/

#endif /* UCT_GAUDI_COPY_EP_H_ */
