# -*- Mode: Autoconf; -*-
#
# See COPYRIGHT in top-level directory.
#

AC_PREREQ(2.63)

m4_include([maint/version.m4])
AC_INIT([izem],ZM_VERSION_m4)

ZM_VERSION=ZM_VERSION_m4
AC_SUBST([ZM_VERSION])

libzm_so_version="libzm_so_version_m4"
AC_SUBST([libzm_so_version])

# Produce a numeric version assuming the following format:
# Version: [MAJ].[MIN].[REV][EXT][EXT_NUMBER]
# Example: 1.0.7rc1 has
#          MAJ = 1
#          MIN = 0
#          REV = 7
#          EXT = rc
#          EXT_NUMBER = 1
#
# Converting to numeric version will convert EXT to a format number:
#          ALPHA (a) = 0
#          BETA (b)  = 1
#          RC (rc)   = 2
#          PATCH (p) = 3
# Regular releases are treated as patch 0
#
# Numeric version will have 1 digit for MAJ, 2 digits for MIN,
# 2 digits for REV, 1 digit for EXT and 2 digits for EXT_NUMBER.
changequote(<<,>>)
V1=`expr $ZM_VERSION : '\([0-9]*\)\.[0-9]*\.*[0-9]*[a-zA-Z]*[0-9]*'`
V2=`expr $ZM_VERSION : '[0-9]*\.\([0-9]*\)\.*[0-9]*[a-zA-Z]*[0-9]*'`
V3=`expr $ZM_VERSION : '[0-9]*\.[0-9]*\.*\([0-9]*\)[a-zA-Z]*[0-9]*'`
V4=`expr $ZM_VERSION : '[0-9]*\.[0-9]*\.*[0-9]*\([a-zA-Z]*\)[0-9]*'`
V5=`expr $ZM_VERSION : '[0-9]*\.[0-9]*\.*[0-9]*[a-zA-Z]*\([0-9]*\)'`
changequote([,])

if test "$V2" -le 9 ; then V2="0$V2" ; fi
if test "$V3" = "" ; then V3="0"; fi
if test "$V3" -le 9 ; then V3="0$V3" ; fi
if test "$V4" = "a" ; then
    V4=0
elif test "$V4" = "b" ; then
    V4=1
elif test "$V4" = "rc" ; then
    V4=2
elif test "$V4" = "" ; then
    V4=3
    V5=0
elif test "$V4" = "p" ; then
    V4=3
fi
if test "$V5" -le 9 ; then V5="0$V5" ; fi

ZM_NUMVERSION=`expr $V1$V2$V3$V4$V5 + 0`
AC_SUBST(ZM_NUMVERSION)

AC_CONFIG_AUX_DIR(m4)
AC_CONFIG_MACRO_DIR(m4)
AC_PROG_CC
AC_HEADER_STDC

dnl ----------------------------------------------------------------------------
dnl check the environment and the function availability
dnl ----------------------------------------------------------------------------
# compute canonical system types
AC_CANONICAL_HOST

dnl ----------------------------------------------------------------------------
dnl user option/feature selection
dnl ----------------------------------------------------------------------------
# debug options
AC_ARG_ENABLE([debug],
[  --enable-debug@<:@=OPTS@:>@   control the level of debugging. "OPTS" is a list of
                          comma separated names below. Default is "none".
                           none - no debugging
                           yes  - add compiler flags, -g -O0 -Wall
                           log  - enable debug event logging
                           all  - all of the above choices],,
[enable_debug=none])

# strip off multiple options, separated by commas
save_IFS="$IFS"
IFS=","
for option in $enable_debug ; do
    case "$option" in
        no|none)
        ;;
        yes)
            debug_flags=yes
        ;;
        log)
            debug_log=yes
        ;;
        all)
            debug_flags=yes
            debug_log=yes
        ;;
        *)
            IFS=$save_IFS
            AC_MSG_WARN([Unknown value $option for enable-debug])
            IFS=","
        ;;
    esac
done
IFS="$save_IFS"

AS_IF([test "x$debug_flags" = "xyes"], [CFLAGS="$CFLAGS -g -O0 -Wall"])
AS_IF([test "x$debug_log" = "xyes"],
    [AC_DEFINE(ZM_CONFIG_USE_DEBUG_LOG, 1, [Define to enable debug logging])])


AM_INIT_AUTOMAKE([-Wall -Wno-portability-recursive -Werror foreign 1.12.3 subdir-objects])

AM_SILENT_RULES([yes])
AM_PROG_AS
AM_PROG_AR

LT_INIT

AC_PROG_INSTALL

dnl ----------------------------------------------------------------------------
dnl config headers
dnl ----------------------------------------------------------------------------
AC_CONFIG_HEADERS([src/include/zm_config.h])
AH_TOP([
/* -*- Mode: C; c-basic-offset:4 ; indent-tabs-mode:nil ; -*- */
/*
 * See COPYRIGHT in top-level directory.
 */

#ifndef ZM_CONFIG_H_INCLUDED
#define ZM_CONFIG_H_INCLUDED
])
AH_BOTTOM([
#endif /* ZM_CONFIG_H_INCLUDED */
])
dnl ----------------------------------------------------------------------------

AC_CONFIG_FILES([Makefile
                 Doxyfile
                 maint/izem.pc
                 src/Makefile
                 test/Makefile
                 test/regres/Makefile
                 test/regres/lock/Makefile
                 test/regres/list/Makefile
                 test/regres/queue/Makefile
                 ])
AC_OUTPUT
