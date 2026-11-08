# Building plyEngine

## Requirements

| | |
|---|---|
| Compiler | MSVC 2022, GCC 12+, or AppleClang 14+ (C++20) |
| CMake | 3.24 or newer |
| Python 3 | with the `jinja2` package (generates the glad GL loader) |
| Node.js | 18+ — only for building the companion PWA (`companion/`) |

All C++ dependencies are pinned and downloaded by CMake `FetchContent` at
configure time, so the first build needs network access. Pins live in
`CMakeLists.txt`; nothing floats to "latest".

## Per-OS packages

### Linux (Debian/Ubuntu)

SDL3 builds from source and needs the usual windowing/GL development
packages, plus a font for the in-game HUD text:

```sh
sudo apt-get install -y \
  libx11-dev libxext-dev libxrandr-dev libxinerama-dev libxcursor-dev \
  libxi-dev libxkbcommon-dev libwayland-dev libdecor-0-dev \
  libgl1-mesa-dev libgles2-mesa-dev libxss-dev libxfixes-dev \
  libxpresent-dev libxshmfence-dev libxtst-dev \
  python3-jinja2 fonts-dejavu-core
```

For headless (CI) rendering checks: `xvfb`, and set `LIBGL_ALWAYS_SOFTWARE=1`
to force Mesa's software rasterizer.

If no system font is found, the demos still run — they skip text rendering
and print a notice. Install `fonts-dejavu-core` to see the HUD.

### Windows

Visual Studio 2022 with the "Desktop development with C++" workload. No other
packages needed; SDL3 is built from source as a static library.

```bat
cmake -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

### macOS

Xcode Command Line Tools. No other packages needed.

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## CMake options

| Option | Default | Effect |
|---|---|---|
| `PLY_BUILD_TESTS` | `ON` | Build the `ctest` suite |
| `PLY_BUILD_EXAMPLES` | `ON` | Build the demo executables |
| `PLY_SANITIZE` | `OFF` | Build with AddressSanitizer + UBSan |

Sanitizer build (this is how the CI finds use-after-free bugs):

```sh
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DPLY_SANITIZE=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

## The companion PWA

The phone controller is a TypeScript + Vite app in `companion/`. The game
server serves its build output over HTTP, so build it once before running
`ply_companion`:

```sh
cd companion
npm ci          # or: npm install
npm run build   # output in companion/dist
```

## Headless demo checks

Each demo has a `--verify` flag that asserts real outcomes, not just
"didn't crash":

```sh
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ./build/ply_tutorial --verify
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ./build/ply_companion --verify --no-mdns --web companion/dist
```

`ply_companion --verify` runs a scripted Autopilot through the real
companion-input queue and asserts a full match: coins collected, deaths to
the pit and the saw, player 2 moving independently, and a win.

## Troubleshooting

- **Configure fails fetching SDL3/glad/…** — the first configure needs
  network access; corporate proxies must allow `github.com`.
- **`jinja2` missing** — `pip install jinja2` (glad generates the loader at
  build time).
- **Window opens but stays black on Linux** — check `DISPLAY`; for CI use
  `xvfb-run -a` with `LIBGL_ALWAYS_SOFTWARE=1`.
- **HUD text missing** — no DejaVu font installed (see Linux packages).
- **`ply_companion` can't serve the phone page** — build `companion/dist`
  first, or pass `--web <path>`.
