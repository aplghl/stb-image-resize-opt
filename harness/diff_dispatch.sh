#!/bin/bash
# Differential test for the runtime-dispatched library: builds the baseline and
# AVX2 variant objects + the dispatch layer, links them into resize_dump, and
# runs the full matrix on every dispatch path (auto, forced base, forced avx2)
# against the pristine oracle.
#
# usage: harness/diff_dispatch.sh
#   QUICK=1 / QUIET=1 supported; CC overrides the compiler.
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build"
mkdir -p "$BUILD"
if [ -z "${CC:-}" ]; then
    if command -v clang >/dev/null 2>&1; then CC=clang; else CC=gcc; fi
fi
export CC
QUICK=${QUICK:-0}
QUIET=${QUIET:-0}
rm -f "$BUILD/crashes.txt"

# Deterministic heap fill (see docs/MEASUREMENT.md): the pristine library has a
# pre-existing uninitialized read in one uint16/wrap/box upsample case. Filling
# allocations identically keeps the oracle and the variants comparable.
export MALLOC_PERTURB_=${MALLOC_PERTURB_:-85}

BASE_FLAGS=${BASE_FLAGS:-"-O3 -march=x86-64-v2 -ffp-contract=off"}
AVX2_FLAGS=${AVX2_FLAGS:-"-O3 -march=x86-64-v3 -ffp-contract=off"}
ORACLE_FLAGS=${ORACLE_FLAGS:-"-O2"}

echo "== dispatch differential (base='$BASE_FLAGS' avx2='$AVX2_FLAGS') =="

echo "-- building oracle --"
$CC $ORACLE_FLAGS -ffp-contract=off -I "$ROOT/upstream" -I "$ROOT/tools" \
    "$ROOT/tools/resize_dump.c" -lm -o "$BUILD/resize_oracle" || exit 1

echo "-- building variants --"
$CC $BASE_FLAGS  -I "$ROOT/src" -c "$ROOT/lib/resize_base.c"     -o "$BUILD/resize_base.o"     || exit 1
$CC $AVX2_FLAGS  -I "$ROOT/src" -c "$ROOT/lib/resize_avx2.c"     -o "$BUILD/resize_avx2.o"     || exit 1
$CC $BASE_FLAGS -DSTBIR_DISPATCH_AVX2 -I "$ROOT/src" -c "$ROOT/lib/resize_dispatch.c" -o "$BUILD/resize_dispatch.o" || exit 1

$CC $BASE_FLAGS -DRESIZE_LINK_LIB -I "$ROOT/src" -I "$ROOT/upstream" -I "$ROOT/tools" \
    "$ROOT/tools/resize_dump.c" "$BUILD/resize_base.o" "$BUILD/resize_avx2.o" "$BUILD/resize_dispatch.o" \
    -lm -o "$BUILD/resize_dispatch_dump" || exit 1

O="$BUILD/resize_oracle"
C="$BUILD/resize_dispatch_dump"

# shellcheck source=matrix.inc.sh
. "$ROOT/harness/matrix.inc.sh"

overall=0
for mode in auto base avx2; do
    if [ "$mode" = auto ]; then unset STBIR_CPU 2>/dev/null || true; else export STBIR_CPU="$mode"; fi
    fail=0; n=0; nfail=0
    run_matrix
    if [ "$fail" -eq 0 ]; then
        echo "PASS [$mode]: $n checks byte-exact"
    else
        echo "FAIL [$mode]: $nfail of $n checks diverged"
        overall=1
    fi
done
rm -f "$BUILD/_o.bin" "$BUILD/_c.bin"
exit $overall
