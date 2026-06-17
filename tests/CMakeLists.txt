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