#!/bin/bash

CONFIG=${1:-config.txt}

cleanup_on_exit() {
    echo -e "\n[run.sh] Interrupted. Cleaning up processes..."
    kill $DISPLAY_PID 2>/dev/null
    kill $CASHIER0_PID 2>/dev/null
    kill $CASHIER1_PID 2>/dev/null
    kill $GENERATOR_PID 2>/dev/null
    
    wait $DISPLAY_PID 2>/dev/null
    wait $CASHIER0_PID 2>/dev/null
    wait $CASHIER1_PID 2>/dev/null
    wait $GENERATOR_PID 2>/dev/null
    
    ./supermarket_named cleanup
    exit 1
}

trap cleanup_on_exit INT TERM

./supermarket_named init "$CONFIG"
if [ $? -ne 0 ]; then
    echo "Init failed (invalid config)"
    exit 1
fi

./supermarket_named display &
DISPLAY_PID=$!

./supermarket_named cashier 0 &
CASHIER0_PID=$!

./supermarket_named cashier 1 &
CASHIER1_PID=$!

./supermarket_named generator &
GENERATOR_PID=$!

wait $GENERATOR_PID
wait $CASHIER0_PID
wait $CASHIER1_PID

kill $DISPLAY_PID 2>/dev/null
wait $DISPLAY_PID 2>/dev/null

./supermarket_named cleanup