# Resolve libvulkan.so.1 through the SYSTEM loader (which has WSI surface support); vcpkg's
# bundled loader is built without a runpath, so a windowed surface can't be created against it.

# GENG_ASSET_DIR lets the demos resolve bundled assets (the tick-label font) by absolute path.
target_add_executable(playground main.cpp)
target_compile_features(playground PRIVATE cxx_std_23)
target_compile_definitions(playground PRIVATE GENG_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
target_link_libraries(playground PRIVATE geng)
set_target_properties(playground PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "")

# Headless PNG render of the same plot — no window required.
target_add_executable(render_png render_png.cpp)
target_compile_features(render_png PRIVATE cxx_std_23)
target_compile_definitions(render_png PRIVATE GENG_ASSET_DIR="${CMAKE_SOURCE_DIR}/assets")
target_link_libraries(render_png PRIVATE geng)
set_target_properties(render_png PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "")
