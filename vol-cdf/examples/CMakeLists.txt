# ------------------------------------------------------------
# Examples
# ------------------------------------------------------------
add_executable(cdf_example cdf_example.c)

# Include VOL connector headers (if the example includes them)
target_include_directories(cdf_example
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)

# Link dependencies via imported targets
target_link_libraries(cdf_example
    PRIVATE
        HDF5::HDF5
        CDF::cdf
)

# Define the plugin path for the tests to use
target_compile_definitions(cdf_example
    PRIVATE
        CDF_VOL_PLUGIN_PATH="${PROJECT_BINARY_DIR}/src"
)

# Ensure example2.cdf is generated before cdf_example tries to copy it
add_dependencies(cdf_example generate_example2)

# ------------------------------------------------------------
# Copy example data next to the executable
# ------------------------------------------------------------
add_custom_command(
    TARGET cdf_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${PROJECT_BINARY_DIR}/test/example2.cdf"
        "$<TARGET_FILE_DIR:cdf_example>/example2.cdf" 
    COMMENT "Copying example2.cdf to cdf_example output directory"
)

# ------------------------------------------------------------
# CTest: run example as a smoke test
# ------------------------------------------------------------
set(TEST_ENVIRONMENT "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src/Release\;${PROJECT_BINARY_DIR}/src;PATH=C:/src/hdf5-dev/install/bin\;C:/src/vcpkg.microsoft/installed/x64-windows/bin\;$ENV{PATH};HDF5_VOL_CONNECTOR=")

add_test(NAME cdf_example_test COMMAND cdf_example)
set_tests_properties(cdf_example_test PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:cdf_example>"
    ENVIRONMENT "${CDF_TEST_ENV}"
)