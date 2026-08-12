#!/usr/bin/env bash
#
# Move the runner's preinstalled vcpkg to the commit vcpkg.json pins as builtin-baseline.
#
# The GitHub runner images ship a vcpkg clone at $VCPKG_INSTALLATION_ROOT checked out at whatever
# commit the image was built from, and build.yaml points VCPKG_ROOT at it rather than cloning our
# own. That is fast, and it is what lets CI and a contributor's machine share one CMake preset --
# but the image is routinely weeks behind upstream, and our baseline is pinned (see the raylib note
# in vcpkg.json, which is why we have a baseline and an override at all).
#
# THREE separate things break when the image and the pinned baseline disagree, and they have to be
# fixed together. Notes 1 and 2 are the image's vcpkg being OLDER than the baseline; note 3 is the
# mirror case, the image's binary being NEWER, and it was found the hard way ten days later. A fetch
# alone is NOT enough -- that was tried first and only cleared the first of these:
#
#   1. The baseline is read out of git:
#
#          git show <builtin-baseline>:versions/baseline.json
#
#      If the commit object is absent, this fails with
#      "fatal: path 'versions/baseline.json' exists on disk, but not in '<sha>'".
#      A plain `git fetch origin <sha>` fixes this one.
#
#   2. Individual package versions are resolved against the version database in the WORKING TREE
#      ($VCPKG_ROOT/versions/*.json), not against the baseline commit. So with only the fetch done,
#      the baseline correctly asks for catch2 3.15.2#1 and vcpkg-cmake-config 2026-07-21 while the
#      on-disk database still tops out at 3.15.2 and 2024-05-23, and every dependency fails with
#      "no version database entry for <port> at <version>".
#
# Both surface identically at the CMake layer: "Running vcpkg install - failed", followed by a
# misleading cascade of "CMAKE_CXX_COMPILER not set" and "unable to find a build program
# corresponding to Ninja". The compiler and Ninja are fine -- CMake never got past the toolchain
# file.
#
# So: fetch the commit, then actually check the tree out at it, which makes the version database and
# the baseline agree by construction. Reading the SHA from vcpkg.json rather than hardcoding it here
# keeps the workflow correct across baseline bumps -- update vcpkg.json and CI follows.
#
# Usage: bash tools/ci/sync_vcpkg_baseline.sh   (run after VCPKG_ROOT is exported, before configure)

set -euo pipefail

root="${VCPKG_ROOT:-${VCPKG_INSTALLATION_ROOT:-}}"
if [ -z "$root" ]; then
    echo "error: neither VCPKG_ROOT nor VCPKG_INSTALLATION_ROOT is set." >&2
    exit 1
fi

if [ ! -d "$root/.git" ]; then
    echo "error: '$root' is not a git clone; cannot sync it to a baseline." >&2
    exit 1
fi

# Deliberately a sed match on a 40-hex string rather than a JSON parse: this runs on three runner
# images before any toolchain is guaranteed, and depending on jq or python here would trade one
# environment assumption for another.
baseline=$(sed -n 's/.*"builtin-baseline"[[:space:]]*:[[:space:]]*"\([0-9a-fA-F]\{40\}\)".*/\1/p' \
    vcpkg.json | head -n 1)

if [ -z "$baseline" ]; then
    echo "vcpkg.json declares no builtin-baseline -- leaving $root as the image shipped it."
    exit 0
fi

if [ "$(git -C "$root" rev-parse HEAD)" = "$baseline" ]; then
    echo "$root is already at baseline $baseline."
    exit 0
fi

echo "Syncing $root to baseline $baseline..."

# No --depth on the fetch: the image's clone is not shallow, and a shallow fetch would truncate its
# history. The fallback covers a server that refuses fetch-by-SHA.
git -C "$root" cat-file -e "${baseline}^{commit}" 2>/dev/null \
    || git -C "$root" fetch origin "$baseline" \
    || git -C "$root" fetch origin

# --force because the image occasionally leaves the tree dirty, and detached HEAD is expected here.
git -C "$root" -c advice.detachedHead=false checkout --force "$baseline"

# 3. The checkout rewinds the SCRIPTS TREE but not vcpkg.exe, which the runner image ships as a
#    prebuilt binary sitting outside git entirely. So the tree moves to the baseline and the binary
#    does not, and when the image's binary is NEWER than the baseline's scripts, vcpkg rejects its
#    own tool data:
#
#        scripts/vcpkg-tools.json: error: $ (a tool data file):
#        document schema version 1 is not supported by this version of vcpkg
#
#    That failure is upstream of everything else. Unable to read its tool data, vcpkg cannot locate
#    the `git` it needs, so the baseline read in note 1 fails too -- reporting "failed to `git show`
#    versions/baseline.json ... may be fixed by fetching commits with `git fetch`" and pointing
#    squarely at a fetch problem that is already fixed. Then comes the same misleading CMake cascade
#    noted above. Observed 2026-08-12 on BOTH windows-latest (C:\vcpkg) and ubuntu-latest
#    (/usr/local/share/vcpkg), ten days after the last green run. macos-latest passed the same run,
#    so the images cross this boundary independently and a green leg proves nothing about the others.
#
#    Bootstrapping downloads the tool version pinned by scripts/vcpkg-tool-metadata.txt AT THIS
#    COMMIT, so the binary and the scripts agree by construction rather than by luck. This is why
#    the fix is a bootstrap and not a baseline bump: bumping chases the images, and the images move
#    on their schedule, not ours.
#
#    Run on all three legs. Two of them were already failing when this was written and the third
#    was not, which is the argument for treating the mismatch as a property of the mechanism rather
#    than of any one image: macos-latest is not immune, it is merely behind.
echo "Bootstrapping vcpkg at $baseline so the binary matches the checked-out scripts..."
if [ "${OS:-}" = "Windows_NT" ] && [ -f "$root/bootstrap-vcpkg.bat" ]; then
    # Git Bash: the .bat needs cmd. `//c` -- doubled -- stops MSYS rewriting the flag as a path.
    (cd "$root" && cmd //c "bootstrap-vcpkg.bat -disableMetrics")
else
    "$root/bootstrap-vcpkg.sh" -disableMetrics
fi

echo "$root is now at $(git -C "$root" rev-parse HEAD)."
