
#include "gaudi_md.h"
#include "gaudi_iface.h"
#include <ucs/sys/module.h>
#include <ucs/sys/string.h>




ucs_status_t
uct_cuda_base_query_md_resources(uct_component_t *component,
                                 uct_md_resource_desc_t **resources_p,
                                 unsigned *num_resources_p)
{
    /*
    const unsigned sys_device_priority = 10;
    ucs_sys_device_t sys_dev;
    ucs_status_t status;
    char device_name[10];
    int num_hpus;
    
    status = UCT_CUDA_CALL(UCS_LOG_LEVEL_DIAG, cudaGetDeviceCount, &num_hpus);
    if ((status != UCS_OK) || (num_hpus == 0)) {
        return uct_md_query_empty_md_resource(resources_p, num_resources_p);
    }
        */

   

    return uct_md_query_single_md_resource(component, resources_p,
                                           num_resources_p);
}

UCS_MODULE_INIT() {
    UCS_MODULE_FRAMEWORK_DECLARE(uct_gaudi);
    UCS_MODULE_FRAMEWORK_LOAD(uct_gaudi, 0);
    return UCS_OK;
}