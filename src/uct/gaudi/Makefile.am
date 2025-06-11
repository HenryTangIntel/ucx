if HAVE_GAUDI
   lib_LTLIBRARIES = libuct_gaudi.la
   libuct_gaudi_la_CFLAGS = $(BASE_CFLAGS) $(GAUDI_CFLAGS)
   libuct_gaudi_la_LIBS = $(GAUDI_LIBS)
   libuct_gaudi_la_SOURCES = \
       gaudi_init.c \
       gaudi_ep.c \
       gaudi_iface.c \
       gaudi_md.c \
       gaudi_device.c
endif