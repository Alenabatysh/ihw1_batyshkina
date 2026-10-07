echo "|========================================================|"
echo "|           SUPERMARKET SIMULATION TEST RUN              |"
echo "|========================================================|"
echo ""

echo "[COMPILE] Building project..."
make clean > /dev/null 2>&1
make > /dev/null 2>&1
if [ $? -ne 0 ]; then
    echo "[ERROR] Compilation failed"
    exit 1
fi
echo "[OK] Compilation successful"
echo ""

PASSED=0
FAILED=0
TOTAL=0

run_test() {
    local config=$1
    local description=$2
    TOTAL=$((TOTAL + 1))

    printf "Test %2d: %-40s " "$TOTAL" "$description"

    ./supermarket "$config" > /tmp/test_output.txt 2>&1
    local exit_code=$?

    if [ $exit_code -ne 0 ]; then
        echo "[FAIL] exit code: $exit_code"
        FAILED=$((FAILED + 1))
        return
    fi

    if [ ! -f simulation.log ]; then
        echo "[FAIL] simulation.log not created"
        FAILED=$((FAILED + 1))
        return
    fi

    if grep -q "Conservation law verified" /tmp/test_output.txt; then
        echo "[PASS]"
        PASSED=$((PASSED + 1))
    else
        echo "[FAIL] conservation law violated"
        FAILED=$((FAILED + 1))
    fi

    rm -f simulation.log
}

run_negative_test() {
    local config=$1
    local description=$2
    TOTAL=$((TOTAL + 1))

    printf "Test %2d: %-40s " "$TOTAL" "$description"

    ./supermarket "$config" > /tmp/test_output.txt 2>&1
    local exit_code=$?

    if [ $exit_code -ne 0 ]; then
        echo "[PASS] (expected failure)"
        PASSED=$((PASSED + 1))
    elif grep -q "ERROR" /tmp/test_output.txt; then
        echo "[PASS] (error detected)"
        PASSED=$((PASSED + 1))
    else
        echo "[FAIL] should have failed but succeeded"
        FAILED=$((FAILED + 1))
    fi

    rm -f simulation.log
}

echo "[RUN] Executing test suite..."
echo ""

echo "--- Positive tests (should succeed) ---"
run_test "tests/test_small.txt" "Small simulation (10 customers)"
run_test "tests/test_medium.txt" "Medium simulation (50 customers)"
run_test "tests/test_large.txt" "Large simulation (200 customers)"
run_test "tests/test_strategy_first.txt" "Strategy: FIRST"
run_test "tests/test_strategy_random.txt" "Strategy: RANDOM"
run_test "tests/test_edge_small_queue.txt" "Edge case: queue capacity = 2"
run_test "tests/test_edge_capacity_1.txt" "Edge case: queue capacity = 1"
run_test "tests/test_edge_one_customer.txt" "Edge case: one customer"
run_test "tests/test_large_values.txt" "Stress test: large values"
run_test "tests/test_whitespace.txt" "Whitespace handling"
run_test "tests/test_partial_config.txt" "Partial configuration"
run_test "tests/test_empty_file.txt" "Empty file (use defaults)"
run_test "tests/test_comments_only.txt" "Comments only (use defaults)"

echo ""
echo "--- Negative tests (should fail gracefully) ---"
run_negative_test "tests/test_invalid_negative.txt" "Invalid: negative values"
run_negative_test "tests/test_invalid_zero.txt" "Invalid: zero customers"
run_negative_test "tests/test_invalid_range.txt" "Invalid: min > max"
run_negative_test "tests/test_invalid_strategy.txt" "Invalid: strategy = 5"

TOTAL=$((TOTAL + 1))
printf "Test %2d: %-40s " "$TOTAL" "Missing config file"
./supermarket "tests/nonexistent.txt" > /tmp/test_output.txt 2>&1
if [ $? -ne 0 ] || grep -q "ERROR\|Failed" /tmp/test_output.txt; then
    echo "[PASS] (handled gracefully)"
    PASSED=$((PASSED + 1))
else
    echo "[FAIL] should have reported error"
    FAILED=$((FAILED + 1))
fi

echo ""
echo "|========================================================|"
echo "|                    TEST RESULTS                        |"
echo "|========================================================|"
printf "| Passed: %-4d Failed: %-4d Total: %-4d                  |\n" "$PASSED" "$FAILED" "$TOTAL"
echo "|========================================================|"
echo ""

if [ $FAILED -eq 0 ]; then
    echo "[RESULT] All tests passed"
    exit 0
else
    echo "[RESULT] Some tests failed"
    exit 1
fi