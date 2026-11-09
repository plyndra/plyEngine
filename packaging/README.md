# Packaging (vcpkg / conan)

plyEngine installs as a CMake package (`find_package(plyEngine)`); see
[../docs/BUILDING.md](../docs/BUILDING.md#installing-the-engine-find_package).
This directory prepares the two C++ package managers on top of that.

## Status: prepared, not shippable — blocked on repository visibility

`github.com/plyndra/plyEngine` is currently a **private** repository. vcpkg
and conan both fetch sources anonymously during install, so neither recipe can
work until the repository is public. That is the only blocker; everything
engine-side is done and tested (below).

The vcpkg portfile fails with an explicit message rather than a hash mismatch,
because "hash mismatch" reads like a corrupted download and sends people
debugging the wrong thing.

### What *has* been executed

The build mode these ports rely on is verified locally, end to end:

```sh
# no FetchContent at all: deps from CMake packages + staged header sources
cmake -B build -DPLY_USE_SYSTEM_DEPS=ON \
  -DCMAKE_PREFIX_PATH="/path/to/sdl3+glm+json+crow/prefix" \
  -DPLY_STB_SOURCE_DIR=<dir with stb_image.h> \
  -DPLY_MINIAUDIO_SOURCE_DIR=<dir with miniaudio.h> \
  -DPLY_MDNS_SOURCE_DIR=<dir with mdns.h> \
  -DASIO_INCLUDE_DIR=<asio/include>
ctest --test-dir build        # 14/14
```

Those are exactly the variables `portfile.cmake` and `conanfile.py` set, so
the *arguments* are proven; the registry plumbing is not.

### What has *not* been executed

- `vcpkg install plyengine --overlay-ports=packaging/vcpkg/port` (needs a
  public repo + the tag-archive SHA512)
- `conan create .` against a live Conan cache (Conan Center's SDL3 package
  name and version are not verified here; the recipe pins `sdl/3.4.20`, which
  may need adjusting to whatever the centre actually publishes)

Do not advertise these recipes as working until that is run.

### To unblock

1. Make the repository public (owner decision — this is also what the beta
   soak criterion "at least one outside consumer builds it" needs).
2. Fill `PLYENGINE_SHA512` in `vcpkg/port/portfile.cmake`:

   ```sh
   curl -LO https://github.com/plyndra/plyEngine/archive/refs/tags/v1.0.0-beta.1.tar.gz
   sha512sum v1.0.0-beta.1.tar.gz
   ```

   and delete the guard above it.
3. `vcpkg install plyengine --overlay-ports=packaging/vcpkg/port`, fix whatever
   surfaces, then either keep the overlay or submit to the registry.
4. Repeat criterion 2 for conan (`conan create .`), adjusting the SDL3
   reference to what Conan Center provides.

## Layout

```
vcpkg/port/{vcpkg.json,portfile.cmake}   port + hard gate
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
