#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR=$(cd "$(dirname "$0")/.." && pwd)
cd "$ROOT_DIR"

PORT=${PORT:-$((22000 + ($$ % 20000)))}
TMP_DIR=$(mktemp -d)
SERVER_PID=""
PUBLISHER_PID=""
CASHIER0_PID=""
CASHIER1_PID=""
GENERATOR_PID=""
CLIENT1_PID=""
CLIENT2_PID=""
CLIENT3_PID=""
STOPPING=0

stop_all() {
    if [[ "$STOPPING" -eq 1 ]]; then return; fi
    STOPPING=1
    local pid
    for pid in "$CLIENT1_PID" "$CLIENT2_PID" "$CLIENT3_PID" "$GENERATOR_PID" "$CASHIER0_PID" "$CASHIER1_PID" "$PUBLISHER_PID" "$SERVER_PID"; do
        if [[ -n "$pid" ]]; then
            kill -INT "$pid" 2>/dev/null || true
        fi
    done
    for pid in "$CLIENT1_PID" "$CLIENT2_PID" "$CLIENT3_PID" "$GENERATOR_PID" "$CASHIER0_PID" "$CASHIER1_PID" "$PUBLISHER_PID" "$SERVER_PID"; do
        if [[ -n "$pid" ]]; then
            wait "$pid" 2>/dev/null || true
        fi
    done
    ./supermarket_named cleanup >/dev/null 2>&1 || true
    rm -rf "$TMP_DIR"
}
trap stop_all EXIT INT TERM

make -s all
./supermarket_named init tests/test_network_dynamic.txt
./supermarket_server 127.0.0.1 "$PORT" >"$TMP_DIR/server.out" 2>"$TMP_DIR/server.err" &
SERVER_PID=$!
sleep 0.2

./supermarket_named publisher 127.0.0.1 "$PORT" >"$TMP_DIR/publisher.out" 2>"$TMP_DIR/publisher.err" &
PUBLISHER_PID=$!
./supermarket_named cashier 0 >"$TMP_DIR/cashier0.out" 2>&1 &
CASHIER0_PID=$!
./supermarket_named cashier 1 >"$TMP_DIR/cashier1.out" 2>&1 &
CASHIER1_PID=$!
./supermarket_named generator >"$TMP_DIR/generator.out" 2>&1 &
GENERATOR_PID=$!

# Observer #1 disconnects deliberately while the simulation is active.
./supermarket_client 127.0.0.1 "$PORT" >"$TMP_DIR/client1.out" 2>"$TMP_DIR/client1.err" &
CLIENT1_PID=$!
sleep 0.4
kill -INT "$CLIENT1_PID" 2>/dev/null || true
wait "$CLIENT1_PID" 2>/dev/null || true
CLIENT1_PID=""

# Two new independent observer processes attach while the same simulation
# continues. They must both receive the final state.
./supermarket_client 127.0.0.1 "$PORT" >"$TMP_DIR/client2.out" 2>"$TMP_DIR/client2.err" &
CLIENT2_PID=$!
./supermarket_client 127.0.0.1 "$PORT" >"$TMP_DIR/client3.out" 2>"$TMP_DIR/client3.err" &
CLIENT3_PID=$!

echo "[network-test] waiting for generator"
wait "$GENERATOR_PID"
GENERATOR_PID=""
echo "[network-test] generator finished"
wait "$CASHIER0_PID"
CASHIER0_PID=""
echo "[network-test] cashier 0 finished"
wait "$CASHIER1_PID"
CASHIER1_PID=""
echo "[network-test] cashier 1 finished"
wait "$PUBLISHER_PID"
PUBLISHER_PID=""
echo "[network-test] publisher finished"
wait "$CLIENT2_PID"
CLIENT2_PID=""
echo "[network-test] observer 2 received final snapshot"
wait "$CLIENT3_PID"
CLIENT3_PID=""
echo "[network-test] observer 3 received final snapshot"

./supermarket_named cleanup >"$TMP_DIR/cleanup.out"
if ! grep -q "Conservation law verified" "$TMP_DIR/cleanup.out"; then
    echo "FAIL: conservation law was not verified"
    cat "$TMP_DIR/cleanup.out"
    exit 1
fi
if ! grep -q "Simulation complete" "$TMP_DIR/client2.out"; then
    echo "FAIL: observer #2 did not receive final snapshot"
    exit 1
fi
if ! grep -q "Simulation complete" "$TMP_DIR/client3.out"; then
    echo "FAIL: observer #3 did not receive final snapshot"
    exit 1
fi
if ! grep -q "observer connected" "$TMP_DIR/server.err"; then
    echo "FAIL: server did not register observers"
    exit 1
fi

kill -INT "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true
SERVER_PID=""

printf "PASS: publisher, two simultaneous observers, disconnect/reconnect, final snapshot and conservation law.\n"
