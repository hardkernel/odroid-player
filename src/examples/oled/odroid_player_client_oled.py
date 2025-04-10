#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
# SPDX-License-Identifier: Apache-2.0

from luma.core.render import canvas
from modules import luma_oled, opclient
import argparse
import os
import psutil
import socket
import signal
import sys
import threading, _thread
import time

PLAYER_NAME = "odroid-player"

g_mem = "0"
g_time = "NULL"
g_uri = "NULL"


def update_ram():
    global g_mem

    try:
        while True:
            for proc in psutil.process_iter(['pid', 'name']):
                proc_name = proc.info['name'].lower()
                if PLAYER_NAME == proc_name:
                    p = psutil.Process(proc.info['pid'])
                    mem = p.memory_percent()
                    g_mem = str(round(mem, 2)) + '%'
                    time.sleep(1)

    except Exception as e:
        print(f"Exception occured while reading ram info: {e}")
    except KeyboardInterrupt:
        print("Update Ram Info Thread Keyboard Interrupt")
    finally:
        _thread.interrupt_main()


def update_time(client):
    global g_time

    try:
        print("Connecting stream...")
        while True:
            try:
                status = client.recv_time()
            except Exception:
                print("Reconnecting stream...")
                time.sleep(1)
                continue

            print("Stream ready")
            data, n_data = client.status_get_data(status)
            if n_data != 2:
                raise RuntimeError("Invalid stream status returned")

            client_stream_path = data[1]
            client.free_status(status)

            stream = socket.socket(socket.AF_UNIX, socket.SOCK_DGRAM)
            stream.bind(client_stream_path)

            while True:
                try:
                    data, _ = stream.recvfrom(64)
                    if not data:
                        continue
                    elif os.path.exists(client_stream_path) == False:
                        break
                    g_time = data.decode()
                    time.sleep(0.01)
                except Exception:
                    raise
    except Exception as e:
        print(f"Exception occured while reading stream: {e}")
    except KeyboardInterrupt:
        print("Stream Thread Keyboard Interrupt")
    finally:
        os._exit(1)


def receive_message(client):
    global g_uri

    try:
        while True:
            status = client.recv_message()
            if not status:
                # timeout
                raise RuntimeError("Couldn't receive message")

            cmd_cmd = client.status_get_cmd(status)
            cmd_type = client.status_get_type(status)
            cmd_status = client.status_get_status(status)

            if cmd_type == "NOTIFY":
                if cmd_status == "TOOMANY":
                    print("NOTIFY: Too many clients")
                    raise RuntimeError("Too many clients")
                elif cmd_status == "TIMEOUT":
                    print("NOTIFY: Client timeout")
                elif cmd_status == "ESERVER":
                    data, _ = client.status_get_data(status)
                    message = data[0].decode()
                    print(f"NOTIFY: Server error occured: {message}")
                elif cmd_status == "RECONNT":
                    print("NOTIFY: Server reconnected")

            elif cmd_type == "BROADCAST":
                data, _ = client.status_get_data(status)
                command = data[0].decode()
                print(f"BROADCAST: {command}")
                if cmd_cmd == "URI" and cmd_status == "BRDINFO":
                    g_uri = command

            client.free_status(status)

    except Exception as e:
        print(f"Exception occured while reading message: {e}")
    except KeyboardInterrupt:
        print("Receive Thread Keyboard Interrupt")
    finally:
        _thread.interrupt_main()


def display(device, client):
    global g_uri
    global g_mem

    status = client.recv_uri()
    data, _ = client.status_get_data(status)
    g_uri = data[0].decode()
    client.free_status(status)

    curr_uri = g_uri
    pos = (0, 0)
    padding = 0
    while True:
        x, y = pos
        with canvas(device) as draw:
            left, top, right, bottom = draw.textbbox((0, 0), g_uri)
            width, height = right - left, bottom - top

            mleft, mtop, mright, mbottom = draw.textbbox((0, 0), g_mem)
            mwidth, mheight = mright - mleft, mbottom - mtop
            
            draw.text((padding + x, y), g_uri, fill="white")
            draw.text((0, 16), g_time, fill="white")
            draw.text((device.width - mwidth, 16), g_mem, fill="white")
        x -= 1
        if (x < -width - padding):
            padding = device.width
            x = 0
        if curr_uri != g_uri:
            curr_uri = g_uri
            x = 0
            padding = 0
        pos = (x, y)


def handle_sigint(signum, frame):
    os._exit(1)


def main():
    signal.signal(signal.SIGINT, handle_sigint)

    parser = argparse.ArgumentParser(description="odroid-player oled client")
    parser.add_argument("--log-level", type=int, default=6,
                        help="set log level to the client. " +
                             "0 is the most verbose and 6 is to supress message"
                       )
    args = parser.parse_args()

    try:
        client = opclient.op_client()
        client.set_log_level(args.log_level)
        client.connect()
        client.prepare()

        device = luma_oled.get_device()
        if not device:
            raise RuntimeError("Couldn't get device")

        t1 = threading.Thread(target=update_time, args=(client,), daemon=True)
        t2 = threading.Thread(target=receive_message, args=(client,), daemon=True)
        t3 = threading.Thread(target=update_ram, daemon=True)

        t1.start()
        t2.start()
        t3.start()

        display(device, client)

    except Exception as e:
        print(f"Exception occured while running program: {e}")
    except KeyboardInterrupt:
        print("Interrupt")
    finally:
        client.destroy()
        print("client destroyed")

if __name__ == "__main__":
    main()


