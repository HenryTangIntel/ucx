#
# Copyright (c) 2024, Habana Labs. All rights reserved.
# See file LICENSE for terms.
#

UCX_CHECK_GAUDI

AC_CONFIG_FILES([src/uct/gaudi/Makefile])

AS_IF([test "x$gaudi_happy" = xyes],
      [uct_modules="${uct_modules}:gaudi"])