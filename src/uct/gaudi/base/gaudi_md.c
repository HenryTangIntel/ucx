#include "gaudi_md.h"
#include "gaudi_iface.h"
#include <ucs/sys/module.h>
#include <ucs/sys/string.h>

ucs_status_t
uct_gaudi_query_md_resources(uct_component_t *component,
                                 uct_md_resource_desc_t **resources_p,
                                 unsigned *num_resources_p)
{
    return uct_md_query_single_md_resource(component, resources_p,
                                           num_resources_p);
}
