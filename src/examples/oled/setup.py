# SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
# SPDX-License-Identifier: Apache-2.0

from setuptools import setup, find_packages

setup(
    name='odroid-player-client-oled',
    version='1.0',
    description='odroid-player-client oled interface',
    author='phillip.choi',
    packages=find_packages(include=['modules', 'modules.*']),
    py_modules=['odroid_player_client_oled'],
    install_requires=[
        line.strip()
        for line in open("requirements.txt").readlines()
        if line.strip() and not line.startswith('#')
    ],
    entry_points={
        'console_scripts': [
            'odroid-player-client-oled = odroid_player_client_oled:main'
        ]
    },
)

