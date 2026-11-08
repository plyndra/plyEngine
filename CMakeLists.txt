cmake_minimum_required(VERSION 3.24)
project(plyEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PLY_BUILD_TESTS "Build plyEngine tests" ON)
option(PLY_BUILD_EXAMPLES "Build plyEngine examples" ON)

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
)
target_include_directories(plyEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/include
)
target_link_libraries(plyEngine
    PUBLIC glad_gl_core_33 glm::glm
    PRIVATE SDL3::SDL3
)

# --- Tests -------------------------------------------------------------------
if(PLY_BUILD_TESTS)
    enable_testing()
    add_executable(ply_smoke tests/smoke.cpp)
    target_link_libraries(ply_smoke PRIVATE plyEngine)
    add_test(NAME smoke COMMAND ply_smoke)
endif()

# --- Examples ----------------------------------------------------------------
if(PLY_BUILD_EXAMPLES)
    add_executable(ply_hello examples/hello/main.cpp)
    target_link_libraries(ply_hello PRIVATE plyEngine)
endif()
