#!/bin/bash
# Head-to-head resize throughput: stock upstream -O2 vs the fork's prebuilt
# runtime-dispatched static library. Optionally held-out PGO (PGO=1).
# Writes results/upstream_o2.csv, results/fork.csv, results/summary.csv.
#
# usage: [PGO=1] harness/bench_vs_upstream.sh
set -eu
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BUILD="$ROOT/build"; mkdir -p "$BUILD" "$ROOT/results"
CC=${CC:-clang}
PIN=${PIN:-3}
PGO=${PGO:-0}
MODE=${MODE:-exact}
ORACLE_FLAGS=${ORACLE_FLAGS:-"-O2"}

echo "== building upstream -O2 =="
# shellcheck disable=SC2086
$CC $ORACLE_FLAGS -ffp-contract=off -I "$ROOT/upstream" -I "$ROOT/tools" \
    "$ROOT/bench/bench.c" -lm -o "$BUILD/bench_up"

echo "== building fork library ($MODE, pgo=$PGO) =="
PGO=$PGO bash "$ROOT/scripts/build_opt.sh" "$MODE" "$BUILD/lib_bench" >/dev/null

$CC -O3 -march=x86-64-v2 -ffp-contract=off -DRESIZE_LINK_LIB \
    -I "$ROOT/src" -I "$ROOT/upstream" -I "$ROOT/tools" \
    "$ROOT/bench/bench.c" "$BUILD/lib_bench/libstb_image_resize2.a" -lm -o "$BUILD/bench_fork"

echo "== timing =="
taskset -c "$PIN" "$BUILD/bench_up"   > "$ROOT/results/upstream_o2.csv"
taskset -c "$PIN" "$BUILD/bench_fork" > "$ROOT/results/fork.csv"

python3 - "$ROOT" "$PGO" <<'PY'
import csv, math, os, sys
root, pgo = sys.argv[1], sys.argv[2]
def load(p):
    d = {}
    with open(os.path.join(root, p), newline="") as f:
        for row in csv.DictReader(f):
            d[row["row_id"]] = row
    return d
a = load("results/upstream_o2.csv")
b = load("results/fork.csv")
rows = []
for k in a:
    if k not in b: continue
    na = float(a[k]["ns_per_resize"]); nb = float(b[k]["ns_per_resize"])
    rows.append((k, a[k], na, nb, na/nb))
print(f"\n{'row':4s} {'in':>10s} {'out':>10s} {'type':6s} {'lay':4s} {'filt':8s} {'up ns':>12s} {'fork ns':>12s} {'speedup':>8s}")
for k, r, na, nb, sp in rows:
    print(f"{k:4s} {r['input_w']+'x'+r['input_h']:>10s} {r['out_w']+'x'+r['out_h']:>10s} "
          f"{r['type']:6s} {r['layout']:4s} {r['filter']:8s} {na:12.0f} {nb:12.0f} {sp:7.3f}x")
ups   = [sp for _,_,_,_,sp in rows]
gm = math.exp(sum(math.log(x) for x in ups)/len(ups))
up2 = [sp for _,r,_,_,sp in rows if int(r['out_w'])*int(r['out_h']) > int(r['input_w'])*int(r['input_h'])]
dn2 = [sp for _,r,_,_,sp in rows if int(r['out_w'])*int(r['out_h']) < int(r['input_w'])*int(r['input_h'])]
upg = math.exp(sum(math.log(x) for x in up2)/len(up2)) if up2 else float('nan')
dng = math.exp(sum(math.log(x) for x in dn2)/len(dn2)) if dn2 else float('nan')
print(f"\ngeomean speedup: {gm:.4f}x ({(gm-1)*100:+.1f}%)   upsample gm: {upg:.3f}x   downsample gm: {dng:.3f}x")
print(f"PGO: {'held-out' if pgo=='1' else 'off'}   mode: {os.environ.get('MODE','exact')}")
with open(os.path.join(root,"results/summary.csv"),"w",newline="") as f:
    w = csv.writer(f)
    w.writerow(["row_id","input_w","input_h","out_w","out_h","type","layout","filter",
                "upstream_ns_per_resize","fork_ns_per_resize","speedup"])
    for k, r, na, nb, sp in rows:
        w.writerow([k,r["input_w"],r["input_h"],r["out_w"],r["out_h"],r["type"],r["layout"],
                    r["filter"],f"{na:.1f}",f"{nb:.1f}",f"{sp:.4f}"])
    w.writerow(["GEOMEAN","","","","","","","","","",f"{gm:.4f}"])
    w.writerow(["UPSAMPLE_GEOMEAN","","","","","","","","","",f"{upg:.4f}"])
    w.writerow(["DOWNSAMPLE_GEOMEAN","","","","","","","","","",f"{dng:.4f}"])
print("wrote results/summary.csv")
PY
