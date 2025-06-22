#
# Copyright (c) 2023. ALL RIGHTS RESERVED.
# - If this is a new file, add your license text here
# See file LICENSE for terms.
#

if HAVE_UCM_GAUDI
noinst_LTLIBRARIES       = libucm_gaudi.la
libucm_gaudi_la_CPPFLAGS = $(BASE_CPPFLAGS) $(GAUDI_CPPFLAGS)
libucm_gaudi_la_CFLAGS   = $(BASE_CFLAGS)
libucm_gaudi_la_LDFLAGS  = $(GAUDI_LDFLAGS)
libucm_gaudi_la_LIBADD   = $(GAUDI_LIBS)
libucm_gaudi_la_SOURCES  = gaudimem.c gaudimem.h

ucm_module_LTLIBRARIES += libucm_gaudi.la
endif

EXTRA_libucm_gaudi_la_SOURCES = gaudimem.c gaudimem.h

DISTCLEANFILES = Makefile.in
