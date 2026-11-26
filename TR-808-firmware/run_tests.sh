#!/bin/bash
# Host unit tests (gcc + Unity). Run from this directory or via `make test`.
set -u
CC=${CC:-gcc}
CFLAGS="-Wall -Werror -std=c99 -fpack-struct"
OUT=$(mktemp -d)
trap 'rm -rf "$OUT"' EXIT
status=0

run() {
	local name=$1; shift
	if ! $CC $CFLAGS "$@" -o "$OUT/$name"; then
		echo "Compilation of $name failed!"
		status=1
		return
	fi
	if ! "$OUT/$name"; then
		echo "Tests in $name failed!"
		status=1
	fi
}

run test_select_instruments select_instrument_with_soloing.c unity.c test_select_instruments.c
run test_logic logic.c unity.c test_logic.c

if [ $status -ne 0 ]; then
	exit 1
fi
echo "All tests passed!"
