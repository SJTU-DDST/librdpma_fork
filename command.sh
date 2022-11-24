#!/usr/bin/bash
sudo ./scripts/nvm_server --host=localhost --port=$1 -use_nvm=$2 -touch_mem=true --nvm_sz=$3 --nvm_file=/dev/dax3.0
