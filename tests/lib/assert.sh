#!/bin/sh

PASS=0
FAIL=0
ERRORS=""

assert_exit() {
    expected="$1"
    actual="$2"
    test_name="$3"
    if [ "$expected" = "$actual" ]; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        ERRORS="$ERRORS\nFAIL [$test_name] exit: expected=$expected actual=$actual"
    fi
}

assert_stdout() {
    expected_file="$1"
    actual="$2"
    test_name="$3"
    expected=$(cat "$expected_file")
    if [ "$expected" = "$actual" ]; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        ERRORS="$ERRORS\nFAIL [$test_name] stdout mismatch"
    fi
}

assert_stderr_contains() {
    fragment="$1"
    actual="$2"
    test_name="$3"
    case "$actual" in
        *"$fragment"*) PASS=$((PASS + 1)) ;;
        *)
            FAIL=$((FAIL + 1))
            ERRORS="$ERRORS\nFAIL [$test_name] stderr does not contain: $fragment"
            ;;
    esac
}

print_summary() {
    total=$((PASS + FAIL))
    if [ "$FAIL" -ne 0 ]; then
        printf '%b\n' "$ERRORS"
        printf '\n%d/%d assertions passed\n' "$PASS" "$total"
        exit 1
    fi
    printf '%d/%d assertions passed\n' "$PASS" "$total"
}
