# ------------------------------------------------------------
# Examples
# ------------------------------------------------------------

add_executable(grib2_example grib2_example.c)

# Include VOL connector headers (if the example includes them)
target_include_directories(grib2_example
    PRIVATE
        "${PROJECT_SOURCE_DIR}/src"
)

# Link dependencies via imported targets
target_link_libraries(grib2_example
    PRIVATE
        HDF5::HDF5
        ECCODES::eccodes
)

# Define the plugin path for the tests to use
target_compile_definitions(grib2_example
    PRIVATE
        GRIB2_VOL_PLUGIN_PATH="${PROJECT_BINARY_DIR}/src"
)

install(TARGETS grib2_example
    RUNTIME DESTINATION bin
)

# ------------------------------------------------------------
# Copy example data next to the executable
# ------------------------------------------------------------

set(GRIB2_SAMPLE "${PROJECT_SOURCE_DIR}/files/example-1.grib2")
if (NOT EXISTS "${GRIB2_SAMPLE}")
    message(FATAL_ERROR "GRIB2 sample not found: ${GRIB2_SAMPLE}")
endif()

add_custom_command(TARGET grib2_example POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${GRIB2_SAMPLE}"
            "$<TARGET_FILE_DIR:grib2_example>/example-1.grib2"
    COMMENT "Copying example-1.grib2 to grib2_example output directory"
)

# ------------------------------------------------------------
# CTest: run example as a smoke test
# ------------------------------------------------------------

# Environment for VOL plugin loading (if the example uses the VOL)
set(EXAMPLE_TEST_ENV
    "HDF5_PLUGIN_PATH=${PROJECT_BINARY_DIR}/src/;${PROJECT_BINARY_DIR}/src/Release;"
    "HDF5_VOL_CONNECTOR="
)

add_test(NAME grib2_example_runs COMMAND grib2_example)
set_tests_properties(grib2_example_runs PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:grib2_example>"
    ENVIRONMENT "${EXAMPLE_TEST_ENV}"
)

