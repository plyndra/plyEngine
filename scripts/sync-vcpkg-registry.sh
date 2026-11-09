#!/usr/bin/env bash
# Sync the repo-root vcpkg registry with the canonical port in
# packaging/vcpkg/port, then update versions/ so the registry stays valid.
#
# The plyEngine repository doubles as its own vcpkg git registry: consumers
# add a vcpkg-configuration.json with
#
#   "registries": [{
#     "kind": "git", "packages": ["plyengine"],
#     "repository": "https://github.com/plyndra/plyEngine",
#     "baseline": "<commit sha>"
#   }]
#
# Usage: scripts/sync-vcpkg-registry.sh [--bump <new-version>]
#   Without --bump: re-sync the port files and append a new version entry with
#   the same version-semver but port-version+1 (content changed).
#   With --bump vX.y.z: assign a new version-semver, port-version 0.
#   Logic lives in scripts/sync_vcpkg_registry.py.

set -euo pipefail
repo_root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec python3 "$repo_root/scripts/sync_vcpkg_registry.py" "$@"
