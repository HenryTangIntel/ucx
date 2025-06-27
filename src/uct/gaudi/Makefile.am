#
# Copyright (c) NVIDIA CORPORATION & AFFILIATES, 2001-2018. ALL RIGHTS RESERVED.
# See file LICENSE for terms.
#

if HAVE_GAUDI

module_LTLIBRARIES      = libuct_gaudi.la
libuct_gaudi_la_CPPFLAGS = $(BASE_CPPFLAGS) $(ROCM_CPPFLAGS)
libuct_gaudi_la_CFLAGS   = $(BASE_CFLAGS)
libuct_gaudi_la_LIBADD   = $(top_builddir)/src/ucs/libucs.la \
                          $(top_builddir)/src/uct/libuct.la
libuct_gaudi_la_LDFLAGS  = $(GAUDI_LDFLAGS) $(GAUDI_LIBS) -version-info $(SOVERSION) \
                          $(patsubst %, -Xlinker %, -L$(GAUDI_ROOT)/lib -rpath $(GAUDI_ROOT)/hip/lib -rpath $(GAUDI_ROOT)/lib) \
                          $(patsubst %, -Xlinker %, --enable-new-dtags) \
                          $(patsubst %, -Xlinker %, -rpath $(GAUDI_ROOT)/lib64) \
                          -L/usr/lib/habanalabs -lhl-thunk


noinst_HEADERS = gaudi_md.h gaudi_iface.h 

libuct_gaudi_la_SOURCES = gaudi_md.c gaudi_iface.c

PKG_CONFIG_NAME=gaudi

include $(top_srcdir)/config/module.am
# TODO: enable pkg-config processing when module static build is enabled
# include $(top_srcdir)/config/module-pkg-config.am

endif
