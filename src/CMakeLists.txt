# Define geng's own libraries here. See README.md for CMake library patterns.
#
# geng builds on the veng_* rendering libraries provided by the libs/veng submodule
# (e.g. veng_render_graph, veng_managers, veng_pipelines). Example:
#
#   target_add_library(geng)
#   target_sources(geng PRIVATE Graph2D.cpp)
#   target_include_directories(geng PUBLIC ${CMAKE_SOURCE_DIR}/include)
#   target_link_libraries(geng PUBLIC veng_render_graph veng_managers veng_pipelines)
#   target_compile_features(geng PUBLIC cxx_std_23)
