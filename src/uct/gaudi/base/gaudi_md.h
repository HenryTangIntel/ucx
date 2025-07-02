#ifndef UCT_GAUDI_MD_H_
#define UCT_GAUDI_MD_H_


#include <uct/api/uct.h>
#include <uct/base/uct_md.h>
#include <ucs/type/status.h>

/* Conditional include for Habana Labs driver */
#include <hlthunk.h>
extern enum hlthunk_device_name devices[];

#define UCT_GAUDI_DEV_NAME_MAX_LEN 64
#define UCT_GAUDI_MAX_DEVICES      8

/* Base info structure for Gaudi devices */
typedef struct uct_gaudi_base_info {
    int device_fd[UCT_GAUDI_MAX_DEVICES];
    int module_id[UCT_GAUDI_MAX_DEVICES];
    int device_id[UCT_GAUDI_MAX_DEVICES];
    char device_busid[UCT_GAUDI_MAX_DEVICES][UCT_GAUDI_DEV_NAME_MAX_LEN]; // Device bus ID
    int num_devices;
} uct_gaudi_base_info_t;

extern uct_gaudi_base_info_t uct_gaudi_base_info;


typedef struct uct_gaudi_md {
    uct_md_t super;
#ifdef HAVE_HLTHUNK_H
    struct hlthunk_hw_ip_info hw_info; /* Hardware IP information */
    enum hlthunk_device_name device_type; /* Type of Gaudi device */
#endif
    int hlthunk_fd;   /* Device handle obtained via hlthunk_open() or regular open() */
    int device_index; /* Index in uct_gaudi_base_info.device_fd array */
    struct {
        int dmabuf_supported; /* DMA_BUF support flag */
    } config;
} uct_gaudi_md_t;

/* Gaudi MD configuration */
typedef struct uct_gaudi_md_config {
    uct_md_config_t             super;
    ucs_ternary_auto_value_t    enable_dmabuf;
    ucs_linear_func_t           uc_reg_cost;
} uct_gaudi_md_config_t;

/* Gaudi memory handle */
typedef struct uct_gaudi_mem {
    void        *vaddr;      /* Virtual address */
    size_t      size;        /* Size of allocation */
    uint64_t    handle;      /* Device memory handle */
    uint64_t    dev_addr;    /* Device address */
    int         dmabuf_fd;   /* DMA-BUF file descriptor */
} uct_gaudi_mem_t;

/* Gaudi remote key */
typedef struct uct_gaudi_key {
    uint64_t    vaddr;       /* Virtual address */
    uint64_t    dev_addr;    /* Device address */
    size_t      size;        /* Size */
} uct_gaudi_key_t;


ucs_status_t uct_gaudi_md_open(uct_component_t *component,
                               const char *md_name,
                               const uct_md_config_t *config,
                               uct_md_h *md_p);
void uct_gaudi_md_close(uct_md_h uct_md);

/* Forward declarations */
ucs_status_t uct_gaudi_md_query(uct_md_h md, uct_md_attr_v2_t *md_attr);
ucs_status_t uct_gaudi_query_md_resources(uct_component_h component,
                                              uct_md_resource_desc_t **resources_p,
                                              unsigned *num_resources_p);

/* Base device management functions */
ucs_status_t uct_gaudi_base_init(void);
void uct_gaudi_base_cleanup(void);
int uct_gaudi_base_get_device_fd(int device_index);

#endif
