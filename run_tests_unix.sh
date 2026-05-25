#!/usr/bin/env bash
# GCC/Clang automated testing for macOS/Linux
# Usage: CXX=g++   ./run_tests_unix.sh
#        CXX=clang++ ./run_tests_unix.sh
#
# Tests the lightweight C++03-compatible test suite.
# Follows the same pattern as run_tests.py (Windows VS/MinGW tests).

set -u

CXX="${CXX:-g++}"

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

    if ! command -v "$CXX" &>/dev/null; then
        echo "SKIP: $CXX not found"
        RESULTS+=("$label|skip|$CXX not found")
        ((SKIP++)) || true
        return
    fi

    local out_exe="$REPO_DIR/test_lightweight/test_unix_$$"

    echo "Compiling: $CXX -x c++ -Wall -O2 $extra_flags"
    echo

    if ! "$CXX" -x c++ -Wall -O2 -I"$REPO_DIR" \
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

# Detect compiler name for display
cxx_name=$("$CXX" --version 2>/dev/null | head -1 || echo "not found")
cxx_base=$(basename "$CXX")

# Main
echo
echo "SimpleIni $cxx_base Test Suite"
echo "============================================================"
echo
echo "Compiler: $cxx_name"
echo

test_lightweight "$cxx_base (C++03)" ""
test_lightweight "$cxx_base (C++17)" "-std=c++17"

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
