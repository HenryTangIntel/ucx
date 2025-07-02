AC_DEFUN([UCX_CHECK_GAUDI],

AS_IF([test "x$gaudi_checked" != "xyes"], [

AC_ARG_WITH([gaudi],
            [AS_HELP_STRING([--with-gaudi=PATH], 
                  [Enable to use of Habana Gaudi.])],
            [], 
            [with_gaudi=guess])

gaudi_happy=no

AS_IF([test "x$with_gaudi" = "xno"],
    [ gaudi_happy = no
      have_gaudi_static = no
    ], 
    [
    CPPFLAGS="$CPPFLAGS -I$with_gaudi/include/habanalabs"
    LDFLAGS="$LDFLAGS -L$with_gaudi/lib/habanalabs"
    AC_CHECK_HEADER([hlthunk.h], [
        AC_DEFINE([HAVE_HLTHUNK_H], [1], [Define if hlthunk.h header is available])
    ],
        [AC_MSG_ERROR([Gaudi headers not found])])
    AC_CHECK_LIB([hl-thunk], [hlthunk_open], [
        gaudi_happy=yes
        AC_DEFINE([HAVE_GAUDI], [1], [Define if Gaudi is available])
    ], [
        AC_MSG_ERROR([Gaudi libraries not found])
    ])
])
gaudi_checked=yes
])

AM_CONDITIONAL([HAVE_GAUDI], [test "x$gaudi_happy" = "xyes"])
)
