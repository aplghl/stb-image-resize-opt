#!/bin/bash
# ABI / symbol-exactness check.
#
# The public symbol set of the dispatched library must equal the upstream
# stb_image_resize2 public set, and the internal _base/_avx2 variant symbols
# must be hidden visibility (not part of the ABI). Works on archives produced by
# either the compiler driver or `zig build` (whose member names contain slashes).
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
nm -g --defined-only "$BUILD/ref.o" | awk '$2=="T"{print $3}' | grep '^stbir_' | sort -u > "$BUILD/ref.syms"

# Parse the archive's symbol tables by member. Member names may be
# `archive(member.o)` or `archive(path/to/member.o)` (zig).
readelf -sW "$LIB" 2>/dev/null | awk '
function member_name(line,   p, q, m, a, n) {
   p=index(line, "("); q=index(line, ")");
   if (p>0 && q>p) m=substr(line, p+1, q-p-1); else m=line;
   n=split(m, a, "/"); return a[n];
}
/^File:/ { member=member_name($0); next }
$4=="FUNC" && $5=="GLOBAL" && $6=="DEFAULT" && $8 ~ /^stbir_/ && member=="resize_dispatch.o" { print $8 }
' | sort -u > "$BUILD/cand.syms"

readelf -sW "$LIB" 2>/dev/null | awk '
function member_name(line,   p, q, m, a, n) {
   p=index(line, "("); q=index(line, ")");
   if (p>0 && q>p) m=substr(line, p+1, q-p-1); else m=line;
   n=split(m, a, "/"); return a[n];
}
/^File:/ { member=member_name($0); next }
$4=="FUNC" && $5=="GLOBAL" && $6=="DEFAULT" && (member=="resize_base.o" || member=="resize_avx2.o") {
   print member ":" $8;
}
' > "$BUILD/variant_default.txt"

if diff -u "$BUILD/ref.syms" "$BUILD/cand.syms" > "$BUILD/symdiff.txt"; then
    echo "PASS: public symbol set exact ($(wc -l < "$BUILD/ref.syms") symbols)"
else
    echo "FAIL: public symbol set differs"; cat "$BUILD/symdiff.txt"; exit 1
fi

if [ -s "$BUILD/variant_default.txt" ]; then
    echo "FAIL: variant symbols are not hidden"; cat "$BUILD/variant_default.txt"; exit 1
fi
echo "PASS: internal _base/_avx2 symbols are hidden"
