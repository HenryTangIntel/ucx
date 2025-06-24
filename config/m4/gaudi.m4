#
# Copyright (c) Habana Labs Ltd. 2024. ALL RIGHTS RESERVED.
# See file LICENSE for terms.
#

AC_DEFUN([UCX_CHECK_GAUDI],[

AS_IF([test "x$gaudi_checked" != "xyes"],
   [
    AC_ARG_WITH([gaudi],
                [AS_HELP_STRING([--with-gaudi=(DIR)], [Enable the use of Gaudi (default is guess).])],
                [], [with_gaudi=guess])

    AS_IF([test "x$with_gaudi" = "xno"],
        [
         gaudi_happy="no"
        ],
        [
         save_CPPFLAGS="$CPPFLAGS"
         save_LDFLAGS="$LDFLAGS"
         save_LIBS="$LIBS"

         GAUDI_CPPFLAGS=""
         GAUDI_LDFLAGS=""
         GAUDI_LIBS=""

         AS_IF([test ! -z "$with_gaudi" -a "x$with_gaudi" != "xyes" -a "x$with_gaudi" != "xguess"],
               [ucx_check_gaudi_dir="$with_gaudi"
                AS_IF([test -d "$with_gaudi/lib64"], [libsuff="64"], [libsuff=""])
                ucx_check_gaudi_libdir="$with_gaudi/lib$libsuff"
                GAUDI_CPPFLAGS="-I$with_gaudi/include"
                GAUDI_LDFLAGS="-L$ucx_check_gaudi_libdir"])

         AS_IF([test ! -z "$with_gaudi_libdir" -a "x$with_gaudi_libdir" != "xyes"],
               [ucx_check_gaudi_libdir="$with_gaudi_libdir"
                GAUDI_LDFLAGS="-L$ucx_check_gaudi_libdir"])

         CPPFLAGS="$CPPFLAGS $GAUDI_CPPFLAGS"
         LDFLAGS="$LDFLAGS $GAUDI_LDFLAGS"

         # Check Gaudi header files
         AC_CHECK_HEADERS([habanalabs/hlthunk.h drm/drm.h],
                          [gaudi_happy="yes"], [gaudi_happy="no"])

         # Check Gaudi libraries (example: hlthunk)
         AS_IF([test "x$gaudi_happy" = "xyes"],
               [AC_CHECK_LIB([hlthunk], [hlthunk_get_device_pci_ids],
                             [GAUDI_LIBS="$GAUDI_LIBS -lhlthunk"], [gaudi_happy="no"])])

         CPPFLAGS="$save_CPPFLAGS"
         LDFLAGS="$save_LDFLAGS"
         LIBS="$save_LIBS"

         AS_IF([test "x$gaudi_happy" = "xyes"],
               [AC_SUBST([GAUDI_CPPFLAGS], ["$GAUDI_CPPFLAGS"])
                AC_SUBST([GAUDI_LDFLAGS], ["$GAUDI_LDFLAGS"])
                AC_SUBST([GAUDI_LIBS], ["$GAUDI_LIBS"])
                AC_DEFINE([HAVE_GAUDI], 1, [Enable Gaudi support])],
               [AS_IF([test "x$with_gaudi" != "xguess"],
                      [AC_MSG_ERROR([Gaudi support is requested but habanalabs packages cannot be found])],
                      [AC_MSG_WARN([Gaudi not found])])])

        ]) # "x$with_gaudi" = "xno"

        gaudi_checked=yes
        AM_CONDITIONAL([HAVE_UCM_GAUDI], [test "x$gaudi_happy" = "xyes"])

   ]) # "x$gaudi_checked" != "xyes"

]) # UCX_CHECK_GAUDI
