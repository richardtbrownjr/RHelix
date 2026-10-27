#!/bin/bash
# tests/run_tests.sh - Run all .rh test files in this directory,
# comparing output against the matching .expected file.
#
# Exit 0 if all pass, 1 if any fail.
#
# Usage from repo root: ./tests/run_tests.sh
# Or just:              bash tests/run_tests.sh

set -u

# Resolve the directory this script lives in, so the runner works
# from any cwd.
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
REPO_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"
RHELIX="$REPO_ROOT/build/rhelix"

if [ ! -x "$RHELIX" ]; then
    echo "error: rhelix binary not found or not executable at $RHELIX"
    echo "run 'make rhelix' first"
    exit 2
fi

passed=0
failed=0
failed_names=()

# Iterate every .rh file in tests/ (not subdirectories like known_issues/)
for rh_file in "$SCRIPT_DIR"/*.rh; do
    [ -f "$rh_file" ] || continue

    base="$(basename "$rh_file" .rh)"
    expected_file="$SCRIPT_DIR/$base.expected"

    if [ ! -f "$expected_file" ]; then
        echo "SKIP  $base  (no .expected file)"
        continue
    fi

    actual="$("$RHELIX" "$rh_file" 2>&1)"
    expected="$(cat "$expected_file")"

    if [ "$actual" = "$expected" ]; then
        echo "PASS  $base"
        passed=$((passed + 1))
    else
        echo "FAIL  $base"
        failed=$((failed + 1))
        failed_names+=("$base")
        echo "  --- expected ---"
        echo "$expected" | sed 's/^/  /'
        echo "  --- actual ---"
        echo "$actual" | sed 's/^/  /'
        echo "  ---"
    fi
done

total=$((passed + failed))
echo ""
echo "========================================"
echo "  Tests run:    $total"
echo "  Tests passed: $passed"
echo "  Tests failed: $failed"
if [ $failed -gt 0 ]; then
    echo ""
    echo "  Failed tests:"
    for name in "${failed_names[@]}"; do
        echo "    - $name"
    done
fi
echo "========================================"

[ $failed -eq 0 ]
