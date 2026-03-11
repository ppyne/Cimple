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

assert_empty_stdout() {
    actual="$1"
    test_name="$2"
    if [ -z "$actual" ]; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        ERRORS="$ERRORS\nFAIL [$test_name] stdout is not empty"
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

assert_stderr_not_contains() {
    fragment="$1"
    actual="$2"
    test_name="$3"
    case "$actual" in
        *"$fragment"*)
            FAIL=$((FAIL + 1))
            ERRORS="$ERRORS\nFAIL [$test_name] stderr contains forbidden fragment: $fragment"
            ;;
        *)
            PASS=$((PASS + 1))
            ;;
    esac
}

assert_stderr_count() {
    fragment="$1"
    expected_count="$2"
    actual="$3"
    test_name="$4"
    count=$(printf '%s' "$actual" | awk -v pat="$fragment" 'index($0, pat) { c++ } END { print c+0 }')
    if [ "$count" = "$expected_count" ]; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        ERRORS="$ERRORS\nFAIL [$test_name] stderr count for '$fragment': expected=$expected_count actual=$count"
    fi
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
