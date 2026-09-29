# Measurement & environment audit

Target: **Intel Core i7-14700F** (Raptor Lake; AVX2 + FMA + BMI2 + F16C, no
AVX-512), WSL2 Ubuntu 24.04. Oracle: `nothings/stb` `stb_image_resize2.h` v2.18,
`sha256 = 173e654634f6ccaad98f603e686ea212eec1fe8ea6d2a5e5e8056efa10ae3880`.

## Toolchain (Layer 0.1 audit)

| Tool | Availability | Notes |
|---|---|---|
| gcc | 13.3.0 on PATH | system fallback |
| clang + LLD | 23.1.2 (hermetic, user space) | primary; `scripts/env.sh` |
| zig | 0.16.0 | cross-compilation |
| valgrind / callgrind | 3.22.0 | present, but unusable on this host: the system `ld.so` is stripped and `strlen` redirection fails (needs `libc6-dbg`). ASan/UBSan carry safety instead. |
| clang ASan/UBSan | present | used for `make sanitize` |
| llvm-mca / llvm-bolt | present in the LLVM tarball | available |
| `perf` | absent (WSL2, no PMU) | AutoFDO/BOLT out of reach |

## Observable Output Contract (OOC)

`stb_image_resize2` is a **pixel transform** (not a codec): it consumes input
pixels and produces an output pixel buffer.

- **exact tier** — the output buffer is **byte-identical** to the pristine
  oracle for the same input and parameters. `-ffp-contract=off`, no fast-math.
- **fast tier** — `fast` build (`-ffast-math` + `STBIR_USE_FMA` in the AVX2
  variant): numerically near-identical; must not be used where `-0.0` vs `+0.0`
  or reassociation matters. Not the default.

## Clock / units

Throughput is reported in **nanoseconds** (min over repeats, pinned with
`taskset`). **Ratios only**; `rdtsc` ticks are never called "cycles" (the TSC
is not the core clock on this host). Sub-~3% per-row differences are noise.

## Corpora

- Generated (`tools/gen.h`, deterministic): `grad`, `plasma`, `random`, `flat`
  at sizes including 1x1, 1x7, 7x1, 3x5, 17x33, 64x48.
- Real images (`corpus/`, reused from the sibling forks) and
  `upstream/tests/pngsuite/primary`.
- The differential matrix (`harness/diff.sh`) sweeps 5 datatypes × 14 layouts ×
  4 edge modes × 7 filters (incl. a user callback) × subrects × padded/negative
  strides × split counts × mixed in/out types: **8,284 checks**, run on every
  dispatch path (`auto`/`base`/`avx2`).

## Exactness findings (flag A/B vs the `-O2` oracle)

| candidate flags | exact? | notes |
|---|---|---|
| `-O2`, `-O3` | yes | baseline |
| `-O3 -march=x86-64-v2` | yes | distribution default |
| `-O3 -march=x86-64-v3` | **yes** | AVX2; +26–34% (see below) |
| `-O3 -march=native` | yes | not drop-in |
| `-O2/-O3 -DSTBIR_NO_SIMD` | yes | scalar is bit-identical to SSE2 |
| `-O3 -mavx` (AVX without AVX2) | **no** (300 cases) | upstream determinism bug in the AVX-only 2-channel downsample; never shipped |
| `-O3 -march=v3 -DSTBIR_USE_FMA` | no | documented fast tier |

## Dispatch decision

`-march=x86-64-v3` is byte-exact and large (+26–34%), but emits AVX2/BMI/FMA, so
it is not drop-in. Baseline SSE2 is bit-identical to scalar and needs no
dispatch. The library therefore compiles **two implementations** — a baseline
(SSE2) and an AVX2 variant — plus a small dispatch layer that selects the AVX2
variant at runtime via self-contained CPUID/XGETBV (no `__builtin_cpu_supports`,
which is unresolvable under `zig cc`). The required feature set is the full
x86-64-v3 set (checked explicitly). `STBIR_CPU=base|avx2` forces a path.

## Pre-existing upstream quirks (documented, oracle untouched)

1. **Uninitialized read in `uint16 … WRAP … BOX` upsample and the `half`
   filter paths.** `STBIR_PROFILE` changes internal struct layout/allocation and
   masks it; with it off, output depends on heap contents (reproduced with
   `MALLOC_PERTURB_`). The differential harness runs with `MALLOC_PERTURB_=85`
   so both binaries read identical bytes, and the oracle is built with the same
   (no) profile setting as the library. This is a real upstream bug, not
   introduced here.
2. **Signed zero differs between the SSE2 and AVX2 code paths for a few
   `half`-output filter cases** (`0x8000` vs `0x0000`, numerically identical).
   To keep the exact tier byte-identical, the dispatcher falls back to the
   baseline for `STBIR_TYPE_HALF_FLOAT` output. AVX2 is used for all other
   output datatypes.
3. **UBSan false positive** in the sRGB encode coders: they index
   `fp32_to_srgb8_tab4` through a deliberately pre-offset pointer, tripping the
   declared-array bounds check on in-range reads. Scoped out with a
   function-scoped `-fsanitize-ignorelist` (`harness/ubsan_ignorelist.txt`).
4. **AVX-only (no AVX2)** differs from SSE2/scalar on 2-channel downsample;
   documented in `REJECTED.md`.

## Measurement caveats

- Resize is compute-bound; there are no plumbing/per-byte-callback rows in the
  throughput suite. Rows are the headline.
- Layout/PGO can move a row a few percent with identical source; the
  dispatch-path kernel isolation (`make kernels`) is where the mechanism lives.
