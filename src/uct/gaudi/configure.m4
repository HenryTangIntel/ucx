#
# Copyright (c) NVIDIA CORPORATION & AFFILIATES, 2001-2021. ALL RIGHTS RESERVED.
# See file LICENSE for terms.
#

AC_DEFUN([UCX_UCT_GAUDI_CHECK],[
    AC_ARG_WITH([gaudi],
        AS_HELP_STRING([--with-gaudi=(DIR)],
                       [Enable the use of Gaudi (default is autodetect).]),
        [],
        [with_gaudi=guess])

    gaudi_happy=no
    AS_IF([test "x$with_gaudi" != "xno"],
          [AS_CASE(["x$with_gaudi"],
                   [x|xguess|xyes],
                       [AC_MSG_NOTICE([Gaudi path was not specified. Guessing ...])
                        with_gaudi="/usr"
                        GAUDI_ROOT="$with_gaudi"],
                   [x/*],
                       [AC_MSG_NOTICE([Gaudi path given as $with_gaudi ...])
                        GAUDI_ROOT="$with_gaudi"],
                   [AC_MSG_NOTICE([Gaudi flags given ...])
                    # No specific flags to parse for Gaudi, just path
                    ])

           SAVE_CPPFLAGS="$CPPFLAGS"
           SAVE_LDFLAGS="$LDFLAGS"
           SAVE_LIBS="$LIBS"

           # Add Gaudi include and lib paths
           CPPFLAGS="-I$GAUDI_ROOT/include/habanalabs -I$GAUDI_ROOT/include/drm $CPPFLAGS"
           LDFLAGS="-L$GAUDI_ROOT/lib $LDFLAGS"

           # Check for a Gaudi specific header or library function
           AC_CHECK_HEADERS([habanalabs/habanalabs.h], [gaudi_happy=yes], [gaudi_happy=no])
           AS_IF([test "x$gaudi_happy" = xyes],
                 [AC_CHECK_LIB([habanalabs], [hl_create_device_list], [gaudi_happy=yes], [gaudi_happy=no])])

           AS_IF([test "x$gaudi_happy" = "xyes"],
                 [AC_DEFINE([HAVE_GAUDI], 1, [Enable Gaudi support])
                  AC_SUBST([GAUDI_CPPFLAGS])
                  AC_SUBST([GAUDI_LDFLAGS])
                  AC_SUBST([GAUDI_LIBS])
                  AC_SUBST([GAUDI_ROOT])
                  uct_modules="${uct_modules}:gaudi"
                  uct_gaudi_modules="gaudi"],
                 [AC_MSG_WARN([Gaudi not found])])

           CPPFLAGS="$SAVE_CPPFLAGS"
           LDFLAGS="$SAVE_LDFLAGS"
           LIBS="$SAVE_LIBS"
          ],
          [AC_MSG_WARN([Gaudi was explicitly disabled])]
    )

    AM_CONDITIONAL([HAVE_GAUDI], [test "x$gaudi_happy" != xno])
])