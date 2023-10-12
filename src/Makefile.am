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

SUBDIRS = 
AM_CPPFLAGS = -pthread -Wall
if ENABLE_SUBTEC
AM_CPPFLAGS += -DENABLE_SUBTEC
endif

plugin_LTLIBRARIES = libAGMPlayer.la

libAGMPlayer_la_SOURCES = agmplayer.c agmplayer.h
libAGMPlayer_la_CFLAGS =  $(GST_CFLAGS)
libAGMPlayer_la_LDFLAGS = $(GST_LIBS)
libAGMPlayer_la_LDFLAGS += -lgstpbutils-1.0 -lgsttag-1.0 -lgstaudio-1.0 -lgstvideo-1.0 -lgstsecmemallocator \
                            $(GST_BASE_LIBS) $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS)
libAGMPlayer_la_LIBTOOLFLAGS = --tag=disable-static

bin_PROGRAMS =  AGMPlayer-sample
AGMPlayer_sample_SOURCES = AGMPlayer-sample.c
AGMPlayer_sample_LDFLAGS = -module -avoid-version
AGMPlayer_sample_LDADD = libAGMPlayer.la

include_HEADERS = agmplayer.h



