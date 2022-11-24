#!/usr/bin/bash
sudo ./scripts/nvm_rrtclient -addr="localhost:$1" -payload=$2 -threads=$3 -use_read=$4