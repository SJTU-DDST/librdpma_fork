#!/bin/bash

REMOTE_USER="yiyang"
REMOTE_IP="192.168.98.74"
REMOTE_DIR="/home/yiyang/doca/applications/build/dma_copy"
HOST_PROGRAM="../build/dma_copy/doca_dma_copy"
REMOTE_PROGRAM="./doca_dma_copy"
SSH_PASS="1111"
OUTPUT_JSON="result.json"
BENCHMARK_FILE="benchmark_results_latency.json"
_RUNS=64
FILE_SIZES=2097152
OPS=20000

cleanup() {
    echo "Killing local and remote processes."
    
    if [ ! -z "$HOST_PID" ]; then
        kill $HOST_PID 2>/dev/null
        echo "Killed local program with PID: $HOST_PID"
    fi

    if [ ! -z "$REMOTE_SSH_PID" ]; then
        sshpass -p "$SSH_PASS" ssh ${REMOTE_USER}@${REMOTE_IP} "pkill -f $REMOTE_PROGRAM" 2>/dev/null
        echo "Killed remote program on $REMOTE_IP"
    fi

    exit 1
}

trap cleanup SIGINT

echo -n "[" >> $BENCHMARK_FILE

for i in $(seq 1 $_RUNS) 
do
    echo "Running program on host for file size: $FILE_SIZES B, operations: $OPS $j"
    $HOST_PROGRAM -p 03:00.0 -r ca:00.0 -f $FILE_SIZES -o $OPS -l 10 &
    HOST_PID=$!

    echo "Running program on remote machine"
    sshpass -p "$SSH_PASS" ssh ${REMOTE_USER}@${REMOTE_IP} "bash -c 'cd ${REMOTE_DIR} && $REMOTE_PROGRAM -p ca:00.0 -f $FILE_SIZES -o $OPS -l 10'" &
    REMOTE_SSH_PID=$!

    wait $HOST_PID
    wait $REMOTE_SSH_PID

    cat $OUTPUT_JSON >> $BENCHMARK_FILE

    if [ $i -ne $_RUNS ]; then
        echo "," >> $BENCHMARK_FILE
    fi

    FILE_SIZES=$((FILE_SIZES - 32 * 1024))
done

echo "]" >> $BENCHMARK_FILE


echo "Benchmark complete. Results saved in $BENCHMARK_FILE"