#!/usr/bin/env bash
# Clang automated testing for macOS/Linux
# Usage: ./run_tests_clang.sh
#
# Tests the lightweight C++03-compatible test suite with clang++.
# Follows the same pattern as run_tests.py (Windows VS/MinGW tests).

set -u

REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
PASS=0
FAIL=0
SKIP=0
RESULTS=()

print_header() {
    echo "============================================================"
    echo "[$1]"
    echo "============================================================"
}

test_lightweight() {
    local label="$1"
    local extra_flags="$2"

    print_header "$label"

    if ! command -v clang++ &>/dev/null; then
        echo "SKIP: clang++ not found"
        RESULTS+=("$label|skip|clang++ not found")
        ((SKIP++)) || true
        return
    fi

    local out_exe="$REPO_DIR/test_lightweight/test_clang_$$"

    echo "Compiling: clang++ -x c++ -Wall -O2 $extra_flags"
    echo

    if ! clang++ -x c++ -Wall -O2 -I"$REPO_DIR" \
         $extra_flags \
         "$REPO_DIR/test_lightweight/test_runner.cpp" \
         "$REPO_DIR/ConvertUTF.c" \
         -o "$out_exe" -lstdc++ 2>&1; then
        echo "FAIL: Compile error"
        RESULTS+=("$label|fail|compile failed")
        ((FAIL++)) || true
        return
    fi

    if ! "$out_exe" 2>&1; then
        echo "FAIL: Runtime error"
        RESULTS+=("$label|fail|runtime failed")
        ((FAIL++)) || true
        rm -f "$out_exe"
        return
    fi

    echo "PASS"
    RESULTS+=("$label|pass|ok")
    ((PASS++)) || true
    rm -f "$out_exe"
}

# Main
echo
echo "SimpleIni Clang Test Suite"
echo "============================================================"
echo

clang_ver=$(clang++ --version 2>/dev/null | head -1 || echo "not found")
echo "Compiler: $clang_ver"
echo

test_lightweight "Clang (C++03)" ""
test_lightweight "Clang (C++17)" "-std=c++17"

# Summary
echo
echo "============================================================"
echo "SUMMARY"
echo "============================================================"

for entry in "${RESULTS[@]}"; do
    IFS='|' read -r name status msg <<< "$entry"
    if [ "$status" = "pass" ]; then
        printf "  %-25s PASS\n" "$name"
    elif [ "$status" = "skip" ]; then
        printf "  %-25s SKIP (%s)\n" "$name" "$msg"
    else
        printf "  %-25s FAIL (%s)\n" "$name" "$msg"
    fi
done

echo
echo "Total: $PASS passed, $FAIL failed, $SKIP skipped"

if [ "$FAIL" -gt 0 ]; then
    echo
    echo "SOME TESTS FAILED"
    exit 1
else
    echo
    echo "ALL TESTS PASSED"
    exit 0
fi
