#!/usr/bin/env bash
# eval-evidence-check.sh — every EVIDENCE line a suite prints must appear
# verbatim in evals/2.0.log.
#
# Why: three closing reviews of unit a2 found the same class of defect — ledger
# prose asserting numbers the tests never measured. A log line is now a
# transcript of the suite's own output, and this check is what keeps it one:
# change the geometry, the transcript changes, the log stops matching, CI fails.
#
# A suite opts in simply by printing a line beginning with "EVIDENCE ". Suites
# that print nothing are skipped, so the check is cheap to adopt per unit.
# Blind spot, stated (the b4 read's O2): a hollowed or broken suite that
# prints nothing VANISHES from this report instead of failing it — run a
# clean build (and the red-run replay) before trusting an all-ok list.
#
# Usage: scripts/eval-evidence-check.sh [build-dir]
# Exit: 0 when every printed transcript is present, 1 otherwise.

set -uo pipefail

repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
build_dir="${1:-$repo_root/build}"
log="$repo_root/evals/2.0.log"

[ -f "$log" ] || { echo "eval-evidence-check: missing $log" >&2; exit 1; }

checked=0
missing=0
for exe in "$build_dir"/ply_*_test; do
    [ -x "$exe" ] || continue
    line="$(timeout 60 "$exe" 2>/dev/null | grep -m1 '^EVIDENCE ' || true)"
    [ -n "$line" ] || continue
    checked=$((checked + 1))
    if grep -Fq -- "$line" "$log"; then
        printf '  ok   %s\n' "$(basename "$exe")"
    else
        printf '  MISS %s\n    printed: %s\n    not in:  evals/2.0.log\n' \
            "$(basename "$exe")" "$line" >&2
        missing=$((missing + 1))
    fi
done

if [ "$checked" -eq 0 ]; then
    echo "  no suites print an EVIDENCE line yet (nothing to check)"
    exit 0
fi
[ "$missing" -eq 0 ] && exit 0
exit 1
