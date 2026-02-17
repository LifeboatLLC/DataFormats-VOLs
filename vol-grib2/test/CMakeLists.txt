# ------------------------------------------------------------
# Tests
# ------------------------------------------------------------

# Build the GRIB2 test runner
add_executable(test_runner test_runner.c grib2_test.c)

# Include connector headers from src/ plus HDF5 (via imported target)
target_include_directories(test_runner
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)

# Link required libraries (HDF5 and ecCodes via imported targets)
target_link_libraries(test_runner
    PRIVATE
        HDF5::HDF5
        ECCODES::eccodes
)

# Define the plugin path for the tests to use
target_compile_definitions(test_runner
    PRIVATE
        GRIB2_VOL_PLUGIN_PATH="${PROJECT_BINARY_DIR}/src"
)

# ------------------------------------------------------------
# Copy test data next to the executable
# ------------------------------------------------------------

set(GRIB2_SAMPLE "${PROJECT_SOURCE_DIR}/files/example-1.grib2")
if (NOT EXISTS "${GRIB2_SAMPLE}")
    message(FATAL_ERROR "GRIB2 sample not found: ${GRIB2_SAMPLE}")
endif()

# Copy after building the executable, into the executable directory
add_custom_command(TARGET test_runner POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GRIB2_SAMPLE}"
            "$<TARGET_FILE_DIR:test_runner>/example-1.grib2"
    COMMENT "Copying example-1.grib2 to test_runner output directory"
)

# ------------------------------------------------------------
# CTest
# ------------------------------------------------------------

# Environment for VOL plugin loading
# Note: include both ".../src/Release" (multi-config generators) and ".../src" (single-config)
set(TEST_ENVIRONMENT
    "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src;${PROJECT_BINARY_DIR}/src/Release;"
    "HDF5_VOL_CONNECTOR="
)

# GRIB2 functionality tests
add_test(NAME basic_open_close COMMAND test_runner basic_open_close)
set_tests_properties(basic_open_close PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:test_runner>"
    ENVIRONMENT "${TEST_ENVIRONMENT}"
)
add_test(NAME open_close COMMAND test_runner open_close)
set_tests_properties(open_close PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:test_runner>"
    ENVIRONMENT "${TEST_ENVIRONMENT}"
)

add_test(NAME link_exists COMMAND test_runner link_exists)
set_tests_properties(link_exists PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:test_runner>"
    ENVIRONMENT "${TEST_ENVIRONMENT}"
)

add_test(NAME multi_link_exists COMMAND test_runner multi_link_exists)
set_tests_properties(multi_link_exists PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:test_runner>"
    ENVIRONMENT "${TEST_ENVIRONMENT}"
)
add_test(NAME link_iterate COMMAND test_runner link_iterate)
set_tests_properties(link_iterate PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:test_runner>"
    ENVIRONMENT "${TEST_ENVIRONMENT}"
)


# If you have more test_runner subcommands, add them similarly:
# add_test(NAME something COMMAND test_runner something)
# set_tests_properties(something PROPERTIES
#     WORKING_DIRECTORY "$<TARGET_FILE_DIR:test_runner>"
#     ENVIRONMENT "${TEST_ENVIRONMENT}"
# )

