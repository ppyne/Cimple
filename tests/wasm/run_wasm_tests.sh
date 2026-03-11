#!/bin/sh
set -eu

CIMPLE_NATIVE="${CIMPLE_BIN:-cimple}"
CIMPLE_WASM="${CIMPLE_WASM:-build-wasm/cimple.js}"
TESTS_DIR="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
WASM_DRIVER="$TESTS_DIR/wasm/run_cimple_wasm.js"
TMP_NATIVE_ERR="${TMPDIR:-/tmp}/cimple_native_stderr_$$"
TMP_WASM_ERR="${TMPDIR:-/tmp}/cimple_wasm_stderr_$$"
TMP_LIST="${TMPDIR:-/tmp}/cimple_wasm_tests_$$"
. "$TESTS_DIR/lib/assert.sh"

node --version >/dev/null 2>&1 || {
    echo "Node.js required for wasm tests" >&2
    exit 1
}

run_compare() {
    src="$1"
    name="${src#$TESTS_DIR/}"
    dir="$(dirname "$src")"

    set --
    if [ -f "$dir/args" ]; then
        while IFS= read -r arg || [ -n "$arg" ]; do
            set -- "$@" "$arg"
        done < "$dir/args"
    fi

    if native_stdout=$("$CIMPLE_NATIVE" run "$src" "$@" 2>"$TMP_NATIVE_ERR"); then
        native_exit=0
    else
        native_exit=$?
    fi

    if wasm_stdout=$(node "$WASM_DRIVER" "$CIMPLE_WASM" run "$src" "$@" 2>"$TMP_WASM_ERR"); then
        wasm_exit=0
    else
        wasm_exit=$?
    fi

    assert_exit "$native_exit" "$wasm_exit" "wasm:$name:exit"

    if [ "$native_stdout" = "$wasm_stdout" ]; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        ERRORS="$ERRORS\nFAIL [wasm:$name] stdout differs"
    fi

    rm -f "$TMP_NATIVE_ERR" "$TMP_WASM_ERR"
}

for root in positive manual; do
    [ -d "$TESTS_DIR/$root" ] || continue
    find "$TESTS_DIR/$root" -name input.ci | sort > "$TMP_LIST"
    while IFS= read -r src; do
        run_compare "$src"
    done < "$TMP_LIST"
done

wasm_exec_test="$TESTS_DIR/wasm/exec_not_supported"
if [ -d "$wasm_exec_test" ]; then
    if actual=$(node "$WASM_DRIVER" "$CIMPLE_WASM" run "$wasm_exec_test/input.ci" 2>"$TMP_WASM_ERR"); then
        actual_exit=0
    else
        actual_exit=$?
    fi
    actual_stderr=$(cat "$TMP_WASM_ERR" 2>/dev/null || true)
    assert_exit "2" "$actual_exit" "wasm:exec_not_supported:exit"
    assert_stderr_contains "not supported on this platform" "$actual_stderr" "wasm:exec_not_supported"
fi

wasm_env_test="$TESTS_DIR/wasm/getenv_fallback"
if [ -d "$wasm_env_test" ]; then
    actual_stdout=$(node "$WASM_DRIVER" "$CIMPLE_WASM" run "$wasm_env_test/input.ci" 2>/dev/null)
    assert_stdout "$wasm_env_test/expected_stdout" "$actual_stdout" "wasm:getenv_fallback"
fi

rm -f "$TMP_LIST"
print_summary
