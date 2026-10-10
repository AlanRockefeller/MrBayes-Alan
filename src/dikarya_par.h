#ifndef DIKARYA_PAR_H_
#define DIKARYA_PAR_H_

/* Dikarya: computing the likelihoods of different chains at the same time.

   With `set beaglethreadcount=N` (N >= 2), a run that qualifies (one
   division on BEAGLE's single-precision 4-state CPU code with dynamic
   scaling, and only the moves Dikarya's analyses use; see
   DkChainParQualifies) computes each generation's likelihoods on N threads,
   one chain per thread, instead of one chain after the other. Everything
   else -- proposals, every random number, acceptance, swaps, output -- stays
   on the main thread in the original order, so the run is the same run:
   each chain has its own scratch buffers, so no chain's arithmetic depends
   on which thread computed it or when. */

extern int                  dkChainPar;         /* YES when this run computes chains' likelihoods concurrently */
extern __thread ModelInfo  *dkModelOverride;    /* this thread's private copy of the division's ModelInfo */

/* the ModelInfo the likelihood functions should use for division d */
#define DK_MODEL(d) (dkModelOverride != NULL ? dkModelOverride : &modelSettings[d])

extern int                 *dkDeferred;        /* [chain]: YES when the job left the numerical rescaling to RunChain */
extern int                 *dkDeferFreq;       /* [chain]: the rescale frequency it was to use */

int     DkChainParExtraScratch (void);
void    DkBeagleRescaleAll (int chain, int d, ModelInfo* m, Tree* tree, MrBFlt* lnL, int rescaleFreqNew);

#endif
