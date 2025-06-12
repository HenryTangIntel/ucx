#
# Copyright (C) Habana Labs, Ltd. 2023. All Rights Reserved.
#

if HAVE_GAUDI

module_LTLIBRARIES      = libuct_gaudi.la
libuct_gaudi_la_CPPFLAGS = $(BASE_CPPFLAGS)
libuct_gaudi_la_CFLAGS   = $(BASE_CFLAGS)
libuct_gaudi_la_LDFLAGS  = $(MODULE_LDFLAGS)
libuct_gaudi_la_LIBADD   = $(HLTHUNK_LIBS)
libuct_gaudi_la_SOURCES  = gaudi_md.c

include $(top_srcdir)/config/module.am

endif
