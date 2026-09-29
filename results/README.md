# results

Benchmark CSVs from the fork. Timings are machine-specific (Intel i7-14700F,
WSL2, clang 23.1.2) and load-sensitive; **ratios** are the signal. Correctness
is verified separately (`make verify`, `make abi`, `make sanitize`,
`make nonvacuous`, `make verify-portable`).

| file | generator | contents |
| ---- | --------- | -------- |
| `summary.csv` | `make bench-vs-upstream` | **headline**: upstream `-O2` vs the dispatched library (`-O3 -march=x86-64-v2 -ffp-contract=off`, held-out PGO), per row + geomean |
| `upstream_o2.csv` | `make bench-vs-upstream` | baseline throughput, upstream header at `-O2` |
| `fork.csv` | `make bench-vs-upstream` | fork throughput |
| `kernels.csv` | `make kernels` | isolated AVX2-vs-base (`STBIR_CPU`) on the same library |
| `consumer.csv` | `make consumer` | downstream program (load → resize → sRGB RGBA) linking the real `.a` vs stock |

Columns: `summary`/`upstream_o2`/`fork` =
`row_id,input_w,input_h,out_w,out_h,type,layout,edge,filter,iters,ns_per_resize,ns_per_outpixel,Mpx_per_s`;
`kernels` = `row_id,config,base_ns_per_resize,avx2_ns_per_resize,speedup`;
`consumer` = `config,stock_ns_per_pixel,fork_ns_per_pixel,speedup`.

Key numbers (regenerate with the commands above):

- Runtime **AVX2 vs base** (isolated, `kernels.csv`): **+34.9% geomean** on
  non-half rows; half output is deliberately baseline (signed-zero exactness).
- **Upstream `-O2` vs dispatched library**: ~**+33-38%** geomean (held-out PGO),
  with the sRGB regime at +10-14%.
- **Downstream consumer** (sRGB RGBA, links the `.a`): **~1.16x** — the honest
  lower bound, because sRGB conversion is the weakest AVX2 regime.

> The sRGB regime and extreme downsample coefficient build are the remaining
> headroom (`docs/REJECTED.md`).
