#!/bin/sh
set -eu

CIMPLE="${CIMPLE_BIN:-cimple}"
TESTS_DIR="$(CDPATH= cd -- "$(dirname "$0")" && pwd)"
TMP_ERR="${TMPDIR:-/tmp}/cimple_stderr_$$"
TMP_LIST="${TMPDIR:-/tmp}/cimple_tests_$$"
. "$TESTS_DIR/lib/assert.sh"

case "$CIMPLE" in
    /*) ;;
    *) CIMPLE="$(pwd)/$CIMPLE" ;;
esac

run_test() {
    dir="$1"
    name="${dir#$TESTS_DIR/}"
    src="$dir/input.ci"
    expected_exit=$(cat "$dir/expected_exit" 2>/dev/null || printf '0')
    expected_stdout="$dir/expected_stdout"
    expected_stderr="$dir/expected_stderr"
    tmp_run_dir="${TMPDIR:-/tmp}/cimple_test_run_$$"

    set --
    if [ -f "$dir/args" ]; then
        while IFS= read -r arg || [ -n "$arg" ]; do
            set -- "$@" "$arg"
        done < "$dir/args"
    fi

    rm -rf "$tmp_run_dir"
    mkdir -p "$tmp_run_dir"

    if actual_stdout=$(cd "$tmp_run_dir" && "$CIMPLE" run "$src" "$@" 2>"$TMP_ERR"); then
        actual_exit=0
    else
        actual_exit=$?
    fi
    actual_stderr=$(cat "$TMP_ERR" 2>/dev/null || true)
    rm -f "$TMP_ERR"
    rm -rf "$tmp_run_dir"

    assert_exit "$expected_exit" "$actual_exit" "$name"

    if [ -f "$expected_stdout" ]; then
        assert_stdout "$expected_stdout" "$actual_stdout" "$name"
    fi

    if [ -f "$expected_stderr" ]; then
        while IFS= read -r fragment || [ -n "$fragment" ]; do
            [ -z "$fragment" ] && continue
            assert_stderr_contains "$fragment" "$actual_stderr" "$name"
        done < "$expected_stderr"
    fi
}

for category in positive negative regression manual; do
    [ -d "$TESTS_DIR/$category" ] || continue
    find "$TESTS_DIR/$category" -name input.ci | sort > "$TMP_LIST"
    while IFS= read -r src; do
        run_test "$(dirname "$src")"
    done < "$TMP_LIST"
done

rm -f "$TMP_LIST"
print_summary
