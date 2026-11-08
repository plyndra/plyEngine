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

## Prebuilt binaries

Each [GitHub Release](https://github.com/plyndra/plyEngine/releases) carries
per-OS archives with the demo executables, the built companion PWA and the
docs, plus a source tarball.

| Platform | Archive | Minimum requirement |
|---|---|---|
| Linux x86-64 | `*-linux-x86_64.tar.gz` | glibc 2.35+ (Ubuntu 22.04+, Debian 12+), OpenGL 3.3 driver — built in a pinned Ubuntu 22.04 container || Windows x86-64 | `*-windows-x86_64.zip` | Windows 10+, OpenGL 3.3 driver |
| macOS arm64 | `*-macos-arm64.tar.gz` | Apple Silicon, built on GitHub's macOS runner |

The release job does not merely build these archives: it unpacks the Linux
one on a clean Ubuntu 22.04 that has **no compiler and no CMake**, installs
only the runtime libraries, and runs both headless checks there. It also
asserts the glibc floor (`objdump -T` symbol check) and builds the tutorial
from the published source tarball. A binary that demands a newer glibc, or an
archive missing a runtime library, fails the release instead of shipping.

Runtime dependencies on a Linux *desktop* are the usual OpenGL stack
transitively (`libgl1`, `libglx-mesa0`, `libgl1-mesa-dri` on Debian/Ubuntu,
plus X11 or Wayland libraries). For a headless run you need exactly:

```sh
sudo apt-get install -y xvfb libgl1-mesa-dri libglx-mesa0 fonts-dejavu-core
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a ./ply_tutorial --verify
```

Unpack and run, e.g.

```sh
./ply_tutorial          # the tutorial game
./ply_companion         # reference platformer (PWA is in ./companion)
```

macOS binaries are unsigned; run `xattr -dr com.apple.quarantine .` in the
unpacked directory if Gatekeeper objects.

## CMake options

| Option | Default | Effect |
|---|---|---|
| `PLY_BUILD_TESTS` | `ON` | Build the `ctest` suite |
| `PLY_BUILD_EXAMPLES` | `ON` | Build the demo executables |
| `PLY_SANITIZE` | `OFF` | Build with AddressSanitizer + UBSan |
| `PLY_INSTALL` | `ON` | Generate install rules and `plyEngineConfig.cmake` |
| `PLY_BUILD_DEPS_INSTALL` | `ON` | Let GLM/json/SDL3 install themselves (needed by `PLY_INSTALL`) |
| `PLY_VERSION_STRING` | `1.0.0-beta.1` | Version reported by `ply::kVersion` (generated header) |

`ply::kVersion` lives in a **generated** header. `PLY_VERSION_STRING` in
`CMakeLists.txt` is the only place the version is authored; the git tag must
match it. Override at configure time with
`-DPLY_VERSION_STRING=1.0.0-alpha.3` if you build a fork.
Sanitizer build (this is how the CI finds use-after-free bugs):

```sh
cmake -B build-asan -DCMAKE_BUILD_TYPE=Debug -DPLY_SANITIZE=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

## Installing the engine (find_package)

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
cmake --install build --prefix /opt/plyengine      # add --config Release on MSVC
```

That writes:

```
/opt/plyengine/include/plyEngine/*.hpp     engine headers (incl. generated Version.hpp)
/opt/plyengine/include/{glm,nlohmann}/     public header dependencies
/opt/plyengine/include/SDL3/               SDL3 headers
/opt/plyengine/lib/libplyEngine.a  (+ glad/stb/miniaudio/SDL3 archives)
/opt/plyengine/lib/cmake/plyEngine/        plyEngineConfig.cmake + targets
/opt/plyengine/lib/cmake/SDL3/             SDL3Config.cmake
```

A consumer then does:

```cmake
find_package(plyEngine CONFIG REQUIRED)   # -DCMAKE_PREFIX_PATH=/opt/plyengine
target_link_libraries(mygame PRIVATE plyEngine::plyEngine)
```

What the consumer must have is only `Threads`, `SDL3`, `glm` and
`nlohmann_json` — all installed into the same prefix by the command above.
Crow/asio, mdns, stb and miniaudio are header-only, compiled into the engine's
objects, and are deliberately not part of the consumer's configure step.

Set `-DPLY_INSTALL=OFF` for an embed-only build with no install rules.

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
- **ASan reports leaks when running a demo** — if every stack frame is inside
  `libGLX_mesa.so`, it is Mesa's GLX state that is never torn down, not
  plyEngine (the equivalent GL-free `ctest` suites are clean). Suppress it with
  `LSAN_OPTIONS=suppressions=<file>` if you need a quiet run. Note that
  `--verify` always simulates a full playthrough (1800 frames), whatever
  `--frames` says.
