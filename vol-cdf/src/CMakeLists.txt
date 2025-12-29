# Set the CDF VOL connector name
set (CDF_VOL_NAME cdf_vol_connector)

# Build the CDF VOL connector
add_library (${CDF_VOL_NAME} SHARED cdf_vol_connector.c)
set_target_properties (${CDF_VOL_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties (${CDF_VOL_NAME} PROPERTIES SOVERSION 1)
set_target_properties (${CDF_VOL_NAME} PROPERTIES PUBLIC_HEADER "cdf_vol_connector.h;cdf_vol_err.h")

# Add HDF5 and CDF include directories explicitly
target_include_directories(${CDF_VOL_NAME} PRIVATE ${HDF5_INCLUDE_DIRS} ${CDF_INCLUDE_DIR})

# Link HDF5 and CDF libraries
target_link_libraries(${CDF_VOL_NAME} PRIVATE HDF5::HDF5 cdf::cdf)

# Install rules
install(TARGETS ${CDF_VOL_NAME}
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
