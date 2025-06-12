cd /workspace/ucx
autoreconf -fiv
./contrib/configure-devel --prefix=$PWD/install-debug \
  CPPFLAGS="-I/usr/include/habanalabs -I/usr/include/libdrm" \
  LDFLAGS="-L/usr/lib/habanalabs" \
  --with-gaudi

make clean
make -j

