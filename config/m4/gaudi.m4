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
                    GAUDI_CPPFLAGS="-I$with_gaudi/../include -I$with_gaudi/../include/habanalabs"],
                   [GAUDI_LDFLAGS="-L/usr/lib/habanalabs"
                    GAUDI_CPPFLAGS="-I/usr/include/habanalabs"])

             # Save original flags and append Gaudi-specific ones
             old_LDFLAGS="$LDFLAGS"
             old_CPPFLAGS="$CPPFLAGS"
             LDFLAGS="$LDFLAGS $GAUDI_LDFLAGS"
             CPPFLAGS="$CPPFLAGS $GAUDI_CPPFLAGS"

             # Check for Gaudi library (libhl-thunk)
             AC_CHECK_LIB([hl-thunk], [hlthunk_open],
                          [
                           # Library found, now check for header
                           AC_CHECK_HEADER([hlthunk.h],
                                          [AC_DEFINE([HAVE_GAUDI], [1], [Enable Gaudi support])
                                           uct_gaudi_modules=":gaudi"
                                           gaudi_enable=yes
                                           AC_SUBST([GAUDI_LIBS], [-lhl-thunk])
                                           AC_SUBST([GAUDI_CFLAGS], [$GAUDI_CPPFLAGS])
                                           AC_SUBST([GAUDI_LDFLAGS], [$GAUDI_LDFLAGS])],
                                          [AS_IF([test "x$with_gaudi" != xno && test "x$with_gaudi" != xyes],
                                                 [AC_MSG_ERROR([Gaudi support requested but hlthunk.h not found])],
                                                 [gaudi_enable=no])])
                          ],
                          [AS_IF([test "x$with_gaudi" != xno && test "x$with_gaudi" != xyes],
                                 [AC_MSG_ERROR([Gaudi support requested but libhl-thunk not found in $with_gaudi])],
                                 [gaudi_enable=no])])

             # Restore original flags
             LDFLAGS="$old_LDFLAGS"
             CPPFLAGS="$old_CPPFLAGS"
            ])

      AM_CONDITIONAL([HAVE_GAUDI], [test "x$gaudi_enable" = "xyes"])
      AC_SUBST([uct_gaudi_modules])
  ])