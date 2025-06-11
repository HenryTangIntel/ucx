if HAVE_GAUDI

module_LTLIBRARIES      = libuct_gaudi.la
libuct_gaudi_la_CPPFLAGS = $(BASE_CPPFLAGS) $(GAUDI_CFLAGS)
libuct_gaudi_la_CFLAGS   = $(BASE_CFLAGS)
libuct_gaudi_la_LDFLAGS  = -version-info $(SOVERSION)
libuct_gaudi_la_LIBADD   = $(GAUDI_LIBS) \
                           $(top_builddir)/src/uct/libuct.la \
                           $(top_builddir)/src/ucs/libucs.la
libuct_gaudi_la_SOURCES = \
    gaudi_init.c \
    gaudi_ep.c \
    gaudi_iface.c \
    gaudi_md.c \
    gaudi_device.c

noinst_HEADERS = \
    gaudi_init.h \
    gaudi_ep.h \
    gaudi_iface.h \
    gaudi_md.h

endif
