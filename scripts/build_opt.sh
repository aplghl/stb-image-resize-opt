#!/bin/bash
# Build the drop-in static library with runtime AVX2 dispatch.
#
#   usage: scripts/build_opt.sh [exact|fast] [outdir]
#     exact (default): byte-identical to the upstream SSE2 oracle
#     fast           : -ffast-math + STBIR_USE_FMA in the AVX2 variant
#
#   env: CC (default clang), PGO=1 to train, ARCH_BASE, ARCH_AVX2
#
# On x86-64 the library contains: a baseline SSE2 variant, an AVX2 variant
# (selected at runtime only for CPUs with the full x86-64-v3 feature set), and a
# dispatch layer exposing the canonical API. On other targets only the baseline
# is built. The public symbols/ABI are the upstream stb_image_resize2 ones.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
MODE=${1:-exact}
OUT=${2:-"$ROOT/build/lib_$MODE"}
CC=${CC:-clang}
PGO=${PGO:-0}
ARCH_BASE=${ARCH_BASE:-x86-64-v2}
ARCH_AVX2=${ARCH_AVX2:-x86-64-v3}

case "$MODE" in
    exact)
        BASE_FP="-ffp-contract=off"; AVX2_FP="-ffp-contract=off"; AVX2_EXTRA="" ;;
    fast)
        BASE_FP="-ffast-math"; AVX2_FP="-ffast-math"; AVX2_EXTRA="-DSTBIR_USE_FMA" ;;
    *)
        echo "unknown mode: $MODE (expected exact|fast)"; exit 2 ;;
esac

BASE_FLAGS="-O3 -march=$ARCH_BASE $BASE_FP"
AVX2_FLAGS="-O3 -march=$ARCH_AVX2 $AVX2_FP $AVX2_EXTRA"

MACHINE=$(uname -m)
HAVE_AVX2_TARGET=0
case "$MACHINE" in x86_64|amd64|i?86) HAVE_AVX2_TARGET=1 ;; esac

mkdir -p "$OUT"

build_objects() { # <extra flags...>
    local extra="$*"
    # Variant TUs get hidden visibility: their renamed symbols are internal to
    # the library, so the exported ABI is exactly the canonical upstream set.
    # shellcheck disable=SC2086
    $CC $BASE_FLAGS $extra -fvisibility=hidden -I "$ROOT/src" -c "$ROOT/lib/resize_base.c" -o "$OUT/resize_base.o"
    # shellcheck disable=SC2086
    $CC $BASE_FLAGS $extra -DSTBIR_DISPATCH_AVX2 -I "$ROOT/src" -c "$ROOT/lib/resize_dispatch.c" -o "$OUT/resize_dispatch.o"
    if [ "$HAVE_AVX2_TARGET" = 1 ]; then
        # shellcheck disable=SC2086
        $CC $AVX2_FLAGS $extra -fvisibility=hidden -I "$ROOT/src" -c "$ROOT/lib/resize_avx2.c" -o "$OUT/resize_avx2.o"
    fi
}

if [ "$PGO" = 1 ]; then
    PROF="$ROOT/build/pgo_lib_$MODE"
    rm -rf "$PROF" && mkdir -p "$PROF"
    echo "PGO: instrumenting ($MODE)"
    build_objects -fprofile-generate="$PROF"

    TRAIN="$OUT/train"
    OBJS="$OUT/resize_base.o $OUT/resize_dispatch.o"
    [ "$HAVE_AVX2_TARGET" = 1 ] && OBJS="$OBJS $OUT/resize_avx2.o"
    # shellcheck disable=SC2086
    $CC $BASE_FLAGS -fprofile-generate="$PROF" -I "$ROOT/src" -I "$ROOT/upstream" -I "$ROOT/tools" \
        "$ROOT/tools/train.c" $OBJS -lm -o "$TRAIN"
    # Held-out workload; train both dispatch paths (avx2 only if the CPU has it).
    MALLOC_PERTURB_=85 STBIR_CPU=base "$TRAIN" >/dev/null 2>&1 || true
    if [ "$HAVE_AVX2_TARGET" = 1 ]; then
        MALLOC_PERTURB_=85 STBIR_CPU=avx2 "$TRAIN" >/dev/null 2>&1 || true
    fi

    case "$CC" in
      *clang*) llvm-profdata merge -output="$PROF/default.profdata" "$PROF"/*.profraw ;;
    esac
    echo "PGO: rebuilding with profile ($MODE)"
    rm -f "$OUT"/*.o
    case "$CC" in
      *clang*) build_objects -fprofile-use="$PROF/default.profdata" ;;
      *)       build_objects -fprofile-use="$PROF" -fprofile-correction ;;
    esac
    rm -f "$TRAIN"
else
    build_objects
fi

OBJS="$OUT/resize_base.o $OUT/resize_dispatch.o"
[ "$HAVE_AVX2_TARGET" = 1 ] && OBJS="$OBJS $OUT/resize_avx2.o"
# shellcheck disable=SC2086
ar rcs "$OUT/libstb_image_resize2.a" $OBJS
cp "$ROOT/src/stb_image_resize2.h" "$OUT/stb_image_resize2.h"
echo "built $OUT/libstb_image_resize2.a (mode=$MODE pgo=$PGO avx2=$HAVE_AVX2_TARGET)"
