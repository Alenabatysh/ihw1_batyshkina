#!/usr/bin/env bash
set -u

CONFIG=${1:-config.txt}
BIND_IP=${BIND_IP:-0.0.0.0}
SERVER_IP=${SERVER_IP:-127.0.0.1}
PORT=${PORT:-5555}

SERVER_PID=""
PUBLISHER_PID=""
CASHIER0_PID=""
CASHIER1_PID=""
GENERATOR_PID=""
CLIENT_PID=""
CLEANED=0
STOPPING=0

cleanup_shared_resources() {
    if [[ "$CLEANED" -eq 0 ]]; then
        ./supermarket_named cleanup || true
        CLEANED=1
    fi
}

stop_processes() {
    local pid
    for pid in "$CLIENT_PID" "$GENERATOR_PID" "$CASHIER0_PID" "$CASHIER1_PID" "$PUBLISHER_PID"; do
        if [[ -n "$pid" ]]; then
            kill -INT "$pid" 2>/dev/null || true
        fi
    done
    if [[ -n "$SERVER_PID" ]]; then
        kill -INT "$SERVER_PID" 2>/dev/null || true
    fi
    for pid in "$CLIENT_PID" "$GENERATOR_PID" "$CASHIER0_PID" "$CASHIER1_PID" "$PUBLISHER_PID" "$SERVER_PID"; do
        if [[ -n "$pid" ]]; then
            wait "$pid" 2>/dev/null || true
        fi
    done
}

on_exit() {
    if [[ "$STOPPING" -eq 1 ]]; then
        return
    fi
    STOPPING=1
    echo -e "\n[run_network.sh] Stopping networked simulation..."
    stop_processes
    cleanup_shared_resources
}
trap on_exit INT TERM

if [[ ! -x ./supermarket_named || ! -x ./supermarket_server || ! -x ./supermarket_client ]]; then
    echo "Binaries are missing. Run 'make all' first."
    exit 1
fi

./supermarket_named init "$CONFIG" || {
    echo "Initialization failed. Check configuration: $CONFIG"
    exit 1
}

./supermarket_server "$BIND_IP" "$PORT" &
SERVER_PID=$!
sleep 0.2

./supermarket_named publisher "$SERVER_IP" "$PORT" &
PUBLISHER_PID=$!
./supermarket_named cashier 0 &
CASHIER0_PID=$!
./supermarket_named cashier 1 &
CASHIER1_PID=$!
./supermarket_named generator &
GENERATOR_PID=$!

# This is just one observer. More observers can be started from other
# terminals/computers with: ./supermarket_client <server_ip> <port>
./supermarket_client "$SERVER_IP" "$PORT" &
CLIENT_PID=$!

echo ""
echo "Networked supermarket started."
echo "Server bind address: $BIND_IP:$PORT"
echo "Publisher destination: $SERVER_IP:$PORT"
echo "To attach another observer: ./supermarket_client <server_ip> $PORT"
echo ""

wait "$GENERATOR_PID" || true
GENERATOR_PID=""
wait "$CASHIER0_PID" || true
CASHIER0_PID=""
wait "$CASHIER1_PID" || true
CASHIER1_PID=""
wait "$PUBLISHER_PID" || true
PUBLISHER_PID=""
wait "$CLIENT_PID" || true
CLIENT_PID=""

cleanup_shared_resources

echo "Simulation completed. The TCP server stays available with the last snapshot."
echo "You can reconnect observers; press Ctrl+C here to stop the server."
wait "$SERVER_PID" || true
