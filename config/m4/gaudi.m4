#
  # Checking for Gaudi support
  #
  AC_DEFUN([CHECK_GAUDI], [
      AC_ARG_WITH([gaudi],
                  AS_HELP_STRING([--with-gaudi=PATH],
                                 [Enable Gaudi support, optionally specifying the path to Gaudi libraries]),
                  [],
                  [with_gaudi=yes])

      uct_gaudi_modules=""
      gaudi_enable=no

      AS_IF([test "x$with_gaudi" != xno],
            [
             # Allow user to specify Gaudi path
             AS_IF([test "x$with_gaudi" != xyes],
                   [GAUDI_LDFLAGS="-L$with_gaudi"
                    GAUDI_CPPFLAGS="-I$with_gaudi/../include"],
                   [GAUDI_LDFLAGS=""
                    GAUDI_CPPFLAGS=""])

             # Save original flags and append Gaudi-specific ones
             old_LDFLAGS="$LDFLAGS"
             old_CPPFLAGS="$CPPFLAGS"
             LDFLAGS="$LDFLAGS $GAUDI_LDFLAGS"
             CPPFLAGS="$CPPFLAGS $GAUDI_CPPFLAGS"

             # Check for Gaudi library (libhl-thunk)
             AC_CHECK_LIB([hl-thunk], [hlthunk_open],
                          [AC_DEFINE([HAVE_GAUDI], [1], [Enable Gaudi support])
                           uct_gaudi_modules=":gaudi"
                           gaudi_enable=yes
                           AC_SUBST([GAUDI_LIBS], [-lhl-thunk])
                           AC_SUBST([GAUDI_CFLAGS], [$GAUDI_CPPFLAGS])],
                          [AS_IF([test "x$with_gaudi" != xno && test "x$with_gaudi" != xyes],
                                 [AC_MSG_ERROR([Gaudi support requested but libhl-thunk not found in $with_gaudi])],
                                 [gaudi_enable=no])])

             # Check for Gaudi headers
             AC_CHECK_HEADER([hl-thunk.h],
                             [],
                             [AS_IF([test "x$with_gaudi" != xno && test "x$with_gaudi" != xyes],
                                    [AC_MSG_ERROR([Gaudi support requested but hl-thunk.h not found])],
                                    [gaudi_enable=no])])

             # Restore original flags
             LDFLAGS="$old_LDFLAGS"
             CPPFLAGS="$old_CPPFLAGS"
            ])

      AM_CONDITIONAL([HAVE_GAUDI], [test "x$gaudi_enable" = "xyes"])
      AC_SUBST([uct_gaudi_modules])
  ])