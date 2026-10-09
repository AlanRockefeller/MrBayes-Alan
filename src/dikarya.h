#ifndef DIKARYA_H_
#define DIKARYA_H_

/*
 *  dikarya.h -- exact fast paths used by the Dikarya build of MrBayes.
 *
 *  Every function here returns exactly what the loop it replaces computes.
 *  The parsimony helpers vectorize bitwise set operations (exact by nature)
 *  and sum the site-pattern weights of the sites that need a change. The
 *  original loops add those float weights one by one into a double; when
 *  every weight is a whole number in [0, 2^24], every partial sum is an
 *  integer below 2^53, so each of those additions is exact and the result
 *  is the integer total whatever the order. The fast paths check that
 *  condition and otherwise run the original loop.
 *
 *  MRBAYES_DIKARYA_SCALAR=1 in the environment disables all of them, for
 *  A/B comparisons.
 */

#include <stdlib.h>
#include <string.h>

#if defined (__x86_64__) && defined (__GNUC__)
#   define DK_X86 1
#   include <immintrin.h>
#   define DK_AVX2 __attribute__((target("avx2")))
#endif

__attribute__((unused)) static int DkEnabled (void)
{
    static int enabled = -1;
    if (enabled < 0)
        {
#if defined (DK_X86)
        __builtin_cpu_init ();
        enabled = (__builtin_cpu_supports ("avx2") && getenv ("MRBAYES_DIKARYA_SCALAR") == NULL) ? 1 : 0;
#else
        enabled = 0;
#endif
        }
    return enabled;
}

#if defined (DK_X86)

/* 1 if every weight is a whole number in [0, 2^24] (so integer summation in
   any order equals the original sequential double sum), else 0. */
DK_AVX2 __attribute__((unused)) static int DkWeightsAreSmallIntegers (const CLFlt *w, int n)
{
    int     k;
    __m256  lo = _mm256_setzero_ps (), hi = _mm256_set1_ps (16777216.0f);
    __m256  bad = _mm256_setzero_ps ();
    float   x;

    for (k=0; k+8<=n; k+=8)
        {
        __m256 v = _mm256_loadu_ps (w + k);
        __m256 r = _mm256_cvtepi32_ps (_mm256_cvttps_epi32 (v));
        /* not (v == r and v >= 0 and v <= 2^24); NaN fails every ordered test */
        __m256 ok = _mm256_and_ps (_mm256_cmp_ps (v, r, _CMP_EQ_OQ),
                     _mm256_and_ps (_mm256_cmp_ps (v, lo, _CMP_GE_OQ), _mm256_cmp_ps (v, hi, _CMP_LE_OQ)));
        bad = _mm256_or_ps (bad, _mm256_xor_ps (ok, _mm256_castsi256_ps (_mm256_set1_epi32 (-1))));
        }
    if (_mm256_movemask_ps (bad) != 0)
        return 0;
    for (; k<n; k++)
        {
        x = w[k];
        if (!(x >= 0.0f && x <= 16777216.0f && x == (float)(int)x))
            return 0;
        }
    return 1;
}

/* four float weights, known to be whole numbers, as four int64 lanes */
DK_AVX2 __attribute__((unused)) static inline __m256i DkWeights4 (const CLFlt *w)
{
    return _mm256_cvtepi32_epi64 (_mm_cvttps_epi32 (_mm_loadu_ps (w)));
}

DK_AVX2 __attribute__((unused)) static inline long long DkSum4 (__m256i v)
{
    __m128i s = _mm_add_epi64 (_mm256_castsi256_si128 (v), _mm256_extracti128_si256 (v, 1));
    return _mm_cvtsi128_si64 (s) + _mm_extract_epi64 (s, 1);
}

/* GetFitchPartials, one BitsLong per site:
       x = s1 & s2;  if (x == 0) { length += w; x = s1 | s2; }  d = x;     */
DK_AVX2 __attribute__((unused)) static MrBFlt DkFitchDown (const BitsLong *s1, const BitsLong *s2, BitsLong *d, const CLFlt *w, int n)
{
    int         c;
    long long   total;
    MrBFlt      length = 0.0;
    BitsLong    x;
    __m256i     zero = _mm256_setzero_si256 (), acc = _mm256_setzero_si256 ();

    if (!DkWeightsAreSmallIntegers (w, n))
        {
        for (c=0; c<n; c++)
            {
            x = s1[c] & s2[c];
            if (x == 0)
                {
                length += w[c];
                x = s1[c] | s2[c];
                }
            d[c] = x;
            }
        return length;
        }
    for (c=0; c+4<=n; c+=4)
        {
        __m256i a = _mm256_loadu_si256 ((const __m256i *)(s1 + c));
        __m256i b = _mm256_loadu_si256 ((const __m256i *)(s2 + c));
        __m256i v = _mm256_and_si256 (a, b);
        __m256i z = _mm256_cmpeq_epi64 (v, zero);
        v = _mm256_or_si256 (v, _mm256_and_si256 (z, _mm256_or_si256 (a, b)));
        _mm256_storeu_si256 ((__m256i *)(d + c), v);
        acc = _mm256_add_epi64 (acc, _mm256_and_si256 (z, DkWeights4 (w + c)));
        }
    total = DkSum4 (acc);
    for (; c<n; c++)
        {
        x = s1[c] & s2[c];
        if (x == 0)
            {
            total += (long long) w[c];
            x = s1[c] | s2[c];
            }
        d[c] = x;
        }
    return (MrBFlt) total;
}

/* GetParsFP, one BitsLong per site (pure set algebra):
       x = P & A;
       if (x != A) x = (L & R) != 0 ? ((L | R) & A) | P : P | A;
       P = x;                                                              */
DK_AVX2 __attribute__((unused)) static void DkFitchFinal (const BitsLong *pL, const BitsLong *pR, BitsLong *pP, const BitsLong *pA, int n)
{
    int         c;
    BitsLong    x;
    __m256i     zero = _mm256_setzero_si256 ();

    for (c=0; c+4<=n; c+=4)
        {
        __m256i vL = _mm256_loadu_si256 ((const __m256i *)(pL + c));
        __m256i vR = _mm256_loadu_si256 ((const __m256i *)(pR + c));
        __m256i vP = _mm256_loadu_si256 ((const __m256i *)(pP + c));
        __m256i vA = _mm256_loadu_si256 ((const __m256i *)(pA + c));
        __m256i x0 = _mm256_and_si256 (vP, vA);
        __m256i keep = _mm256_cmpeq_epi64 (x0, vA);                         /* x == A: keep x */
        __m256i lrZero = _mm256_cmpeq_epi64 (_mm256_and_si256 (vL, vR), zero);
        __m256i one = _mm256_or_si256 (_mm256_and_si256 (_mm256_or_si256 (vL, vR), vA), vP);
        __m256i two = _mm256_or_si256 (vP, vA);
        __m256i alt = _mm256_blendv_epi8 (one, two, lrZero);
        _mm256_storeu_si256 ((__m256i *)(pP + c), _mm256_blendv_epi8 (alt, x0, keep));
        }
    for (; c<n; c++)
        {
        x = pP[c] & pA[c];
        if (x != pA[c])
            {
            if ((pL[c] & pR[c]) != 0)
                x = ((pL[c] | pR[c]) & pA[c]) | pP[c];
            else
                x = pP[c] | pA[c];
            }
        pP[c] = x;
        }
}

/* Parsimony cost of joining two branches, one BitsLong per site:
       sum of w[k] over sites with ((x1 | x2) & (y1 | y2)) == 0
   wholeWeights: result of DkWeightsAreSmallIntegers (w, n).               */
DK_AVX2 __attribute__((unused)) static MrBFlt DkParsJoinLength (const BitsLong *x1, const BitsLong *x2, const BitsLong *y1, const BitsLong *y2,
                                        const CLFlt *w, int n, int wholeWeights)
{
    int         k;
    long long   total;
    MrBFlt      length = 0.0;
    __m256i     zero = _mm256_setzero_si256 (), acc = _mm256_setzero_si256 ();

    if (!wholeWeights)
        {
        for (k=0; k<n; k++)
            if (((x1[k] | x2[k]) & (y1[k] | y2[k])) == 0)
                length += w[k];
        return length;
        }
    for (k=0; k+4<=n; k+=4)
        {
        __m256i x = _mm256_or_si256 (_mm256_loadu_si256 ((const __m256i *)(x1 + k)), _mm256_loadu_si256 ((const __m256i *)(x2 + k)));
        __m256i y = _mm256_or_si256 (_mm256_loadu_si256 ((const __m256i *)(y1 + k)), _mm256_loadu_si256 ((const __m256i *)(y2 + k)));
        __m256i z = _mm256_cmpeq_epi64 (_mm256_and_si256 (x, y), zero);
        acc = _mm256_add_epi64 (acc, _mm256_and_si256 (z, DkWeights4 (w + k)));
        }
    total = DkSum4 (acc);
    for (; k<n; k++)
        if (((x1[k] | x2[k]) & (y1[k] | y2[k])) == 0)
            total += (long long) w[k];
    return (MrBFlt) total;
}

#endif  /* DK_X86 */

#endif  /* DIKARYA_H_ */
