#!/usr/bin/bash

rm -rf build

rm *.txt

meson build

ninja -C build

./build/src/host/host

sshpass -p "1111" scp *.txt yiyang@192.168.98.114:/home/yiyang/librdpma_fork/levelkv