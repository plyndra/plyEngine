#!/usr/bin/env python3
"""Sync the repo-root vcpkg registry with packaging/vcpkg/port.

The plyEngine repository doubles as its own vcpkg git registry
(ports/plyengine + versions/, per the registry contract). This script:

  1. mirrors the canonical port into ports/plyengine (and writes the
     formatted manifest back to packaging/vcpkg/port so both stay identical),
  2. reformats with `vcpkg format-manifest` -- the versions-db generator
     otherwise aborts with "not properly formatted",
  3. regenerates the versions database with
     `vcpkg --x-builtin-ports-root=<repo>/ports
     --x-builtin-registry-versions-dir=<repo>/versions x-add-version --all`,
     which computes git-tree hashes itself; hand-computed hashes are how a
     registry silently breaks,
  4. commits anything that changed and prints the baseline commit to quote in
     a consumer's vcpkg-configuration.json.

Run via scripts/sync-vcpkg-registry.sh. Needs a vcpkg binary (VCPKG_EXE env
var, or /tmp/vcpkg/vcpkg, or $HOME/vcpkg/vcpkg).
"""
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
PORT_DIR = ROOT / "ports" / "plyengine"
SRC_DIR = ROOT / "packaging" / "vcpkg" / "port"
VERSIONS_DIR = ROOT / "versions"
PORT_SPECS = ["ports", "packaging/vcpkg/port"]
VERSIONS_SPECS = ["versions"]
GIT_IDENTITY = ["-c", "user.name=MerverliPy",
                "-c", "user.email=merverlipy@users.noreply.github.com"]


def git(*args, check=True):
    return subprocess.run(["git", *GIT_IDENTITY, *args], cwd=ROOT, check=check,
                          capture_output=True, text=True).stdout.strip()


def head() -> str:
    return subprocess.run(["git", "rev-parse", "HEAD"], cwd=ROOT,
                          capture_output=True, text=True).stdout.strip()


def find_vcpkg() -> str:
    exe = os.environ.get("VCPKG_EXE")
    if exe:
        return exe
    for cand in ("/tmp/vcpkg/vcpkg", os.path.expanduser("~/vcpkg/vcpkg")):
        if Path(cand).exists():
            return cand
    raise SystemExit("sync-vcpkg-registry: no vcpkg binary found "
                     "(set VCPKG_EXE)")


def run(exe: str, *args: str) -> subprocess.CompletedProcess:
    result = subprocess.run([exe, *args], cwd=ROOT, capture_output=True,
                            text=True)
    if result.returncode != 0:
        raise SystemExit(f"{' '.join(args)} failed:\n"
                         f"{result.stdout}{result.stderr}")
    return result


def main() -> int:
    exe = find_vcpkg()
    version = json.loads((SRC_DIR / "vcpkg.json").read_text())["version-semver"]

    # 1. Mirror the canonical port files.
    PORT_DIR.mkdir(parents=True, exist_ok=True)
    shutil.copy2(SRC_DIR / "vcpkg.json", PORT_DIR / "vcpkg.json")
    shutil.copy2(SRC_DIR / "portfile.cmake", PORT_DIR / "portfile.cmake")

    # 2. Canonical formatting, then write the formatted manifest back so the
    #    packaging source and the registry copy are byte-identical.
    run(exe, "format-manifest", str(PORT_DIR / "vcpkg.json"))
    shutil.copy2(PORT_DIR / "vcpkg.json", SRC_DIR / "vcpkg.json")

    # 3. Commit port changes so their tree hashes exist.
    dirty = git("status", "--porcelain", "--", *PORT_SPECS)
    if dirty:
        git("add", "ports", "packaging/vcpkg/port")
        git("commit", "-m", "vcpkg registry: sync port files")
        print("port files synced and committed")

    # 4. Regenerate the versions database with x-add-version redirection.
    if not (VERSIONS_DIR / "baseline.json").exists():
        VERSIONS_DIR.mkdir(parents=True, exist_ok=True)
        (VERSIONS_DIR / "baseline.json").write_text('{\n  "default": {}\n}\n')
    before = git("status", "--porcelain", "--", "versions")
    db_file = VERSIONS_DIR / "p-" / "plyengine.json"
    # If the same version is re-recorded with a different tree (port content
    # changed without a version bump), overwrite it. Only safe while the entry
    # is unpublished; after publishing, the sync caller must instead bump the
    # version in packaging/vcpkg/port/vcpkg.json first.
    adds = ["x-add-version", "--all"]
    if db_file.exists() and version in db_file.read_text():
        adds.append("--overwrite-version")
    run(exe, f"--x-builtin-ports-root={ROOT / 'ports'}",
        f"--x-builtin-registry-versions-dir={VERSIONS_DIR}", *adds)

    # 5. Commit the database update and report the baseline commit.
    after = git("status", "--porcelain", "--", *VERSIONS_SPECS)
    if after:
        git("add", "versions")
        git("commit", "-m", "vcpkg registry: update versions database")
    base = head()
    version = json.loads((PORT_DIR / "vcpkg.json").read_text())[
        "version-semver"]
    entry = json.loads((VERSIONS_DIR / "p-" / "plyengine.json").read_text())
    state = "updated" if after else "unchanged"
    print(f"versions db {state}: plyengine {version}, "
          f"entries {len(entry['versions'])}")
    print(f"vcpkg-configuration.json baseline: {base}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
