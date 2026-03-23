# tools/CMakeLists.txt

# Build generator executable
add_executable(generate_example2_cdf generate_example2_cdf.c)
target_include_directories(generate_example2_cdf PRIVATE ${CDF_INCLUDE_DIR})
target_link_libraries(generate_example2_cdf PRIVATE CDF::cdf)