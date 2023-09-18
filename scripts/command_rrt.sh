#!/usr/bin/bash
sudo ./nvm_rrtserver --port=$1 --nvm_sz=$2 --nvm_file=/dev/dax12.0 -non_null=true --threads=$3 --use_read=$4 --use_nvm=$5 --clflush=false
