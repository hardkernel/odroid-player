#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# Copyright (c) 2014-2022 Richard Hull and contributors
# Copyright (C) 2025 Phillip Choi for Hardkernel.

import sys
import threading
import time
from luma.core import cmdline, error

def get_device(args=None):
    if args is None:
        args = sys.argv[1:]
    parser = cmdline.create_parser(description="luma arguments")
    args = parser.parse_args(args)

    # pre-defined args on odroid-c5
    args.display = "ssd1306"
    args.width = 128
    args.height = 32
    args.i2c_port = 0
    args.i2c_address = 0x3c

    try:
        device = cmdline.create_device(args)
        return device
    except error.Error as e:
        parser.error(e)
        return None


