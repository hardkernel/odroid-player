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
##########################################################################

SUBDIRS = 
AM_CPPFLAGS = -pthread -Wall
if ENABLE_SUBTEC
AM_CPPFLAGS += -DENABLE_SUBTEC
endif

plugin_LTLIBRARIES = libagmplayer.la libopclient.la
libagmplayer_la_SOURCES = server/agmp/agmplayer.c \
                          server/agmp/agmplayer.h
libagmplayer_la_CFLAGS = $(GST_CFLAGS)
libagmplayer_la_LDFLAGS = $(GST_LIBS)
libagmplayer_la_LDFLAGS += -lgstpbutils-1.0 -lgsttag-1.0 -lgstaudio-1.0 \
                           -lgstvideo-1.0 -lgstsecmemallocator \
                           $(GST_BASE_LIBS) $(GST_LIBS) $(GST_PLUGINS_BASE_LIBS)
libagmplayer_la_LIBTOOLFLAGS = --tag=disable-static

libopclient_la_SOURCES = client/opc/opclient.c \
                         client/opc/opclient.h \
                         client/opc/opc_cmd.c \
                         client/opc/opc_cmd.h \
                         client/opc/opc_utils.c \
                         client/opc/opc_utils.h \
                         client/opc/monitor/monitor.c \
                         client/opc/monitor/monitor.h \
                         client/opc/receive/receive.c \
                         client/opc/receive/receive.h \
                         client/opc/response/response.c \
                         client/opc/response/response.h \
                         client/opc/send/send.c \
                         client/opc/send/send.h \
                         client/opc/utils/logger.c \
                         client/opc/utils/logger.h \
                         client/opc/utils/queue.c \
                         client/opc/utils/queue.h \
                         message/message.pb-c.c \
                         message/message.pb-c.h
libopclient_la_CFLAGS = $(GST_CFLAGS) $(PROTOBUF_C_CFLAGS)
libopclient_la_LDFLAGS = $(GST_LIBS)  $(PROTOBUF_C_LIBS)
libopclient_la_LIBTOOLFLAGS = --tag=disable-static

bin_PROGRAMS = odroid-player odroid-player-client
odroid_player_SOURCES = server/main.c \
                        server/agmp/agmp_cmds.c \
                        server/agmp/agmp_utils.c \
                        server/client/client.c \
                        server/player/player.c \
                        server/playlist/playlist.c \
                        server/receive/receive.c \
                        server/response/response.c \
                        server/send/send.c \
                        server/server/server.c \
                        server/socket/socket.c \
                        server/stream/stream.c \
                        server/utils/fd_set.c \
                        server/utils/logger.c \
                        server/utils/queue.c \
                        server/utils/watchdog.c \
                        message/message.pb-c.c
odroid_player_CFLAGS = $(GST_CFLAGS) $(PROTOBUF_C_CFLAGS)
odroid_player_LDFLAGS = $(GST_LIBS) $(PROTOBUF_C_LIBS)
odroid_player_LDFLAGS += -module -avoid-version
odroid_player_LDADD = libagmplayer.la

odroid_player_client_SOURCES = client/main.c \
                               client/utils.c \
                               message/message.pb-c.c
odroid_player_client_CFLAGS = $(GST_CFLAGS) $(PROTOBUF_C_CFLAGS)
odroid_player_client_LDFLAGS = $(GST_LIBS) $(PROTOBUF_C_LIBS)
odroid_player_client_LDFLAGS += -module -avoid-version
odroid_player_client_LDADD = libopclient.la

