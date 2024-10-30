#!/usr/bin/bash

cd ..

meson build

ninja -C build

cd build/dma_copy

./doca_dma_copy -p 03:00.0 -r b5:00.0 -f 4096 -o 40000 -l 20 -t 128