#
# Copyright (c) 2024, Habana Labs Ltd. an Intel Company
#

UCX_CHECK_GAUDI

AS_IF([test "x$gaudi_happy" = "xyes"], [uct_modules="${uct_modules}:gaudi"])
uct_gaudi_modules=""
AC_DEFINE_UNQUOTED([uct_gaudi_MODULES], ["${uct_gaudi_modules}"], [GAUDI loadable modules])
