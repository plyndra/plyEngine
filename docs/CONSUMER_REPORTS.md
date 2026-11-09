# Outside-consumer build reports (beta-soak criterion 2)

`docs/ROADMAP.md` — "Beta soak: the exit criteria for v1.0.0" — gates the
1.0.0 promotion on at least one consumer *outside the project* building the
engine successfully via `find_package(plyEngine)` or FetchContent, with the
version and OS named. Owner self-testing cannot substitute (see the
`v1.0.0-alpha.1` release history: green CI is not evidence).

This file is the log for such reports. If you built something with
plyEngine, add a row (or open an issue and a maintainer will transcribe it).

| Date | Consumer (name/handle optional) | OS | plyEngine version | Consumption method | What they built | Evidence link |
|---|---|---|---|---|---|---|
| — | — | — | — | — | — | — |

Report template:

```markdown
- **Date:** YYYY-MM-DD
- **Consumer:** <handle or name, public if possible>
- **OS:** <e.g. Ubuntu 24.04, Windows 11, macOS 15>
- **Compiler:** <e.g. gcc 14.2, MSVC 19.44, AppleClang 17>
- **plyEngine version:** <e.g. 1.0.0-beta.3>
- **Method:** find_package / FetchContent / vcpkg / manual install
- **What was built:** <the scratch consumer, a game, a tool, ...>
- **Friction:** <anything that bit you, including doc errors>
- **Evidence:** <link to a repo, gist, issue, CI log — anything checkable>
```

Rules for what counts (from the roadmap): a build by someone other than the
engine maintainer, through one of the documented consumption paths, on a
named OS + version. Reports whose method or version cannot be verified from
the evidence link are marked as `unverified` in the table until resolved.

## In-repo reports that do *not* count

For transparency, the checks run by the maintainer (all green as of
2026-11-09, see the memory graph and CI):

- scratch `find_package` consumer, vcpkg overlay install + registry install,
  ASan/UBSan suite, installed-SDK consumer on the tag itself. These are the
  gate runs, not criterion-2 evidence.
