# Contributing to plyEngine

Thanks for contributing. plyEngine is MIT-licensed and uses **no CLA**.

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

4. Open a PR. CI runs the build matrix (Windows / Linux / macOS).

## License

All contributions are under the project's MIT license (see `LICENSE`). Do not
contribute code under a license that is incompatible with MIT (e.g. GPL,
PolyForm, or any non-commercial clause).
