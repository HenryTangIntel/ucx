noinst_LTLIBRARIES = libuct_gaudi.la

libuct_gaudi_la_SOURCES = \
    gaudi_device.c \
    gaudi_config.c \
    gaudi_md.c \
    gaudi_iface.c \
    gaudi_ep.c

AM_CPPFLAGS = -I$(top_srcdir)/src \
              -I/usr/include/habanalabs

libuct_gaudi_la_LDFLAGS = -module -avoid-version -L/usr/lib/habanalabs
libuct_gaudi_la_LIBADD = -lhl-thunk 