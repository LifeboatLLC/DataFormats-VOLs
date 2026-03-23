# Build the CDF test (now includes test file generation)
add_executable (test_runner test_runner.c cdf_tests.c)

# Add HDF5 and CDF include directories
target_include_directories (test_runner PRIVATE "${PROJECT_SOURCE_DIR}/src" ${HDF5_INCLUDE_DIRS} ${CDF_INCLUDE_DIR})

# Define the plugin path for the tests to use
target_compile_definitions (test_runner PRIVATE
	CDF_VOL_PLUGIN_PATH="${PROJECT_BINARY_DIR}/src")

# Link test_runner with required libraries
target_link_libraries (test_runner PRIVATE HDF5::HDF5) 


# Set default working directory for all tests in this CMakeLists.txt to be the "build/test" directory where the test CDF files are located
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    PROPERTY TEST_WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# Copy the CDF data file "example1.cdf" to the build directory
configure_file(
    "${PROJECT_SOURCE_DIR}/files/example1.cdf"
    "${CMAKE_CURRENT_BINARY_DIR}/example1.cdf"
    COPYONLY
)

# # Add custom command to generate example2.cdf before running tests
set(GENERATED_CDF "${CMAKE_CURRENT_BINARY_DIR}/example2.cdf")
add_custom_command(
    OUTPUT ${GENERATED_CDF}
    COMMAND "$<TARGET_FILE:generate_example2_cdf>"
    DEPENDS generate_example2_cdf
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Generating example2.cdf"
)
add_custom_target(generate_example2
    DEPENDS ${GENERATED_CDF}
)

# Make test_runner depend on the generated example2.cdf to ensure it's created before tests run
add_dependencies(test_runner generate_example2)

# Add individual test cases
set(TEST_ENVIRONMENT "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src/Release\;${PROJECT_BINARY_DIR}/src;PATH=C:/src/hdf5-dev/install/bin\;C:/src/vcpkg.microsoft/installed/x64-windows/bin\;$ENV{PATH};HDF5_VOL_CONNECTOR=")

# CDF functionality tests
add_test(NAME open_close COMMAND test_runner open_close)
set_tests_properties(open_close PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME check_existence_and_open COMMAND test_runner check_existence_and_open)
set_tests_properties(check_existence_and_open PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME multi_links_exist COMMAND test_runner multi_links_exist)
set_tests_properties(multi_links_exist PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME link_attr_iterate_test COMMAND test_runner link_attr_iterate_test)
set_tests_properties(link_attr_iterate_test PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME read_variable COMMAND test_runner read_variable)
set_tests_properties(read_variable PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME dataset_datatype_conversion COMMAND test_runner dataset_datatype_conversion)
set_tests_properties(dataset_datatype_conversion PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME read_variable_attribute COMMAND test_runner read_variable_attribute)
set_tests_properties(read_variable_attribute PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME read_all_global_attributes COMMAND test_runner read_all_global_attributes)
set_tests_properties(read_all_global_attributes PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME read_indexed_global_attribute COMMAND test_runner read_indexed_global_attribute)
set_tests_properties(read_indexed_global_attribute PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")

add_test(NAME read_basic_rVariable_and_rEntry COMMAND test_runner read_basic_rVariable_and_rEntry)
set_tests_properties(read_basic_rVariable_and_rEntry PROPERTIES ENVIRONMENT "${TEST_ENVIRONMENT}")


