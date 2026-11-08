# Contributing to plyEngine

Thanks for contributing. plyEngine is MIT-licensed and uses **no CLA**.
All participation is governed by our [Code of Conduct](CODE_OF_CONDUCT.md).

Bugs and feature requests: use the
[issue templates](https://github.com/plyndra/plyEngine/issues/new/choose).
User-visible changes belong in [CHANGELOG.md](CHANGELOG.md) (Keep a
Changelog format) under the `Unreleased` heading.

## Developer Certificate of Origin (DCO)

Every commit must carry a `Signed-off-by` line certifying you wrote the change
or otherwise have the right to contribute it. Add it with `git commit -s`.

```
Signed-off-by: Your Name <you@example.com>
```

This is the same mechanism used by the Linux kernel and Git. By signing off
you assert the [Developer Certificate of Origin](https://developercertificate.org/),
version 1.1.

## Workflow

1. Fork and branch from `main`.
2. Keep commits small and self-contained.
3. Run the build and tests before opening a PR:

   ```sh
   cmake -B build
   cmake --build build
   ctest --test-dir build --output-on-failure
   ```

4. Open a PR. CI runs the build matrix (Windows / Linux / macOS), the
   sanitizer job, and the headless `--verify` checks.

## License

All contributions are under the project's MIT license (see `LICENSE`). Do not
contribute code under a license that is incompatible with MIT (e.g. GPL,
PolyForm, or any non-commercial clause).
