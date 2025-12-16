# Set the GeoTIFF VOL connector name
set (GEOTIFF_VOL_NAME geotiff_vol_connector)

# Build the GeoTIFF VOL connector
add_library (${GEOTIFF_VOL_NAME} SHARED geotiff_vol_connector.c)
set_target_properties (${GEOTIFF_VOL_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties (${GEOTIFF_VOL_NAME} PROPERTIES SOVERSION 1)
set_target_properties (${GEOTIFF_VOL_NAME} PROPERTIES PUBLIC_HEADER "geotiff_vol_connector.h;geotiff_vol_err.h")

# Always add HDF5 include directories explicitly
target_include_directories(${GEOTIFF_VOL_NAME} PRIVATE ${HDF5_INCLUDE_DIRS})

# Link libraries
if (_have_config)
    target_link_libraries(${GEOTIFF_VOL_NAME} PRIVATE HDF5::HDF5 ${_TIFF_TARGET} ${_GEOTIFF_TARGET} )
else()
    target_link_libraries(${GEOTIFF_VOL_NAME} PRIVATE HDF5::HDF5 ${TIFF_LIBRARIES} ${GEOTIFF_LIBRARIES})
    target_include_directories(${GEOTIFF_VOL_NAME} PRIVATE ${TIFF_INCLUDE_DIRS} ${GEOTIFF_INCLUDE_DIRS})
    target_compile_options(${GEOTIFF_VOL_NAME} PRIVATE ${TIFF_CFLAGS_OTHER} ${GEOTIFF_CFLAGS_OTHER})
endif()

# Install rules
install(TARGETS ${GEOTIFF_VOL_NAME}
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
