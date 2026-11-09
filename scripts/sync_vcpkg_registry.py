#!/usr/bin/env python3
"""Sync the repo-root vcpkg registry with packaging/vcpkg/port.

Called by scripts/sync-vcpkg-registry.sh (sqlite-free, stdlib only).
Protocol: ports/<n>/{vcpkg.json,portfile.cmake} committed in git, then
versions/<n>.json + versions/baseline.json reference the port dir's tree hash.
"""
import json
import subprocess
import sys
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
GIT_IDENTITY = ["-c", "user.name=MerverliPy",
                "-c", "user.email=merverlipy@users.noreply.github.com"]


def git(*args: str) -> str:
    return subprocess.run(
        ["git", *GIT_IDENTITY, *args], cwd=ROOT, check=True,
        capture_output=True, text=True).stdout.strip()


def main() -> int:
    bump = None
    if len(sys.argv) > 2 and sys.argv[1] == "--bump":
        bump = sys.argv[2]

    port_dir = ROOT / "ports" / "plyengine"
    src_dir = ROOT / "packaging" / "vcpkg" / "port"
    port_dir.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src_dir / "vcpkg.json", port_dir / "vcpkg.json")
    shutil.copy2(src_dir / "portfile.cmake", port_dir / "portfile.cmake")

    status = git("status", "--porcelain", "--", "ports/plyengine")
    if status:
        git("add", "ports/plyengine")
        git("commit", "-m", "vcpkg registry: sync port from packaging/vcpkg/port")

    tree = git("rev-parse", "HEAD:ports/plyengine")

    manifest_version = json.loads(
        (port_dir / "vcpkg.json").read_text())["version-semver"]

    if bump:
        version, port_version = bump, 0
    else:
        versions = json.loads(
            (ROOT / "versions" / "p-" / "plyengine.json").read_text())["versions"]
        version = manifest_version
        port_version = max(
            (e.get("port-version", 0) for e in versions
             if e.get("version-semver") == version),
            default=-1) + 1

    versions_doc = json.loads(
        (ROOT / "versions" / "p-" / "plyengine.json").read_text())
    # No-op when this exact content (same tree) is already recorded under the
    # same version; only genuinely changed port content gets a new entry.
    already = any(e.get("version-semver") == version
                  and e.get("git-tree") == tree
                  for e in versions_doc["versions"])
    if not already:
        entry = {"git-tree": tree, "version-semver": version,
                 "port-version": port_version}
        versions_doc["versions"].append(entry)
        versions_doc["versions"].sort(
            key=lambda e: (e.get("version-semver", ""),
                           e.get("port-version", 0)))
    new_versions_text = json.dumps(versions_doc, indent=2) + "\n"
    if new_versions_text != (ROOT / "versions" / "p-" / "plyengine.json").read_text():
        (ROOT / "versions" / "p-" / "plyengine.json").write_text(new_versions_text)

    if not already:
        baseline = json.loads(
            (ROOT / "versions" / "baseline.json").read_text())
        baseline["default"]["plyengine"] = {"baseline": tree,
                                            "port-version": port_version}
        new_baseline_text = json.dumps(baseline, indent=2) + "\n"
        if new_baseline_text != \
                (ROOT / "versions" / "baseline.json").read_text():
            (ROOT / "versions" / "baseline.json").write_text(
                new_baseline_text)

    print(f"registry synced: plyengine {version} "
          f"(port-version {port_version})"
          + (" [unchanged]" if already else " [changed]"))
    if already:
        print(f"git-tree: {tree} (unchanged, no version bump)")
        print(f"vcpkg-configuration.json baseline: {git('rev-parse', 'HEAD')}")
        return 0
    print(f"git-tree: {tree}")
    git("add", "versions")
    diff = subprocess.run(["git", *GIT_IDENTITY, "diff", "--cached", "--quiet"],
                          cwd=ROOT).returncode
    if diff != 0:
        git("commit", "-m",
            f"vcpkg registry: record version {version} "
            f"port-version {port_version}")
    print(f"vcpkg-configuration.json baseline: {git('rev-parse', 'HEAD')}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
