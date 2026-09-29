# stb-image-resize-opt

[![CI](https://github.com/aplghl/stb-image-resize-opt/actions/workflows/ci.yml/badge.svg)](https://github.com/aplghl/stb-image-resize-opt/actions/workflows/ci.yml)
[![Latest release](https://img.shields.io/github/v/release/aplghl/stb-image-resize-opt)](https://github.com/aplghl/stb-image-resize-opt/releases/latest)
[![License: MIT OR Unlicense](https://img.shields.io/badge/license-MIT%20OR%20Unlicense-blue.svg)](#license)

A performance fork of [stb_image_resize2](https://github.com/nothings/stb)
(`stb_image_resize2.h` v2.18) that is a **byte-identical drop-in replacement**.
The resampler source is *unchanged*; the speedup comes from a prebuilt static
library that ships both a baseline (SSE2) and an **AVX2** implementation and
selects the AVX2 one at runtime on capable CPUs, plus held-out PGO.

- **Upstream base:** `nothings/stb` `stb_image_resize2.h` v2.18, vendored in
  `upstream/` as the pristine oracle. `sha256`:
  `173e654634f6ccaad98f603e686ea212eec1fe8ea6d2a5e5e8056efa10ae3880`.
- **License:** MIT OR Unlicense, same as upstream.
- **Status:** the exact tier is byte-identical to upstream on every dispatch
  path; the AVX2 path is used for all output datatypes except `half` (see
  limitations).

`src/stb_image_resize2.h` is byte-identical to the oracle. Only new library
sources under `lib/` are added; the public API, structs and ABI are unchanged
(22 exported symbols, verified by `make abi`).

## Results

Stock upstream is compiled the way consumers compile it (`-O2`). The fork
library is `-O3 -march=x86-64-v2 -ffp-contract=off` with runtime AVX2 dispatch
and **held-out** PGO. Intel i7-14700F, clang 23.1.2, WSL2. Reproduce with
`make bench-vs-upstream`, `make kernels`, `make consumer`.

| workload | speedup vs upstream `-O2` |
| --- | --- |
| Mixed suite (`results/summary.csv`) | **+36–38% geomean** (held-out PGO; +32.8% no PGO) |
| AVX2 vs base, isolated (`results/kernels.csv`) | **+34.9% geomean** (non-half) |
| uint8 / uint16 / float | 1.2–1.7× per row |
| sRGB (weakest regime) | 1.10–1.22× |
| `half` output | 1.0× (baseline by design) |
| downstream consumer (`make consumer`) | **1.23×** (held-out PGO; honest lower bound) |

The sRGB conversion path and extreme-downsample coefficient build are the
remaining headroom (`docs/REJECTED.md`).

## How it works

`lib/` compiles the header twice — baseline and `-march=x86-64-v3` AVX2 — with
each variant's public symbols renamed (`*_base`, `*_avx2`), plus a small
dispatch layer that exposes the canonical API and picks a variant with
self-contained CPUID/XGETBV (no `__builtin_cpu_supports`). Variant symbols are
hidden, so the exported ABI is exactly upstream. `STBIR_CPU=base|avx2` forces a
path for testing. The exact tier required `-ffp-contract=off`; scalar, SSE2,
AVX2, `-march=native` and C++ were all verified byte-identical.

## Usage

### Option 1 — prebuilt static library (recommended)

```sh
scripts/build_opt.sh exact          # byte-identical
PGO=1 scripts/build_opt.sh exact    # + held-out PGO
# or:
zig build -Doptimize=ReleaseFast
zig build -Doptimize=ReleaseFast -Dtarget=aarch64-linux-musl
```

Then include the (unchanged) header **without** the implementation macro and
link the archive:

```c
#include "stb_image_resize2.h"   /* declarations only */
/* ... stbir_resize(...), stbir_resize_extended(...) ... */
```
link `build/lib_exact/libstb_image_resize2.a` (or `zig-out/lib/...`).

### Option 2 — header only

Drop in `src/stb_image_resize2.h` exactly as upstream and compile with
`-O3 -march=x86-64-v2 -ffp-contract=off`. You get the baseline build; the AVX2
dispatch requires the prebuilt library (or compiling the consumer itself with
`-march=x86-64-v3`, which is not portable).

## Verifying

The harness needs only a C compiler (and `zig` for the cross-target checks).

```sh
make verify             # dispatched archive differential (8,284 x 3 paths) + ABI
make verify-candidate   # in-header candidate differential
make verify-portable    # scalar / v2 / v3 / native / C++ / 4 cross-targets
make sanitize           # ASan+UBSan over the resize matrix
make nonvacuous         # corrupt-on-purpose must FAIL the suite
make kernels            # isolated AVX2-vs-base -> results/kernels.csv
make bench-vs-upstream  # held-out PGO vs upstream -O2 -> results/summary.csv
make consumer           # downstream integration benchmark -> results/consumer.csv
```

`upstream/` is the pristine oracle (never edited); the candidate is `src/`
plus `lib/`.

## Compatibility

| configuration | result |
| --- | --- |
| x86-64 (SSE2 baseline) | byte-exact; AVX2 auto-used on v3 CPUs |
| pre-Haswell x86-64 | byte-exact; baseline only |
| non-x86 (aarch64, ...) | byte-exact; single scalar/NEON build |
| `-DSTBIR_NO_SIMD` | byte-exact, scalar fallback |
| C and C++ | compiles clean |
| gcc, clang, `zig cc` | byte-exact |
| aarch64-linux-musl, x86_64-windows-gnu, aarch64-macos | cross-compiles |

## Known upstream quirks (documented, not fixed)

- An uninitialized read in `uint16 … WRAP … BOX` upsample and some `half`
  filter paths; `STBIR_PROFILE` masks it. `docs/MEASUREMENT.md`.
- SSE2 vs AVX2 differ in the sign bit of zero for 48 `half`-output cases;
  the dispatcher falls back to baseline for `half` output.
- UBSan false positive in the sRGB encode table lookup (pre-offset pointer).

## Repository layout

```
src/stb_image_resize2.h   unchanged drop-in header (= oracle)
lib/                      baseline / AVX2 variants + dispatch layer
upstream/                 pristine nothings/stb (oracle, never edited)
tools/resize_dump.c       differential driver (raw output bytes)
tools/train.c             held-out PGO training driver
bench/bench.c             throughput regime matrix
harness/                  diff, dispatch, abi, sanitize, nonvacuous, portable,
                          bench*, kernbench, consumer/
scripts/                  env.sh, build_opt.sh
build.zig Makefile        multi-target / convenience targets
docs/                     MEASUREMENT.md, REJECTED.md, PORT_STATUS.md
results/                  benchmark CSVs (see results/README.md)
```

## License

MIT OR Unlicense, same as upstream. Based on
[stb](https://github.com/nothings/stb) by Sean Barrett, Jeff Roberts, Jorge L
Rodriguez and contributors. This is an unofficial fork and is **not endorsed by
the upstream author**.
