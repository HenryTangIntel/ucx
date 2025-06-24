UCX_CHECK_GAUDI
AS_IF([test "x$gaudi_happy" = "xyes"], [ucm_modules="${ucm_modules}:gaudi"])
AC_CONFIG_FILES([src/ucm/gaudi/Makefile])
