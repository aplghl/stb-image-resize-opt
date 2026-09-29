#!/bin/bash
# Build and run the downstream consumer against the stock upstream header and
# against the prebuilt fork library, then commit the speedup to
# results/consumer.csv. usage: [PGO=1] harness/consumer/run.sh
set -eu
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
BUILD="$ROOT/build/consumer"; mkdir -p "$BUILD" "$ROOT/results"
CC=${CC:-clang}
PIN=${PIN:-3}
PGO=${PGO:-0}
IMG=${IMG:-"$ROOT/corpus/plasma_512.png"}
[ -e "$IMG" ] || IMG="$ROOT/corpus/grad_640_480.png"

echo "== consumer: stock upstream -O2 =="
$CC -O2 -I "$ROOT/upstream" "$ROOT/harness/consumer/consumer.c" -lm -o "$BUILD/stock"

echo "== consumer: fork library (pgo=$PGO) =="
PGO=$PGO bash "$ROOT/scripts/build_opt.sh" exact "$BUILD/lib" >/dev/null
$CC -O3 -march=x86-64-v2 -ffp-contract=off -DRESIZE_LINK_LIB \
    -I "$ROOT/src" -I "$ROOT/upstream" "$ROOT/harness/consumer/consumer.c" \
    "$BUILD/lib/libstb_image_resize2.a" -lm -o "$BUILD/fork"

python3 - "$ROOT" "$BUILD/stock" "$BUILD/fork" "$IMG" "$PIN" <<'PY'
import subprocess, sys, csv, math, os
root, stock, fork, img, pin = sys.argv[1:6]
def run(b):
    out = subprocess.run(["taskset","-c",pin,b,img], capture_output=True, text=True).stdout.strip().splitlines()
    return {r.split(",")[0]: float(r.split(",")[1]) for r in out}
s, f = run(stock), run(fork)
rows = []
print(f"\n{'config':8s} {'stock ns/px':>12s} {'fork ns/px':>12s} {'speedup':>8s}")
for k in ["down2","down4","same","up2","thumb"]:
    r = s[k]/f[k]; rows.append((k,s[k],f[k],r))
    print(f"{k:8s} {s[k]:12.5f} {f[k]:12.5f} {r:7.3f}x")
gm = math.exp(sum(math.log(x[3]) for x in rows)/len(rows))
print(f"\nconsumer geomean: {gm:.4f}x ({(gm-1)*100:+.1f}%)")
with open(os.path.join(root,"results/consumer.csv"),"w") as o:
    o.write("config,stock_ns_per_pixel,fork_ns_per_pixel,speedup\n")
    for k,a,b,r in rows: o.write(f"{k},{a},{b},{r}\n")
    o.write(f"GEOMEAN,,,{gm}\n")
PY
