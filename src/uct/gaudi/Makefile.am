#
# Copyright (c) 2023. ALL RIGHTS RESERVED.
# - If this is a new file, add your license text here
# See file LICENSE for terms.
#

if HAVE_GAUDI
noinst_LTLIBRARIES       = libuct_gaudi.la
libuct_gaudi_la_CPPFLAGS = $(BASE_CPPFLAGS)
libuct_gaudi_la_CFLAGS   = $(BASE_CFLAGS)
libuct_gaudi_la_SOURCES  = # Placeholder for source files
libuct_gaudi_la_LIBADD   = $(UCT_LIBS)

uct_gaudi_md_sources = \
    gaudi_md.c \
    gaudi_md.h

uct_gaudi_iface_sources = \
    gaudi_iface.c \
    gaudi_iface.h

libuct_gaudi_la_SOURCES += $(uct_gaudi_md_sources)
libuct_gaudi_la_SOURCES += $(uct_gaudi_iface_sources)

# Example for a sub-component like 'copy' in 'ze'
# uct_gaudi_copy_sources = \
#    copy/gaudi_copy_iface.c \
#    copy/gaudi_copy_iface.h \
#    copy/gaudi_copy_ep.c \
#    copy/gaudi_copy_ep.h \
#    copy/gaudi_copy_md.c \
#    copy/gaudi_copy_md.h
#
# libuct_gaudi_la_SOURCES += $(uct_gaudi_copy_sources)

endif

# Example for pkg-config file if Gaudi has one
#if HAVE_GAUDI_MODULE # This should align with AM_CONDITIONAL in configure.m4
#pkgconfigdir             = $(libdir)/pkgconfig
#pkgconfig_DATA           = ucx-gaudi.pc
#EXTRA_DIST               = ucx-gaudi.pc.in
#CLEANFILES               = ucx-gaudi.pc
#endif

# List of all source files the top-level Makefile.am should know about
EXTRA_libuct_gaudi_la_SOURCES = \
    $(uct_gaudi_md_sources) \
    $(uct_gaudi_iface_sources) # Add other source groups here e.g. $(uct_gaudi_copy_sources)

DISTCLEANFILES = Makefile.in
