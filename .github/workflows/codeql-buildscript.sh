#!/usr/bin/env bash

sudo apt-get -y install gcc-arm-none-eabi
cd software/build
cmake ..
make
