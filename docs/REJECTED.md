# Rejected / not-taken experiments (measured)

Recorded so they are not retried blindly. i7-14700F, clang 23.1.2, WSL2.

## Rejected

- **AVX-only (`-mavx`, no AVX2) as a variant.** `harness/diff.sh -O3 -mavx`
  diverges from the oracle on **300** checks (2-channel downsample), i.e.
  pristine upstream is not deterministic between SSE2 and its AVX-only code
  path. AVX2 is exact, so the dispatch target is the full x86-64-v3 set; AVX
  alone is never shipped.
- **FMA as an exact option.** `STBIR_USE_FMA` diverges (2,329/8,348) as
  upstream documents; it is available only in the `fast` build.
- **`-march=x86-64-v3` as the default compile.** Byte-exact but emits AVX2/BMI,
  so it is not drop-in for pre-Haswell CPUs. Solved instead with runtime
  dispatch; `-march=x86-64-v2` remains the distribution baseline.
- **`-march=native` as a shipped artifact.** Not portable across consumers.
- **Monolithic (single-TU) PGO.** On the header-included benchmark, held-out
  PGO measured **neutral** (0.9985x) — the code is already well laid out. PGO is
  worthwhile only for the split-TU dispatched library (+32.8% → +38.0%), where
  it improves the variant/dispatch code layout.
- **AVX2 for `half` output.** Pristine upstream's SSE2 and AVX2 paths disagree
  on the sign bit of zero for 48 half-output filter cases; AVX2 cannot be
  byte-exact there, so the dispatcher falls back to baseline for half output.

## Not taken (remaining headroom)

- **sRGB decode/encode kernels** are the weakest AVX2 regime (+10–14% vs
  +26–34% elsewhere); the table-lookup conversion dominates. A dedicated AVX2
  sRGB coder is the next candidate.
- **Extreme downsample coefficient build** (`r8`, `thumb`): the gather path is
  fast, but coefficient generation and memory touching dominate at very small
  outputs. The upstream TODO list also names wide-scanline cache blocking and a
  many-at-once coefficient generator.
- **NEON/AArch64 kernel.** No non-x86 kernel beyond the scalar fallback; the
  AArch64 baseline already has NEON via the header, but runtime dispatch there
  was not attempted.
- **BOLT/Propeller/AutoFDO.** No PMU under WSL2; not attempted.
