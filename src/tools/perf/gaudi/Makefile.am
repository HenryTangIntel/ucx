if HAVE_GAUDI

moduledir     = $(libdir)/ucx
module_LTLIBRARIES               = libucx_perftest_gaudi.la
libucx_perftest_gaudi_la_CPPFLAGS = $(BASE_CPPFLAGS) $(GAUDI_CPPFLAGS)
libucx_perftest_gaudi_la_CFLAGS   = $(BASE_CFLAGS) $(GAUDI_CFLAGS)
libucx_perftest_gaudi_la_LDFLAGS  = $(GAUDI_LDFLAGS) -version-info $(SOVERSION)
libucx_perftest_gaudi_la_LIBADD   = $(GAUDI_LIBS)
libucx_perftest_gaudi_la_SOURCES  = gaudi_alloc.c

endif