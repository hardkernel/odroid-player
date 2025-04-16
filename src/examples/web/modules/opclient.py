#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
# SPDX-License-Identifier: Apache-2.0

import ctypes
import os
import time


SYSTEM_LIB_PATH = "/usr/lib/aarch64-linux-gnu/libopclient.so"
OPC_LIB_PATH = "../../.libs/libopclient.so"


def load_library():
    paths = [
        os.path.abspath(SYSTEM_LIB_PATH),
        os.path.abspath(OPC_LIB_PATH)
    ]

    for path in paths:
        try:
            return ctypes.CDLL(path)
        except OSError as e:
            print(f"Failed to load library at {path}: {e}")

    raise RuntimeError("Unable to load library")


opc = load_library()

# opc_init
opc.opc_init.restype = ctypes.POINTER(ctypes.c_void_p)

# opc_connect
opc.opc_connect.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
opc.opc_connect.restype = ctypes.c_int

# opc_prepare
opc.opc_prepare.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
opc.opc_prepare.restype = ctypes.c_int

# opc_destroy
opc.opc_destroy.argtypes = [ctypes.POINTER(ctypes.c_void_p)]

# opc_send_play
opc.opc_send_play.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
opc.opc_send_play.restype = ctypes.POINTER(ctypes.c_void_p)

# opc_send_stop
opc.opc_send_stop.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
opc.opc_send_stop.restype = ctypes.POINTER(ctypes.c_void_p)

# opc_send_pause
opc.opc_send_pause.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
opc.opc_send_pause.restype = ctypes.POINTER(ctypes.c_void_p)

# opc_send_next
opc.opc_send_next.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
opc.opc_send_next.restype = ctypes.POINTER(ctypes.c_void_p)

# opc_send_prev
opc.opc_send_prev.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
opc.opc_send_prev.restype = ctypes.POINTER(ctypes.c_void_p)

# opc_recv_time
opc.opc_recv_time.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
opc.opc_recv_time.restype = ctypes.POINTER(ctypes.c_void_p)

# opc_recv_amsg
opc.opc_recv_amsg.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
opc.opc_recv_amsg.restype = ctypes.POINTER(ctypes.c_void_p)

# opc_recv_uri
opc.opc_recv_uri.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
opc.opc_recv_uri.restype = ctypes.POINTER(ctypes.c_void_p)

# opc_status_get_status
opc.opc_status_get_status.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
opc.opc_status_get_status.restype = ctypes.c_int

# opc_status_get_type
opc.opc_status_get_type.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
opc.opc_status_get_type.restype = ctypes.c_int

# opc_status_get_msg_idx
opc.opc_status_get_msg_idx.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
opc.opc_status_get_msg_idx.restype = ctypes.c_uint

# opc_status_get_n_data
opc.opc_status_get_n_data.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
opc.opc_status_get_n_data.restype = ctypes.c_uint

# opc_status_get_data
opc.opc_status_get_data.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
opc.opc_status_get_data.restype = ctypes.POINTER(ctypes.c_char_p)

# opc_free_status
opc.opc_free_status.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
opc.opc_free_status.restype = None

# opc_set_log_level
opc.opc_set_log_level.argtypes = [ctypes.c_int]
opc.opc_set_log_level.restype = None


class op_client:
    def __init__(self):
        self.client = opc.opc_init()
        if not self.client:
            raise RuntimeError("Failed to create client")

    def connect(self):
        print("Connecting to server...")
        ret = opc.opc_connect(self.client)
        if ret == 0:
            raise RuntimeError("Failed to connect server")
        else:
            print("Server connected")

    def prepare(self):
        ret = opc.opc_prepare(self.client)
        if ret == 0:
            raise RuntimeError("Failed to prepare client")

    def destroy(self):
        opc.opc_destroy(self.client)

    def send_play(self):
        status = opc.opc_send_play(self.client)
        if not status:
            raise RuntimeError("Failed to send play command")
        return status

    def send_stop(self):
        status = opc.opc_send_stop(self.client)
        if not status:
            raise RuntimeError("Failed to send stop command")
        return status

    def send_pause(self):
        status = opc.opc_send_pause(self.client)
        if not status:
            raise RuntimeError("Failed to send pause command")
        return status

    def send_next(self):
        status = opc.opc_send_next(self.client)
        if not status:
            raise RuntimeError("Failed to send next command")
        return status

    def send_prev(self):
        status = opc.opc_send_prev(self.client)
        if not status:
            raise RuntimeError("Failed to send prev command")
        return status

    def recv_time(self):
        status = opc.opc_recv_time(self.client)
        if not status:
            raise RuntimeError("Failed to send time command")
        return status

    def recv_message(self):
        # Beware, this is a blocking function to receive async messages
        return opc.opc_recv_amsg(self.client)

    def recv_uri(self):
        status = opc.opc_recv_uri(self.client)
        if not status:
            raise RuntimeError("Failed to send uri command")
        return status

    # Beware, you need to update this dictionary
    # after you change the protobuf message!
    cmd_status = { 0: "SUCCESS",
                   1: "INVALID",
                   2: "TOOMANY",
                   3: "TIMEOUT",
                   4: "ESERVER",
                   5: "ECLIENT",
                   6: "RECONNT",
                   7: "BRDINFO" }

    def status_get_status(self, status):
        if not status:
            raise RuntimeError("Invalid status")
        ret = opc.opc_status_get_status(status)
        if ret < 0:
            raise RuntimeError("Invalid status")
        return self.cmd_status[ret]

    # Beware, you need to update this dictionary
    # after you change the protobuf message!
    cmd_type = { 0: "COMMAND",
                 1: "STREAM",
                 2: "NOTIFY",
                 3: "BROADCAST" }

    def status_get_type(self, status):
        if not status:
            raise RuntimeError("Invalid status")
        ret = opc.opc_status_get_type(status)
        if ret < 0:
            raise RuntimeError("Invalid status")
        return self.cmd_type[ret]

    # Beware, you need to update this dictionary
    # after you change the protobuf message!
    cmd_cmd = { 0: "NONE",
                1: "PLAY",
                2: "STOP",
                3: "PAUSE",
                4: "NEXT",
                5: "PREV",
                6: "QUIT",
                7: "SEEK",
                8: "TIME",
                9:  "URI",
                10: "ACK" }

    def status_get_cmd(self, status):
        if not status:
            raise RuntimeError("Invalid status")
        ret = opc.opc_status_get_cmd(status)
        if ret < 0:
            raise RuntimeError("Invalid status")
        return self.cmd_cmd[ret]

    def status_get_msg_idx(self, status):
        if not status:
            raise RuntimeError("Invalid status")
        return opc.opc_status_get_msg_idx(status)

    def status_get_data(self, status):
        if not status:
            raise RuntimeError("Invalid status")

        n_data = opc.opc_status_get_n_data(status)
        data = opc.opc_status_get_data(status)

        if n_data == 0 or not data:
            raise RuntimeError("Invalid status")
        return data, n_data

    def free_status(self, status):
        if not status:
            raise RuntimeError("Invalid status")
        opc.opc_free_status(status)

    def set_log_level(self, level):
        if level < 0 or level > 6:
            raise RuntimeError("Invalid log level")
        opc.opc_set_log_level(level)


