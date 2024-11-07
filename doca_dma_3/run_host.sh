#!/usr/bin/bash

ninja -C build

./build/doca_dma_host/doca_dma_copy_host -p b5:00.0 -f 512 -t 16 -l 70 &

sleep 1

sshpass -p '1111' scp buffer_info_*.txt export_desc_*.txt yiyang@192.168.98.114:/home/yiyang/librdpma_fork/doca_dma_3
