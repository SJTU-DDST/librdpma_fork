#!/bin/bash

REMOTE_USER="yiyang"
REMOTE_IP="192.168.98.75"
REMOTE_DIR="/home/yiyang/librdpma_fork/doca_dma_3"
HOST_PROGRAM="./build/doca_dma_dpu/doca_dma_copy_dpu"
REMOTE_PROGRAM="./build/doca_dma_host/doca_dma_copy_host"
SSH_PASS="1111"
OUTPUT_JSON="result.json"
BENCHMARK_FILE_NAME="bench"
_GROUPS=1
_RUNS=10
OPS=1000000
INIT_PAYLOAD=64
NUM_WORK_TASKS=8
THREADS=8

num=1

while [[ -f "${BENCHMARK_FILE_NAME}_${num}.json" ]]
do
    ((num++))
done

BENCHMARK_FILE="${BENCHMARK_FILE_NAME}_${num}.json"

# cleanup() {
#     echo "Killing local and remote processes."
    
#     if [ ! -z "$HOST_PID" ]; then
#         kill $HOST_PID 2>/dev/null
#         echo "Killed local program with PID: $HOST_PID"
#     fi

#     if [ ! -z "$REMOTE_SSH_PID" ]; then
#         sshpass -p "$SSH_PASS" ssh ${REMOTE_USER}@${REMOTE_IP} "pkill -f $REMOTE_PROGRAM" 2>/dev/null
#         echo "Killed remote program on $REMOTE_IP"
#     fi

#     exit 1
# }

# trap cleanup SIGINT

echo -n "[" > $BENCHMARK_FILE

for j in $(seq 1 $_GROUPS)
do

    PAYLOAD=$INIT_PAYLOAD
    # NUM_WORK_TASKS=$INIT_NUM_WORK_TASKS
    # THREADS=$INIT_THREADS

    echo -n "[" >> $BENCHMARK_FILE

    for i in $(seq 1 $_RUNS) 
    do
        echo "Running program on host for payload size: $PAYLOAD B, operations: $OPS, number of working tasks: $NUM_WORK_TASKS"
        sshpass -p "$SSH_PASS" ssh ${REMOTE_USER}@${REMOTE_IP} "cd ${REMOTE_DIR} && $REMOTE_PROGRAM -p b5:00.0 -f $PAYLOAD -t $THREADS -l 10" &

        sleep 1

        echo "Copying metadata"
        sshpass -p "$SSH_PASS" scp yiyang@192.168.98.75:/home/yiyang/librdpma_fork/doca_dma_3/\{export_desc_*.txt,buffer_info_*.txt\} .

        echo "Running program on dpu"
        $HOST_PROGRAM -p 03:00.0 -f $PAYLOAD -o $OPS -w $NUM_WORK_TASKS -t $THREADS -l 10 &
        DPU_PID=$!

        wait $DPU_PID

        cat $OUTPUT_JSON >> $BENCHMARK_FILE

        if [ $i -ne $_RUNS ]; then
            echo "," >> $BENCHMARK_FILE
        fi

        PAYLOAD=$((PAYLOAD * 2))
        # THREADS=$((THREADS * 2))
        # NUM_WORK_TASKS=$((NUM_WORK_TASKS * 2))
    done

    echo "]" >> $BENCHMARK_FILE

    if [ $j -ne $_GROUPS ]; then
        echo "," >> $BENCHMARK_FILE
    fi

    # THREADS=$((THREADS * 2))
    NUM_WORK_TASKS=$((NUM_WORK_TASKS * 4))

done

echo "]" >> $BENCHMARK_FILE

echo "Benchmark complete. Results saved in $BENCHMARK_FILE"