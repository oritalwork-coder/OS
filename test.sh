#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_test() {
    echo -e "${BLUE}[TEST]${NC} $1"
}

print_pass() {
    echo -e "${GREEN}[PASS]${NC} $1"
}

print_fail() {
    echo -e "${RED}[FAIL]${NC} $1"
}

print_info() {
    echo -e "${YELLOW}[INFO]${NC} $1"
}

# First, build the project
print_test "Building the project..."
./build.sh || {
    print_fail "Build failed. Cannot run tests."
    exit 1
}

echo ""
print_test "Starting pipeline system tests..."
echo ""

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Set to 1 to see debug logs, 0 to hide them
DEBUG_MODE=1

# Function to run a test
run_test() {
    local test_name="$1"
    local input="$2"
    local pipeline="$3"
    local expected="$4"
    
    print_test "Running: $test_name"
    print_info "Pipeline: $pipeline"
    print_info "Input:    '$input'"
    
    # Run the test and capture output
    if [ $DEBUG_MODE -eq 1 ]; then
        # Show logs - useful for debugging
        echo -e "${YELLOW}--- Debug Output ---${NC}"
        # Filter out INFO/ERROR lines and get only the actual logger output
        actual=$(echo -e "${input}\n<END>" | ./output/analyzer $pipeline 2>&1 | tee /dev/stderr | grep "^\[logger\]" | grep -v "^\[INFO\]" | grep -v "^\[ERROR\]" | head -n 1)
        echo -e "${YELLOW}--- End Debug ---${NC}"
    else
        # Hide logs - clean test output
        actual=$(echo -e "${input}\n<END>" | ./output/analyzer $pipeline 2>/dev/null | grep "^\[logger\]" | head -n 1)
    fi
    
    if [ "$actual" = "$expected" ]; then
        print_pass "$test_name"
        echo -e "${GREEN}  Expected:${NC} '$expected'"
        echo -e "${GREEN}  Got:     ${NC} '$actual'"
        ((TESTS_PASSED++))
    else
        print_fail "$test_name"
        echo -e "${RED}  Expected:${NC} '$expected'"
        echo -e "${RED}  Got:     ${NC} '$actual'"
        ((TESTS_FAILED++))
    fi
    echo ""
}

# Test 1: Basic uppercaser
run_test "Basic Uppercaser" \
    "hello" \
    "10 uppercaser logger" \
    "[logger] HELLO"

# Test 2: Basic rotator
run_test "Basic Rotator" \
    "hello" \
    "10 rotator logger" \
    "[logger] ohell"

# Test 3: Basic flipper
run_test "Basic Flipper" \
    "hello" \
    "10 flipper logger" \
    "[logger] olleh"

# Test 4: Basic expander
run_test "Basic Expander" \
    "abc" \
    "10 expander logger" \
    "[logger] a b c"

# Test 5: Uppercaser + Rotator chain
run_test "Uppercaser + Rotator" \
    "hello" \
    "10 uppercaser rotator logger" \
    "[logger] OHELL"

# Test 6: Uppercaser + Flipper chain
run_test "Uppercaser + Flipper" \
    "abc" \
    "10 uppercaser flipper logger" \
    "[logger] CBA"

# Test 7: Rotator + Flipper chain
run_test "Rotator + Flipper" \
    "hello" \
    "10 rotator flipper logger" \
    "[logger] lleho"

# Test 8: Empty string
run_test "Empty String" \
    "" \
    "10 uppercaser logger" \
    "[logger] "

# Test 9: Single character
run_test "Single Character" \
    "x" \
    "10 rotator logger" \
    "[logger] x"

# Test 10: Numbers and symbols
run_test "Numbers and Symbols" \
    "123!@#" \
    "10 uppercaser logger" \
    "[logger] 123!@#"

# Test 11: Complex chain with three plugins
run_test "Three Plugin Chain" \
    "test" \
    "10 uppercaser rotator flipper logger" \
    "[logger] SETT"

# Test 12: Expander + Uppercaser
run_test "Expander + Uppercaser" \
    "hi" \
    "10 expander uppercaser logger" \
    "[logger] H I"

# Test 13: Flipper + Expander
run_test "Flipper + Expander" \
    "abc" \
    "10 flipper expander logger" \
    "[logger] c b a"

# Test 14: Long chain
run_test "Long Chain" \
    "hello" \
    "10 uppercaser flipper rotator expander logger" \
    "[logger] H O L L E"

# Test 15: Just logger (no transformation)
run_test "Logger Only" \
    "test123" \
    "10 logger" \
    "[logger] test123"

# Test error cases
print_test "Testing error handling..."
echo ""

# Test invalid arguments
print_test "Testing invalid queue size..."
./output/analyzer -5 logger 2>/dev/null && print_fail "Should have failed with negative queue size" || print_pass "Correctly rejected negative queue size"

print_test "Testing missing arguments..."
./output/analyzer 2>/dev/null && print_fail "Should have failed with missing arguments" || print_pass "Correctly rejected missing arguments"

print_test "Testing invalid plugin..."
echo "test" | ./output/analyzer 10 nonexistent 2>/dev/null && print_fail "Should have failed with invalid plugin" || print_pass "Correctly rejected invalid plugin"

echo ""
echo "================================"
echo "Test Results:"
echo "  Passed: $TESTS_PASSED"
echo "  Failed: $TESTS_FAILED"
echo "================================"

if [ $TESTS_FAILED -eq 0 ]; then
    print_pass "All tests passed!"
    exit 0
else
    print_fail "Some tests failed."
    exit 1
fi