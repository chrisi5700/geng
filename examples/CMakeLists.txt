# Examples of the public geng API.
#
# Headless examples render offscreen (PNG / frame sequences) and need only core geng. Windowed
# examples open a surface through the geng-glfw front-end and are gated behind GENG_BUILD_WINDOW.
#
# All examples resolve libvulkan.so.1 through the SYSTEM loader (which has WSI surface support);
# vcpkg's bundled loader has no runpath, so a windowed surface can't be created against it.

# Compiled into every example so they exit clean under the ASan preset (the only leaks are the
# third-party Vulkan-loader/libdbus ones the runtime hooks in this file suppress). See the file header.
set(GENG_LSAN_SUPPRESSIONS ${CMAKE_CURRENT_SOURCE_DIR}/lsan_suppressions.cpp)

# --- headless examples (always built) ---
function(geng_add_headless_example name)
    target_add_executable(${name} ${name}.cpp)
    target_sources(${name} PRIVATE ${GENG_LSAN_SUPPRESSIONS})
    target_compile_features(${name} PRIVATE cxx_std_23)
    target_link_libraries(${name} PRIVATE geng)
    set_target_properties(${name} PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "")
endfunction()

geng_add_headless_example(figure_png)
geng_add_headless_example(circle_video)
geng_add_headless_example(mandelbrot)
geng_add_headless_example(d20_advantage)

# --- windowed examples (need a surface + the geng-glfw front-end) ---
if(NOT GENG_BUILD_WINDOW)
    return()
endif()

function(geng_add_example name)
    target_add_executable(${name} ${name}.cpp)
    target_sources(${name} PRIVATE ${GENG_LSAN_SUPPRESSIONS})
    target_compile_features(${name} PRIVATE cxx_std_23)
    target_link_libraries(${name} PRIVATE geng geng-glfw)
    set_target_properties(${name} PROPERTIES BUILD_WITH_INSTALL_RPATH TRUE INSTALL_RPATH "")
endfunction()

geng_add_example(growing_sine)
geng_add_example(lissajous)
geng_add_example(live_signals)
geng_add_example(function_explorer)
geng_add_example(scatter_shapes)
