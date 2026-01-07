# ------------------------------------------------------------
# Examples
# ------------------------------------------------------------

add_executable(bufr_example bufr_example.c)

# Include VOL connector headers (if the example includes them)
target_include_directories(bufr_example
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)

# Link dependencies via imported targets
target_link_libraries(bufr_example
    PRIVATE
        HDF5::HDF5
        ECCODES::eccodes
)

# Define the plugin path for the tests to use
target_compile_definitions(bufr_example
    PRIVATE
        BUFR_VOL_PLUGIN_PATH="${PROJECT_BINARY_DIR}/src"
)

install(TARGETS bufr_example
    RUNTIME DESTINATION bin
)

# ------------------------------------------------------------
# Copy example data next to the executable
# ------------------------------------------------------------

set(BUFR_SAMPLE "${PROJECT_SOURCE_DIR}/files/temp.bufr")
if (NOT EXISTS "${BUFR_SAMPLE}")
    message(FATAL_ERROR "BUFR sample not found: ${BUFR_SAMPLE}")
endif()

add_custom_command(TARGET bufr_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${BUFR_SAMPLE}"
            "$<TARGET_FILE_DIR:bufr_example>/temp.bufr"
    COMMENT "Copying temp.bufr to bufr_example output directory"
)

# ------------------------------------------------------------
# CTest: run example as a smoke test
# ------------------------------------------------------------

# Environment for VOL plugin loading (if the example uses the VOL)
set(EXAMPLE_TEST_ENV
    "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src/;${PROJECT_BINARY_DIR}/src/Release;"
    "HDF5_VOL_CONNECTOR="
)

add_test(NAME bufr_example_runs COMMAND bufr_example)
set_tests_properties(bufr_example_runs PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:bufr_example>"
    ENVIRONMENT "${EXAMPLE_TEST_ENV}"
)

