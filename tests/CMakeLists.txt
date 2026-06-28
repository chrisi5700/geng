include(CTest)
find_package(Catch2 3 REQUIRED)
include(Catch)

function(add_test_executable TARGET_NAME SRC_FILES)
    target_add_executable(${TARGET_NAME} ${SRC_FILES})
    target_link_libraries(${TARGET_NAME} PRIVATE Catch2::Catch2WithMain)
    catch_discover_tests(${TARGET_NAME} DISCOVERY_MODE POST_BUILD)
endfunction()

# Add test executables here, e.g.:
#   add_test_executable(my_test my_test.cpp TestTypes.hpp)
#   target_link_libraries(my_test PRIVATE my_lib)

# Public umbrella header compile-guard — fails the build if <geng/geng.hpp> stops compiling standalone.
add_test_executable(umbrella_test UmbrellaTests.cpp)
target_link_libraries(umbrella_test PRIVATE geng)

# Scatter renderer CPU side (build_markers / resolve_marker_color). Device-free; reaches the internal
# scene header in src/ for white-box coverage of the marker instance assembly.
add_test_executable(scatter_test ScatterTests.cpp)
target_link_libraries(scatter_test PRIVATE geng)
target_include_directories(scatter_test PRIVATE ${geng_SOURCE_DIR}/src)

# Bar renderer CPU side (build_bars / bar_extent_points / resolve_bar_color). Same white-box setup.
add_test_executable(bar_test BarTests.cpp)
target_link_libraries(bar_test PRIVATE geng)
target_include_directories(bar_test PRIVATE ${geng_SOURCE_DIR}/src)

# Categorical axis CPU side (resolve_keyed / category_ticks). Same white-box setup.
add_test_executable(categorical_test CategoricalTests.cpp)
target_link_libraries(categorical_test PRIVATE geng)
target_include_directories(categorical_test PRIVATE ${geng_SOURCE_DIR}/src)