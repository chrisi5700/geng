# geng — a 2D graphing library rendered through the veng reactive render graph.
#
# `GLM_FORCE_DEPTH_ZERO_TO_ONE` must match veng (it sets the same define publicly) so the glm
# matrices geng builds agree with veng's clip-space convention — and, since glm is header-only,
# so every translation unit sees one consistent definition (no ODR mismatch). Linking veng::veng
# already carries this define, but geng uses glm directly so it states the requirement too.
#
# `GENG_SHADER_DIR` lets the renderer hand geng's own shaders/ directory to `Context::create`
# as an extra Slang search path, so `GraphicsNode("line.vert", ...)` resolves our `.slang` files.
# `GENG_SHADER_DIR` / `GENG_ASSET_DIR` are the compile-time defaults; both honour an environment
# override at runtime (see Figure.cpp) so an installed package can relocate them.

find_package(glm CONFIG REQUIRED)

target_add_library(geng)
target_sources(geng PRIVATE
        Bounds2D.cpp Window.cpp
        Theme.cpp Figure.cpp Interactor.cpp)
target_include_directories(geng PUBLIC $<BUILD_INTERFACE:${geng_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_compile_definitions(geng PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_compile_definitions(geng PRIVATE GENG_SHADER_DIR="${geng_SOURCE_DIR}/shaders")
# GENG_ASSET_DIR lets Figure resolve its bundled default tick-label font by absolute path.
target_compile_definitions(geng PRIVATE GENG_ASSET_DIR="${geng_SOURCE_DIR}/assets")
target_compile_features(geng PUBLIC cxx_std_23)
# veng::veng is veng's umbrella target: one link carries the whole engine and its usage requirements.
# feng::feng is the text-rendering library (font atlas + glyph layout + text render-graph wiring); the
# tick-label text geng draws goes through it. feng links veng::veng transitively.
target_link_libraries(geng PUBLIC glm::glm veng::veng feng::feng)
add_library(geng::geng ALIAS geng)

# geng-glfw — the optional GLFW windowed front-end (WindowApp). It owns a window + swapchain and
# presents a Figure via Figure::render_into, so it stays a thin layer over the core library: the
# graphing code itself never depends on a window. Gated so a headless consumer (Qt, PNG) can skip it.
option(GENG_BUILD_WINDOW "Build the GLFW windowed front-end (geng-glfw) and windowed examples" ON)
if(GENG_BUILD_WINDOW)
    target_add_library(geng-glfw)
    target_sources(geng-glfw PRIVATE WindowApp.cpp)
    target_include_directories(geng-glfw PUBLIC $<BUILD_INTERFACE:${geng_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
    target_compile_features(geng-glfw PUBLIC cxx_std_23)
    # Core geng links veng (and glfw, transitively) PUBLIC, so WindowApp.cpp resolves the swapchain /
    # command / context managers and the GLFW-backed geng::Window through this single link.
    target_link_libraries(geng-glfw PUBLIC geng)
    add_library(geng::geng-glfw ALIAS geng-glfw)
endif()

# --- Install / export: make find_package(geng) work --------------------------------------------
# Gated by GENG_INSTALL (default: top-level only — see the root CMakeLists, which also flips
# VENG_INSTALL on so veng::veng is co-installed and gengConfig's find_dependency(veng) resolves it).
if(GENG_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    set(_geng_install_targets geng)
    if(GENG_BUILD_WINDOW)
        list(APPEND _geng_install_targets geng-glfw)
    endif()

    install(TARGETS ${_geng_install_targets}
            EXPORT gengTargets
            ARCHIVE  DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY  DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME  DESTINATION ${CMAKE_INSTALL_BINDIR}
            INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

    install(DIRECTORY ${geng_SOURCE_DIR}/include/geng DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    # geng's shaders + bundled assets (the default tick-label font); the library finds them via the
    # GENG_SHADER_DIR / GENG_ASSET_DIR env vars (gengConfig exposes the paths as geng_SHADER_DIR /
    # geng_ASSET_DIR).
    install(DIRECTORY ${geng_SOURCE_DIR}/shaders/ DESTINATION ${CMAKE_INSTALL_DATADIR}/geng/shaders)
    install(DIRECTORY ${geng_SOURCE_DIR}/assets/  DESTINATION ${CMAKE_INSTALL_DATADIR}/geng/assets)

    install(EXPORT gengTargets
            NAMESPACE geng::
            FILE gengTargets.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/geng)

    configure_package_config_file(
            ${geng_SOURCE_DIR}/cmake/gengConfig.cmake.in
            ${CMAKE_CURRENT_BINARY_DIR}/gengConfig.cmake
            INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/geng
            PATH_VARS CMAKE_INSTALL_DATADIR)
    write_basic_package_version_file(
            ${CMAKE_CURRENT_BINARY_DIR}/gengConfigVersion.cmake
            VERSION ${PROJECT_VERSION}
            COMPATIBILITY SameMajorVersion)
    install(FILES
            ${CMAKE_CURRENT_BINARY_DIR}/gengConfig.cmake
            ${CMAKE_CURRENT_BINARY_DIR}/gengConfigVersion.cmake
            DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/geng)
endif()
