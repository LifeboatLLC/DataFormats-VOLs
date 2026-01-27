# Set the GRIB2 VOL connector name
set (GRIB2_VOL_NAME grib2_vol_connector)

# Build the GRIB2 VOL connector
add_library (${GRIB2_VOL_NAME} SHARED grib2_vol_connector.c)
set_target_properties (${GRIB2_VOL_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties (${GRIB2_VOL_NAME} PROPERTIES SOVERSION 1)
set_target_properties (${GRIB2_VOL_NAME} PROPERTIES PUBLIC_HEADER "grib2_vol_connector.h;grib2_vol_err.h")

# HDF5
target_link_libraries(${GRIB2_VOL_NAME}
    PRIVATE
        HDF5::HDF5
)

# ecCodes
target_link_libraries(${GRIB2_VOL_NAME}
    PRIVATE
        ECCODES::eccodes
)


# Install rules
install(TARGETS ${GRIB2_VOL_NAME}
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
