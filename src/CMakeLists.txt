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
target_sources(geng PRIVATE Bounds2D.cpp Window.cpp Renderer.cpp OffscreenRenderer.cpp FontAtlas.cpp StbTrueTypeImpl.cpp)
target_include_directories(geng PUBLIC ${CMAKE_SOURCE_DIR}/include)
# SYSTEM: stb is not -Wall/clang-tidy/cppcheck clean — keep its header out of geng's analysis.
target_include_directories(geng SYSTEM PRIVATE ${Stb_INCLUDE_DIR})
target_compile_definitions(geng PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)
target_compile_definitions(geng PRIVATE GENG_SHADER_DIR="${CMAKE_SOURCE_DIR}/shaders")
target_compile_features(geng PUBLIC cxx_std_23)
target_link_libraries(geng
        PUBLIC glm::glm
               veng_nodes
               veng_managers
               veng_render_graph
               veng_resources
               veng_assets
               veng_context)
