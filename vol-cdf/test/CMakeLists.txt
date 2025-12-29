# Build the GeoTIFF test (now includes test file generation)
add_executable (test_runner test_runner.c cdf_tests.c)

# Add HDF5 and CDF include directories
target_include_directories (test_runner PRIVATE "${PROJECT_SOURCE_DIR}/src" ${HDF5_INCLUDE_DIRS} ${CDF_INCLUDE_DIR})

# Define the plugin path for the tests to use
target_compile_definitions (test_runner PRIVATE
	CDF_VOL_PLUGIN_PATH="${PROJECT_BINARY_DIR}/src")

# Link test_runner with required libraries
target_link_libraries (test_runner PRIVATE HDF5::HDF5) 

# Copy the CDF data file to the build directory
file (COPY "${PROJECT_SOURCE_DIR}/test/example1.cdf" DESTINATION "${PROJECT_SOURCE_DIR}/build/test")

# Add individual test cases
set(TEST_ENVIRONMENT "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src/Release\;${PROJECT_BINARY_DIR}/src;PATH=C:/src/hdf5-dev/install/bin\;C:/src/vcpkg.microsoft/installed/x64-windows/bin\;$ENV{PATH};HDF5_VOL_CONNECTOR=")

# CDF functionality tests
add_test(NAME open_close COMMAND test_runner open_close)
set_tests_properties(open_close PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")
