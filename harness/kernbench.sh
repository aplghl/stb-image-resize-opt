#!/bin/bash
# Isolated kernel/ISA comparison: the same dispatched library run with
# STBIR_CPU=base vs STBIR_CPU=avx2. This isolates the AVX2 implementation from
# flags/PGO and writes results/kernels.csv.
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build"; mkdir -p "$BUILD" "$ROOT/results"
CC=${CC:-clang}
PIN=${PIN:-3}

echo "== building exact dispatched library (no PGO) =="
bash "$ROOT/scripts/build_opt.sh" exact "$BUILD/lib_kern" >/dev/null
$CC -O3 -march=x86-64-v2 -ffp-contract=off -DRESIZE_LINK_LIB \
    -I "$ROOT/src" -I "$ROOT/upstream" -I "$ROOT/tools" \
    "$ROOT/bench/bench.c" "$BUILD/lib_kern/libstb_image_resize2.a" -lm -o "$BUILD/bench_kern"

echo "== timing base vs avx2 =="
MALLOC_PERTURB_=85 STBIR_CPU=base taskset -c "$PIN" "$BUILD/bench_kern" > "$BUILD/kern_base.csv"
MALLOC_PERTURB_=85 STBIR_CPU=avx2 taskset -c "$PIN" "$BUILD/bench_kern" > "$BUILD/kern_avx2.csv"

python3 - "$ROOT" <<'PY'
import csv, math, sys, os
root = sys.argv[1]
def load(p):
    d = {}
    for r in csv.DictReader(open(p)):
        d[r["row_id"]] = r
    return d
a = load(os.path.join(root, "build/kern_base.csv"))
b = load(os.path.join(root, "build/kern_avx2.csv"))
sp = []
print(f"\n{'row':4s} {'config':34s} {'base ns':>12s} {'avx2 ns':>12s} {'avx2/base':>10s}")
with open(os.path.join(root, "results/kernels.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["row_id", "config", "base_ns_per_resize", "avx2_ns_per_resize", "speedup"])
    for k in a:
        if k not in b: continue
        na = float(a[k]["ns_per_resize"]); nb = float(b[k]["ns_per_resize"])
        conf = f'{a[k]["input_w"]}x{a[k]["input_h"]}->{a[k]["out_w"]}x{a[k]["out_h"]} {a[k]["type"]}/{a[k]["layout"]}'
        r = na / nb
        if a[k]["type"] != "half":
            sp.append(r)
        print(f"{k:4s} {conf:34s} {na:12.0f} {nb:12.0f} {r:9.3f}x")
        w.writerow([k, conf, f"{na:.1f}", f"{nb:.1f}", f"{r:.4f}"])
    gm = math.exp(sum(math.log(x) for x in sp) / len(sp))
    w.writerow(["GEOMEAN", "non-half rows", "", "", f"{gm:.4f}"])
print(f"\navx2 vs base geomean (non-half): {gm:.4f}x ({(gm-1)*100:+.1f}%)")
print("wrote results/kernels.csv")
PY
