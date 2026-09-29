#!/bin/bash
# ABI / symbol-exactness check.
#
# The public symbol set of the dispatched library must equal the upstream
# stb_image_resize2 public set, and the internal _base/_avx2 variant symbols
# must be hidden visibility (not part of the ABI).
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build/abi"; mkdir -p "$BUILD"
if [ -z "${CC:-}" ]; then
    if command -v clang >/dev/null 2>&1; then CC=clang; else CC=gcc; fi
fi
export CC
LIB=${1:-"$ROOT/build/lib_exact/libstb_image_resize2.a"}

echo "== abi: reference (upstream) vs $LIB =="
$CC -O2 -ffp-contract=off -I "$ROOT/upstream" -c "$ROOT/lib/stb_image_resize2.c" -o "$BUILD/ref.o"

# Reference: global default-visibility public functions.
nm -g --defined-only "$BUILD/ref.o" | awk '$2=="T"{print $3}' | grep '^stbir_' | sort -u > "$BUILD/ref.syms"

# Candidate: extract dispatch.o from the archive and list its CANONICAL symbols.
DISP=$(mktemp -d)
cp "$LIB" "$DISP/lib.a"
( cd "$DISP" && ar x lib.a )
# The archive contains resize_dispatch.o; canonical symbols have DEFAULT visibility.
readelf -sW "$DISP/resize_dispatch.o" \
  | awk '$4=="FUNC" && $5=="GLOBAL" && $6=="DEFAULT" {print $8}' \
  | grep '^stbir_' | sort -u > "$BUILD/cand.syms"

# Variant symbols must be hidden.
readelf -sW "$DISP/resize_base.o" \
  | awk '$4=="FUNC" && $5=="GLOBAL" && $6=="DEFAULT" {print $8}' \
  | grep -E '_base$' > "$BUILD/base_default.txt" || true
[ -s "$BUILD/base_default.txt" ] && BC=1 || BC=0
if [ -f "$DISP/resize_avx2.o" ]; then
  readelf -sW "$DISP/resize_avx2.o" \
    | awk '$4=="FUNC" && $5=="GLOBAL" && $6=="DEFAULT" {print $8}' \
    | grep -E '_avx2$' > "$BUILD/avx2_default.txt" || true
  [ -s "$BUILD/avx2_default.txt" ] && AC=1 || AC=0
else
  AC=0
fi
rm -rf "$DISP"

if diff -u "$BUILD/ref.syms" "$BUILD/cand.syms" > "$BUILD/symdiff.txt"; then
  echo "PASS: public symbol set exact ($(wc -l < "$BUILD/ref.syms") symbols)"
else
  echo "FAIL: public symbol set differs"; cat "$BUILD/symdiff.txt"; exit 1
fi

if [ "$BC" = 1 ] || [ "$AC" = 1 ]; then
  echo "FAIL: variant symbols are not hidden"; cat "$BUILD/base_default.txt" "$BUILD/avx2_default.txt"; exit 1
fi
echo "PASS: internal _base/_avx2 symbols are hidden"
