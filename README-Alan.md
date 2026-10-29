# MrBayes-Alan

MrBayes 3.2.9, optimized by Alan Rockefeller for [Dikarya](https://dikarya.us).
The original pass used Claude Opus 5.5; the newest pass used **Codex with max
reasoning (“codex max”), 2026-10-27**.

The latest changes accelerate transition matrices, reuse tip-state products and
skip zero-weight terms in the default branch-length prior while preserving the
original arithmetic. Build this checkout with `bash alan/build-mrbayes-alan.sh`;
it includes the matching BEAGLE patch and checks output identity before installing.

The measurements below come from the optimization workspace and its frozen
production `dikarya2` reference. This checkout packages the same optimizations
on top of `main`, preserving its additional portability and threading fixes,
and adds an alias fallback found during integration. The timing tables precede
that fallback; they identify the measured campaign builds.
The [evidence notes](alan/reports/codex-max-20261027/README.md) distinguish the
campaign measurements from checks of the integrated repository build.

## How much faster?

There are two different comparisons: the total improvement over stock, and the
additional improvement over the already optimized `dikarya2` build. A speedup is
**reference time / latest time**. CPU-time reduction is
**100 × (1 − latest CPU time / reference CPU time)**; these percentages are not
interchangeable with a percentage increase in throughput.

### Latest build versus stock MrBayes

“Stock” here means **MrBayes 3.2.9, commit `4f741bc`, with BEAGLE 4.0.1's ordinary
CPU kernels**. The campaign reference disables `-march=native`, matching the
distribution's baseline x86-64 configuration. The repository build script also
passes `-ffp-contract=off` explicitly to both MrBayes and BEAGLE to disable
floating-point contraction independently of architecture tuning. The campaign reference was
`build-mb-stock/src/mb` with `bg-stock-inst/lib` in the optimization workspace.
It includes BEAGLE acceleration.

The direct comparison uses three real Dikarya alignments, GTR+Gamma, two runs
of four chains, fixed production seeds, samplefreq=500, and one requested thread.
The host exposes two virtual CPUs (Xeon E5-2690 v4, AVX2), running Ubuntu 24.04
with GCC 13.3.
Each build runs twice in stock/latest/latest/stock order. Every comparison also
checks all output files and stdout. CPU time is user plus system time for the
MrBayes process; wall time includes interference from other work on the host.

**The latest build provides 1.76–2.08× CPU throughput versus stock, using
43.3–51.9% less CPU time on these one-thread workloads.** Measured wall-clock
speedups were **1.81–2.16×** in this campaign.

| Input | Generations | Stock CPU s | Latest CPU s | CPU speedup | CPU time saved | Stock wall s | Latest wall s | Wall speedup |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| yb4s | 25,000 | 21.80 | 11.20 | 1.947× | 48.6% | 22.68 | 11.29 | 2.008× |
| tr9b | 10,000 | 23.08 | 11.09 | 2.080× | 51.9% | 24.31 | 11.25 | 2.162× |
| cad46114 | 2,000 | 23.33 | 13.23 | 1.763× | 43.3% | 24.31 | 13.45 | 1.807× |

See the [stock comparison summary](alan/reports/codex-max-20261027/readme-stock-summary.json)
and [individual measurements and commands](alan/reports/codex-max-20261027/readme-stock-raw.json).
The stock samples encountered more background work than the latest-build samples,
so wall-time ratios include that scheduling effect; CPU ratios show the reduction
in process CPU consumption. This campaign also had less contention than the
previous-build campaign below, so compare builds within each table.

These are whole analyses at the listed generation counts, including setup and
summaries. They are measurements of these workloads, not a claim that every
alignment gets the same speedup. The direct stock timing campaign above is at
one thread. Separate integration checks also compare the latest build with
stock at two threads; those are correctness checks rather than a new timing
campaign.

### Latest build versus the previous optimization

The previous reference is the original **MrBayes 3.2.9-dikarya2 + BEAGLE
4.0.1-dikarya2** pair. The frozen executable in `optimization/baseline/mb` is
byte-identical to `/opt/mrbayes-3.2.9-dikarya2/bin/mb`. Both previous and latest
builds retain those version banners; use the
[build manifest](alan/reports/codex-max-20261027/build-manifest.json) to identify the new
binary and library hashes.

The preceding optimization campaign measured the following ABBA averages on
this two-vCPU host. It had substantial background computation, typically around
one CPU. **At one thread, the newest changes provide 1.021–1.081× CPU throughput,
or 2.1–8.1% faster CPU throughput and 2.0–7.5% less CPU time.**

| Input | Threads | Generations | Previous CPU s | Latest CPU s | CPU speedup | Previous wall s | Latest wall s |
|---|---:|---:|---:|---:|---:|---:|---:|
| yb4s — 36 taxa, 394 patterns | 1 | 25,000 | 13.72 | 12.95 | 1.059× | 19.29 | 17.92 |
| tr9b — 129 taxa, 209 patterns | 1 | 10,000 | 13.94 | 12.89 | 1.081× | 17.36 | 17.91 |
| cad46114 — 188 taxa, 1191 patterns | 1 | 2,000 | 15.79 | 15.46 | 1.021× | 20.89 | 21.31 |
| yb4s | 2 | 25,000 | 14.14 | 14.54 | 0.973× | 17.14 | 14.91 |
| tr9b | 2 | 10,000 | 15.45 | 13.60 | 1.136× | 14.43 | 14.85 |
| cad46114 | 2 | 2,000 | 15.98 | 16.16 | 0.989× | 15.04 | 16.13 |

**Two-thread results were mixed.** Some cases used more CPU or took longer;
these measurements do not establish a consistent two-thread latency gain.
The original and latest builds also adapt between concurrent and sequential
chain evaluation according to measured performance, which adds scheduling
variability under contention. See the [summary](alan/reports/codex-max-20261027/benchmark.json)
and [individual measurements](alan/reports/codex-max-20261027/benchmark-raw.json).

### What the previous optimization had already achieved

The earlier Dikarya report recorded these quiet-host wall times against stock
3.2.9, before the newest Codex pass:

| Input | Generations | Stock 3.2.9 | Previous dikarya2, 1 thread | Previous dikarya2, 2 threads |
|---|---:|---:|---:|---:|
| yb4s | 100,000 | 85.3 s | 45.5 s | 28.8 s |
| 27d6 | 100,000 | 65.8 s | 37.7 s | 23.5 s |
| tr9b | 30,000 | 59.7 s | 33.5 s | 21.0 s |
| 1nek | 30,000 | 64.2 s | 37.1 s | 24.3 s |
| s33 | 6,000 | 47.3 s | 27.3 s | 19.3 s |
| cad46114 | 3,000 | 36.0 s | 21.3 s | 15.1 s |

That previous build was already about **1.7–1.9× faster at one thread and
2.4–3.0× faster at two threads** than stock 3.2.9 in those tests. These historical
wall times come from `/var/www/dikarya/ops/mrbayes/README.md`; they are not new
measurements of the latest build. They must not be multiplied by the newest
CPU-time ratios to manufacture a total wall-clock speedup.

Ubuntu's older `/usr/bin/mb` is a separate 3.2.7a reference. Upstream's later
NodeSlider proposal-ratio fix changes MCMC results, so output identity with
3.2.7a is not the target. Exactness is checked against the same 3.2.9 inference
algorithm.

## What changed in the Codex max pass

### 1. Exact SIMD transition matrices

The four-state eigen-decomposition coefficients are transposed and cached when
the eigen decomposition changes. AVX2 then evaluates 16 independent transition
matrix entries in parallel. Each entry retains the original four float products,
the initial zero and the same addition order. The exponential's argument still
uses the original casts and double arithmetic; `exp(0)` is exactly one. The
original positivity/NaN clamp and matrix padding are preserved.

Code: patch 7 in [the BEAGLE patch series](alan/beagle-4.0.1-dikarya2.patch),
covering `DikaryaAVX.h`, `EigenDecompositionCube.h` and
`EigenDecompositionCube.hpp` in `libhmsbeagle/CPU/`.

### 2. Reuse the 25 possible tip-state products

Four nucleotide states plus the missing-state code give only 25 possible pairs
of tip states. For pattern ranges of at least 128 sites, the code validates and
calculates each site's `5*state1+state2` index once and reuses it across rate
categories. Each category calculates the 25 four-state products once; each site
copies its product and applies its own fixed scale factor when required.

Short ranges and unusual state values retain the original calculation. Existing
exact underflow recovery remains active. Index scratch is private to each thread,
and its TLS accessor stays outside the hot loops. The API microbenchmark measured
approximately **2× faster two-tip kernels at 209 and 394 patterns**; that is a
kernel result, not a 2× whole-program improvement.

Code: patch 7, `DikaryaAVX.h`, in [the BEAGLE patch series](alan/beagle-4.0.1-dikarya2.patch).

### 3. Skip zero-weight log-length sums in the default prior

For the default gamma-Dirichlet branch prior, `a=c=1`, so both log-length-sum
coefficients are zero. When every branch length is finite and positive, those
sums are finite and contribute zero. The new path skips their calculation while
summing tree length in precisely the original order and retaining the remaining
prior expression. Other parameters, zero, negative or nonfinite lengths use the
original path; `0*log(0)`, for example, must retain its original NaN behavior.

Code: [LogDirPrior in mcmc.c](src/mcmc.c).

### 4. Reproducible build and library loading

The retained build uses **O2**, disables floating-point contraction, retains
runtime AVX2 dispatch, and keeps MrBayes's debug/numerical failure checks enabled.
BEAGLE embeds `$ORIGIN` in its library path so the packaged executable finds its
own CPU plugin. Standalone runs without `LD_LIBRARY_PATH` were checked against
the reference and the loader trace confirms the intended library is loaded.

The previous optimizations remain: exact AVX2 likelihood and parsimony kernels,
guarded handling of subnormal arithmetic, cached branch-length logarithms,
concurrent chain likelihoods with proposals/random draws on the main thread, and
pattern threading that preserves the likelihood reduction order.

### 5. Preserve the original kernel for aliased partial updates

An extra integration check found an existing edge case in the previous optimized
library: if the destination is also a partials input, a speculative underflow
pass can overwrite that input before retrying. All 840 matrix/non-aliased harness
records matched stock, while 131 aliased records differed. Patch 8 sends aliased
tip/partials and partials/partials updates, including fixed scaling, through the
original kernel. Ordinary updates with separate input and output buffers retain
the optimized path.

## Byte-identical results and the scope of validation

The previous-build comparison passed **131 canonical run comparisons**: 53 saved
production inputs at one and two threads, eight model/prior variants, a large
single-chain pattern-threading case, scalar fallbacks, the actual website default
invocation, and the standalone package. Two comparisons intentionally preserve
an existing malformed-input failure.

The new direct-stock benchmark adds **six latest-versus-stock comparisons**, all
identical after the same metadata normalization, plus three matching stock reruns.
These are recorded separately from the 131 canonical comparisons.

The independent public-BEAGLE-API harness returned **227,404,800 byte-identical
bytes** against the frozen library. It covers transition matrices, normal and
subnormal values, missing states, short/odd pattern counts, scale factors,
buffer reuse and aliased updates.

The comparator checks every output file and stdout. Its established timing,
clock/run-ID and version/resource-banner fields are normalized; scientific
output, checkpoints, seeds, diagnostics and exit status must match without a
numeric tolerance. Most canonical validation runs use 1,000–2,000 generations;
the longer benchmark cases use 10,000–25,000. These are not million-generation
validation claims.

Evidence: [validation summary](alan/reports/codex-max-20261027/validation-summary.json),
[API fingerprints](alan/reports/codex-max-20261027/kernel-hashes.json),
[build manifest](alan/reports/codex-max-20261027/build-manifest.json), and the
[evidence and reproduction notes](alan/reports/codex-max-20261027/README.md).

The integrated repository build is checked separately, including the alias
fallback: 11 synthetic model/chain/thread comparisons and staged/native install
checks, six real-input comparisons against stock at one and two threads, and
227,404,800 raw API bytes matching stock with AVX2 enabled and disabled. These
checks and the integrated artifact hashes are in the
[integration validation](alan/reports/codex-max-20261027/integration-validation.json).

## Experiments that were rejected

- **Subtree site grouping:** output matched, but group construction and
  gathering/scattering partials cost more than the arithmetic saved. Cached
  maps and sparse evaluation still did not win on the tested alignments.
- **O3, native scheduling and profile-guided optimization:** no reliable overall
  win on this host. O2 is the retained configuration.
- **0- and 50-microsecond spin waits:** different cases benefited differently.
  The original 300-microsecond default remains unchanged.

The rejected experiment artifacts remain in the development workspace's
`optimization/experiments/` and `optimization/reports/` directories. They are
excluded from this repository's default build.

## Future optimizations worth investigating

These are proposals, not implemented features or measured speedup promises.
Each needs an exactness argument, a measured benefit on Dikarya workloads, and
the same output comparisons before adoption.

1. **Cache projected child partials.** A node repeatedly evaluates a transition
   matrix times each child's partials, even when only the other child changed.
   Instrument the hit rate for a cache keyed by physical child buffer, its content
   version, matrix index and matrix version. Reuse an already rounded projection
   only while all dependencies are unchanged. Invalidate on every relevant buffer,
   matrix, rate and eigen update, and account for scaling. Bound memory and measure
   cache traffic; an extra full partials buffer per edge could erase the gain.
2. **Keep site repeats compressed through multiple operations.** Revisit the
   rejected grouping experiment only with an approach that avoids repeatedly
   expanding and gathering full arrays. Reuse maps across unchanged topology,
   choose the path from measured repeat density, and include fixed-scale-factor
   bits in equivalence checks. Preserve each site's original result and the final
   site summation order.
3. **Recover underflows in smaller tiles.** Measure where subnormals occur within
   a category. A guarded tile path might redo only the affected pattern blocks
   instead of a whole category. Batch MXCSR changes carefully: extra flag checks
   can cost more than they save on small or mostly normal inputs. Gradual
   underflow and every original rounding step must remain exact.
4. **Reduce waiting under real worker contention.** Profile proposal preparation,
   chain queueing, useful likelihood work and idle waits separately on both idle
   and busy hosts. An adaptive spin/sleep policy could outperform a fixed timeout,
   but the 0/50-microsecond experiments do not establish a universal setting.
   Keep the worker's thread budget and the main-thread RNG/acceptance order.
5. **Optimize remaining work only after profiling it.** Candidates include
   `CopyTrees`, parsimony weight-validation scans that can be cached with explicit
   invalidation, and byte-preserving output buffering. Future ISA-specific kernels
   should use runtime dispatch and retain each lane's arithmetic order. Revisit
   compiler tuning only with fresh representative data and a demonstrated win.
6. **Extend the long-run evidence.** Before a broader release, add isolated-host
   benchmarks, more seeds, million-generation runs, both worker budgets, numerical
   failure/fallback cases and race checks. Measure CPU cost, wall latency and peak
   memory separately; do not infer latency improvements from CPU reductions alone.

Reducing generations, changing proposals, approximating exponentials/logarithms,
using fast-math/FMA reassociation, or changing the likelihood reduction order
would change the requested analysis or its bits and is outside this optimization
goal.

## Building it

```bash
git clone https://github.com/AlanRockefeller/MrBayes-Alan.git
cd MrBayes-Alan
bash alan/build-mrbayes-alan.sh            # installs into ~/mrbayes-3.2.9-dikarya2
~/mrbayes-3.2.9-dikarya2/bin/mb -v
```

`alan/build-mrbayes-alan.sh` downloads BEAGLE 4.0.1 and upstream MrBayes 3.2.9 and
builds four things: stock and patched BEAGLE, stock MrBayes, and this checkout. It
then runs stock and patched on a synthetic 96-taxon alignment for 11 model, chain
and thread layouts. It installs only if every output matches byte for byte, which
takes about 5 minutes on 2 cores. Set `PREFIX` to install elsewhere, e.g.
`sudo PREFIX=/opt/mrbayes-3.2.9-dikarya2 bash alan/build-mrbayes-alan.sh`. It needs
`git`, `cmake`, `make`, `gcc`/`g++`, `python3`, `awk` and network access to GitHub.

Built and tested on Ubuntu 24.04 with gcc 13.3 on x86-64. The fast kernels need
AVX2; without it BEAGLE falls back to its original code, and results stay the same.
Concurrent chains use POSIX threads, so a Windows build leaves them out and computes
the chains one after the other, as stock MrBayes does (not tested on Windows).

## Using it

Run it like any MrBayes. To use two threads, put this before `set seed`:

```
set beaglethreadcount=2;
set seed=12345 swapseed=12345;
```

Order matters: every `set beagle...` command reruns MrBayes's analysis setup, which
draws from the seeded random stream. After `set seed` it would change the run;
before it, results are identical to a run without it.

Chains are computed concurrently when the analysis has one data partition of DNA or
RNA on BEAGLE's single-precision CPU code (MrBayes's default) with dynamic scaling,
at least two chains, and the standard unrooted-tree moves. That excludes Gibbs
sampling, autocorrelated gamma, ancestral-state or site-rate reports, stepping-stone
sampling, character reweighting and `append`. Other analyses run as before, with
BEAGLE splitting large alignments by pattern instead. Two threads use about 1.6 times
the memory of one, because each chain keeps its own scratch buffers.

## License and credits

MrBayes is distributed under the GNU General Public License, version 3 (`COPYING`);
so are these changes. BEAGLE is distributed under the GNU Lesser General Public
License, version 3; the BEAGLE patch is offered under the same terms. MrBayes is by
John Huelsenbeck, Fredrik Ronquist and colleagues, and is maintained by NBIS at
<https://github.com/NBISweden/MrBayes>. BEAGLE is by the BEAGLE Working Group at
<https://github.com/beagle-dev/beagle-lib>.
