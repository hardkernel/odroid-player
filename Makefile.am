##########################################################################
# Copyright 2018 RDK Management
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation, version 2
# of the license.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
# Boston, MA 02110-1301, USA.
##########################################################################

SUBDIRS = 
AM_CPPFLAGS = -pthread -Wall
if ENABLE_SUBTEC
AM_CPPFLAGS += -DENABLE_SUBTEC
endif


bin_PROGRAMS = agmplayer
agmplayer_SOURCES = agmplayer.c agmplayer-kb.c agmplayer-kb.h
agmplayer_CFLAGS =  $(GST_CFLAGS)
agmplayer_LDFLAGS = $(GST_LIBS)
agmplayer_LDADD = -lgstpbutils-1.0 -lgsttag-1.0 -lgstaudio-1.0 -lgstvideo-1.0 \
                            $(GST_BASE_LIBS) $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS)
noinst_HEADERS = agmplayer-kb.h
