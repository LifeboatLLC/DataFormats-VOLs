
# Build the gnuplot demo
add_executable(gnuplot_demo gnuplot_demo.c)
target_include_directories(gnuplot_demo PRIVATE "${PROJECT_SOURCE_DIR}/src" ${HDF5_INCLUDE_DIRS})
target_link_libraries(gnuplot_demo PRIVATE HDF5::HDF5 Tiff::Tiff GeoTiff::GeoTiff m)

# Build the iterate and read demo
add_executable(iterate_and_read_demo iterate_and_read_demo.c)
target_include_directories(iterate_and_read_demo PRIVATE "${PROJECT_SOURCE_DIR}/src" ${HDF5_INCLUDE_DIRS})
target_link_libraries(iterate_and_read_demo PRIVATE HDF5::HDF5 Tiff::Tiff GeoTiff::GeoTiff m)

# Build the metadata demo
add_executable(metadata_demo metadata_demo.c)
target_include_directories(metadata_demo PRIVATE "${PROJECT_SOURCE_DIR}/src" ${HDF5_INCLUDE_DIRS})
target_link_libraries(metadata_demo PRIVATE HDF5::HDF5 Tiff::Tiff GeoTiff::GeoTiff m)


# Build the coordinates demo
add_executable(coordinates_demo coordinates_demo.c)
target_include_directories(coordinates_demo PRIVATE "${PROJECT_SOURCE_DIR}/src" ${HDF5_INCLUDE_DIRS})
target_link_libraries(coordinates_demo PRIVATE HDF5::HDF5 Tiff::Tiff GeoTiff::GeoTiff m)



# Install the example programs
install(TARGETS gnuplot_demo iterate_and_read_demo metadata_demo
                coordinates_demo
        RUNTIME DESTINATION bin)
