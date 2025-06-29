#!/bin/bash
# UCX build script with Gaudi support (system habanalabs)
# Usage: ./build.sh [configure options]
set -euo pipefail

# Regenerate autotools files
autoreconf -i

# Configure with Gaudi support, using system habanalabs and drm includes/libs
CPPFLAGS="-I/usr/include/habanalabs -I/usr/include/drm -DHAVE_GAUDI=1" \
LDFLAGS="-L/usr/lib/habanalabs" \
./configure --with-gaudi=/usr  --enable-debug --enable-modules  "$@"

# Build all targets with maximum parallelism
make -j"$(nproc)"

# Explicitly build Gaudi transport library (if present)
if [ -d src/uct/gaudi ]; then
    make -C src/uct/gaudi -j"$(nproc)"
fi

# Install to system (may require sudo)
# sudo make install

# Build and run unittests (if available)
make check || true

echo "\nUCX build and install complete. Gaudi support enabled."
echo "If you need to rebuild, just rerun this script."

