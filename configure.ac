##########################################################################
#
# Copyright (C) 2021 Amlogic Corporation.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
##########################################################################
dnl                                               -*- Autoconf -*-
dnl Process this file with autoconf to produce a configure script.

dnl AC_PREREQ([2.69])
AC_INIT(agmplayer, version-1.0)
AC_CONFIG_SRCDIR([src])

AM_INIT_AUTOMAKE([foreign no-dist-gzip dist-bzip2 1.9])
LT_INIT

dnl subdirectories.
#SUBDIRS=""

dnl Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_CPP
AC_PROG_CXXCPP

dnl use pretty build output with automake >= 1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])],
  [AM_DEFAULT_VERBOSITY=1
   AC_SUBST(AM_DEFAULT_VERBOSITY)])

dnl **********************************
dnl checks for dependencies
dnl **********************************
AC_HEADER_STDC

dnl Checks for header files.
AC_CHECK_HEADERS([limits.h memory.h stdlib.h string.h sys/socket.h unistd.h])

dnl Checks for typedefs, structures, and compiler characteristics.
dnl AC_CHECK_HEADER_STDBOOL
AC_TYPE_UINT8_T

dnl Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset strdup strerror])

dnl GStreamer version. Use 1.10 by default
AC_ARG_ENABLE(gstreamer1,
              AS_HELP_STRING([--enable-gstreamer1],
                             [Use GStreamer 1.x]), ,
              [enable_gstreamer1=auto])

ENABLE_SUBTEC = false
AC_ARG_ENABLE([subtec],
              AS_HELP_STRING([--enable-subtec],[enable subtec filter + sink (default is no)]),
              [
                case "${enableval}" in
                 yes) ENABLE_SUBTEC=true;;
                 no)  ENABLE_SUBTEC=false;;
                 *) AC_MSG_ERROR([bad value ${enableval} for --enable-subtec]) ;;
                esac
              ],
              [echo "subtec is disabled"])
AM_CONDITIONAL([ENABLE_SUBTEC], [test x$ENABLE_SUBTEC = xtrue])
AC_MSG_NOTICE([subtec is $ENABLE_SUBTEC])

AS_IF([test "x$enable_gstreamer1" = "xyes"], [
  GST_MAJORMINOR=1.0
  PKG_CHECK_MODULES([GST], [gstreamer-1.0 >= 1.4], have_gst1="yes", have_gst1="no")

  AS_IF([test "x$have_gst1" = "xyes"], [
    AC_DEFINE(USE_GST1, 1, [Build with GStreamer 1.x])
  ], [])
], [])

AS_IF([test "x$enable_gstreamer1" = "xyes" -a "x$have_gst1" != "xyes"], [
   AC_MSG_ERROR([Could not find GStreamer 1.x dependencies:

$GST_PKG_ERRORS])], [])

AS_IF([test "x$have_gst1" != "xyes"], [
  GST_MAJORMINOR=0.10
  PKG_CHECK_MODULES([GST], [gstreamer-0.10 >= 0.10.28])
], [])

plugindir="\$(libdir)/"
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 0.10.28])
AC_SUBST(GST_MAJORMINOR)
AC_SUBST(ENABLE_SUBTEC)
AC_SUBST(plugindir)
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
