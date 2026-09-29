#!/bin/bash
# Differential test of a prebuilt static library archive against the pristine
# oracle. Builds resize_dump with -DRESIZE_LINK_LIB and links the archive.
#
# usage: harness/diff_lib.sh <path/to/libstb_image_resize2.a>
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build"; mkdir -p "$BUILD"
LIB=${1:?usage: diff_lib.sh <lib.a>}
if [ -z "${CC:-}" ]; then
    if command -v clang >/dev/null 2>&1; then CC=clang; else CC=gcc; fi
fi
export CC
QUICK=${QUICK:-0}
QUIET=${QUIET:-0}
ORACLE_FLAGS=${ORACLE_FLAGS:-"-O2"}
rm -f "$BUILD/crashes.txt"
export MALLOC_PERTURB_=${MALLOC_PERTURB_:-85}

echo "== differential vs archive: $LIB =="
$CC $ORACLE_FLAGS -ffp-contract=off -I "$ROOT/upstream" -I "$ROOT/tools" \
    "$ROOT/tools/resize_dump.c" -lm -o "$BUILD/resize_oracle" || exit 1
$CC -O3 -march=x86-64-v2 -ffp-contract=off -DRESIZE_LINK_LIB \
    -I "$ROOT/src" -I "$ROOT/upstream" -I "$ROOT/tools" \
    "$ROOT/tools/resize_dump.c" "$LIB" -lm -o "$BUILD/resize_lib" || exit 1

O="$BUILD/resize_oracle"
C="$BUILD/resize_lib"
# shellcheck source=matrix.inc.sh
. "$ROOT/harness/matrix.inc.sh"

overall=0
for mode in auto base avx2; do
    if [ "$mode" = auto ]; then unset STBIR_CPU 2>/dev/null || true; else export STBIR_CPU="$mode"; fi
    fail=0; n=0; nfail=0
    run_matrix
    if [ "$fail" -eq 0 ]; then echo "PASS [$mode]: $n checks byte-exact"
    else echo "FAIL [$mode]: $nfail of $n checks diverged"; overall=1; fi
done
rm -f "$BUILD/_o.bin" "$BUILD/_c.bin"
exit $overall
