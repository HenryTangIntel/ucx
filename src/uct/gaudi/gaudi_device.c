#include "gaudi_md.h"
#include <ucs/sys/ptr_arith.h>
#include <ucs/arch/cpu.h>
#include <ucs/debug/log.h>
#include <ucs/sys/module.h>
#include <ucs/sys/sys.h>
#include <ucs/config/parser.h>

ucs_config_field_t uct_gaudi_md_config_table[] = {
    {"", "", NULL,
     ucs_offsetof(uct_gaudi_md_config_t, super), UCS_CONFIG_TYPE_TABLE(uct_md_config_table)},
    {"RCACHE", "try", "Enable registration cache",
     ucs_offsetof(uct_gaudi_md_config_t, enable_rcache), UCS_CONFIG_TYPE_TERNARY},
    {NULL}
};

