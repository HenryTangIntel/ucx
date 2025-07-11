if HAVE_GAUDI

module_LTLIBRARIES      = libuct_gaudi.la
libuct_gaudi_la_CPPFLAGS = $(BASE_CPPFLAGS) $(GAUDI_CPPFLAGS)
libuct_gaudi_la_CFLAGS   = $(BASE_CFLAGS) $(GAUDI_CFLAGS)
libuct_gaudi_la_LIBADD   = $(GAUDI_LIBS) $(top_builddir)/src/ucs/libucs.la
libuct_gaudi_la_LDFLAGS  = $(UCT_MODULE_LDFLAGS) $(GAUDI_LDFLAGS)

noinst_HEADERS = \
	base/gaudi_md.h \
	gaudi_copy/gaudi_copy_md.h \
	gaudi_copy/gaudi_copy_iface.h \
	gaudi_copy/gaudi_copy_ep.h \
	gaudi_ipc/gaudi_ipc_md.h \
	gaudi_ipc/gaudi_ipc_iface.h \
	gaudi_dma/gaudi_dma_iface.h

libuct_gaudi_la_SOURCES = \
	base/gaudi_md.c \
	gaudi_copy/gaudi_copy_md.c \
	gaudi_copy/gaudi_copy_iface.c \
	gaudi_copy/gaudi_copy_ep.c \
	gaudi_ipc/gaudi_ipc_md.c \
	gaudi_ipc/gaudi_ipc_iface.c \
	gaudi_dma/gaudi_dma_iface.c

include $(top_srcdir)/config/module.am

endif