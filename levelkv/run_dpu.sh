#!/usr/bin/bash

rm -rf build

meson build

ninja -C build

./build/src/dpu/dpu