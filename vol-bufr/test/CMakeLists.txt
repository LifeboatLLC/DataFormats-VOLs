# ------------------------------------------------------------
# Tests
# ------------------------------------------------------------

# Build the BUFR test runner
add_executable(test_runner test_runner.c bufr_test.c)

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
        BUFR_VOL_PLUGIN_PATH="${PROJECT_BINARY_DIR}/src"
)

# ------------------------------------------------------------
# Copy test data next to the executable
# ------------------------------------------------------------

set(BUFR_SAMPLE "${PROJECT_SOURCE_DIR}/files/temp.bufr")
if (NOT EXISTS "${BUFR_SAMPLE}")
    message(FATAL_ERROR "BUFR sample not found: ${BUFR_SAMPLE}")
endif()

# Copy after building the executable, into the executable directory
add_custom_command(TARGET test_runner POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${BUFR_SAMPLE}"
            "$<TARGET_FILE_DIR:test_runner>/temp.bufr"
    COMMENT "Copying temp.bufr to test_runner output directory"
)

# ------------------------------------------------------------
# CTest
# ------------------------------------------------------------

# Environment for VOL plugin loading
# Note: include both ".../src/Release" (multi-config generators) and ".../src" (single-config)
set(TEST_ENVIRONMENT
    "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src/Release;${PROJECT_BINARY_DIR}/src;"
    "HDF5_VOL_CONNECTOR="
)

# BUFR functionality tests
add_test(NAME open_close COMMAND test_runner open_close)
add_test(NAME dataset_open_close COMMAND test_runner dataset_open_close)
set_tests_properties(open_close PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:test_runner>"
    ENVIRONMENT "${TEST_ENVIRONMENT}"
)

# If you have more test_runner subcommands, add them similarly:
# add_test(NAME something COMMAND test_runner something)
# set_tests_properties(something PROPERTIES
#     WORKING_DIRECTORY "$<TARGET_FILE_DIR:test_runner>"
#     ENVIRONMENT "${TEST_ENVIRONMENT}"
# )

