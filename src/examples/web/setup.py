# SPDX-FileCopyrightText: 2025 Phillip Choi for Hardkernel
# SPDX-License-Identifier: Apache-2.0

from setuptools import setup, find_packages

setup(
    name='odroid-player-client-web',
    version='1.0',
    description='odroid-player-client web interface',
    author='phillip.choi',
    packages=find_packages(include=['modules', 'modules.*']),
    include_package_data=True,
    py_modules=['odroid_player_client_web'],
    install_requires=[
        line.strip()
        for line in open("requirements.txt").readlines()
        if line.strip() and not line.startswith('#')
    ],
    entry_points={
        'console_scripts': [
            'odroid-player-client-web = odroid_player_client_web:main'
        ]
    },
)

