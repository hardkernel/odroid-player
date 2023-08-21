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

plugin_LTLIBRARIES = libkeyboard.la libAGMPlayer.la
libkeyboard_la_SOURCES = keyboard.c keyboard.h
libkeyboard_la_CFLAGS =  $(GST_CFLAGS)
libkeyboard_la_LDFLAGS = $(GST_LIBS)
libkeyboard_la_LIBTOOLFLAGS = --tag=disable-static

libAGMPlayer_la_SOURCES = agmplayer.c agmplayer.h
libAGMPlayer_la_CFLAGS =  $(GST_CFLAGS)
libAGMPlayer_la_LDFLAGS = $(GST_LIBS)
libAGMPlayer_la_LDFLAGS += -lgstpbutils-1.0 -lgsttag-1.0 -lgstaudio-1.0 -lgstvideo-1.0 \
                            $(GST_BASE_LIBS) $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS)
libAGMPlayer_la_LIBTOOLFLAGS = --tag=disable-static


bin_PROGRAMS =  AGMPlayer-sample #agmplayer
AGMPlayer_sample_SOURCES = AGMPlayer-sample.c
AGMPlayer_sample_LDFLAGS = -module -avoid-version
AGMPlayer_sample_LDADD = libkeyboard.la libAGMPlayer.la

#agmplayer is for debug, reserved
#agmplayer_SOURCES = agmplayer.c
#agmplayer_CFLAGS =  $(GST_CFLAGS)
#agmplayer_LDFLAGS = $(GST_LIBS) -module -avoid-version
#agmplayer_LDADD = -lgstpbutils-1.0 -lgsttag-1.0 -lgstaudio-1.0 -lgstvideo-1.0  libkeyboard.la \
#                            $(GST_BASE_LIBS) $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS)

include_HEADERS = agmplayer.h



