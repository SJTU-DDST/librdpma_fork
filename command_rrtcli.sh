o!/usr/bin/bash
sudo ./scripts/nvm_rrtclient -addr="192.168.98.74:$1" -payload=$2 -threads=$3 -use_read=$4 -coros=4 -id=1 -round_up=true -round_payload=256
