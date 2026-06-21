cmake_minimum_required(VERSION 3.28)
project(geng VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF) # Causes errors with clang-tidy when used with gcc builds.

# Generate install/export rules so a downstream can find_package(geng). Defaults to on only when geng
# is the top-level project (an add_subdirectory consumer manages its own install).
option(GENG_INSTALL "Generate geng install and export rules" ${PROJECT_IS_TOP_LEVEL})

# Include CMake modules for target helpers and compiler settings
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(TargetHelpers)

# Default cache variables (can be overridden by CMakePresets.json or command line)
set(ENABLE_COMPILER_WARNINGS ON CACHE BOOL "Enable compiler warnings")
set(TREAT_WARNINGS_AS_ERRORS OFF CACHE BOOL "Treat warnings as errors")
set(ENABLE_SANITIZER_ADDRESS OFF CACHE BOOL "Enable AddressSanitizer")
set(ENABLE_SANITIZER_THREAD OFF CACHE BOOL "Enable ThreadSanitizer")
set(ENABLE_SANITIZER_UNDEFINED OFF CACHE BOOL "Enable UndefinedBehaviorSanitizer")
set(ENABLE_LTO OFF CACHE BOOL "Enable Link Time Optimization")
set(ENABLE_CLANG_TIDY OFF CACHE BOOL "Enable clang-tidy static analysis")
set(CLANG_TIDY_WARNINGS_AS_ERRORS OFF CACHE BOOL "Treat clang-tidy warnings as errors")
set(ENABLE_CPPCHECK OFF CACHE BOOL "Enable cppcheck static analysis")
set(ENABLE_COVERAGE OFF CACHE BOOL "Enable code coverage instrumentation")
set(STRICT_MODE OFF CACHE BOOL "Enable all checks and treat everything as errors")
set(ENABLE_CCACHE OFF CACHE BOOL "Enable ccache for faster rebuilds")

# veng — Vulkan rendering backend (git submodule under libs/veng), used for rendering.
# Consumed via add_subdirectory: because geng is the top-level project, veng sees
# PROJECT_IS_TOP_LEVEL=OFF and so builds only its libraries — its example apps, test
# suite, and docs stay off. Added before our own targets so they can link veng_* libs.
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/veng/CMakeLists.txt"
   OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/feng/CMakeLists.txt")
    message(FATAL_ERROR
        "libs/veng or libs/feng is missing — initialize submodules with:\n"
        "  git submodule update --init --recursive")
endif()

# veng (render-graph backend) and feng (text rendering, which depends on veng) are third-party
# dependencies. Build them with our warnings and sanitizers (so the ASan runtime and flags stay
# consistent across the link), but do NOT subject their sources to geng's clang-tidy, cppcheck, or
# coverage — each has its own quality gates. Their target helpers read these cache vars when defining
# targets, so we shadow them OFF for their scope only, then restore the cache values for geng below.
set(ENABLE_CLANG_TIDY OFF)
set(ENABLE_CPPCHECK OFF)
set(ENABLE_COVERAGE OFF)
# When geng installs, veng and feng must install too (geng's exported targets link veng::veng and
# feng::feng, which have to be in export sets under the same prefix); flip their install options on.
if(GENG_INSTALL)
    set(VENG_INSTALL ON)
    set(FENG_INSTALL ON)
endif()
# SYSTEM marks their interface include dirs as -isystem for geng's targets, so geng's own -Wall/-Werror,
# clang-tidy, and cppcheck don't analyse their headers when pulled into geng translation units (the
# tooling above only spares each library's *own* compilation). veng is added first so feng (and geng)
# can link veng::veng; feng then reuses that one veng target instead of building its vendored copy.
add_subdirectory(libs/veng SYSTEM)
add_subdirectory(libs/feng SYSTEM)
unset(ENABLE_CLANG_TIDY)
unset(ENABLE_CPPCHECK)
unset(ENABLE_COVERAGE)

add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(bench)

# Enable testing at the top level so `ctest` run from the build root recurses into tests/
# (catch_discover_tests registers there). Without this the root CTestTestfile is empty and CI's
# `ctest --test-dir build/<preset>` reports "No tests were found".
enable_testing()
add_subdirectory(tests)

# Optional Doxygen API docs (doxygen-awesome-css theme). Top-level dev concern only — never built
# for an add_subdirectory() consumer.
if(PROJECT_IS_TOP_LEVEL)
    include(Docs)
endif()