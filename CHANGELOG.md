# Changelog

## v1.0.0

First release.

- Runtime AVX2 dispatch for `stb_image_resize2` v2.18; `src/stb_image_resize2.h`
  is byte-identical to upstream.
- Baseline (SSE2) + AVX2 variants with hidden symbols and a canonical dispatch
  layer (22 exported symbols, same ABI as upstream).
- Held-out PGO: +36–38% geomean vs upstream `-O2`; +34.9% isolated AVX2-vs-base.
- Exact tier verified byte-identical over 8,284 checks × 3 dispatch paths;
  ASan+UBSan clean; ABI/non-vacuous/portability gates.
- `half` output falls back to baseline (upstream SSE2/AVX2 signed-zero
  divergence). Pre-existing upstream quirks documented, not fixed.
