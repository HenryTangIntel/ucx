#
# Copyright (c) 2023. ALL RIGHTS RESERVED.
# - If this is a new file, add your license text here
# See file LICENSE for terms.
#

AC_DEFUN([UCX_UCT_GAUDI_CHECK],[
    AC_ARG_WITH([gaudi],
        AS_HELP_STRING([--with-gaudi=@<:@path@:>@],
                       [Build Gaudi transport. @<:@default=auto@:>@. Optionally specify path to Gaudi installation.]),
        [with_gaudi=$withval],
        [with_gaudi=auto])

    gaudi_status="no (not checked)" # Initialize status

    AS_IF([test "x$with_gaudi" != "xno"], [
        AC_MSG_CHECKING([for gaudi])

        gaudi_CPPFLAGS_SAVED="$CPPFLAGS"
        gaudi_LDFLAGS_SAVED="$LDFLAGS"
        gaudi_LIBS_SAVED="$LIBS"

        gaudi_CPPFLAGS=""
        gaudi_LDFLAGS=""
        gaudi_LIBS=""

        # Default to "no" unless specific checks pass
        gaudi_status="no"

        # If a path is provided with --with-gaudi=<path>
        AS_IF([test "x$with_gaudi" != "xyes" -a "x$with_gaudi" != "xauto" -a "x$with_gaudi" != "xno"], [
            gaudi_CPPFLAGS="-I$with_gaudi/include" # Example include path
            gaudi_LDFLAGS="-L$with_gaudi/lib"     # Example lib path
            # Add more specific paths if Gaudi SDK has a known structure
        ])
        CPPFLAGS="$CPPFLAGS $gaudi_CPPFLAGS"
        LDFLAGS="$LDFLAGS $gaudi_LDFLAGS"
        # LIBS="$LIBS -lgaudidriver" # Example library, replace with actual Gaudi library

        # TODO: Actual detection logic for Gaudi SDK
        # This is a placeholder. You'll need to:
        # 1. Check for Gaudi header uapi/hlthunk.h
        # 2. Check for Gaudi library libhlthunk
        # 3. Optionally use pkg-config if Gaudi provides a .pc file (not doing this yet)

        gaudi_header_found="no"
        AC_PREPROC_IFELSE([AC_LANG_PROGRAM([[#include <uapi/hlthunk.h>
                                          ]], [[return 0;]])],
                          [gaudi_header_found="yes"],
                          []) # No 'else' action, just check CPPFLAGS

        gaudi_lib_found="no"
        AS_IF([test "$gaudi_header_found" = "yes"], [
            AC_CHECK_LIB([hlthunk], [hlthunk_device_open], # Assuming hlthunk_device_open is a valid function in the lib
                         [gaudi_lib_found="yes"
                          LIBS="-lhlthunk $LIBS"], # Prepend to LIBS
                         [gaudi_lib_found="no"],
                         [$gaudi_LDFLAGS]) # Use potential LDFLAGS from --with-gaudi=<path>/lib
        ])

        AS_IF([test "$gaudi_header_found" = "yes" -a "$gaudi_lib_found" = "yes"], [
            gaudi_status="yes (found uapi/hlthunk.h and libhlthunk)"
            AC_MSG_RESULT([yes])
            # gaudi_CPPFLAGS and gaudi_LDFLAGS are already part of CPPFLAGS/LDFLAGS for the checks
            # We need to ensure GAUDI_LIBS is set for the Makefile.am
            GAUDI_LIBS="-lhlthunk" # Set this for substitution
        ], [
            # Reset status message if not fully found
            AC_MSG_RESULT([no (header found: $gaudi_header_found, lib found: $gaudi_lib_found)])
            gaudi_status="no (details: header $gaudi_header_found, lib $gaudi_lib_found)"
            AS_IF([test "x$with_gaudi" = "xyes"], [
                 AC_MSG_ERROR([Gaudi explicitly requested with --with-gaudi, but uapi/hlthunk.h or libhlthunk not found. Searched CPPFLAGS: $CPPFLAGS, LDFLAGS: $LDFLAGS (Please provide path or ensure it is in standard locations)])
            ])
        ])

        CPPFLAGS="$gaudi_CPPFLAGS_SAVED"
        LDFLAGS="$gaudi_LDFLAGS_SAVED"
        LIBS="$gaudi_LIBS_SAVED" # Restore original LIBS
    ])

    AS_IF([test "$gaudi_status" = "yes (found uapi/hlthunk.h and libhlthunk)"], [
        AC_DEFINE([HAVE_GAUDI], [1], [Have Gaudi support (hlthunk))])
        AC_SUBST([GAUDI_CPPFLAGS], [$gaudi_CPPFLAGS]) # These are the ones from --with-gaudi=<path>/include
        AC_SUBST([GAUDI_LDFLAGS], [$gaudi_LDFLAGS])  # These are the ones from --with-gaudi=<path>/lib
        AC_SUBST([GAUDI_LIBS]) # GAUDI_LIBS was set to -lhlthunk above
        uct_modules="${uct_modules}:gaudi"
        uct_gaudi_modules="gaudi" # Define this for the summary message
        AC_SUBST([uct_gaudi_modules])
    ])

    AM_CONDITIONAL([HAVE_GAUDI], [test "$gaudi_status" = "yes (found uapi/hlthunk.h and libhlthunk)"])
    AM_CONDITIONAL([HAVE_GAUDI_MODULE], [test "$gaudi_status" = "yes (found uapi/hlthunk.h and libhlthunk)"])
])
