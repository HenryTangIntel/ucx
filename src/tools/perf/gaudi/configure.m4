#
# Copyright (c) NVIDIA CORPORATION & AFFILIATES, 2024. ALL RIGHTS RESERVED.
#
# See file LICENSE for terms.
#

AC_ARG_WITH([gaudi],
        [AS_HELP_STRING([--with-gaudi=DIR], [Enable Gaudi support])],
        [],
        [with_gaudi=yes])

AS_IF([test "x$with_gaudi" != "xno"],
      [
       # Simple check - look for hlthunk header in local hlthunk directory
       GAUDI_CPPFLAGS="-I${srcdir}/hlthunk/include/uapi"
       GAUDI_LDFLAGS="-L${srcdir}/hlthunk/build/src"
       GAUDI_LIBS="-lhlthunk"
       
       # Check if hlthunk directory exists
       AS_IF([test -d "${srcdir}/hlthunk"],
             [
              ucx_perftest_modules="${ucx_perftest_modules}:gaudi"
              AC_SUBST([GAUDI_CPPFLAGS])
              AC_SUBST([GAUDI_LDFLAGS]) 
              AC_SUBST([GAUDI_LIBS])
              AC_DEFINE([HAVE_GAUDI], [1], [Enable Gaudi support])
              gaudi_enabled=yes
             ],
             [
              AS_IF([test "x$with_gaudi" = "xyes"],
                    [AC_MSG_WARN([Gaudi directory not found, disabling Gaudi support])],
                    [AC_MSG_ERROR([Gaudi directory not found])])
              gaudi_enabled=no
             ])
      ],
      [gaudi_enabled=no])

AM_CONDITIONAL([HAVE_GAUDI], [test "x$gaudi_enabled" = "xyes"])