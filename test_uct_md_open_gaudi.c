#include <stdio.h>
#include <uct/api/uct.h>

int main() {
    uct_md_h md;
    uct_md_config_t *md_config;
    ucs_status_t status;

    // Read the MD config for "gaudi"
    status = uct_md_config_read(NULL, "gaudi", NULL, &md_config);
    if (status != UCS_OK) {
        printf("uct_md_config_read failed: %s\n", ucs_status_string(status));
        return 1;
    }

    // Open the MD for "gaudi"
    status = uct_md_open(NULL, "gaudi", md_config, &md);
    if (status == UCS_OK) {
        printf("uct_md_open succeeded for gaudi!\n");
        uct_md_close(md);
    } else {
        printf("uct_md_open failed for gaudi: %s\n", ucs_status_string(status));
    }

    uct_config_release(md_config);
    return 0;
}