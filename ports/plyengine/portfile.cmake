# vcpkg portfile for plyEngine.
#
# vcpkg forbids network access during a build, so the engine is configured
# with PLY_USE_SYSTEM_DEPS=ON: the CMake-packaged dependencies (SDL3, glm,
# nlohmann_json, Crow/asio) come from vcpkg, and the three header-only ones
# with no upstream build system (stb, miniaudio, mdns) are staged by this
# portfile and passed as source directories.
#
# Note: PLYENGINE_REF/SHA are the release being packaged. Bump both when a new
# tag is cut (the release workflow prints the tarball hash).

set(PLYENGINE_VERSION "1.0.0")

# Tag-archive SHA512, exactly as vcpkg_from_github fetches it (GitHub archive
# URL). Verified by sha512sum of the downloaded v1.0.0-beta.3 tag archive on
# 2026-11-09. Bump both when a new tag is cut (the release workflow prints the
# tarball hash).
set(PLYENGINE_SHA512 "29409e3cf5021bf94dd0531778bde29589385e6191f54cc78e1d24eedec6412a440b51f822b87c36a48d2515d410ce6bc64755391c0e073fbfb697b43b77dd38")

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO plyndra/plyEngine
    REF "v${PLYENGINE_VERSION}"
    SHA512 "${PLYENGINE_SHA512}"
    HEAD_REF main
)

# Header-only dependencies with no upstream CMake package. Pinned to the exact
# commits/tags the engine's own FetchContent build uses, so the registry build
# and the source build consume identical code.
vcpkg_from_github(
    OUT_SOURCE_PATH STB_SOURCE_PATH
    REPO nothings/stb
    REF 4e9b368bfc071e97206ea5c6adba68dd0cbf8897
    SHA512 60227ea75172e89eb50233ee1e0eef1cb6ee1a93926e45bd62f009d15ce8863d129f2a82d336d2082714bcf26319c96800bca3622a67f6324ce060acf7a570a1
)

vcpkg_from_github(
    OUT_SOURCE_PATH MINIAUDIO_SOURCE_PATH
    REPO mackron/miniaudio
    REF 0.11.26
    SHA512 5b5031bc41e57c8f46a3a27c240bc69a7e2e3180d914329c0b73122ad7d678a77dfb5e5018042ef36ccd25bf00151802908dc8166c906edbf739029bf6017c29
)

vcpkg_from_github(
    OUT_SOURCE_PATH MDNS_SOURCE_PATH
    REPO mjansson/mdns
    REF a569c4759bd47e0f2a7bfc4d4c19620445782806
    SHA512 8fdfa267115fa2dc5dcb34ed2e54a60c46bcdbd7094d7c3ec177d75e9799bfcf159f353d39387d67e59149fbe74b0c88adada83b9c089303e7c6815d4c0e309f
)

# The scripting layer (M9) links the real Lua 5.4 reference sources, staged
# from the same pinned archive the engine's own FetchContent build uses.
vcpkg_from_github(
    OUT_SOURCE_PATH LUA_SOURCE_PATH
    REPO lua/lua
    REF v5.4.7
    SHA512 1e02cb1bf541037f11e44a828266506538b845a8d7d1db12a4bccf3e8533f7766435ddc9a779fcdd62ed2f260b87b652a58a7a261ad01fa30835883d923e8cb4
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DPLY_USE_SYSTEM_DEPS=ON
        -DPLY_BUILD_TESTS=OFF
        -DPLY_BUILD_EXAMPLES=OFF
        -DPLY_BUILD_DEPS_INSTALL=OFF
        -DPLY_INSTALL=ON
        "-DPLY_STB_SOURCE_DIR=${STB_SOURCE_PATH}"
        "-DPLY_MINIAUDIO_SOURCE_DIR=${MINIAUDIO_SOURCE_PATH}"
        "-DPLY_MDNS_SOURCE_DIR=${MDNS_SOURCE_PATH}"
        "-DPLY_LUA_SOURCE_DIR=${LUA_SOURCE_PATH}"
    MAYBE_UNUSED_VARIABLES
        PLY_STB_SOURCE_DIR
        PLY_MINIAUDIO_SOURCE_DIR
        PLY_MDNS_SOURCE_DIR
        PLY_LUA_SOURCE_DIR
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME plyEngine CONFIG_PATH lib/cmake/plyEngine)

# The engine is static-only; drop the empty bin/ tree some generators create.
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include"
                    "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
