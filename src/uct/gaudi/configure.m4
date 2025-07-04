#
# Copyright (c) 2024, Habana Labs Ltd. an Intel Company
#

UCX_CHECK_GAUDI

uct_gaudi_modules=""
AS_IF([test "x$gaudi_happy" = "xyes"], [uct_gaudi_modules="${uct_gaudi_modules}:gaudi"])
AC_DEFINE_UNQUOTED([uct_gaudi_MODULES], ["${uct_gaudi_modules}"], [GAUDI loadable modules])
