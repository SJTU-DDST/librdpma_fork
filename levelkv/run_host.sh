#!/usr/bin/bash

rm -rf build

rm *.txt

meson build

ninja -C build

./build/src/host/host