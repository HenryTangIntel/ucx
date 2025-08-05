#
# Copyright (c) NVIDIA CORPORATION & AFFILIATES, 2024. ALL RIGHTS RESERVED.
#
# See file LICENSE for terms.
#

if HAVE_GAUDI

module_LTLIBRARIES = libperf_gaudi.la

libperf_gaudi_la_CPPFLAGS = \
    $(BASE_CPPFLAGS) \
    $(GAUDI_CPPFLAGS) \
    -I$(top_srcdir)/src \
    -I$(top_builddir)/src

libperf_gaudi_la_LDFLAGS = \
    $(GAUDI_LDFLAGS) \
    -version-info $(SOVERSION)

libperf_gaudi_la_LIBADD = \
    $(GAUDI_LIBS)

libperf_gaudi_la_SOURCES = \
    gaudi_alloc.c

endif