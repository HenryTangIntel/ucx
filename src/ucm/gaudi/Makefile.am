#
# Copyright (c) 2023. ALL RIGHTS RESERVED.
# - If this is a new file, add your license text here
# See file LICENSE for terms.
#

if HAVE_GAUDI

# moduledir = $(libdir)/ucx/ucm
module_LTLIBRARIES = libucm_gaudi.la

libucm_gaudi_la_CPPFLAGS = -D_GNU_SOURCE $(BASE_CPPFLAGS) $(GAUDI_CPPFLAGS)
libucm_gaudi_la_CFLAGS   = $(BASE_CFLAGS)
libucm_gaudi_la_LIBADD   =
libucm_gaudi_la_LDFLAGS  = $(UCM_MODULE_LDFLAGS) $(GAUDI_LDFLAGS) $(GAUDI_LIBS) -version-info $(SOVERSION)

libucm_gaudi_la_SOURCES = gaudimem.c

noinst_HEADERS = gaudimem.h

endif


