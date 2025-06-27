#include "gaudi_iface.h"
#include <string.h>
#include <ucs/sys/math.h>

ucs_status_t uct_gaudi_iface_query(uct_iface_h tl_iface, uct_iface_attr_t *iface_attr)
{
    iface_attr->cap.flags = UCT_IFACE_FLAG_AM_SHORT |
                      UCT_IFACE_FLAG_PUT_SHORT |
                      UCT_IFACE_FLAG_GET_SHORT;

    iface_attr->cap.put.max_short = 256;
    iface_attr->cap.get.max_short = 256;
    iface_attr->cap.am.max_short = 256;
    iface_attr->latency.c = 1e-6;
    iface_attr->latency.m = 0;
    iface_attr->bandwidth.shared = 10000 * UCS_MBYTE;

    return UCS_OK;
}

ucs_status_t uct_gaudi_iface_get_address(uct_iface_h tl_iface, uct_iface_addr_t *iface_addr)
{
    // Fill iface_addr with a dummy value for now
    uint64_t dummy_addr = 0xDEADBEEF;
    memcpy(iface_addr, &dummy_addr, sizeof(dummy_addr));
    return UCS_OK;
}

int uct_gaudi_iface_is_reachable(const uct_iface_h tl_iface,
                                 const uct_device_addr_t *dev_addr,
                                 const uct_iface_addr_t *iface_addr)
{
    // For now, always return reachable
    return 1;
}
