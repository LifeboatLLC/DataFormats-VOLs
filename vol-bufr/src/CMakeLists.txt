# Set the BUFR VOL connector name
set (BUFR_VOL_NAME bufr_vol_connector)

# Build the BUFR VOL connector
add_library (${BUFR_VOL_NAME} SHARED bufr_vol_connector.c)
set_target_properties (${BUFR_VOL_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties (${BUFR_VOL_NAME} PROPERTIES SOVERSION 1)
set_target_properties (${BUFR_VOL_NAME} PROPERTIES PUBLIC_HEADER "bufr_vol_connector.h;bufr_vol_err.h")

# HDF5
target_link_libraries(${BUFR_VOL_NAME}
    PRIVATE
        HDF5::HDF5
)

# ecCodes
target_link_libraries(${BUFR_VOL_NAME}
    PRIVATE
        ECCODES::eccodes
)


# Install rules
install(TARGETS ${BUFR_VOL_NAME}
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
