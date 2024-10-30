#!/bin/bash

REMOTE_USER="yiyang"
REMOTE_IP="192.168.98.75"
REMOTE_DIR="/home/yiyang/doca/applications/build/dma_copy"
HOST_PROGRAM="../build/dma_copy/doca_dma_copy"
REMOTE_PROGRAM="./doca_dma_copy"
SSH_PASS="1111"
OUTPUT_JSON="result.json"
BENCHMARK_FILE_NAME="bench_bw_pl_threads"
_GROUPS=7
_RUNS=64
THREADS=1
OPS=40000
INIT_PAYLOAD=$((1024 * 1024 * 2))

num=1

while [[ -f "${BENCHMARK_FILE_NAME}_${num}.json" ]]
do
    ((num++))
done

BENCHMARK_FILE="${BENCHMARK_FILE_NAME}_${num}.json"

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

echo -n "[" > $BENCHMARK_FILE

for j in $(seq 1 $_GROUPS)
do

    PAYLOAD=$INIT_PAYLOAD

    echo -n "[" >> $BENCHMARK_FILE

    for i in $(seq 1 $_RUNS) 
    do
        echo "Running program on host for payload size: $PAYLOAD B, operations: $OPS, groups: $j"
        $HOST_PROGRAM -p 03:00.0 -r b5:00.0 -f $PAYLOAD -o $OPS -t $THREADS -l 10 &
        HOST_PID=$!

        echo "Running program on remote machine"
        sshpass -p "$SSH_PASS" ssh ${REMOTE_USER}@${REMOTE_IP} "bash -c 'cd ${REMOTE_DIR} && $REMOTE_PROGRAM -p b5:00.0 -f $PAYLOAD -o $OPS -t $THREADS -l 10'" &
        REMOTE_SSH_PID=$!

        wait $HOST_PID
        wait $REMOTE_SSH_PID

        cat $OUTPUT_JSON >> $BENCHMARK_FILE

        if [ $i -ne $_RUNS ]; then
            echo "," >> $BENCHMARK_FILE
        fi

        PAYLOAD=$((PAYLOAD - 32 * 1024))
    done

    echo "]" >> $BENCHMARK_FILE

    if [ $j -ne $_GROUPS ]; then
        echo "," >> $BENCHMARK_FILE
    fi

    THREADS=$((THREADS * 2))

done

echo "]" >> $BENCHMARK_FILE

echo "Benchmark complete. Results saved in $BENCHMARK_FILE"
