#ifndef UCT_GAUDI_COPY_IFACE_H_
#define UCT_GAUDI_COPY_IFACE_H_

#include <uct/base/uct_iface.h>
#include <uct/gaudi/copy/gaudi_copy_md.h> // For MD config

#define UCT_GAUDI_COPY_TL_NAME "gaudi_copy"

/**
 * @brief Gaudi Copy interface configuration
 */
typedef struct uct_gaudi_copy_iface_config {
    uct_iface_config_t      super;
    /* Add Gaudi Copy interface specific config options here if any */
    unsigned                max_bcopy; /* Max bcopy size */
} uct_gaudi_copy_iface_config_t;


/**
 * @brief Gaudi Copy interface
 *
 * Represents a communication interface for Gaudi copy operations.
 */
typedef struct uct_gaudi_copy_iface {
    uct_iface_t             super;
    uct_gaudi_copy_md_t     *md;       /* MD this iface belongs to */
    /* Add other iface specific fields */
    struct {
        size_t              max_bcopy;
    } config;
} uct_gaudi_copy_iface_t;


extern uct_tl_component_t uct_gaudi_copy_tl; // Should be uct_gaudi_copy_component

ucs_status_t uct_gaudi_copy_iface_query(uct_iface_h tl_iface, uct_iface_attr_t *iface_attr);
ucs_status_t uct_gaudi_copy_iface_flush(uct_iface_h tl_iface, unsigned flags, uct_completion_t *comp);
ucs_status_t uct_gaudi_copy_iface_fence(uct_iface_h tl_iface, unsigned flags);

ucs_status_t uct_gaudi_copy_iface_get_address(uct_iface_h tl_iface, uct_iface_addr_t *addr);
int uct_gaudi_copy_iface_is_reachable(const uct_iface_h tl_iface,
                                      const uct_device_addr_t *dev_addr,
                                      const uct_iface_addr_t *iface_addr);

#endif /* UCT_GAUDI_COPY_IFACE_H_ */
