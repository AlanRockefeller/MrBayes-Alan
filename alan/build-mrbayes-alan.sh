#!/bin/bash
# Build MrBayes-Alan (MrBayes 3.2.9-dikarya2) with its own patched BEAGLE 4.0.1,
# check that it reproduces stock MrBayes 3.2.9 byte for byte, and install it.
#
#   bash alan/build-mrbayes-alan.sh                       # into ~/mrbayes-3.2.9-dikarya2
#   sudo PREFIX=/opt/mrbayes-3.2.9-dikarya2 bash alan/build-mrbayes-alan.sh
#
# What it does:
#   - clones BEAGLE v4.0.1 and builds it twice: stock (with only the first
#     patch, "Build without -march=native", as Debian and Ubuntu build it) and
#     with alan/beagle-4.0.1-dikarya2.patch;
#   - clones upstream MrBayes at the commit this branch starts from and builds
#     it (stock), and builds this checkout (patched), both in the configuration
#     of Ubuntu's mrbayes package: Debian's compiler flags, --enable-debug (so
#     the run-time checks, including the "Log likelihood nan" abort, stay in);
#   - runs stock and patched on a synthetic 96-taxon alignment for a set of
#     model and chain layouts, at one and two threads, and refuses to install
#     unless every output file and stdout match (apart from timings, run stamps
#     and version banners: see alan/compare-mrbayes-runs.py);
#   - installs mb into $PREFIX/bin and the patched BEAGLE into $PREFIX/lib (mb
#     finds it there by its run path): staged and checked first, so that a
#     failure leaves an existing installation as it was.
#
# Needs git, cmake, make, gcc/g++, python3, awk and network access to GitHub.
# Built and verified on Ubuntu 24.04 (gcc 13.3, x86-64). The fast kernels need
# AVX2; without it BEAGLE falls back to its original code.
set -euo pipefail

VER=dikarya2
PREFIX=${PREFIX:-$HOME/mrbayes-3.2.9-$VER}
SRC=${SRC:-$PREFIX-build}
# absolute paths: the build steps change directory, and mb's library run path
# is $PREFIX/lib
case "$PREFIX" in /*) ;; *) PREFIX=$PWD/$PREFIX ;; esac
case "$SRC" in /*) ;; *) SRC=$PWD/$SRC ;; esac
MB_UPSTREAM=https://github.com/NBISweden/MrBayes.git
MB_BRANCH=release/v3.2.9
MB_BASE=4f741bcfe41f65b7f4d560cec98614d3d9e29326
BG_UPSTREAM=https://github.com/beagle-dev/beagle-lib.git
BG_TAG=v4.0.1
BG_BASE=bf35ad4a28e5f03edaa10dfd77c1b54eee0d9595
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/.." && pwd)
BG_PATCH=$HERE/beagle-4.0.1-$VER.patch
MARK=.mrbayes-alan-build

# Debian/Ubuntu 24.04 package build flags (dpkg-buildflags with hardening=+all)
DEB_CFLAGS="-g -O2 -fno-omit-frame-pointer -mno-omit-leaf-frame-pointer -flto=auto -ffat-lto-objects -fstack-protector-strong -fstack-clash-protection -Wformat -Werror=format-security -fcf-protection"
DEB_CPPFLAGS="-Wdate-time -D_FORTIFY_SOURCE=3"
DEB_LDFLAGS="-Wl,-Bsymbolic-functions -flto=auto -ffat-lto-objects -Wl,-z,relro -Wl,-z,now"

for f in "$BG_PATCH" "$HERE/selftest-data.awk" "$HERE/compare-mrbayes-runs.py" "$REPO/src/mcmc.c"; do
    [ -f "$f" ] || { echo "missing $f" >&2; exit 1; }
done
for tool in git cmake make g++ gcc python3 awk; do
    command -v "$tool" >/dev/null || { echo "missing build tool: $tool" >&2; exit 1; }
done
grep -qw avx2 /proc/cpuinfo 2>/dev/null || echo "note: no AVX2 here; the patched kernels fall back to the original code" >&2
mkdir -p "$PREFIX" && [ -w "$PREFIX" ] || { echo "cannot write to $PREFIX (set PREFIX)" >&2; exit 1; }

# $SRC is deleted and rebuilt: only if this script made it (or it is empty)
if [ -e "$SRC" ] && [ ! -f "$SRC/$MARK" ] && [ -n "$(ls -A "$SRC" 2>/dev/null)" ]; then
    echo "$SRC exists and was not made by this script; set SRC to a new directory" >&2
    exit 1
fi
rm -rf "$SRC"
mkdir -p "$SRC"
touch "$SRC/$MARK"
am() { git -C "$1" -c user.name=build-mrbayes-alan -c user.email=build@localhost \
           am --quiet --committer-date-is-author-date "$2"; }

# ---- source ----------------------------------------------------------------
git clone --quiet --branch "$MB_BRANCH" "$MB_UPSTREAM" "$SRC/mb-stock"
git -C "$SRC/mb-stock" -c advice.detachedHead=false checkout --quiet "$MB_BASE" \
    || { echo "$MB_BASE is not on $MB_BRANCH any more; refusing to build" >&2; exit 1; }
mkdir -p "$SRC/mb-$VER"
if git -C "$REPO" rev-parse --verify --quiet HEAD >/dev/null; then
    [ -z "$(git -C "$REPO" status --porcelain --untracked-files=no -- src)" ] \
        || echo "note: building the committed tree; uncommitted changes in src/ are not included" >&2
    git -C "$REPO" archive --format=tar HEAD | tar -x -C "$SRC/mb-$VER"
else
    cp -a "$REPO/." "$SRC/mb-$VER/"
fi
git -c advice.detachedHead=false clone --quiet --branch "$BG_TAG" "$BG_UPSTREAM" "$SRC/bg-stock"
[ "$(git -C "$SRC/bg-stock" rev-parse HEAD)" = "$BG_BASE" ] \
    || { echo "$BG_TAG is not $BG_BASE any more; refusing to build" >&2; exit 1; }
cp -a "$SRC/bg-stock" "$SRC/bg-$VER"
am "$SRC/bg-$VER" "$BG_PATCH"
# the stock reference is built like the distributions build it: patch 1 only
# (no -march=native, which lets g++ fuse multiplies and adds into FMAs)
awk '/^From [0-9a-f]+ /{n++} n==1' "$BG_PATCH" > "$SRC/beagle-build.patch"
am "$SRC/bg-stock" "$SRC/beagle-build.patch"

# ---- build -----------------------------------------------------------------
# BEAGLE's core library loads its CPU plugin by name: a run path of $ORIGIN
# makes it find the one beside it. mb's run path is the final $PREFIX/lib;
# the self-test points LD_LIBRARY_PATH at the build's libraries instead.
build_beagle() {   # <name>
    mkdir -p "$SRC/build-bg-$1"
    (cd "$SRC/build-bg-$1" &&
     cmake "$SRC/bg-$1" -DBUILD_OPENCL=OFF -DBUILD_CUDA=OFF -DBUILD_JNI=OFF -DBUILD_SSE=OFF \
         -DCMAKE_INSTALL_PREFIX="$SRC/bg-$1-inst" -DCMAKE_INSTALL_RPATH='$ORIGIN' \
         >"$SRC/cmake-bg-$1.log" 2>&1 || { tail -30 "$SRC/cmake-bg-$1.log"; exit 1; }
     nice -n 10 make -j"$(nproc)" hmsbeagle hmsbeagle-cpu >"$SRC/build-bg-$1.log" 2>&1 \
         || { grep -m20 "error" "$SRC/build-bg-$1.log"; exit 1; }
     make install >"$SRC/install-bg-$1.log" 2>&1 || { tail -20 "$SRC/install-bg-$1.log"; exit 1; })
}
build_mrbayes() {  # <name> <run path>
    mkdir -p "$SRC/build-mb-$1"
    (cd "$SRC/build-mb-$1" &&
     CFLAGS="$DEB_CFLAGS" CPPFLAGS="$DEB_CPPFLAGS" LDFLAGS="$DEB_LDFLAGS" \
     BEAGLE_CFLAGS="-I$SRC/bg-$1-inst/include/libhmsbeagle-1" \
     BEAGLE_LIBS="-L$SRC/bg-$1-inst/lib -Wl,--enable-new-dtags -Wl,-rpath,$2 -lhmsbeagle" \
         "$SRC/mb-$1/configure" --with-beagle=yes --enable-debug=yes \
             --disable-sse --disable-avx --disable-fma --without-readline \
             >"$SRC/configure-mb-$1.log" 2>&1 || { tail -30 "$SRC/configure-mb-$1.log"; exit 1; }
     nice -n 10 make -j"$(nproc)" -C src >"$SRC/build-mb-$1.log" 2>&1 \
         || { grep -m20 "error" "$SRC/build-mb-$1.log"; exit 1; })
}
build_beagle stock
build_beagle "$VER"
build_mrbayes stock "$SRC/bg-stock-inst/lib"
build_mrbayes "$VER" "$PREFIX/lib"
REF=$SRC/build-mb-stock/src/mb
NEW=$SRC/build-mb-$VER/src/mb
REF_LIB=$SRC/bg-stock-inst/lib
NEW_LIB=$SRC/bg-$VER-inst/lib
LD_LIBRARY_PATH=$NEW_LIB "$NEW" -v | grep -i version

# ---- self-test: identical results to stock 3.2.9 ---------------------------
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
awk -f "$HERE/selftest-data.awk" > "$tmp/data.nex"
status=0
n=0
# GTR+G with the split-frequency stop rule, the other substitution/rate
# models, one run of one chain, 3x3 chains; and the same with two threads,
# which computes the chains' likelihoods concurrently -- except for the single
# chain, which BEAGLE splits by site pattern instead (forced on for this
# 480-pattern alignment, which would otherwise not be split).
while IFS='|' read -r env pre lset prset mcmc; do
    n=$((n+1))
    for side in ref new; do
        bin=$REF; lib=$REF_LIB; [ "$side" = new ] && { bin=$NEW; lib=$NEW_LIB; }
        d=$tmp/$side$n; mkdir -p "$d"
        { cat "$tmp/data.nex"
          printf '\nbegin mrbayes;\n   set autoclose=yes nowarn=yes;\n'
          [ -n "$pre" ] && printf '   %s\n' "$pre"
          printf '   set seed=20261009 swapseed=20261009;\n   lset %s;\n' "$lset"
          [ -n "$prset" ] && printf '   prset %s;\n' "$prset"
          printf '   mcmc %s relburnin=yes burninfrac=0.25;\n' "$mcmc"
          printf '   sump relburnin=yes burninfrac=0.25;\n   sumt relburnin=yes burninfrac=0.25;\n   quit;\nend;\n'
        } > "$d/input.nex"
        # shellcheck disable=SC2086
        (cd "$d" && env $env LD_LIBRARY_PATH="$lib" nice -n 10 "$bin" input.nex > stdout.txt 2>&1) \
            || { echo "$side build failed: $lset $mcmc" >&2; status=1; }
    done
    if python3 -I "$HERE/compare-mrbayes-runs.py" "$tmp/ref$n" "$tmp/new$n" -q; then
        echo "self-test ok:   lset $lset; mcmc $mcmc ${pre:+($pre)}"
    else
        echo "self-test FAIL: lset $lset; mcmc $mcmc ${pre:+($pre)}" >&2
        status=1
    fi
done <<'EOF'
X=1||nst=6 rates=gamma||ngen=3000 nchains=4 nruns=2 samplefreq=20 printfreq=500 mcmcdiagn=yes stoprule=yes stopval=0.01
X=1||nst=2 rates=invgamma||ngen=2000 nchains=4 nruns=2 samplefreq=20 printfreq=500 mcmcdiagn=yes stoprule=yes stopval=0.01
X=1||nst=1 rates=equal|statefreqpr=fixed(equal)|ngen=2000 nchains=4 nruns=2 samplefreq=20 printfreq=500
X=1||nst=6 rates=propinv|statefreqpr=fixed(equal)|ngen=2000 nchains=4 nruns=2 samplefreq=20 printfreq=500
X=1||nst=1 rates=gamma||ngen=3000 nchains=1 nruns=1 samplefreq=20 printfreq=500
X=1||nst=6 rates=invgamma||ngen=2000 nchains=3 nruns=3 samplefreq=20 printfreq=500
X=1|set beaglethreadcount=2;|nst=6 rates=gamma||ngen=3000 nchains=4 nruns=2 samplefreq=20 printfreq=500 mcmcdiagn=yes stoprule=yes stopval=0.01
X=1|set beaglethreadcount=2;|nst=2 rates=invgamma||ngen=2000 nchains=4 nruns=2 samplefreq=20 printfreq=500 mcmcdiagn=yes stoprule=yes stopval=0.01
X=1|set beaglethreadcount=2;|nst=6 rates=invgamma||ngen=2000 nchains=3 nruns=3 samplefreq=20 printfreq=500
X=1|set beaglethreadcount=2;|nst=1 rates=equal|statefreqpr=fixed(equal)|ngen=2000 nchains=2 nruns=2 samplefreq=20 printfreq=500
BEAGLE_DIKARYA_MIN_PATTERNS=1|set beaglethreadcount=2;|nst=1 rates=gamma||ngen=3000 nchains=1 nruns=1 samplefreq=20 printfreq=500
EOF
[ "$status" -eq 0 ] || { echo "not installed: this build does not reproduce stock MrBayes 3.2.9" >&2; exit 1; }

# ---- install -----------------------------------------------------------------
# Everything is copied into a staging directory inside $PREFIX and checked
# there first; only then is each file renamed into place (mb last). A rename
# replaces a file in one step, and a running mb keeps the files it has open.
# If anything fails before that, $PREFIX is left as it was.
stage=$(mktemp -d "$PREFIX/.mrbayes-alan-stage.XXXXXX")
trap 'rm -rf "$tmp" "$stage"' EXIT
mkdir -p "$stage/bin" "$stage/lib" "$stage/share"
for f in "$NEW_LIB"/libhmsbeagle*.so*; do
    if [ -L "$f" ]; then
        ln -s "$(readlink "$f")" "$stage/lib/$(basename "$f")"
    else
        install -m 0644 "$f" "$stage/lib/$(basename "$f")"
    fi
done
install -m 0755 "$NEW" "$stage/bin/mb"
install -m 0644 "$BG_PATCH" "$stage/share/$(basename "$BG_PATCH")"

check_install() {  # <dir holding bin/mb> <label> [library path to use]
    local d=$tmp/$2
    mkdir -p "$d"; cp "$tmp/new1/input.nex" "$d/"
    (cd "$d" && env -u LD_LIBRARY_PATH ${3:+LD_LIBRARY_PATH=$3} nice -n 10 "$1/bin/mb" input.nex > stdout.txt 2>&1) || true
    grep -q "Using BEAGLE v4.0.1-$VER" "$d/stdout.txt" \
        || { echo "$2 mb does not load the patched BEAGLE" >&2; return 1; }
    python3 -I "$HERE/compare-mrbayes-runs.py" "$tmp/ref1" "$d" -q \
        || { echo "$2 mb does not reproduce the self-test" >&2; return 1; }
}
# the staged copy with its own libraries (mb's run path is $PREFIX/lib, a
# RUNPATH, which LD_LIBRARY_PATH overrides)
check_install "$stage" staged "$stage/lib" || { echo "not installed" >&2; exit 1; }

mkdir -p "$PREFIX/bin" "$PREFIX/lib" "$PREFIX/share"
for f in "$stage"/lib/* "$stage"/share/* "$stage/bin/mb"; do
    mv -fT "$f" "$PREFIX/${f#"$stage"/}"
done
rm -rf "$stage"

# the installed binary, with no library path set, must load the patched
# library from $PREFIX and still reproduce the first self-test case
check_install "$PREFIX" installed || exit 1
echo "installed: $PREFIX/bin/mb ($("$PREFIX/bin/mb" -v | grep -i version | tr -s ' '))"
echo "build files are in $SRC (safe to delete)"
