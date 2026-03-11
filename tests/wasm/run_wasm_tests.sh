#!/bin/sh
set -eu

CIMPLE_NATIVE="${CIMPLE_BIN:-cimple}"
CIMPLE_WASM="${CIMPLE_WASM:-build-wasm/cimple.js}"
TESTS_DIR="$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)"
WASM_DRIVER="$TESTS_DIR/wasm/run_cimple_wasm.js"
TMP_WASM_ERR="${TMPDIR:-/tmp}/cimple_wasm_stderr_$$"
TMP_LIST="${TMPDIR:-/tmp}/cimple_wasm_tests_$$"
. "$TESTS_DIR/lib/assert.sh"

case "$CIMPLE_NATIVE" in
    /*) ;;
    *) CIMPLE_NATIVE="$(pwd)/$CIMPLE_NATIVE" ;;
esac

case "$CIMPLE_WASM" in
    /*) ;;
    *) CIMPLE_WASM="$(pwd)/$CIMPLE_WASM" ;;
esac

node --version >/dev/null 2>&1 || {
    echo "Node.js required for wasm tests" >&2
    exit 1
}

run_wasm_case() {
    dir="$1"
    name="${dir#$TESTS_DIR/}"
    src="$dir/input.ci"
    expected_exit=$(cat "$dir/expected_exit" 2>/dev/null || printf '0')
    expected_stdout="$dir/expected_stdout"
    expected_stderr="$dir/expected_stderr"
    require_empty_stdout="$dir/require_empty_stdout"
    forbidden_stderr="$dir/forbidden_stderr"
    stderr_counts="$dir/stderr_counts"
    tmp_wasm_dir="${TMPDIR:-/tmp}/cimple_wasm_run_$$"
    stdin_file="$dir/stdin"

    [ -f "$dir/skip_wasm" ] && return

    set --
    if [ -f "$dir/args" ]; then
        while IFS= read -r arg || [ -n "$arg" ]; do
            set -- "$@" "$arg"
        done < "$dir/args"
    fi

    rm -rf "$tmp_wasm_dir"
    mkdir -p "$tmp_wasm_dir"

    if [ -f "$stdin_file" ]; then
        if wasm_stdout=$(cd "$tmp_wasm_dir" && node "$WASM_DRIVER" "$CIMPLE_WASM" run "$src" "$@" < "$stdin_file" 2>"$TMP_WASM_ERR"); then
            wasm_exit=0
        else
            wasm_exit=$?
        fi
    elif wasm_stdout=$(cd "$tmp_wasm_dir" && node "$WASM_DRIVER" "$CIMPLE_WASM" run "$src" "$@" 2>"$TMP_WASM_ERR"); then
        wasm_exit=0
    else
        wasm_exit=$?
    fi
    wasm_stderr=$(cat "$TMP_WASM_ERR" 2>/dev/null || true)

    assert_exit "$expected_exit" "$wasm_exit" "wasm:$name:exit"

    if [ -f "$expected_stdout" ]; then
        assert_stdout "$expected_stdout" "$wasm_stdout" "wasm:$name"
    elif [ -f "$require_empty_stdout" ]; then
        assert_empty_stdout "$wasm_stdout" "wasm:$name"
    fi

    if [ -f "$expected_stderr" ]; then
        while IFS= read -r fragment || [ -n "$fragment" ]; do
            [ -z "$fragment" ] && continue
            assert_stderr_contains "$fragment" "$wasm_stderr" "wasm:$name"
        done < "$expected_stderr"
    fi

    if [ -f "$forbidden_stderr" ]; then
        while IFS= read -r fragment || [ -n "$fragment" ]; do
            [ -z "$fragment" ] && continue
            assert_stderr_not_contains "$fragment" "$wasm_stderr" "wasm:$name"
        done < "$forbidden_stderr"
    fi

    if [ -f "$stderr_counts" ]; then
        while IFS='|' read -r fragment expected_count || [ -n "$fragment" ]; do
            [ -z "$fragment" ] && continue
            assert_stderr_count "$fragment" "$expected_count" "$wasm_stderr" "wasm:$name"
        done < "$stderr_counts"
    fi

    if [ -f "$require_empty_stdout" ] && [ ! -f "$expected_stdout" ]; then
        :
    fi

    rm -f "$TMP_WASM_ERR"
    rm -rf "$tmp_wasm_dir"
}

for root in positive negative regression manual; do
    [ -d "$TESTS_DIR/$root" ] || continue
    find "$TESTS_DIR/$root" -name input.ci | sort > "$TMP_LIST"
    while IFS= read -r src; do
        run_wasm_case "$(dirname "$src")"
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
