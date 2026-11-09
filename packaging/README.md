# Packaging (vcpkg / conan)

plyEngine installs as a CMake package (`find_package(plyEngine)`); see
[../docs/BUILDING.md](../docs/BUILDING.md#installing-the-engine-find_package).
This directory prepares the two C++ package managers on top of that.

## Status: vcpkg executed and verified; conan still prepared-only

The repository went public on 2026-11-09, which removed the fetch blocker.

### vcpkg — executed end to end (2026-11-09)

The port in [`vcpkg/port/`](vcpkg/port) was run against a live vcpkg
bootstrap (2026-10-29 toolchain):

- Bumped to `1.0.0-beta.2` with the real tag-archive SHA512 (the private-repo
  hard gate was removed — it served its purpose while the repo was closed).
- Two real defects surfaced and were fixed:
  1. `vcpkg.json` `supports` used `!(uwp | arm & windows)` without parens —
     the manifest schema rejects mixed `&`/`|`; now `!(uwp | (arm & windows))`.
  2. The port predated M9: offline mode now requires `PLY_LUA_SOURCE_DIR`,
     so the portfile also stages Lua 5.4.7 (same pinned archive the engine's
     own FetchContent build uses) and passes `-DPLY_LUA_SOURCE_DIR`.
- `vcpkg install plyengine --overlay-ports=packaging/vcpkg/port` succeeds
  (builds sdl3, glm, nlohmann-json, crow, asio, then the engine; ~31 s), and
  a scratch consumer configured with the vcpkg toolchain does
  `find_package(plyEngine CONFIG REQUIRED)`, links, and prints
  `consumer sees plyEngine 1.0.0-beta.2`.

Remaining for registry publication: submit the port upstream to
[microsoft/vcpkg](https://github.com/microsoft/vcpkg) (`ports/plyengine`) or
span a curated registry.

### The engine-side mode both recipes rely on — executed earlier

```sh
# no FetchContent at all: deps from CMake packages + staged header sources
cmake -B build -DPLY_USE_SYSTEM_DEPS=ON \
  -DCMAKE_PREFIX_PATH="/path/to/sdl3+glm+json+crow/prefix" \
  -DPLY_STB_SOURCE_DIR=<dir with stb_image.h> \
  -DPLY_MINIAUDIO_SOURCE_DIR=<dir with miniaudio.h> \
  -DPLY_MDNS_SOURCE_DIR=<dir with mdns.h> \
  -DASIO_INCLUDE_DIR=<asio/include>
ctest --test-dir build
```

### conan — not yet executed

- `conan create .` against a live Conan cache (Conan Center's SDL3 package
  name and version are not verified here; the recipe pins `sdl/3.4.20`, which
  may need adjusting to whatever the centre actually publishes).

Do not advertise the conan recipe as working until that is run.

## Layout

```
vcpkg/port/{vcpkg.json,portfile.cmake}   port (verified 2026-11-09)
vcpkg/usage                              consumer snippet (CMake)
conan/conanfile.py                       recipe (CMakeToolchain + CMakeDeps)
conan/conandata.yml                      pinned sources + hashes
```

## Why the engine has a system-deps mode at all

`PLY_USE_SYSTEM_DEPS=ON` exists because package managers forbid network access
during a build, while the default build fetches every dependency at configure
time. The mode switches the five CMake-packaged dependencies (SDL3, glm,
nlohmann_json, Crow, asio) to `find_package` and takes the three header-only
ones (stb, miniaudio, mdns) from packager-staged directories. The OpenGL
loader is vendored in `third_party/glad`, so no code generation or Python is
needed either.
