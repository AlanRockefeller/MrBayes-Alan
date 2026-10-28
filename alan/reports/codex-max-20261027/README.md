# Codex max optimization evidence, 2026-10-27

These reports record the optimization campaign in
`/usr/local/src/mrbayes-3.2.9-dikarya2`, using real saved Dikarya inputs. The
frozen previous executable matches the production `3.2.9-dikarya2` executable.
Stock is upstream MrBayes 3.2.9 at `4f741bc` and BEAGLE 4.0.1 at `bf35ad4a`,
with native architecture tuning disabled. CPU time is process user plus system
time. All times are seconds, except the explicitly labelled microbenchmarks.

- `benchmark.json` and `benchmark-raw.json`: latest versus frozen previous build,
  two runs per build per case in ABBA order, at one and two requested threads.
- `readme-stock-summary.json` and `readme-stock-raw.json`: direct stock versus
  latest, three cases and two runs per build in ABBA order, one requested thread.
  All six latest-versus-stock comparisons and all three stock repeats matched.
  The raw file includes each command and normalized output hashes.
- `validation-summary.json`: 131 canonical comparisons against the frozen
  previous build, including two expected malformed-input failures. This count
  excludes the new direct-stock comparisons.
- `kernel-hashes.json`: fingerprints of the public-API harness's binary output;
  the final and reference outputs match at 227,404,800 bytes. Other entries are
  earlier experiment fingerprints, not extra independent final checks.
- `micro-baseline.jsonl` and `micro-indexed-o2.jsonl`: kernel timings; `SS` is two
  tips, `SP` a tip and partials, `PP` two partials, and `tiny` selects subnormals.
- `build-manifest.json`: hashes of the original campaign's binaries and source
  files. These identify those measured artifacts, not a fresh build from this
  repository, which also retains fixes already present on `main`.

The reports contain timings and fingerprints, not submitted sequence data or
sampled trees. Saved production inputs and generated analysis files are local
campaign artifacts and are not included in this repository.

## Reproduce the public API comparison

The build script runs its synthetic end-to-end self-tests automatically. It
leaves stock and patched BEAGLE under its `SRC` directory (by default
`$HOME/mrbayes-3.2.9-dikarya2-build`). With that build directory available, run
from this repository's root:

```sh
set -eu
build_dir="$HOME/mrbayes-3.2.9-dikarya2-build"
g++ -std=c++11 -O2 -ffp-contract=off -I"$build_dir/bg-dikarya2" \
  alan/check-kernels.cpp -L"$build_dir/bg-stock-inst/lib" -lhmsbeagle \
  -o /tmp/mrbayes-check-kernels
LD_LIBRARY_PATH="$build_dir/bg-stock-inst/lib" /tmp/mrbayes-check-kernels > /tmp/mb-stock.bin
LD_LIBRARY_PATH="$build_dir/bg-dikarya2-inst/lib" /tmp/mrbayes-check-kernels > /tmp/mb-latest.bin
cmp /tmp/mb-stock.bin /tmp/mb-latest.bin
```

The harness uses deterministic synthetic values, covers transition matrices,
short and odd pattern counts, rate categories, missing states, subnormal
partials, scaling, buffer reuse and aliased updates, and writes binary doubles.
There is no numeric tolerance. `alan/bench-kernels.cpp` can be compiled the same
way to reproduce the kernel timing method; replace the source filename and omit
binary-output redirection.

## Scope

The original campaign preceded integration into this repository. The PR keeps
`main`'s Windows/POSIX thread guards, per-analysis chain reset and scalar-thread
fixes and applies the new arithmetic-preserving optimizations. It also fixes an
existing aliased-update fallback: a speculative underflow pass could overwrite
its input before a retry. Campaign API fingerprints compare against the previous
optimized library, which shares that behavior. The integrated build is checked
directly against stock, including the aliased updates, so its API fingerprint
differs from that historical optimized-library fingerprint. Integration checks
are recorded separately in `integration-validation.json`.
Do not treat historical two-thread wall times as a new benchmark of the latest
build, or multiply ratios from different campaigns to estimate a speedup.
