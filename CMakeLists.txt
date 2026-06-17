cmake_minimum_required(VERSION 3.28)
project(geng)
set(CMAKE_CXX_SCAN_FOR_MODULES OFF) # Causes errors with clang-tidy when used with gcc builds.

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
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/libs/veng/CMakeLists.txt")
    message(FATAL_ERROR
        "libs/veng is missing — initialize submodules with:\n"
        "  git submodule update --init --recursive")
endif()

# veng is a third-party dependency. Build it with our warnings and sanitizers (so the
# ASan runtime and flags stay consistent across the link), but do NOT subject its sources
# to geng's clang-tidy, cppcheck, or coverage — those are our own quality gates, not veng's
# (veng ships no .clang-tidy, so our strict root config would otherwise govern its code).
# veng's target helpers read these cache vars when it defines its targets, so we shadow
# them OFF for veng's scope only, then restore the cache values for geng's targets below.
set(ENABLE_CLANG_TIDY OFF)
set(ENABLE_CPPCHECK OFF)
set(ENABLE_COVERAGE OFF)
# SYSTEM marks veng's interface include dirs as -isystem for geng's targets, so geng's own
# -Wall/-Werror, clang-tidy, and cppcheck don't analyse veng's headers when they're pulled into
# geng translation units (the tooling above only spares veng's *own* compilation).
add_subdirectory(libs/veng SYSTEM)
unset(ENABLE_CLANG_TIDY)
unset(ENABLE_CPPCHECK)
unset(ENABLE_COVERAGE)

add_subdirectory(src)
add_subdirectory(playground)
add_subdirectory(bench)
add_subdirectory(tests)