#!/usr/bin/bash

rm *.txt

ninja -C build

./build/src/host/host