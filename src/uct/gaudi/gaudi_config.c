#include "gaudi_md.h"
#include <ucs/arch/cpu.h>
#include <ucs/debug/log.h>
#include <ucs/sys/module.h>
#include <ucs/config/parser.h>

ucs_config_field_t uct_gaudi_md_config_table[] = {
    {"", "", NULL,
     ucs_offsetof(uct_gaudi_md_config_t, super), UCS_CONFIG_TYPE_TABLE(uct_md_config_table)},

    {"RCACHE", "try", "Enable registration cache",
     ucs_offsetof(uct_gaudi_md_config_t, enable_rcache), UCS_CONFIG_TYPE_TERNARY},

    {NULL}
};

void uct_gaudi_pg_align_addr(void **addr, size_t *length)
{
    size_t page_size = ucs_get_page_size();
    uintptr_t aligned_addr = ucs_align_down((uintptr_t)*addr, page_size);
    *length += (uintptr_t)*addr - aligned_addr;
    *addr = (void*)aligned_addr;
    *length = ucs_align_up(*length, page_size);
}

ucs_status_t uct_gaudi_rkey_unpack(uct_component_t *component, const void *rkey_buffer,
                                  uct_rkey_t *rkey_p, void **handle_p)
{
    uct_gaudi_key_t *packed = (uct_gaudi_key_t *)ucs_malloc(sizeof(uct_gaudi_key_t), "gaudi_rkey");
    if (packed == NULL) {
        return UCS_ERR_NO_MEMORY;
    }

    memcpy(packed, rkey_buffer, sizeof(uct_gaudi_key_t));
    *handle_p = packed;
    *rkey_p = (uintptr_t)packed;

    return UCS_OK;
}

ucs_status_t uct_gaudi_rkey_release(uct_component_t *component, uct_rkey_t rkey, void *handle)
{
    ucs_free(handle);
    return UCS_OK;
}
