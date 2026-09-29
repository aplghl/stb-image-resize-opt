#!/bin/bash
# Build and run the throughput matrix for one variant, writing results/<label>.csv.
# usage: harness/bench.sh <label> <include_dir> [flags...]
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
LABEL=${1:?label}; shift
INC=${1:?include dir}; shift
CC=${CC:-clang}
PIN=${PIN:-3}
FLAGS=${*:--O3 -march=x86-64-v2 -ffp-contract=off}
mkdir -p "$ROOT/build" "$ROOT/results"

# shellcheck disable=SC2086
$CC $FLAGS -ffp-contract=off -I "$INC" -I "$ROOT/upstream" -I "$ROOT/tools" \
    "$ROOT/bench/bench.c" -lm -o "$ROOT/build/bench_$LABEL"
taskset -c "$PIN" "$ROOT/build/bench_$LABEL" > "$ROOT/results/$LABEL.csv"
echo "wrote results/$LABEL.csv"
