#!/usr/bin/env bash
# Release readiness check for plyEngine.
#
# Encodes the mechanical parts of the release gate (docs/ROADMAP.md) so they
# are reproducible, not remembered:
#
#   1. version consistency   PLY_VERSION_STRING == top CHANGELOG heading == tag
#   2. public API drift      engine/include may change *additively* only since
#                            the freeze tag; removals need --allow-api-change
#   3. documentation         every relative markdown link resolves
#   4. tests                 ctest passes in the given build directory
#
# Usage:
#   scripts/release-check.sh                 # check the working tree
#   scripts/release-check.sh --tag v1.0.0    # also require the tag to exist
#   scripts/release-check.sh --build build-asan --allow-api-change
#
# Exit status: 0 when every check passes, 1 otherwise.

set -uo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$repo_root"

build_dir="build"
tag=""
freeze_tag="v1.0.0-beta.1"
allow_api_change=0

while [ $# -gt 0 ]; do
    case "$1" in
        --tag) tag="$2"; shift 2 ;;
        --build) build_dir="$2"; shift 2 ;;
        --freeze) freeze_tag="$2"; shift 2 ;;
        --allow-api-change) allow_api_change=1; shift ;;
        -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
        *) echo "unknown argument: $1" >&2; exit 2 ;;
    esac
done

failures=0
note() { printf '  %s\n' "$*"; }
pass() { printf '\033[32m  PASS\033[0m %s\n' "$*"; }
fail() { printf '\033[31m  FAIL\033[0m %s\n' "$*"; failures=$((failures + 1)); }
section() { printf '\n== %s ==\n' "$*"; }

# --- 1. Version consistency -------------------------------------------------
section "version consistency"

cmake_version="$(sed -n 's/^set(PLY_VERSION_STRING "\([^"]*\)".*/\1/p' CMakeLists.txt | head -1)"
changelog_version="$(sed -n 's/^## \[\([^]]*\)\].*/\1/p' CHANGELOG.md | head -1)"

if [ -z "$cmake_version" ]; then
    fail "PLY_VERSION_STRING not found in CMakeLists.txt"
else
    note "CMakeLists.txt: $cmake_version"
fi
if [ -z "$changelog_version" ]; then
    fail "no '## [x.y.z]' heading in CHANGELOG.md"
else
    note "CHANGELOG.md:   $changelog_version"
fi

if [ -n "$cmake_version" ] && [ "$cmake_version" != "$changelog_version" ]; then
    fail "CMakeLists.txt and CHANGELOG.md disagree on the version"
else
    [ -n "$cmake_version" ] && pass "version is consistent"
fi

if [ -n "$tag" ]; then
    if git rev-parse -q --verify "refs/tags/$tag" >/dev/null; then
        pass "tag $tag exists"
    else
        fail "tag $tag does not exist"
    fi
    if [ -n "$cmake_version" ] && [ "$tag" != "v$cmake_version" ]; then
        fail "tag $tag does not match version $cmake_version"
    fi
fi

# --- 2. Public API drift ----------------------------------------------------
section "public API drift since $freeze_tag"

if ! git rev-parse -q --verify "refs/tags/$freeze_tag" >/dev/null; then
    note "freeze tag $freeze_tag not found — skipping (fetch tags first)"
else
    # numstat: "<added>\t<removed>\t<path>". Deletions in a public header are
    # the cheap, reliable signal that the frozen surface changed shape.
    deletions_total=0
    while IFS=$'\t' read -r added removed path; do
        [ -z "${path:-}" ] && continue
        if [ "$removed" = "-" ]; then
            fail "$path was deleted"
            deletions_total=$((deletions_total + 1))
            continue
        fi
        if [ "$removed" -gt 0 ]; then
            note "$path: +$added -$removed"
            deletions_total=$((deletions_total + removed))
        fi
    done < <(git diff --numstat "$freeze_tag" -- engine/include)

    if [ "$deletions_total" -eq 0 ]; then
        pass "public headers changed additively only"
    elif [ "$allow_api_change" -eq 1 ]; then
        note "API change allowed by --allow-api-change (record it in DESIGN.md)"
    else
        fail "$deletions_total removed/changed line(s) in engine/include"
        note "additions are fine; removals/renames need a deprecation cycle"
        note "if this is deliberate, re-run with --allow-api-change"
    fi
fi

# --- 3. Documentation links -------------------------------------------------
section "documentation links"

broken=0
# The planning set is included: a decision record whose links rot is worse than
# no record, and these files are only checked if they are listed here.
for file in README.md CHANGELOG.md CONTRIBUTING.md CODE_OF_CONDUCT.md \
            docs/*.md docs/planning/*.md docs/planning/adr/*.md; do
    [ -f "$file" ] || continue
    dir="$(dirname "$file")"
    # Skip fenced code blocks: `[&](ply::App& a)` in a C++ sample is not a link.
    while IFS= read -r link; do
        case "$link" in
            http*|mailto:*|'#'*) continue ;;
        esac
        target="$dir/${link%%#*}"
        if [ ! -e "$target" ]; then
            fail "$file -> $link"
            broken=$((broken + 1))
        fi
    done < <(awk '/^```/{inblock=!inblock; next} !inblock' "$file" \
                 | grep -oE '\]\([^)]+\)' | sed 's/^](//; s/)$//' || true)
done
[ "$broken" -eq 0 ] && pass "all relative links resolve"

# --- 4. Tests ---------------------------------------------------------------
section "eval evidence transcripts"

# A ledger line must be a transcript of what the suite printed, not prose about
# what it meant to check. Three a2 reviews found only that class of defect.
if [ -x "$repo_root/scripts/eval-evidence-check.sh" ]; then
    if evidence_output="$(scripts/eval-evidence-check.sh "$build_dir" 2>&1)"; then
        note "$(printf '%s' "$evidence_output" | tr '\n' ' ')"
        pass "every printed EVIDENCE line is in evals/2.0.log"
    else
        printf '%s\n' "$evidence_output" >&2
        fail "an EVIDENCE line is not in evals/2.0.log"
    fi
fi

section "ctest ($build_dir)"

if [ ! -d "$build_dir" ]; then
    fail "build directory '$build_dir' does not exist (configure it first)"
elif ctest --test-dir "$build_dir" --output-on-failure >/tmp/ply-release-check-ctest.log 2>&1; then
    pass "$(grep -oE '[0-9]+% tests passed[^)]*' /tmp/ply-release-check-ctest.log | head -1)"
else
    fail "ctest failed — see /tmp/ply-release-check-ctest.log"
    tail -n 20 /tmp/ply-release-check-ctest.log | sed 's/^/    /'
fi

# --- Summary ----------------------------------------------------------------
printf '\n'
if [ "$failures" -eq 0 ]; then
    printf '\033[32mrelease check: PASS\033[0m\n'
    exit 0
fi
printf '\033[31mrelease check: %d failure(s)\033[0m\n' "$failures"
exit 1
