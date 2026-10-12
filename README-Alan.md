# MrBayes-Alan

MrBayes 3.2.9, made faster by Alan Rockefeller with Claude Opus 5.5 for
[Dikarya](https://dikarya.us), where it builds the Bayesian trees.

**Same results, faster.** For the same input, options and seed, every output
file -- sampled parameters and trees, `.mcmc` diagnostics, checkpoints, `sump` and
`sumt` summaries -- and the screen output apart from timings, the run stamp and the
version banners are byte-identical to stock MrBayes 3.2.9 with stock BEAGLE 4.0.1,
both built the way Debian and Ubuntu build them. (That matters for BEAGLE:
upstream's own build adds `-march=native`, which changes single-precision results;
see below.) That holds at one thread and at two, and for runs that end on
`Log likelihood nan`. Nothing about the MCMC changed; the speed comes from doing
the same arithmetic in less time.

## How much faster

Real fungal DNA alignments from Dikarya jobs, with its settings (GTR+G, 2 runs x 4
chains, sample every 500 generations, split-frequency stop rule), a fixed number of
generations, wall time. Intel Xeon E5-2690 v4, 2 vCPUs, Ubuntu 24.04. The builds were run
interleaved (A B B A), each run on an otherwise idle host.

| taxa x site patterns | generations | stock 3.2.9 | MrBayes-Alan, 1 thread | MrBayes-Alan, 2 threads |
|---|---|---|---|---|
| 36 x 394 | 100,000 | 85.3 s | 45.5 s (1.88x) | 28.8 s (2.96x) |
| 34 x 342 | 100,000 | 65.8 s | 37.7 s (1.74x) | 23.5 s (2.79x) |
| 129 x 209 | 30,000 | 59.7 s | 33.5 s (1.78x) | 21.0 s (2.84x) |
| 134 x 195 | 30,000 | 64.2 s | 37.1 s (1.73x) | 24.3 s (2.64x) |
| 240 x 564 | 6,000 | 47.3 s | 27.3 s (1.73x) | 19.3 s (2.44x) |
| 188 x 1191 | 3,000 | 36.0 s | 21.3 s (1.69x) | 15.1 s (2.39x) |

Against Ubuntu's packaged MrBayes 3.2.7a, which Dikarya ran before: 1.60-1.77x
faster on one thread and 2.29-2.72x on two. A 129-taxon analysis of a million
generations that took 33 minutes now takes about 13 on two threads.

Both builds use the same compiler flags as Ubuntu's `mrbayes` package and
`--enable-debug`, as Ubuntu builds it (that is what keeps the `Log likelihood nan`
check). Stock 3.2.9 here is upstream's `release/v3.2.9` at `4f741bc`.

## What changed

Most of a MrBayes run is spent in [BEAGLE](https://github.com/beagle-dev/beagle-lib),
the library it computes likelihoods with, so MrBayes-Alan comes with a patched
BEAGLE 4.0.1: `alan/beagle-4.0.1-dikarya2.patch`. Each commit message gives the
argument for why its results are exact, and what it measured.

In BEAGLE (single-precision, 4-state CPU code, which is what MrBayes uses for DNA):

1. **Built without `-march=native`.** Upstream's CMake forces it, and g++ then fuses
   multiplies and adds into FMAs, which changes single-precision results. Debian and
   Ubuntu build without it, and so does the stock reference here.
2. **AVX2 kernels** for every likelihood update MrBayes makes (partials from two
   partials, a tip and partials, two tips; the edge likelihood; rescaling). They
   do the same IEEE operations on the same operands in the same order as the
   original, two site patterns per 256-bit vector, never an FMA.
3. **No subnormal stalls.** On large trees, partial likelihoods routinely become
   subnormal, and on Intel CPUs every float multiply that touches one costs a
   ~120-cycle microcode assist: half the kernel time on a 129-taxon tree. Each block
   is first computed with flush-to-zero on. If the CPU's underflow flag stays clear,
   that result is provably the IEEE one. Otherwise the block is redone with the risky
   multiplies in double precision, which is exact since it is one correctly rounded
   product. Which path runs changes the time, never a bit.
4. **Calls on different buffers may run at the same time** (per-thread scratch, with
   or without AVX2), so MrBayes can compute different chains at once.
5. **Exact threading by site pattern** for large alignments (768+ patterns) when
   chains cannot be split: the per-pattern log likelihoods are summed in pattern
   order, so results do not depend on the thread count. BEAGLE's own threading,
   which sums per partition and so changes results, is not used.

In MrBayes (`src/`, four commits on top of `4f741bc`):

1. **Chains computed concurrently.** With `set beaglethreadcount=2`, each
   generation's eight chain likelihoods are computed two at a time. Every random
   number is still drawn on the main thread in MrBayes's order: a chain's move and
   its acceptance draw together, and each chain swap's draw kept in its place. So it
   is the same run. Each chain gets its own scratch buffers. The one piece of scratch
   MrBayes genuinely shares between chains (the scaler-node flags used in numerical
   rescaling, whose stale entries it reads) is still handled on the main thread in
   chain order. Two threads are not always faster: next to another busy process
   they keep waiting for each other. So the run measures its speed both ways and
   uses the faster.
2. **Parsimony-biased moves** (ParsSPR, ParsTBR) in AVX2: exact set algebra, and
   integer sums of integer site weights (checked at run time).
3. **`log(branch length)` remembered per node** for the gamma-Dirichlet branch-length
   prior, which summed it over every branch twice per move.
4. Version `3.2.9-dikarya2` (BEAGLE `4.0.1-dikarya2`), so a log names the build.

The MrBayes moves, priors, proposal ratios, scaling scheme and output are unchanged.

## How it was checked

All comparisons are against stock 3.2.9 with stock BEAGLE 4.0.1, built the same way
(Ubuntu's compiler flags; BEAGLE without `-march=native`):

- 44 real alignments (7 to 240 taxa) x 2 model/chain settings each: all 88 runs
  identical at one thread, and all 88 identical at two threads (with BEAGLE's
  pattern threading forced on where chains are not split).
- The BEAGLE kernels, stock and patched, compiled side by side and compared bit for
  bit on more than 120,000 random cases, including zeros, -0, subnormals, infinities
  and NaN. Breaking the exact path by one ulp is caught.
- Two real analyses that end on `Log likelihood nan`, run to the end at two threads:
  both stop at the same generation with identical output.
- ThreadSanitizer, with both libraries instrumented, on the AVX2 kernels and on
  BEAGLE's original code (`BEAGLE_DIKARYA_SCALAR=1`): no data races.
- Stock BEAGLE 4.0.1 built this way matches Ubuntu's BEAGLE 3.1.2 (88 of 88), and a
  rebuild of Ubuntu's MrBayes 3.2.7a with these flags gives the same output as the
  packaged binary (checked on one alignment), so the reference is built the way the
  distribution builds it.

Note that 3.2.9 itself gives different results from 3.2.7a at the same seed: 3.2.9
fixes the Nodeslider proposal ratio for non-clock trees.

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
