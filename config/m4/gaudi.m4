#
# Copyright (c) 2024, Habana Labs. All rights reserved.
# See file LICENSE for terms.
#

AC_DEFUN([UCX_CHECK_GAUDI],[

AS_IF([test "x$gaudi_checked" != "xyes"],[

    AC_ARG_WITH([gaudi],
                [AS_HELP_STRING([--with-gaudi=(DIR)], [Enable the use of Gaudi (default is guess).])],
                [], [with_gaudi=guess])

    save_LDFLAGS="$LDFLAGS"
    save_CPPFLAGS="$CPPFLAGS"
    save_CFLAGS="$CFLAGS"
    save_LIBS="$LIBS"

    AS_IF([test "x$with_gaudi" = "xno"],
        [gaudi_happy=no],
        [
            AS_IF([test ! -z "$with_gaudi" -a "x$with_gaudi" != "xyes" -a "x$with_gaudi" != "xguess"],
                [
                    ucx_check_gaudi_dir="$with_gaudi"
                    ucx_check_gaudi_libdir="$with_gaudi/lib"
                    # Try different possible header locations
                    AS_IF([test -d "$with_gaudi/../include/uapi"], [
                        GAUDI_CPPFLAGS="-I$with_gaudi/../include/uapi"
                    ], [test -d "$with_gaudi/include"], [
                        GAUDI_CPPFLAGS="-I$with_gaudi/include"
                    ], [
                        GAUDI_CPPFLAGS="-I$with_gaudi"
                    ])
                    GAUDI_LDFLAGS="-L$ucx_check_gaudi_libdir"
                ])

            CPPFLAGS="$CPPFLAGS $GAUDI_CPPFLAGS"
            LDFLAGS="$LDFLAGS $GAUDI_LDFLAGS"
            LIBS="$LIBS -lhlthunk"

            AC_CHECK_HEADERS([hlthunk.h],
                [gaudi_happy=yes],
                [gaudi_happy=no])

            AS_IF([test "x$gaudi_happy" = "xyes"],
                [
                    AC_CHECK_LIB([hlthunk], [hlthunk_open],
                        [
                            gaudi_happy=yes
                            GAUDI_LIBS="-lhlthunk"
                        ],
                        [gaudi_happy=no])
                ])

            AS_IF([test "x$gaudi_happy" = "xyes"],
                [
                    AC_MSG_CHECKING([for Gaudi device constants])
                    AC_COMPILE_IFELSE([AC_LANG_PROGRAM([
                        [#include <hlthunk.h>]],
                        [[
                        enum hlthunk_device_name device = HLTHUNK_DEVICE_GAUDI;
                        return 0;
                        ]])],
                        [gaudi_happy=yes],
                        [gaudi_happy=no])
                    AC_MSG_RESULT([$gaudi_happy])
                ])
        ])

    AS_IF([test "x$gaudi_happy" = "xyes"],
        [
            AC_SUBST([GAUDI_CFLAGS], ["$GAUDI_CFLAGS"])
            AC_SUBST([GAUDI_CPPFLAGS], ["$GAUDI_CPPFLAGS"])
            AC_SUBST([GAUDI_LDFLAGS], ["$GAUDI_LDFLAGS"])
            AC_SUBST([GAUDI_LIBS], ["$GAUDI_LIBS"])
            AC_DEFINE([HAVE_GAUDI], [1], [Enable Gaudi support])
        ],
        [
            AS_IF([test "x$with_gaudi" != "xguess"],
                [AC_MSG_ERROR([Gaudi support is requested but Gaudi packages cannot be found])],
                [AC_MSG_WARN([Gaudi not found])]
            )
        ])

    LDFLAGS="$save_LDFLAGS"
    CPPFLAGS="$save_CPPFLAGS"
    CFLAGS="$save_CFLAGS"
    LIBS="$save_LIBS"

    gaudi_checked=yes
])

AM_CONDITIONAL([HAVE_GAUDI], [test "x$gaudi_happy" = xyes])

])