AC_ARG_WITH([gaudi],
    [AS_HELP_STRING([--with-gaudi=PATH], [Path to Habana HL-Thunk installation])],
    [],
    [with_gaudi=no])

AC_DEFINE([HAVE_GAUDI], [1], [Define to 1 if Gaudi support is enabled])

AS_IF([test "x$with_gaudi" != "xno"], [
    gaudi_inc_path=$with_gaudi/include/habanalabs
    gaudi_lib_path=$with_gaudi/lib/habanalabs

    AC_MSG_CHECKING([for hlthunk.h in $gaudi_inc_path])
    CPPFLAGS_SAVE="$CPPFLAGS"
    CPPFLAGS="$CPPFLAGS -I$gaudi_inc_path"

    AC_CHECK_HEADER([hlthunk.h], [
        AC_MSG_RESULT([yes])
        AM_CONDITIONAL([HAVE_GAUDI], [true])
        
        # Now check for the symbol in the library
        AC_MSG_CHECKING([for hlthunk_get_version in libhl-thunk])
        LDFLAGS_SAVE="$LDFLAGS"
        LIBS_SAVE="$LIBS"
        LDFLAGS="$LDFLAGS -L$gaudi_lib_path"
        LIBS="$LIBS -lhl-thunk"

        AC_LINK_IFELSE(
            [AC_LANG_PROGRAM([[#include <hlthunk.h>]],
                             [[return hlthunk_get_version();]])],
            [
                AC_MSG_RESULT([yes])
                AC_DEFINE([HAVE_GAUDI], [1], [Define if Gaudi support is enabled])
            ],
            [
                AC_MSG_RESULT([no])
                AC_MSG_ERROR([libhl-thunk not found or missing hlthunk_get_version symbol])
            ])

        LDFLAGS="$LDFLAGS_SAVE"
        LIBS="$LIBS_SAVE"

    ], [
        AC_MSG_RESULT([no])
        AC_MSG_ERROR([hlthunk.h not found in $gaudi_inc_path])
    ])

    CPPFLAGS="$CPPFLAGS_SAVE"
], [
    AM_CONDITIONAL([HAVE_GAUDI], [false])
])

AM_CONDITIONAL([HAVE_GAUDI], [test "x$have_gaudi" = "xyes"])
AS_IF([test "x$have_gaudi" = "xyes"], [
    uct_gaudi_modules="gaudi"
])
AC_SUBST([uct_gaudi_modules])
