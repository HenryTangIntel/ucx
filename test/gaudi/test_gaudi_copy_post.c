#include <uct/gaudi/copy/gaudi_copy_ep.h>
#include <uct/gaudi/copy/gaudi_copy_iface.h>
#include <uct/gaudi/copy/gaudi_copy_md.h>
#include <ucs/type/status.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>

int main() {
    // UCX framework objects
    uct_component_h *components = NULL;
    unsigned num_components = 0, i;
    uct_component_h gaudi_comp = NULL;
    uct_md_config_t *md_config = NULL;
    uct_md_h md = NULL;
    ucs_status_t status;
    int found = 0;

    // Query all UCX components
    status = uct_query_components(&components, &num_components);
    assert(status == UCS_OK);

    // Find the Gaudi component by name
    for (i = 0; i < num_components; ++i) {
        uct_component_attr_t attr = {.field_mask = UCT_COMPONENT_ATTR_FIELD_NAME};
        status = uct_component_query(components[i], &attr);
        assert(status == UCS_OK);
        if (strcmp(attr.name, "gaudi_copy") == 0) {
            gaudi_comp = components[i];
            printf("Found component: %s\n", attr.name);
            found = 1;
            break;
        }
        
    }

    uct_gaudi_copy_iface_t iface = {0};
    uct_base_ep_t ep = {0};
    char src[64] = "UCX Gaudi test data";
    char dst[64] = {0};

    if (found) {
        // Read MD config
        status = uct_md_config_read(gaudi_comp, NULL, NULL, &md_config);
        assert(status == UCS_OK);
        // Open the MD (device)
        status = uct_md_open(gaudi_comp, NULL, md_config, &md);
        if (status != UCS_OK) {
            printf("Failed to open Gaudi MD, skipping real device test.\n");
            md = NULL;
        }
        uct_config_release(md_config);
    }

    if (md) {
        // Use real MD
        iface.super.super.md = md;
        printf("Using real Gaudi MD with hlthunk_fd: %d\n", 
               ((uct_gaudi_copy_md_t*)md)->hlthunk_fd);
    } else {
        // Fallback to mock
        static uct_gaudi_copy_md_t mock_md = {0};
        mock_md.hlthunk_fd = -1;
        iface.super.super.md = (uct_md_h)&mock_md;
    }
    ep.super.iface = (uct_iface_t*)&iface;

    // Dummy completion callback
    void completion_cb(void *request, ucs_status_t status) {
        printf("Completion callback called with status: %s (%d)\n", ucs_status_string(status), status);
    }
    // Call the function under test with completion callback
    status = uct_gaudi_copy_post_gaudi_async_copy((uct_ep_h)&ep, dst, src, sizeof(src), completion_cb);
    printf("uct_gaudi_copy_post_gaudi_async_copy returned: %s (%d)\n", ucs_status_string(status), status);

    if (md) {
        // If real device, expect UCS_OK and data copied
        if (status != UCS_OK) {
            printf("ERROR: Async copy failed with status %s (%d)\n", ucs_status_string(status), status);
        } else {
            if (strcmp(src, dst) != 0) {
                printf("ERROR: Data not copied correctly. src='%s', dst='%s'\n", src, dst);
            } else {
                printf("SUCCESS: Data copied correctly.\n");
            }
        }
        uct_md_close(md);
    } else {
        // For mock (-1 fd), expect UCS_ERR_INVALID_PARAM
        if (status != UCS_ERR_INVALID_PARAM) {
            printf("ERROR: Mock async copy did not return UCS_ERR_INVALID_PARAM, got %s (%d)\n", ucs_status_string(status), status);
        }
    }

    if (components) {
        uct_release_component_list(components);
    }
    return 0;
}
