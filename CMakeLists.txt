cmake_minimum_required(VERSION 3.24)
project(plyEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PLY_BUILD_TESTS "Build plyEngine tests" ON)
option(PLY_BUILD_EXAMPLES "Build plyEngine examples" ON)
option(PLY_SANITIZE "Build with AddressSanitizer + UndefinedBehaviorSanitizer" OFF)

if(PLY_SANITIZE)
    add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=address,undefined)
endif()

include(FetchContent)

# --- SDL3 --------------------------------------------------------------------
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    sdl3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG        release-3.4.20
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(sdl3)

# --- glad (OpenGL loader) ----------------------------------------------------
FetchContent_Declare(
    glad
    GIT_REPOSITORY https://github.com/Dav1dde/glad.git
    GIT_TAG        v2.0.8
    GIT_SHALLOW    TRUE
    SOURCE_SUBDIR  cmake
)
FetchContent_MakeAvailable(glad)
glad_add_library(glad_gl_core_33 REPRODUCIBLE API gl:core=3.3)

# --- glm (math) --------------------------------------------------------------
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
set(GLM_ENABLE_EXPERIMENTAL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        1.0.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(glm)

# --- nlohmann/json (scene + spritesheet metadata) ----------------------------
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install OFF CACHE BOOL "" FORCE)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

# --- stb (image + truetype decoding; no build system upstream) ---------------
FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        4e9b368bfc071e97206ea5c6adba68dd0cbf8897
)
FetchContent_MakeAvailable(stb)

add_library(stb_impl STATIC engine/src/stb_impl.cpp)
target_include_directories(stb_impl PUBLIC ${stb_SOURCE_DIR})

# --- Core engine library -----------------------------------------------------
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
)
target_include_directories(plyEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/include
)
target_link_libraries(plyEngine
    PUBLIC glad_gl_core_33 glm::glm nlohmann_json::nlohmann_json
    PRIVATE SDL3::SDL3 stb_impl
)

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

    add_executable(ply_image_test tests/image.cpp)
    target_link_libraries(ply_image_test PRIVATE plyEngine)
    target_compile_definitions(ply_image_test PRIVATE
        PLY_TEST_DATA_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/data")
    add_test(NAME image COMMAND ply_image_test)
endif()

# --- Examples ----------------------------------------------------------------
if(PLY_BUILD_EXAMPLES)
    add_executable(ply_hello examples/hello/main.cpp)
    target_link_libraries(ply_hello PRIVATE plyEngine)

    add_executable(ply_ecs examples/ecs/main.cpp)
    target_link_libraries(ply_ecs PRIVATE plyEngine)

    add_executable(ply_assets examples/assets/main.cpp)
    target_link_libraries(ply_assets PRIVATE plyEngine)
endif()
