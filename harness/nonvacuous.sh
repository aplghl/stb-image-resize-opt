#!/bin/bash
# Non-vacuous proof: the differential suite must FAIL on a deliberately
# corrupted candidate. Corrupts a copy of src/ (a numeric constant that changes
# output) and requires harness/diff.sh to report divergence; then requires the
# unmodified candidate to pass.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
COR="$ROOT/build/nonvacuous_src"
if [ -z "${CC:-}" ]; then
    if command -v clang >/dev/null 2>&1; then CC=clang; else CC=gcc; fi
fi
export CC

rm -rf "$COR"; mkdir -p "$COR"
cp "$ROOT/src/stb_image_resize2.h" "$COR/stb_image_resize2.h"

# Corrupt a numeric constant used by every datatype.
sed -i 's/#define stbir__max_uint8_as_float             255\.0f/#define stbir__max_uint8_as_float             255.5f/' "$COR/stb_image_resize2.h"
if cmp -s "$COR/stb_image_resize2.h" "$ROOT/src/stb_image_resize2.h"; then
    echo "FAIL: corruption did not apply (pattern not found)"; exit 1
fi

echo "== nonvacuous: corrupted candidate must FAIL =="
if SRC_DIR="$COR" QUICK=1 QUIET=1 bash "$ROOT/harness/diff.sh" -O2 >/dev/null 2>&1; then
    echo "FAIL: suite passed a corrupted candidate (vacuous)"; exit 1
fi
echo "PASS: corrupted candidate rejected"

echo "== nonvacuous: pristine candidate must PASS =="
if SRC_DIR="$ROOT/src" QUICK=1 QUIET=1 bash "$ROOT/harness/diff.sh" -O2 >/dev/null 2>&1; then
    echo "PASS: pristine candidate accepted"
else
    echo "FAIL: pristine candidate rejected"; exit 1
fi
rm -rf "$COR"
