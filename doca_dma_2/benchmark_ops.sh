#!/bin/bash

REMOTE_USER="yiyang"
REMOTE_IP="192.168.98.74"
REMOTE_DIR="/home/yiyang/doca/applications/build/dma_copy"
HOST_PROGRAM="../build/dma_copy/doca_dma_copy"
REMOTE_PROGRAM="./doca_dma_copy"
SSH_PASS="1111"
OUTPUT_JSON="result.json"
BENCHMARK_FILE="benchmark_results_ops.json"
RUNS=10
OPS=200000

echo -n "[" > $BENCHMARK_FILE

for i in $(seq 1 $RUNS); do
    echo "Running program on host for number of operations: $OPS"
    $HOST_PROGRAM -p 03:00.0 -r ca:00.0 -f 4096 -o $OPS -l 10 &
    HOST_PID=$!

    echo "Running program on remote machine"
    sshpass -p "$SSH_PASS" ssh ${REMOTE_USER}@${REMOTE_IP} "cd ${REMOTE_DIR} && $REMOTE_PROGRAM -p ca:00.0 -f 4096 -o $OPS -l 10" &
    REMOTE_PID=$!

    wait $HOST_PID
    wait $REMOTE_PID

    if [ $i -ne 1 ]; then
        echo "," >> $BENCHMARK_FILE
    fi

    cat $OUTPUT_JSON >> $BENCHMARK_FILE

    OPS=$((OPS - 20000))
done

echo "]" >> $BENCHMARK_FILE

echo "Benchmark complete. Results saved in $BENCHMARK_FILE"