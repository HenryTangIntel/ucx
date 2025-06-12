#include "gaudi_md.h"
#include <uct/base/uct_md_device.h>
#include <uct/base/uct_md_mem.h>
#include <ucs/sys/string.h>

ucs_status_t uct_gaudi_query_md_resources(uct_md_resource_desc_t **resources_p,
                                          unsigned *num_resources_p)
{
    return uct_md_query_single("gaudi", resources_p, num_resources_p);
}

static ucs_status_t uct_gaudi_md_open(const char *md_name, const uct_md_config_t *config,
                                      uct_md_h *md_p)
{
    *md_p = NULL; // placeholder
    return UCS_OK;
}

UCT_MD_COMPONENT_DEFINE(uct_gaudi_md_component,
                        "gaudi",
                        uct_gaudi_query_md_resources,
                        uct_gaudi_md_open,
                        NULL,
                        0)

UCT_MD_REGISTER_TL(&uct_gaudi_md_component, gaudi)
