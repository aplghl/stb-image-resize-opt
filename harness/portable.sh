#!/bin/bash
# Portability matrix: every configuration must stay byte-exact.
#   - scalar (STBIR_NO_SIMD), SSE2 baseline, AVX2 baseline, native
#   - C++ compilation
#   - cross-compilation (aarch64-linux-musl, x86_64-windows-gnu, aarch64-macos)
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build/portable"; mkdir -p "$BUILD"
if [ -z "${CC:-}" ]; then
    if command -v clang >/dev/null 2>&1; then CC=clang; else CC=gcc; fi
fi
fail=0

run_diff() { # <label> <CC> <flags...>
    local label=$1 cc=$2; shift 2
    local out
    out=$(QUICK=1 QUIET=1 CC="$cc" bash "$ROOT/harness/diff.sh" "$@" 2>&1 | tail -1)
    printf "%-30s : %s\n" "$label" "$out"
    case "$out" in PASS*) ;; *) fail=1 ;; esac
}

echo "== exactness across ISA configs =="
run_diff "scalar -O2"          "$CC" "-O2 -DSTBIR_NO_SIMD"
run_diff "sse2 -O3 -march=v2"  "$CC" "-O3 -march=x86-64-v2"
run_diff "avx2 -O3 -march=v3"  "$CC" "-O3 -march=x86-64-v3"
run_diff "native -O3"          "$CC" "-O3 -march=native"

echo "== C++ =="
CXX=""
command -v clang++ >/dev/null 2>&1 && CXX=clang++
[ -z "$CXX" ] && command -v g++ >/dev/null 2>&1 && CXX=g++
if [ -n "$CXX" ]; then
    run_diff "c++ -O2" "$CXX" "-O2"
else
    echo "  (no C++ compiler found)"
fi

echo "== cross-compile (zig cc, linkable objects) =="
if command -v zig >/dev/null 2>&1; then
    for tgt in aarch64-linux-musl x86_64-linux-musl x86_64-windows-gnu aarch64-macos; do
        printf "%-30s : " "$tgt"
        if zig cc -target "$tgt" -O2 -ffp-contract=off -I "$ROOT/src" \
              -c "$ROOT/lib/stb_image_resize2.c" -o "$BUILD/resize_${tgt}.o" >/dev/null 2>&1; then
            echo "compiles"
        else
            echo "FAILED"; fail=1
        fi
    done
else
    echo "  (zig not found)"
fi

exit $fail
