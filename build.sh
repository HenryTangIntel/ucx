# Regenerate the build system
./autogen.sh

# Configure with Gaudi support
./contrib/configure-devel --prefix=$PWD/install-debug \
    CPPFLAGS="-I/usr/include/habanalabs -I/usr/include/libdrm" \
    LDFLAGS="-L/usr/lib/habanalabs" \
    --with-gaudi

# Build
make -j$(nproc)
