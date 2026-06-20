# geng — a 2D graphing library rendered through the veng reactive render graph.
#
# `GLM_FORCE_DEPTH_ZERO_TO_ONE` must match veng (it sets the same define publicly) so the glm
# matrices geng builds agree with veng's clip-space convention — and, since glm is header-only,
# so every translation unit sees one consistent definition (no ODR mismatch).
#
# `GENG_SHADER_DIR` lets the renderer hand geng's own shaders/ directory to `Context::create`
# as an extra Slang search path, so `GraphicsNode("line.vert", ...)` resolves our `.slang` files.

find_package(glm CONFIG REQUIRED)
find_package(Stb REQUIRED) # stb_truetype, for the font atlas

target_add_library(geng)
target_sources(geng PRIVATE
        Bounds2D.cpp Window.cpp FontAtlas.cpp StbTrueTypeImpl.cpp
        Theme.cpp Figure.cpp Interactor.cpp)
target_include_directories(geng PUBLIC ${CMAKE_SOURCE_DIR}/include)
# SYSTEM: stb is not -Wall/clang-tidy/cppcheck clean — keep its header out of geng's analysis.
target_include_directories(geng SYSTEM PRIVATE ${Stb_INCLUDE_DIR})
target_compile_definitions(geng PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_compile_definitions(geng PRIVATE GENG_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders")
# GENG_ASSET_DIR lets Figure resolve its bundled default tick-label font by absolute path.
target_compile_definitions(geng PRIVATE GENG_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
target_compile_features(geng PUBLIC cxx_std_23)
target_link_libraries(geng
        PUBLIC glm::glm
               veng_nodes
               veng_managers
               veng_render_graph
               veng_resources
               veng_assets
               veng_context)

# geng-glfw — the optional GLFW windowed front-end (WindowApp). It owns a window + swapchain and
# presents a Figure via Figure::render_into, so it stays a thin layer over the core library: the
# graphing code itself never depends on a window. Gated so a headless consumer (Qt, PNG) can skip it.
option(GENG_BUILD_WINDOW "Build the GLFW windowed front-end (geng-glfw) and windowed examples" ON)
if(GENG_BUILD_WINDOW)
    target_add_library(geng-glfw)
    target_sources(geng-glfw PRIVATE WindowApp.cpp)
    target_include_directories(geng-glfw PUBLIC ${CMAKE_SOURCE_DIR}/include)
    target_compile_features(geng-glfw PUBLIC cxx_std_23)
    # Core geng links veng (and glfw, transitively) PUBLIC, so WindowApp.cpp resolves the swapchain /
    # command / context managers and the GLFW-backed geng::Window through this single link.
    target_link_libraries(geng-glfw PUBLIC geng)
endif()
