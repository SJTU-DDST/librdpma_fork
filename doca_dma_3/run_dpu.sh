#!/usr/bin/bash

ninja -C build

./build/doca_dma_dpu/doca_dma_copy_dpu -p 03:00.0 -f 4096 -w 16 -o 3000000 -t 16 -l 70