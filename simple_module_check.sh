#!/bin/bash

UCX_INSTALL_DIR="/workspace/ucx/install"

echo "# Creating Gaudi Module Check"

cat > check_gaudi.c << 'EOL'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Directly check if module file exists
int main(int argc, char **argv) {
    FILE *module_file = fopen("/workspace/ucx/modules/libuct_gaudi.so", "r");
    
    if (module_file != NULL) {
        printf("SUCCESS: Gaudi module libuct_gaudi.so exists\n");
        fclose(module_file);
        
        // Also check if the file is a valid shared object
        FILE *check_result = popen("file /workspace/ucx/modules/libuct_gaudi.so", "r");
        
        if (check_result != NULL) {
            char result[1024];
            if (fgets(result, sizeof(result), check_result)) {
                printf("%s", result);
                if (strstr(result, "shared object") != NULL) {
                    printf("VALID: File is a proper shared object\n");
                } else {
                    printf("INVALID: File is not a proper shared object\n");
                }
            }
            pclose(check_result);
        }
        
        // Check Habana hardware presence
        printf("\nHabana Hardware Status:\n");
        system("lspci -d 1da3: | wc -l");
        
        // Check kernel driver
        printf("\nKernel module status:\n");
        system("lsmod | grep habanalabs");
        
        return 0;
    } else {
        printf("FAILURE: Gaudi module libuct_gaudi.so does not exist\n");
        return 1;
    }
}
EOL

gcc -o check_gaudi check_gaudi.c

echo -e "\n# Running Gaudi Module Check\n"
./check_gaudi
