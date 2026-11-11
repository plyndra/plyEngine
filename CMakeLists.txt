cmake_minimum_required(VERSION 3.24)

# Single source of truth for the version string. The git tag must match it
# (docs/ROADMAP.md release checklist); Version.hpp is generated from this
# value so the header can never drift from the build.
set(PLY_VERSION_STRING "1.0.1" CACHE STRING "plyEngine version string")

project(plyEngine VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PLY_BUILD_TESTS "Build plyEngine tests" ON)
option(PLY_BUILD_EXAMPLES "Build plyEngine examples" ON)
option(PLY_INSTALL "Generate install rules for find_package(plyEngine)" ON)
option(PLY_BUILD_DEPS_INSTALL "Let fetchcontent dependencies install themselves (needed by PLY_INSTALL)" ON)
option(PLY_BUILD_SCRIPTING "Build the Lua scripting layer" ON)
option(PLY_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

if(PLY_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

include(FetchContent)

# Dependency acquisition mode.
#
#   OFF (default) - fetch and build every dependency at configure time. Needs
#                   network access on the first configure.
#   ON            - use find_package for every dependency that ships a CMake
#                   config (SDL3, glm, nlohmann_json, Crow+asio) and take the
#                   header-only ones (stb, miniaudio, mdns) from the source
#                   directories the packager stages. This is the mode package
#                   managers use: vcpkg and conan forbid network access during
#                   a build.
option(PLY_USE_SYSTEM_DEPS "Use find_package/source dirs instead of FetchContent (offline builds)" OFF)

# --- SDL3 --------------------------------------------------------------------
# SDL3 is a static dependency of an installed engine, so it must install its
# own archive + headers + SDL3Config.cmake (SDL_INSTALL defaults to OFF when
# SDL3 is consumed as a subproject, which breaks find_dependency(SDL3)).
set(SDL_INSTALL ${PLY_INSTALL} CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
if(PLY_USE_SYSTEM_DEPS)
    find_package(SDL3 CONFIG REQUIRED)
else()
    FetchContent_Declare(
        sdl3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG        release-3.4.20
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(sdl3)
endif()

# --- glad (OpenGL loader, vendored) ------------------------------------------
# The loader is generated output committed under third_party/glad (see its
# README): generation needs Python + jinja2 + a network fetch at build time,
# which broke CI twice and blocks offline package builds (vcpkg/conan).
add_library(glad_gl_core_33 STATIC third_party/glad/src/gl.c)
set_target_properties(glad_gl_core_33 PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include>")
target_include_directories(glad_gl_core_33 PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include)

# --- glm (math) --------------------------------------------------------------
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# glm and nlohmann/json are header-only and appear in plyEngine's public
# headers, so an installed SDK must ship their headers and CMake configs too.
set(GLM_BUILD_INSTALL ${PLY_BUILD_DEPS_INSTALL} CACHE BOOL "" FORCE)
set(GLM_ENABLE_EXPERIMENTAL OFF CACHE BOOL "" FORCE)
if(PLY_USE_SYSTEM_DEPS)
    find_package(glm CONFIG REQUIRED)
else()
    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG        1.0.3
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(glm)
endif()

# --- nlohmann/json (scene + spritesheet metadata) ----------------------------
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install ${PLY_BUILD_DEPS_INSTALL} CACHE BOOL "" FORCE)
if(PLY_USE_SYSTEM_DEPS)
    find_package(nlohmann_json CONFIG REQUIRED)
else()
    FetchContent_Declare(
        nlohmann_json
        GIT_REPOSITORY https://github.com/nlohmann/json.git
        GIT_TAG        v3.12.0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(nlohmann_json)
endif()

# --- stb (image + truetype decoding; no build system upstream) ---------------
# In system-deps mode the packager stages the headers and passes
# -DPLY_STB_SOURCE_DIR=<dir containing stb_image.h>.
if(PLY_USE_SYSTEM_DEPS)
    if(NOT PLY_STB_SOURCE_DIR)
        message(FATAL_ERROR "PLY_USE_SYSTEM_DEPS=ON requires PLY_STB_SOURCE_DIR "
                            "(directory containing stb_image.h)")
    endif()
    set(stb_SOURCE_DIR "${PLY_STB_SOURCE_DIR}")
else()
    FetchContent_Declare(
        stb
        GIT_REPOSITORY https://github.com/nothings/stb.git
        GIT_TAG        4e9b368bfc071e97206ea5c6adba68dd0cbf8897
    )
    FetchContent_MakeAvailable(stb)
endif()

add_library(stb_impl STATIC engine/src/stb_impl.cpp)
# PRIVATE: consumers never include stb headers (nothing public does), so the
# installed export stays free of third-party include paths.
target_include_directories(stb_impl PRIVATE ${stb_SOURCE_DIR})

# --- miniaudio (audio device, mixing, decoding; single header) ---------------
if(PLY_USE_SYSTEM_DEPS)
    if(NOT PLY_MINIAUDIO_SOURCE_DIR)
        message(FATAL_ERROR "PLY_USE_SYSTEM_DEPS=ON requires PLY_MINIAUDIO_SOURCE_DIR "
                            "(directory containing miniaudio.h)")
    endif()
    set(miniaudio_SOURCE_DIR "${PLY_MINIAUDIO_SOURCE_DIR}")
else()
    FetchContent_Declare(
        miniaudio
        GIT_REPOSITORY https://github.com/mackron/miniaudio.git
        GIT_TAG        0.11.26
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(miniaudio)
endif()

# --- asio (Crow's transport) -------------------------------------------------
# Crow's own CMake config pulls asio in when it is found, so system-deps mode
# only needs Crow's config.
if(PLY_USE_SYSTEM_DEPS)
    if(PLY_CROW_SOURCE_DIR)
        # Packager-staged full Crow checkout (e.g. Conan, where Conan Center has
        # no usable Crow recipe). Its own CMake exports the same INTERFACE target
        # a find_package(Crow) config would; it finds asio via find_package(asio)
        # -> asio::asio, so the packager must also make asio discoverable.
        find_package(asio CONFIG REQUIRED)
        add_subdirectory("${PLY_CROW_SOURCE_DIR}" crow_build EXCLUDE_FROM_ALL)
        set(crow_SOURCE_DIR "${PLY_CROW_SOURCE_DIR}")
        if(NOT ASIO_INCLUDE_DIR)
            get_target_property(ASIO_INCLUDE_DIR asio::asio
                                INTERFACE_INCLUDE_DIRECTORIES)
        endif()
    else()
        # Crow's config locates asio through its own Findasio, which accepts
        # ASIO_INCLUDE_DIR (or an asio::asio config package). A packager supplies
        # one of the two; nothing is fetched here.
        find_package(Crow CONFIG REQUIRED)
    endif()
else()
    FetchContent_Declare(
        asio
        GIT_REPOSITORY https://github.com/chriskohlhoff/asio.git
        GIT_TAG        asio-1-39-0
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(asio)
    set(ASIO_INCLUDE_DIR "${asio_SOURCE_DIR}/asio/include" CACHE PATH "" FORCE)
endif()

# --- Crow (embedded HTTP + WebSocket server) ---------------------------------
set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
if(NOT PLY_USE_SYSTEM_DEPS)
    FetchContent_Declare(
        crow
        GIT_REPOSITORY https://github.com/CrowCpp/Crow.git
        GIT_TAG        v1.3.5
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(crow)
endif()

# --- mjansson/mdns (public-domain mDNS/DNS-SD advertising) -------------------
if(PLY_USE_SYSTEM_DEPS)
    if(NOT PLY_MDNS_SOURCE_DIR)
        message(FATAL_ERROR "PLY_USE_SYSTEM_DEPS=ON requires PLY_MDNS_SOURCE_DIR "
                            "(directory containing mdns.h)")
    endif()
    set(mdns_SOURCE_DIR "${PLY_MDNS_SOURCE_DIR}")
else()
    FetchContent_Declare(
        mdns
        GIT_REPOSITORY https://github.com/mjansson/mdns.git
        GIT_TAG        a569c4759bd47e0f2a7bfc4d4c19620445782806
    )
    FetchContent_MakeAvailable(mdns)
endif()

# --- Lua (scripting) ---------------------------------------------------------
# Pinned Lua 5.4 source built as a static library. Only diagnostics-friendly
# libraries are opened by the binding (see engine/src/Scripting.cpp), but the
# full reference implementation is linked so nothing is silently missing.
if(PLY_BUILD_SCRIPTING)
    if(PLY_USE_SYSTEM_DEPS)
        # Offline builds must not fetch: the packager stages the Lua sources.
        if(NOT PLY_LUA_SOURCE_DIR)
            message(FATAL_ERROR "PLY_USE_SYSTEM_DEPS=ON with scripting requires "
                                "PLY_LUA_SOURCE_DIR (directory containing lapi.c)")
        endif()
        set(lua_SOURCE_DIR "${PLY_LUA_SOURCE_DIR}")
    else()
        FetchContent_Declare(
            lua
            URL https://github.com/lua/lua/archive/refs/tags/v5.4.7.tar.gz
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE
        )
        FetchContent_MakeAvailable(lua)
    endif()

    # Everything except the standalone interpreters (lua.c, luac.c via onelua.c)
    # and luaconf-hosted test harness (ltests.c).
    set(PLY_LUA_SOURCES
        lapi.c lauxlib.c lbaselib.c lcode.c lcorolib.c lctype.c ldblib.c
        ldebug.c ldo.c ldump.c lfunc.c lgc.c linit.c liolib.c llex.c
        lmathlib.c lmem.c loadlib.c lobject.c lopcodes.c loslib.c lparser.c
        lstate.c lstring.c lstrlib.c ltable.c ltablib.c ltm.c lundump.c
        lutf8lib.c lvm.c lzio.c
    )
    list(TRANSFORM PLY_LUA_SOURCES PREPEND "${lua_SOURCE_DIR}/")
    add_library(lua STATIC ${PLY_LUA_SOURCES})
    # BUILD_INTERFACE only: Scripting.hpp does not expose Lua types, so a
    # consumer never includes <lua.h> and nothing needs installing.
    target_include_directories(lua PUBLIC $<BUILD_INTERFACE:${lua_SOURCE_DIR}>)
    if(UNIX AND NOT APPLE)
        target_link_libraries(lua PUBLIC m dl)
    endif()
endif()

add_library(miniaudio_impl STATIC engine/src/miniaudio_impl.cpp)
target_include_directories(miniaudio_impl PRIVATE ${miniaudio_SOURCE_DIR})
if(UNIX AND NOT APPLE)
    target_link_libraries(miniaudio_impl PUBLIC dl pthread m)
endif()

# --- Core engine library -----------------------------------------------------
find_package(Threads REQUIRED)  # Crow/asio and miniaudio both need it

add_library(plyEngine STATIC
    engine/src/engine.cpp
    engine/src/Window.cpp
    engine/src/Input.cpp
    engine/src/Shader.cpp
    engine/src/Texture.cpp
    engine/src/SpriteAtlas.cpp
    engine/src/SpriteBatch.cpp
    engine/src/OrthoCamera.cpp
    engine/src/World.cpp
    engine/src/Transform.cpp
    engine/src/FixedTimestep.cpp
    engine/src/App.cpp
    engine/src/Image.cpp
    engine/src/Spritesheet.cpp
    engine/src/Font.cpp
    engine/src/TextRenderer.cpp
    engine/src/Sprite.cpp
    engine/src/Physics.cpp
    engine/src/PhysicsBody.cpp
    engine/src/Tilemap.cpp
    engine/src/UIAtlas.cpp
    engine/src/StructuralHash.cpp
    engine/src/NineSlice.cpp
    engine/src/UILayout.cpp
    engine/src/UIBar.cpp
    engine/src/UISlot.cpp
    engine/src/UIHotbar.cpp
    engine/src/UIPanel.cpp
    engine/src/RenderTarget.cpp
    engine/src/PixelText.cpp
    engine/src/UIRouter.cpp
    engine/src/Minimap.cpp
    engine/src/MapLoader.cpp
    engine/src/UIMargins.cpp
    engine/src/SpatialHashGrid.cpp
    engine/src/DebugDrawer.cpp
    engine/src/AudioSystem.cpp
    engine/src/CompanionProtocol.cpp
    engine/src/RoomCode.cpp
    engine/src/CompanionSession.cpp
    engine/src/CompanionInput.cpp
    engine/src/CompanionServer.cpp
    engine/src/MdnsAdvertiser.cpp
)
if(PLY_BUILD_SCRIPTING)
    target_sources(plyEngine PRIVATE engine/src/Scripting.cpp)
endif()
target_include_directories(plyEngine PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/engine/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated>
    $<INSTALL_INTERFACE:include>
)

# The public headers require C++20 (defaulted comparison operators in
# Entity/ControllerState are the ones a consumer notices first). Declaring it
# PUBLIC makes the requirement propagate to anything that links plyEngine —
# without this, a consumer project compiles the engine's headers with the
# compiler default and fails. Verified with a scratch add_subdirectory
# consumer, and guarded at release time by the smoke-source job.
target_compile_features(plyEngine PUBLIC cxx_std_20)

# Generated version header: PLY_VERSION_STRING above is the only place the
# version is written by hand. It lands in the same generated include root the
# target exports above, and is installed alongside the hand-written headers.
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/generated/plyEngine)
configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/include/plyEngine/Version.hpp.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/plyEngine/Version.hpp
    @ONLY
)

# Third-party include paths stay PRIVATE: no public plyEngine header includes
# glad, stb, miniaudio, Crow, asio or mdns, so an installed SDK ships only our
# headers plus GLM and nlohmann/json (the two the API exposes types from).
target_include_directories(plyEngine PRIVATE
    ${mdns_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glad/include
    ${stb_SOURCE_DIR}
    ${miniaudio_SOURCE_DIR}
    ${crow_SOURCE_DIR}/include
    ${ASIO_INCLUDE_DIR}
)

target_link_libraries(plyEngine
    PUBLIC glad_gl_core_33 glm::glm nlohmann_json::nlohmann_json Threads::Threads
    PRIVATE
        # SDL3 is static, so it must reach the consumer's link line, but it
        # owns its own export set (it is a separate project). Adding this
        # reference to the exported interface would make CMake demand SDL3's
        # internal targets (SDL3_Headers) join our export set, which cascades
        # forever. Instead the reference is build-only here, and
        # plyEngineConfig.cmake appends SDL3::SDL3 at find_package() time.
        $<BUILD_INTERFACE:SDL3::SDL3>
        stb_impl miniaudio_impl
)

# Crow/asio are header-only and only used to compile CompanionServer.cpp, so
# they are a build-time-only dependency: BUILD_INTERFACE keeps them out of the
# installed link interface (a consumer must not need Crow to link the engine).
target_link_libraries(plyEngine PRIVATE $<BUILD_INTERFACE:Crow::Crow>)

if(PLY_BUILD_SCRIPTING)
    # Static Lua must be on a consumer's link line, so it joins the export set
    # like the other internal archives (glad, stb_impl, miniaudio_impl).
    target_link_libraries(plyEngine PRIVATE lua)
endif()


# --- Install (find_package(plyEngine)) ---------------------------------------
if(PLY_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    set(PLY_INSTALL_TARGETS plyEngine glad_gl_core_33 stb_impl miniaudio_impl)
    if(PLY_BUILD_SCRIPTING)
        list(APPEND PLY_INSTALL_TARGETS lua)
    endif()

    install(TARGETS ${PLY_INSTALL_TARGETS}
        EXPORT plyEngineTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/engine/include/plyEngine
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
        FILES_MATCHING PATTERN "*.hpp"
    )
    # The generated version header is part of the public API.
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/generated/plyEngine/Version.hpp
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/plyEngine
    )

    install(EXPORT plyEngineTargets
        NAMESPACE plyEngine::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plyEngine
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/plyEngineConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/plyEngineConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plyEngine
    )
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/plyEngineConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )
    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/plyEngineConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/plyEngineConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/plyEngine
    )
endif()

# --- Tests -------------------------------------------------------------------
if(PLY_BUILD_TESTS)
    enable_testing()
    add_executable(ply_smoke tests/smoke.cpp)
    target_link_libraries(ply_smoke PRIVATE plyEngine)
    add_test(NAME smoke COMMAND ply_smoke)

    # Pure-math tests: no GL context required, so they run on any CI runner.
    add_executable(ply_atlas_test tests/atlas.cpp)
    target_link_libraries(ply_atlas_test PRIVATE plyEngine)
    add_test(NAME atlas COMMAND ply_atlas_test)

    add_executable(ply_world_test tests/world.cpp)
    target_link_libraries(ply_world_test PRIVATE plyEngine)
    add_test(NAME world COMMAND ply_world_test)

    add_executable(ply_world_stress_test tests/world_stress.cpp)
    target_link_libraries(ply_world_stress_test PRIVATE plyEngine)
    add_test(NAME world_stress COMMAND ply_world_stress_test)

    add_executable(ply_timestep_test tests/timestep.cpp)
    target_link_libraries(ply_timestep_test PRIVATE plyEngine)
    add_test(NAME timestep COMMAND ply_timestep_test)

    add_executable(ply_transform_test tests/transform.cpp)
    target_link_libraries(ply_transform_test PRIVATE plyEngine)
    add_test(NAME transform COMMAND ply_transform_test)

    add_executable(ply_assets_test tests/assets.cpp)
    target_link_libraries(ply_assets_test PRIVATE plyEngine)
    add_test(NAME assets COMMAND ply_assets_test)

    add_executable(ply_spritesheet_test tests/spritesheet.cpp)
    target_link_libraries(ply_spritesheet_test PRIVATE plyEngine)
    add_test(NAME spritesheet COMMAND ply_spritesheet_test)

    # UI atlas schema: pure data, so the round-trip and named-error graders
    # (E-a1.1, E-a1.2) run without a GL context.
    # UI geometry: nine-slice quad emission and the structural hash (E-a2.x).
    # Integer-only output, so no GL context and no floating-point determinism.
    add_executable(ply_nine_slice_test tests/nine_slice.cpp)
    target_link_libraries(ply_nine_slice_test PRIVATE plyEngine)
    add_test(NAME nine_slice COMMAND ply_nine_slice_test)

    add_executable(ply_ui_atlas_test tests/ui_atlas.cpp)
    target_link_libraries(ply_ui_atlas_test PRIVATE plyEngine)
    target_compile_definitions(ply_ui_atlas_test PRIVATE
        PLY_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME ui_atlas COMMAND ply_ui_atlas_test)

    # UI layout: anchors, offset+scale, integer snapping and the two-process
    # layout hash (E-a4.x). Pure integer math, so no GL context and no
    # floating-point determinism.
    add_executable(ply_ui_layout_test tests/ui_layout.cpp)
    target_link_libraries(ply_ui_layout_test PRIVATE plyEngine)
    add_test(NAME ui_layout COMMAND ply_ui_layout_test)

    # Bar widget geometry: fill fraction -> pixel width, clamps, ticks (E-a5.1).
    # Pure integer math, so no GL context and no floating-point determinism.
    add_executable(ply_ui_bar_test tests/ui_bar.cpp)
    target_link_libraries(ply_ui_bar_test PRIVATE plyEngine)
    add_test(NAME ui_bar COMMAND ply_ui_bar_test)

    # Slot state mapping: state -> frame region, stack-count formatting
    # (E-a5.2), graded against the fixture atlas. Pure data, no GL context.
    add_executable(ply_ui_slot_test tests/ui_slot.cpp)
    target_link_libraries(ply_ui_slot_test PRIVATE plyEngine)
    target_compile_definitions(ply_ui_slot_test PRIVATE
        PLY_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME ui_slot COMMAND ply_ui_slot_test)

    # Hotbar: row layout, selection wrap, hit-test after layout rounding
    # (E-a5.3). Pure integer math, so no GL context and no floating-point
    # determinism.
    add_executable(ply_ui_hotbar_test tests/ui_hotbar.cpp)
    target_link_libraries(ply_ui_hotbar_test PRIVATE plyEngine)
    add_test(NAME ui_hotbar COMMAND ply_ui_hotbar_test)

    # Panel title bar + tooltip clamping (E-a5.4): the tooltip must never leave
    # the logical viewport for any cursor position. Pure integer math.
    add_executable(ply_ui_panel_test tests/ui_panel.cpp)
    target_link_libraries(ply_ui_panel_test PRIVATE plyEngine)
    add_test(NAME ui_panel COMMAND ply_ui_panel_test)

    # Render target policy: integer upscale, nearest mapping of a 1 px line,
    # camera snap and scene layer order (E-a7.x). Geometry and order only — no
    # GL context, so it runs on any CI runner.
    add_executable(ply_render_target_test tests/render_target.cpp)
    target_link_libraries(ply_render_target_test PRIVATE plyEngine)
    add_test(NAME render_target COMMAND ply_render_target_test)

    # Pixel text: BMFont parse, baseline-aligned integer layout, ink bounds
    # equal to the drawn quads, and glyph hashes stable at 10/20 px (E-a6.1).
    add_executable(ply_pixel_text_test tests/pixel_text.cpp)
    target_link_libraries(ply_pixel_text_test PRIVATE plyEngine)
    target_compile_definitions(ply_pixel_text_test PRIVATE
        PLY_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME pixel_text COMMAND ply_pixel_text_test)

    # Input routing: UI consumes before world, press-capture keeps the release,
    # and the scripted synthetic input source replays deterministically
    # (E-a10.1). Pure data, so no window and no device.
    add_executable(ply_ui_router_test tests/ui_router.cpp)
    target_link_libraries(ply_ui_router_test PRIVATE plyEngine)
    add_test(NAME ui_router COMMAND ply_ui_router_test)

    # Minimap: world -> minimap px at the four corners and the centre, the
    # marker transform at integer scale, and the render-target downscale
    # (E-a8.1). Pure integer math, so no GL context and no floating-point
    # determinism.
    add_executable(ply_minimap_test tests/minimap.cpp)
    target_link_libraries(ply_minimap_test PRIVATE plyEngine)
    add_test(NAME minimap COMMAND ply_minimap_test)

    # Map loader: native JSON map + static tileset slice, load/save/load
    # round-trip, named validator errors and the world-quad row flip
    # (E-a9.1). Pure data, so no texture and no GL context.
    add_executable(ply_map_loader_test tests/map_loader.cpp)
    target_link_libraries(ply_map_loader_test PRIVATE plyEngine)
    target_compile_definitions(ply_map_loader_test PRIVATE
        PLY_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME map_loader COMMAND ply_map_loader_test)

    # Margin derivation from the alpha contour (E-a3.1/E-a3.2): derived ==
    # declared on the ui.png fixture, and the derived caps stay fixed across
    # three panel sizes. Pure data: an in-memory RGBA buffer, no GL context.
    add_executable(ply_ui_margins_test tests/ui_margins.cpp)
    target_link_libraries(ply_ui_margins_test PRIVATE plyEngine)
    target_compile_definitions(ply_ui_margins_test PRIVATE
        PLY_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME ui_margins COMMAND ply_ui_margins_test)

    add_executable(ply_image_test tests/image.cpp)
    target_link_libraries(ply_image_test PRIVATE plyEngine)
    target_compile_definitions(ply_image_test PRIVATE
        PLY_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME image COMMAND ply_image_test)

    add_executable(ply_physics_test tests/physics.cpp)
    target_link_libraries(ply_physics_test PRIVATE plyEngine)
    add_test(NAME physics COMMAND ply_physics_test)

    add_executable(ply_companion_server_test tests/companion_server.cpp)
    target_link_libraries(ply_companion_server_test PRIVATE plyEngine)
    target_compile_definitions(ply_companion_server_test PRIVATE
        PLY_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME companion_server COMMAND ply_companion_server_test)
    set_tests_properties(companion_server PROPERTIES TIMEOUT 60)

    add_executable(ply_companion_test tests/companion.cpp)
    target_link_libraries(ply_companion_test PRIVATE plyEngine Threads::Threads)
    add_test(NAME companion COMMAND ply_companion_test)

    if(PLY_BUILD_SCRIPTING)
        add_executable(ply_scripting_test tests/scripting.cpp)
        target_link_libraries(ply_scripting_test PRIVATE plyEngine)
        add_test(NAME scripting COMMAND ply_scripting_test)
    endif()

    add_executable(ply_audio_test tests/audio.cpp)
    target_link_libraries(ply_audio_test PRIVATE plyEngine)
    target_compile_definitions(ply_audio_test PRIVATE
        PLY_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME audio COMMAND ply_audio_test)

    # Reference-demo game logic: header-only, GL-free playthrough of the
    # M7 platformer driven by the scripted Autopilot.
    add_executable(ply_platformer_test tests/platformer.cpp)
    target_link_libraries(ply_platformer_test PRIVATE plyEngine)
    target_include_directories(ply_platformer_test PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/examples/companion)
    add_test(NAME platformer COMMAND ply_platformer_test)
endif()

# --- Examples ----------------------------------------------------------------
if(PLY_BUILD_EXAMPLES)
    add_executable(ply_hello examples/hello/main.cpp)
    target_link_libraries(ply_hello PRIVATE plyEngine)

    add_executable(ply_ecs examples/ecs/main.cpp)
    target_link_libraries(ply_ecs PRIVATE plyEngine)

    add_executable(ply_assets examples/assets/main.cpp)
    target_link_libraries(ply_assets PRIVATE plyEngine)

    add_executable(ply_physics examples/physics/main.cpp)
    target_link_libraries(ply_physics PRIVATE plyEngine)

    add_executable(ply_companion examples/companion/main.cpp)
    target_link_libraries(ply_companion PRIVATE plyEngine)

    add_executable(ply_tutorial examples/tutorial/main.cpp)
    target_link_libraries(ply_tutorial PRIVATE plyEngine)

    if(PLY_BUILD_SCRIPTING)
        add_executable(ply_scripting examples/scripting/main.cpp)
        target_link_libraries(ply_scripting PRIVATE plyEngine)
    endif()
endif()
