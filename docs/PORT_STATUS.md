# PORT_STATUS — what changed and provenance

Oracle: `upstream/stb_image_resize2.h` v2.18, sha256
`173e654634f6ccaad98f603e686ea212eec1fe8ea6d2a5e5e8056efa10ae3880`.
Never edited.

**`src/stb_image_resize2.h` is byte-identical to the oracle.** The speedup is
not a source edit to the resampler; it is delivered by the prebuilt library:

| component | status | change |
|---|---|---|
| `src/stb_image_resize2.h` | unchanged | byte-identical to upstream |
| `lib/resize_base.c` | new | baseline (SSE2) variant, public symbols renamed `*_base` |
| `lib/resize_avx2.c` | new | AVX2 variant (`-march=x86-64-v3`), symbols renamed `*_avx2` |
| `lib/resize_rename.h` | new | suffix-based symbol renaming |
| `lib/resize_dispatch.c` | new | canonical API; CPUID/XGETBV AVX2 dispatch; half output → baseline |
| `lib/stb_image_resize2.c` | unchanged | single-TU build for non-x86 targets / native compile |

Delivered gains, all byte-exact:

1. **Runtime AVX2 dispatch** — the dominant win. The library ships SSE2 and
   AVX2 implementations; AVX2 is selected on x86-64-v3 CPUs. Baseline stays
   drop-in. (+26–34% on uint8/uint16/float; the sRGB regime is +10–14%.)
2. **Recommended flags** — `-O3 -march=x86-64-v2 -ffp-contract=off`.
3. **Held-out PGO** — trained on `tools/train.c` workloads disjoint from
   `bench/bench.c` (+32.8% → +38.0% geomean on the dispatched build).

## Provenance ledger

- Exactness: `harness/diff.sh` (monolithic) and `harness/diff_lib.sh`
  (prebuilt archive), 8,284 checks × 3 dispatch paths.
- ABI: `harness/abi.sh` (22 public symbols exact; internal variants hidden).
- Safety: `harness/sanitize.sh` (2,892 ASan+UBSan runs).
- Non-vacuous: `harness/nonvacuous.sh`.
- Portability: `harness/portable.sh` (scalar/v2/v3/native, C++, 4 cross-targets).
- Headline: `results/summary.csv` (`make bench-vs-upstream`), regime matrix.
- Kernel isolation: `results/kernels.csv` (`make kernels`).
- Downstream: `results/consumer.csv` (`make consumer`).

## Oracle-hash check

```
sha256sum upstream/stb_image_resize2.h
# 173e654634f6ccaad98f603e686ea212eec1fe8ea6d2a5e5e8056efa10ae3880
```
Enforced in CI (job `oracle-integrity`).
