#!/usr/bin/bash
./scripts/nvm_client -addr="localhost:$1" --coros=8  --id=0 --use_nic_idx=0 --use_read=$2 --payload=$3 --add_sync=false --address_space=2 --random=true