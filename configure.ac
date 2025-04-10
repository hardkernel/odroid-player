##########################################################################
#
# Copyright (C) 2021 Amlogic Corporation.
# Copyright (C) 2025 Phillip Choi for Hardkernel.
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
# This file has been modified by Phillip Choi (phillip.choi@hardkernel.com).
# Original copyright belongs to Amlogic Corporation under Apache License 2.0.
#
##########################################################################
dnl                                               -*- Autoconf -*-
dnl Process this file with autoconf to produce a configure script.

dnl AC_PREREQ([2.69])
AC_INIT(odroid-player, version-1.0.0, phillip.choi@hardkernel.com)
AC_CONFIG_SRCDIR([src])

AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign 1.9 subdir-objects])
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
AC_CHECK_HEADERS([assert.h 
                  ctype.h 
                  dirent.h
                  errno.h
                  fcntl.h
                  limits.h 
                  memory.h 
                  netinet/in.h
                  pthread.h
                  setjmp.h
                  stdarg.h
                  stdbool.h
                  stddef.h 
                  stdio.h
                  stdlib.h 
                  signal.h
                  string.h 
                  sys/select.h 
                  sys/socket.h 
                  sys/stat.h
                  sys/un.h
                  termios.h
                  time.h
                  unistd.h])

dnl Checks for typedefs, structures, and compiler characteristics.
dnl AC_CHECK_HEADER_STDBOOL
AC_TYPE_UINT8_T

# TODO ...
dnl Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([assert free malloc memset strdup strerror])

dnl Check subtec is enabled
# ENABLE_SUBTEC = false
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
AC_SUBST(ENABLE_SUBTEC)

dnl Check GStreamer version. Use 1.10 by default
GST_MAJORMINOR=1.0
AC_SUBST(GST_MAJORMINOR)

PKG_CHECK_MODULES([GST], [gstreamer-1.0 >= 1.20],
                  [have_gst1="yes"],
                  [have_gst1="no"])

AS_IF([test "x$have_gst1" = "xyes"], [
    AC_DEFINE(USE_GST1, 1, [Build with GStreamer 1.x])
], [
    AC_MSG_ERROR([Could not find GStreamer 1.x dependencies: $GST_PKG_ERRORS])
])

plugindir="\$(libdir)/"
AC_SUBST(plugindir)

dnl Check GLib version
PKG_CHECK_MODULES([GLIB], [glib-2.0 >= 0.10.28],
                  [have_glib="yes"],
                  [have_glib="no"])

AS_IF([test "x$have_glib" = "xyes"], [
    AC_DEFINE(USE_GLIB, 1, [Build with GLib-2.0])
], [
    AC_MSG_ERROR([Could not find GLib-2.0 dependencies])
])

dnl Check Protobuf-c version
PKG_CHECK_MODULES([PROTOBUF_C], [libprotobuf-c >= 1.0.0],
                  [have_proto="yes"],
                  [have_proto="no"])

AS_IF([test "x$have_proto" = "xyes"], [
    AC_DEFINE(USE_GLIB, 1, [Build with Protobuf-c])
], [
    AC_MSG_ERROR([Could not find Protobuf-c dependencies])
])

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

