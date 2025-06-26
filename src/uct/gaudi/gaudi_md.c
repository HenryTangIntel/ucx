#include <ucs/debug/memtrack.h>
#include <ucs/sys/sys.h>  // Add this for ucs_free and other memory functions
#include "gaudi_md.h"
#include <hlthunk.h>
#include <uct/base/uct_md.h>

enum hlthunk_device_name devices[] = {
        HLTHUNK_DEVICE_GAUDI3,
        HLTHUNK_DEVICE_GAUDI2,
        HLTHUNK_DEVICE_GAUDI,
        HLTHUNK_DEVICE_DONT_CARE
};

/*
static uct_md_ops_t uct_gaudi_md_ops = {
    .close        = uct_gaudi_md_close,
    .query        = uct_gaudi_md_query,
    .mem_alloc    = uct_gaudi_md_mem_alloc,
    .mem_free     = uct_gaudi_md_mem_free,
    .mem_reg      = uct_gaudi_md_mem_reg,
    .mem_dereg    = uct_gaudi_md_mem_dereg,
};
*/
/* Forward declarations */
/*
static ucs_status_t uct_gaudi_md_query(uct_md_h md, uct_md_attr_v2_t *md_attr);
static ucs_status_t uct_gaudi_query_md_resources(uct_component_h component,
                                              uct_md_resource_desc_t **resources_p,
                                              unsigned *num_resources_p);
*/




void uct_gaudi_md_close(uct_md_h  uct_md)
{
    uct_gaudi_md_t *md = ucs_derived_of(uct_md, uct_gaudi_md_t);
    hlthunk_close(md->hlthunk_fd);
    ucs_free(md);

}

/* Basic MD query that sets memory domain properties */
ucs_status_t uct_gaudi_md_query(uct_md_h md, uct_md_attr_v2_t *md_attr)
{
    md_attr->flags = UCT_MD_FLAG_REG | UCT_MD_FLAG_ALLOC;
    md_attr->reg_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST);
    md_attr->alloc_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST);
    md_attr->access_mem_types = UCS_BIT(UCS_MEMORY_TYPE_HOST);
    md_attr->detect_mem_types = 0;
    md_attr->max_alloc = UINT64_MAX;
    md_attr->max_reg = UINT64_MAX;
    md_attr->rkey_packed_size = 0;
    md_attr->reg_cost.c = 0;
    md_attr->reg_cost.m = 0;
    return UCS_OK;
}





static uct_md_ops_t uct_gaudi_md_ops = {
    .close        = uct_gaudi_md_close,
    .query        = uct_gaudi_md_query,
    .mem_alloc    = NULL, //uct_gaudi_md_mem_alloc,
    .mem_free     = NULL, //uct_gaudi_md_mem_free,
    .mem_reg      = NULL, //uct_gaudi_md_mem_reg,
    .mem_dereg    = NULL, //uct_gaudi_md_mem_dereg,
};




/* Query available Gaudi MD resources */
ucs_status_t uct_gaudi_query_md_resources(uct_component_h component,
                                               uct_md_resource_desc_t **resources_p,
                                               unsigned *num_resources_p)
{
    uct_md_resource_desc_t *resources;
    int fd;
    for (int i = 0; i < 4; i++) {
        fd = hlthunk_open(devices[i], NULL);
        if (fd >= 0) break;
    }

    hlthunk_close(fd);
    
    resources = ucs_malloc(sizeof(*resources), "gaudi md resource");
    if (resources == NULL) {
        *resources_p = NULL;
        *num_resources_p = 0;
        return UCS_ERR_NO_MEMORY;
    }
    
    strncpy(resources->md_name, "gaudi", UCT_MD_NAME_MAX);
    *resources_p = resources;
    *num_resources_p = 1;
    
    return UCS_OK;
}


/* Define the component first so it can be referenced */
static uct_component_t uct_gaudi_component = {
    .query_md_resources = uct_gaudi_query_md_resources,
    .md_open            = uct_gaudi_md_open,
    .cm_open            = NULL,
    .rkey_unpack        = NULL,
    .rkey_ptr           = NULL,
    .rkey_release       = NULL,
    .name               = "gaudi",
    .md_config          = {
        .name           = "Gaudi memory domain",
        .prefix         = "GAUDI_",
        .table          = NULL,
        .size           = sizeof(uct_md_config_t),
    },
};

ucs_status_t uct_gaudi_md_open(uct_component_t *component,
                               const char *md_name,
                               const uct_md_config_t *config,
                               uct_md_h *md_p)
{
    uct_gaudi_md_t *md = ucs_malloc(sizeof(*md), "uct_gaudi_md");
    if (!md) return UCS_ERR_NO_MEMORY;


    
    for (int i = 0; i < 4; i++) {
        md->hlthunk_fd = hlthunk_open(devices[i], NULL);
        if (md->hlthunk_fd >= 0) break;
    }

    md->super.ops = &uct_gaudi_md_ops;
    md->super.component = &uct_gaudi_component;
    *md_p = &md->super;
    return UCS_OK;
}




 UCT_COMPONENT_REGISTER(&uct_gaudi_component)