> **MrBayes-Alan** is MrBayes 3.2.9 made faster by Alan Rockefeller with Claude Opus 5.5,
> with **byte-identical output** to stock MrBayes 3.2.9 for the same input and seed.
> On real fungal alignments it runs **1.7-1.9x faster on one thread** and **2.4-3.0x
> faster on two** than stock 3.2.9. The speed comes from a patched BEAGLE library
> (exact AVX2 kernels, no subnormal stalls) and from computing chains concurrently.
> See **[README-Alan.md](README-Alan.md)** for what changed, the benchmarks, how it was
> checked, and how to build it (`bash alan/build-mrbayes-alan.sh`).
>
> The original MrBayes README follows.

---

Hi,

This is the GitHub repository for MrBayes, a program used for Bayesian
inference of phylogeny.


Build instructions, very very short version (would install MrBayes as
"/usr/local/bin/mb" on a Unix system):

    git clone --depth=1 https://github.com/NBISweden/MrBayes.git
    cd MrBayes
    ./configure
    make && sudo make install

    Refer to the document called "INSTALL" for further information.


Current repository directory layout:

    .
    ├── autotools      : Files relating to the build system
    ├── doc
    │   └── manual     : The manual
    ├── examples       : Example input data sets
    └── src            : C source code


The MrBayes web site is available at

    http://mrbayes.net/


Historical releases are tagged with "v3.2.0" (etc.) from release 3.2.0
onwards.

The 'master' branch contains releases.

The 'develop' branch should hopefully, at all times, contain working
development code that at least compiles.  Apart from simple fixes, new
development happens on feature branches off the 'develop' branch, and
are merged back into 'develop' when finished.  This more or less follows
the "git-flow" branching model.

The 'gh-pages' branch contains the web site (only).

The 'testing' branch contains testing data files (only).

After release 3.2.6 (November 2015), MrBayes entered "maintenance mode",
and its care has been passed over to NBIS (National Bioinformatics
Infrastructure Sweden, http://nbis.se/).  The original development
team is still involved with the program, but any future releases will
probably be geared more towards bug-fixes rather than addition of new
features.

With kind regards,
    Andreas (Kusalananda) Kähäri, and
    Johan Nylander
    NBIS
