# Build the GeoTIFF test (now includes test file generation)
add_executable (test_geotiff test_geotiff.c plugin_tests.c geotiff_tests.c)
target_include_directories (test_geotiff PRIVATE "${PROJECT_SOURCE_DIR}/src" ${HDF5_INCLUDE_DIRS} ${GEOTIFF_INCLUDE_DIRS})

# Define the plugin path for the tests to use
target_compile_definitions(test_geotiff PRIVATE
    GEOTIFF_VOL_PLUGIN_PATH="${PROJECT_BINARY_DIR}/src")

# Link test_geotiff with required libraries
if (_have_config)
    target_link_libraries (test_geotiff PRIVATE HDF5::HDF5 TIFF::TIFF ${_GEOTIFF_TARGET})
else()
    target_link_libraries (test_geotiff PRIVATE HDF5::HDF5 ${TIFF_LIBRARIES} ${GEOTIFF_LIBRARIES})
endif()

# Build the h5dump-like VOL connector test
add_executable (test_h5dump_vol test_h5dump_vol.c)
target_include_directories (test_h5dump_vol PRIVATE "${PROJECT_SOURCE_DIR}/src" ${HDF5_INCLUDE_DIRS} ${GEOTIFF_INCLUDE_DIRS})
target_link_libraries (test_h5dump_vol PRIVATE HDF5::HDF5)

# Add the GeoTIFF test (if a sample GeoTIFF file exists)
if(EXISTS "${PROJECT_SOURCE_DIR}/test/sample.tif")
    add_test (test_geotiff_read test_geotiff_read "${PROJECT_SOURCE_DIR}/test/sample.tif")
    set_tests_properties(test_geotiff_read PROPERTIES
        ENVIRONMENT "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src")
endif()

# Add individual test cases
set(TEST_ENVIRONMENT "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src/Release\;${PROJECT_BINARY_DIR}/src;PATH=C:/src/hdf5-dev/install/bin\;C:/src/vcpkg.microsoft/installed/x64-windows/bin\;$ENV{PATH};HDF5_VOL_CONNECTOR=")

# Plugin registration tests
add_test(NAME test_getters COMMAND test_geotiff test_getters)
set_tests_properties(test_getters PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME test_multiple_registration COMMAND test_geotiff test_multiple_registration)
set_tests_properties(test_multiple_registration PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME test_registration_by_name COMMAND test_geotiff test_registration_by_name)
set_tests_properties(test_registration_by_name PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME test_registration_by_value COMMAND test_geotiff test_registration_by_value)
set_tests_properties(test_registration_by_value PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

# GeoTIFF functionality tests
add_test(NAME open_grayscale COMMAND test_geotiff open_grayscale)
set_tests_properties(open_grayscale PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME open_rgb COMMAND test_geotiff open_rgb)
set_tests_properties(open_rgb PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME read_grayscale COMMAND test_geotiff read_grayscale)
set_tests_properties(read_grayscale PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME read_rgb COMMAND test_geotiff read_rgb)
set_tests_properties(read_rgb PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME band_read_rgb COMMAND test_geotiff band_read_rgb)
set_tests_properties(band_read_rgb PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")
