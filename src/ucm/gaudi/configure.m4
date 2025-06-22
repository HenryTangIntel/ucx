#
# Copyright (c) 2023. ALL RIGHTS RESERVED.
# - If this is a new file, add your license text here
# See file LICENSE for terms.
#

AC_DEFUN([UCX_UCM_GAUDI_CHECK],[
    AC_ARG_ENABLE([ucm-gaudi],
        AS_HELP_STRING([--enable-ucm-gaudi],
                       [Enable Gaudi memory hooks in UCM. @<:@default=yes, if Gaudi is found@:>@]),
        [enable_ucm_gaudi=$enableval],
        [enable_ucm_gaudi=auto]) # Default to auto

    ucm_gaudi_status="no"
    AS_IF([test "x$enable_ucm_gaudi" != "xno" && test "x$HAVE_GAUDI" = "x1"], [
        # If Gaudi is found (HAVE_GAUDI=1 from the UCT check) and ucm-gaudi is not 'no'
        # We need to check if Gaudi's API provides necessary functions for memory hooks.
        # This is a placeholder for such checks.
        # For now, assume if Gaudi is present, UCM hooks can be built.

        # Example check (replace with actual Gaudi API function/header needed for UCM)
        # AC_CHECK_HEADER([gaudi_hooks_api.h], [ucm_gaudi_header_found="yes"], [ucm_gaudi_header_found="no"])
        # if test "$ucm_gaudi_header_found" = "yes"; then
        #    ucm_gaudi_status="yes"
        # else
        #    if test "x$enable_ucm_gaudi" = "xyes"; then
        #        AC_MSG_ERROR([UCM Gaudi explicitly requested, but required Gaudi API for hooks not found.])
        #    fi
        #    ucm_gaudi_status="no (Gaudi hook API not found)"
        # fi

        # Placeholder: Assume UCM Gaudi is possible if HAVE_GAUDI is true
        ucm_gaudi_status="yes (assuming possible with Gaudi)"
        AC_DEFINE([HAVE_UCM_GAUDI], [1], [Have UCM Gaudi memory hooks])
        ucm_modules="${ucm_modules}:gaudi"
    ], [
        if test "x$enable_ucm_gaudi" = "xyes" && test "x$HAVE_GAUDI" != "x1"; then
            AC_MSG_WARN([UCM Gaudi explicitly requested (--enable-ucm-gaudi), but Gaudi support itself is not available/enabled. UCM Gaudi hooks will be disabled.])
        elif test "x$enable_ucm_gaudi" = "xyes"; then
            AC_MSG_WARN([UCM Gaudi explicitly requested (--enable-ucm-gaudi), but prerequisite checks failed. UCM Gaudi hooks will be disabled.])
        fi
        ucm_gaudi_status="no (Gaudi not available or ucm-gaudi disabled)"
    ])

    AM_CONDITIONAL([HAVE_UCM_GAUDI], [test "$ucm_gaudi_status" = "yes (assuming possible with Gaudi)"])
])
