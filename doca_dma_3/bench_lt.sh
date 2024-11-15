#!/bin/bash

REMOTE_USER="yiyang"
REMOTE_IP="192.168.98.75"
REMOTE_DIR="/home/yiyang/librdpma_fork/doca_dma_3"
HOST_PROGRAM="./build/doca_dma_dpu/doca_dma_copy_dpu"
REMOTE_PROGRAM="./build/doca_dma_host/doca_dma_copy_host"
SSH_PASS="1111"
OUTPUT_JSON="result.json"
BENCHMARK_FILE_NAME="bench"
_GROUPS=6
_RUNS=7
OPS=10000000
INIT_PAYLOAD=256
NUM_WORK_TASKS=1
THREADS=16

num=1

while [[ -f "${BENCHMARK_FILE_NAME}_${num}.json" ]]
do
    ((num++))
done

BENCHMARK_FILE="${BENCHMARK_FILE_NAME}_${num}.json"

echo -n "[" > $BENCHMARK_FILE

for j in $(seq 1 $_GROUPS)
do

    PAYLOAD=$INIT_PAYLOAD

    echo -n "[" >> $BENCHMARK_FILE

    for i in $(seq 1 $_RUNS) 
    do
        echo "Running program on host for payload size: $PAYLOAD B, operations: $OPS, number of working tasks: $NUM_WORK_TASKS"
        sshpass -p "$SSH_PASS" ssh ${REMOTE_USER}@${REMOTE_IP} "cd ${REMOTE_DIR} && $REMOTE_PROGRAM -p b5:00.0 -f $PAYLOAD -t $THREADS -l 10" & 

        sleep 1

        echo "Copying metadata"
        sshpass -p "$SSH_PASS" scp yiyang@192.168.98.75:/home/yiyang/librdpma_fork/doca_dma_3/\{export_desc_*.txt,buffer_info_*.txt\} .

        echo "Running program on dpu"
        $HOST_PROGRAM -p 03:00.0 -f $PAYLOAD -o $OPS -w $NUM_WORK_TASKS -t $THREADS -l 10 

        cat $OUTPUT_JSON >> $BENCHMARK_FILE

        if [ $i -ne $_RUNS ]; then
            echo "," >> $BENCHMARK_FILE
        fi

        PAYLOAD=$((PAYLOAD * 2))
    done

    echo "]" >> $BENCHMARK_FILE

    if [ $j -ne $_GROUPS ]; then
        echo "," >> $BENCHMARK_FILE
    fi

    NUM_WORK_TASKS=$((NUM_WORK_TASKS * 2))

done

echo "]" >> $BENCHMARK_FILE

echo "Benchmark complete. Results saved in $BENCHMARK_FILE"