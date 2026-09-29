#!/bin/bash
# Byte-exact differential test: candidate (src/) vs pristine upstream oracle.
#
# Builds resize_dump twice (oracle from upstream/, candidate from src/) and runs
# a large parameter matrix, requiring byte-identical raw output buffers AND
# identical exit codes. The oracle is always built with canonical upstream flags
# (-O2) so numeric changes from candidate source OR candidate flags are caught.
#
# usage: harness/diff.sh [candidate flags...]
#   ORACLE_FLAGS="..."  override oracle flags (default "-O2")
#   CC=clang            compiler
#   QUICK=1             reduced matrix
#   QUIET=1             only print the summary
set -u
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build"
mkdir -p "$BUILD"

if [ -z "${CC:-}" ]; then
    if command -v clang >/dev/null 2>&1; then CC=clang; else CC=gcc; fi
fi
export CC

CAND_FLAGS=${CAND_FLAGS:-"-O2"}
[ $# -gt 0 ] && CAND_FLAGS="$*"
ORACLE_FLAGS=${ORACLE_FLAGS:-"-O2"}
QUICK=${QUICK:-0}
QUIET=${QUIET:-0}
SRC_DIR=${SRC_DIR:-"$ROOT/src"}
rm -f "$BUILD/crashes.txt"

# Deterministic heap fill: a pre-existing upstream uninitialized read (see
# docs/MEASUREMENT.md) makes one uint16/wrap/box upsample case depend on heap
# contents. Filling allocations identically in both binaries keeps the
# differential like-for-like without editing the oracle.
export MALLOC_PERTURB_=${MALLOC_PERTURB_:-85}

echo "== differential: oracle='$ORACLE_FLAGS' candidate='$CAND_FLAGS' cc=$CC src=$SRC_DIR =="

$CC $ORACLE_FLAGS -ffp-contract=off -I "$ROOT/upstream" -I "$ROOT/tools" \
    "$ROOT/tools/resize_dump.c" -lm -o "$BUILD/resize_oracle" || exit 1
$CC $CAND_FLAGS -ffp-contract=off -I "$SRC_DIR" -I "$ROOT/upstream" -I "$ROOT/tools" \
    "$ROOT/tools/resize_dump.c" -lm -o "$BUILD/resize_cand" || exit 1

O="$BUILD/resize_oracle"
C="$BUILD/resize_cand"

# shellcheck source=matrix.inc.sh
. "$ROOT/harness/matrix.inc.sh"
run_matrix
rm -f "$BUILD/_o.bin" "$BUILD/_c.bin"
matrix_summary
exit $?
