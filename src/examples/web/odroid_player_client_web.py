#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
# SPDX-License-Identifier: Apache-2.0

from flask import Flask, render_template, request, jsonify
from flask_socketio import SocketIO
from engineio.async_drivers import threading
from modules import opclient
import argparse
import os.path
import socket
import signal
import sys
import _thread
import time


PROGRAM = "odroid-player-client-web"
app = Flask(__name__, template_folder=os.path.join(os.path.dirname(__file__), 'modules', 'templates'))
app.config['TEMPLATES_AUTO_RELOAD'] = True

socketio = SocketIO(app, async_mode="threading", cors_allowed_origins="*")
g_client = None


def update_time(client):
    try:
        while True:
            try:
                status = client.recv_time()
            except Exception:
                print("connecting stream...")
                time.sleep(1)
                continue

            data, n_data = client.status_get_data(status)
            if n_data != 2:
                raise RuntimeError("Invalid stream return status")

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
                    socketio.emit("update_time", {"current_time": data.decode()})
                    time.sleep(0.01)
                except socket.timeout:
                    break
                except Exception:
                    raise
    except Exception as e:
        print(f"{PROGRAM}: Exception occured while reading stream: {e}")
        _thread.interrupt_main()


def receive_message(client):
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
                    socketio.emit("message", {"message": message})
                elif cmd_status == "RECONNT":
                    print("NOTIFY: Server reconnected")
                    socketio.emit("message", {"message": "reconnected"})

            elif cmd_type == "BROADCAST":
                if cmd_cmd == "URI" and cmd_status == "BRDINFO":
                    data, _ = client.status_get_data(status)
                    uri = data[0].decode()
                    print(f"BROADCAST: URI update: {uri}")
                    socketio.emit("update_uri", {"uri": uri})
                else:
                    data, _ = client.status_get_data(status)
                    message = data[0].decode()
                    print(f"BROADCAST: {message}")
                    socketio.emit("broadcast", {"message": message})

            client.free_status(status)

    except Exception as e:
        print(f"{PROGRAM}: Exception occured while reading message: {e}")
        _thread.interrupt_main()


@app.route("/")
def index():
    return render_template("index.html")


@app.route("/control", methods=["POST"])
def control():
    try:
        command = request.json.get("command")
        uri = None

        if command == "play":
            status = g_client.send_play()
            print(f"status: {g_client.status_get_status(status)}")
            g_client.free_status(status)
        elif command == "pause":
            status = g_client.send_pause()
            print(f"status: {g_client.status_get_status(status)}")
            g_client.free_status(status)
        elif command == "next":
            status = g_client.send_next()
            print(f"status: {g_client.status_get_status(status)}")
            g_client.free_status(status)

            status = g_client.recv_uri()
            data, _ = g_client.status_get_data(status)
            uri = data[0].decode()
            g_client.free_status(status)
        elif command == "prev":
            status = g_client.send_prev()
            print(f"status: {g_client.status_get_status(status)}")
            g_client.free_status(status)

            status = g_client.recv_uri()
            data, _ = g_client.status_get_data(status)
            uri = data[0].decode()
            g_client.free_status(status)
        elif command == "uri":
            status = g_client.recv_uri()
            data, _ = g_client.status_get_data(status)
            uri = data[0].decode()
            g_client.free_status(status)
        else:
            return jsonify({"status": "error", 
                            "message": "Invalid command"}), 400

        return jsonify({"status": "success",
                        "message": f"Command {command} executed",
                        "uri": uri})
    except Exception as e:
            return jsonify({"status": "error", 
                            "message": f"Exception occured: {e}"}), 500


def handle_sigint(signum, frame):
    os._exit(1)


def main():
    global g_client

    signal.signal(signal.SIGINT, handle_sigint)

    parser = argparse.ArgumentParser(description="odroid-player web client")
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

        print(client)

        g_client = client

        socketio.start_background_task(update_time, client)
        socketio.start_background_task(receive_message, client)
        
        socketio.run(app, host="0.0.0.0", port=5000, debug=False)

    except Exception as e:
        print(f"{PROGRAM}: Exception occured while running program: {e}")
    except KeyboardInterrupt:
        pass # interrupt from threads
    finally:
        client.destroy()
        print("client destroyed")


if __name__ == "__main__":
    main()


