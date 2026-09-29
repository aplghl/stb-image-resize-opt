# stb-image-resize-opt convenience targets. Source scripts/env.sh first so the
# hermetic clang toolchain is on PATH (or override CC).
#
# The oracle is ALWAYS the pristine upstream header; the shipped artifact is the
# runtime-dispatched static library built by scripts/build_opt.sh.

ROOT := $(CURDIR)
ifeq ($(origin CC),default)
CC := $(shell command -v clang 2>/dev/null || command -v gcc 2>/dev/null || echo cc)
endif
export CC

# Recommended exact build (byte-identical; see docs/MEASUREMENT.md).
EXACT_FLAGS ?= -O3 -march=x86-64-v2 -ffp-contract=off

.PHONY: all verify verify-lib verify-dispatch verify-portable abi sanitize \
        nonvacuous bench bench-vs-upstream kernels consumer lib lib-fast clean help

all: verify

## Full correctness gate: monolithic differential + dispatched archive + ABI.
verify: verify-dispatch verify-lib
verify-lib:
	bash scripts/build_opt.sh exact
	bash harness/diff_lib.sh build/lib_exact/libstb_image_resize2.a
	bash harness/abi.sh

## Differential of the in-header candidate vs the oracle.
verify-candidate:
	bash harness/diff.sh $(EXACT_FLAGS)

## Every dispatch path (auto/base/avx2) of separately-built variant objects.
verify-dispatch:
	bash harness/diff_dispatch.sh

## Portability matrix: scalar / ISA variants / C++ / cross-targets.
verify-portable:
	bash harness/portable.sh

## ABI/symbol exactness vs upstream (22 public symbols; variants hidden).
abi:
	bash harness/abi.sh

## ASan+UBSan over the resize matrix.
sanitize:
	bash harness/sanitize.sh

## Prove the differential suite is non-vacuous (corrupt-on-purpose fails).
nonvacuous:
	bash harness/nonvacuous.sh

## Throughput of the dispatched library (writes results/fork.csv).
bench:
	bash harness/bench.sh fork src $(EXACT_FLAGS)

## Head-to-head vs stock upstream -> results/summary.csv (PGO=1 for held-out).
bench-vs-upstream:
	bash harness/bench_vs_upstream.sh

## Isolated AVX2-vs-base -> results/kernels.csv.
kernels:
	bash harness/kernbench.sh

## Downstream consumer program vs the library -> results/consumer.csv.
consumer:
	bash harness/consumer/run.sh

## Build the exact / fast static library.
lib:
	bash scripts/build_opt.sh exact
lib-fast:
	bash scripts/build_opt.sh fast

clean:
	rm -rf build gmon.out .zig-cache zig-out

help:
	@grep -E '^## ' Makefile | sed 's/^## //'
