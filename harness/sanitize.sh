#!/bin/bash
# ASan + UBSan over the resize matrix. Builds the dispatched library and the
# differential driver with sanitizers and runs a representative matrix,
# requiring a clean exit for every case.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build/sanitize"; mkdir -p "$BUILD"
if [ -z "${CC:-}" ]; then
    if command -v clang >/dev/null 2>&1; then CC=clang; else CC=gcc; fi
fi
export CC
SAN="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
SAN="$SAN -fsanitize-ignorelist=$ROOT/harness/ubsan_ignorelist.txt"
OPT="-O1 -g -ffp-contract=off"
# Deterministic heap; also avoids the pre-existing uninitialized-read quirk.
export MALLOC_PERTURB_=${MALLOC_PERTURB_:-85}
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=1:abort_on_error=1}

echo "== sanitize: building instrumented library + driver =="
$CC $OPT $SAN -I "$ROOT/src" -c "$ROOT/lib/resize_base.c" -o "$BUILD/base.o"
$CC $OPT $SAN -march=x86-64-v3 -I "$ROOT/src" -c "$ROOT/lib/resize_avx2.c" -o "$BUILD/avx2.o"
$CC $OPT $SAN -DSTBIR_DISPATCH_AVX2 -I "$ROOT/src" -c "$ROOT/lib/resize_dispatch.c" -o "$BUILD/dispatch.o"
$CC $OPT $SAN -DRESIZE_LINK_LIB -I "$ROOT/src" -I "$ROOT/upstream" -I "$ROOT/tools" \
    "$ROOT/tools/resize_dump.c" "$BUILD/base.o" "$BUILD/avx2.o" "$BUILD/dispatch.o" \
    -lm -o "$BUILD/resize_san"

D="$BUILD/resize_san"
n=0
run() { "$D" "$@" >/dev/null || { echo "SANITIZE FAIL: $*"; exit 1; }; n=$((n + 1)); }

for mode in "" base avx2; do
    if [ -z "$mode" ]; then unset STBIR_CPU 2>/dev/null || true; else export STBIR_CPU="$mode"; fi
    for type in uint8 uint8_srgb uint16 float half; do
        for layout in 1ch 2ch rgb 4ch rgba bgra rgba_pm ra_pm; do
            for edge in clamp reflect wrap zero; do
                for filter in box triangle catmullrom mitchell point other; do
                    run gen:plasma --out 120x90 --in-dims 64x48 --in-type "$type" \
                        --in-layout "$layout" --edge "$edge" --filter "$filter" --quiet
                done
            done
        done
    done
done

# mixed types, strides, subrects, splits
for pair in "uint8 float" "float uint8" "half float" "float half" "uint16 uint8"; do
    set -- $pair
    run gen:plasma --out 80x60 --in-type "$1" --out-type "$2" --in-layout rgba --quiet
done
run gen:plasma --in-dims 64x48 --out 100x75 --in-type uint8 --in-layout rgba --stride-in 263 --stride-out 407 --quiet
run gen:plasma --in-dims 64x48 --out 80x60 --in-type uint8 --in-layout rgba --in-subrect 0.1 0.2 0.85 0.9 --quiet
run gen:plasma --in-dims 64x48 --out 80x60 --in-type uint8 --in-layout rgba --out-subrect 5 7 40 30 --quiet
for sp in 2 3 4; do
    run gen:plasma --in-dims 128x96 --out 300x200 --in-type uint8 --in-layout rgba --splits "$sp" --quiet
done
for rel in corpus/plasma_512.png upstream/tests/pngsuite/primary/basn2c16.png; do
    [ -e "$ROOT/$rel" ] && run "$ROOT/$rel" --out 200x130 --in-layout rgba --quiet
done

echo "PASS: $n sanitized runs clean"
