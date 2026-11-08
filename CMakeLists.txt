cmake_minimum_required(VERSION 3.21)
project(plyEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PLY_BUILD_TESTS "Build plyEngine tests" ON)
# Wired in at their milestones:
option(PLY_ENABLE_SDL3 "Enable SDL3 foundation (M1)" OFF)
option(PLY_ENABLE_COMPANION "Enable embedded companion server (M6)" OFF)

# --- Core engine library -----------------------------------------------------
# No external dependencies at M0; SDL3/GLM/Crow are added at their milestones.
add_library(plyEngine STATIC
    engine/src/engine.cpp
)
target_include_directories(plyEngine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/engine/include
)

# --- Tests -------------------------------------------------------------------
if(PLY_BUILD_TESTS)
    enable_testing()
    add_executable(ply_smoke tests/smoke.cpp)
    target_link_libraries(ply_smoke PRIVATE plyEngine)
    add_test(NAME smoke COMMAND ply_smoke)
endif()
