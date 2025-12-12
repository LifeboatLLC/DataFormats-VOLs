# Set the GeoTIFF VOL connector name
set (GEOTIFF_VOL_NAME geotiff_vol_connector)

# Note: TIFF and GeoTIFF are now found in the top-level CMakeLists.txt
# Prefer vcpkg CONFIG packages for TIFF and libgeotiff; fall back to pkg-config/manual
set(_have_config FALSE)

# Figure out which TIFF target name exists
set(_TIFF_TARGET "")
if (TARGET TIFF::TIFF)
    set(_TIFF_TARGET TIFF::TIFF)
elseif (TARGET TIFF::tiff)
    set(_TIFF_TARGET TIFF::tiff)
endif()

# Figure out which GeoTIFF target name exists
set(_GEOTIFF_TARGET "")
if (TARGET geotiff::geotiff)
    set(_GEOTIFF_TARGET geotiff::geotiff)
    message(STATUS "Found GeoTIFF target: geotiff::geotiff")
elseif (TARGET GeoTIFF::geotiff)
    set(_GEOTIFF_TARGET GeoTIFF::geotiff)
    message(STATUS "Found GeoTIFF target: GeoTIFF::geotiff")
elseif (TARGET GeoTIFF::GeoTIFF)
    set(_GEOTIFF_TARGET GeoTIFF::GeoTIFF)
    message(STATUS "Found GeoTIFF target: GeoTIFF::GeoTIFF")
elseif (TARGET geotiff_library)
    set(_GEOTIFF_TARGET geotiff_library)
    message(STATUS "Found GeoTIFF target: geotiff_library")
else()
    message(STATUS "No GeoTIFF CMake target found")
endif()

if (_TIFF_TARGET AND _GEOTIFF_TARGET)
    set(_have_config TRUE)
    message(STATUS "Using CMake config mode for TIFF and GeoTIFF")
    # Extract include directories from the imported GeoTIFF target
    get_target_property(GEOTIFF_INCLUDE_DIRS ${_GEOTIFF_TARGET} INTERFACE_INCLUDE_DIRECTORIES)

    # Check if headers are in a 'geotiff' subdirectory and adjust include path
    set(_GEOTIFF_ADJUSTED_INCLUDES "")
    foreach(_dir ${GEOTIFF_INCLUDE_DIRS})
        if(EXISTS "${_dir}/geotiff/geotiff.h")
            # Headers are in geotiff/ subdirectory, add that specific path
            list(APPEND _GEOTIFF_ADJUSTED_INCLUDES "${_dir}/geotiff")
            message(STATUS "GeoTIFF headers found in subdirectory: ${_dir}/geotiff")
        elseif(EXISTS "${_dir}/geotiff.h")
            # Headers are directly in include dir
            list(APPEND _GEOTIFF_ADJUSTED_INCLUDES "${_dir}")
            message(STATUS "GeoTIFF headers found directly in: ${_dir}")
        else()
            # Fallback: just add the dir
            list(APPEND _GEOTIFF_ADJUSTED_INCLUDES "${_dir}")
        endif()
    endforeach()
    set(GEOTIFF_INCLUDE_DIRS "${_GEOTIFF_ADJUSTED_INCLUDES}")

    # Propagate to parent scope so tests can include headers
    set(GEOTIFF_INCLUDE_DIRS "${GEOTIFF_INCLUDE_DIRS}" PARENT_SCOPE)
else()
    message(STATUS "_TIFF_TARGET=${_TIFF_TARGET}, _GEOTIFF_TARGET=${_GEOTIFF_TARGET}")
    message(STATUS "Falling back to pkg-config/manual discovery")
endif()

if (NOT _have_config)
    message(STATUS "Falling back to alternate discovery for TIFF/geotiff (vcpkg HINTS, pkg-config last)")
    # Try to find TIFF without pkg-config to support Windows easily
    if (NOT TARGET TIFF::TIFF)
        set(_TIFF_INC_HINT "")
        set(_TIFF_LIB_HINT "")
        if (DEFINED VCPKG_ROOT AND DEFINED VCPKG_TARGET_TRIPLET)
            set(_TIFF_INC_HINT "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include")
            set(_TIFF_LIB_HINT "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/lib")
        endif()
        find_path(TIFF_INCLUDE_DIR tiffio.h HINTS ${_TIFF_INC_HINT})
        find_library(TIFF_LIBRARY NAMES tiff HINTS ${_TIFF_LIB_HINT})
        if (TIFF_INCLUDE_DIR AND TIFF_LIBRARY)
            set(TIFF_INCLUDE_DIRS ${TIFF_INCLUDE_DIR})
            set(TIFF_LIBRARIES ${TIFF_LIBRARY})
        else()
            # Last resort: pkg-config
            find_package(PkgConfig REQUIRED)
            pkg_check_modules(TIFF REQUIRED libtiff-4)
        endif()
    endif()

    # Try to locate GeoTIFF headers/libs with vcpkg hints if available
    set(_VCPKG_INC_HINT "")
    set(_VCPKG_LIB_HINT "")
    if (DEFINED VCPKG_ROOT AND DEFINED VCPKG_TARGET_TRIPLET)
        set(_VCPKG_INC_HINT "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/include")
        set(_VCPKG_LIB_HINT "${VCPKG_ROOT}/installed/${VCPKG_TARGET_TRIPLET}/lib")
    endif()
    if (EXISTS "${_VCPKG_INC_HINT}/geotiff/geotiff.h")
        set(GEOTIFF_INCLUDE_DIR "${_VCPKG_INC_HINT}")
    endif()

    find_path(GEOTIFF_INCLUDE_DIR geotiff/geotiff.h
              HINTS ${_VCPKG_INC_HINT}
              PATHS /usr/include)
    find_library(GEOTIFF_LIBRARY NAMES geotiff
                 HINTS ${_VCPKG_LIB_HINT}
                 PATHS /usr/lib /usr/lib/x86_64-linux-gnu)

    if(NOT GEOTIFF_INCLUDE_DIR OR NOT GEOTIFF_LIBRARY)
        message(FATAL_ERROR "libgeotiff not found. Ensure GeoTIFF is installed (e.g., via vcpkg) or provide headers/libs.")
    endif()

    set(GEOTIFF_LIBRARIES ${GEOTIFF_LIBRARY})
    set(GEOTIFF_INCLUDE_DIRS ${GEOTIFF_INCLUDE_DIR} "/usr/include/geotiff")
    # Propagate to parent scope so tests can include headers
    set(GEOTIFF_INCLUDE_DIRS "${GEOTIFF_INCLUDE_DIRS}" PARENT_SCOPE)
    set(GEOTIFF_LIBRARIES "${GEOTIFF_LIBRARIES}" PARENT_SCOPE)
    set(TIFF_INCLUDE_DIRS "${TIFF_INCLUDE_DIRS}" PARENT_SCOPE)
    set(TIFF_LIBRARIES "${TIFF_LIBRARIES}" PARENT_SCOPE)
    set(TIFF_CFLAGS_OTHER "${TIFF_CFLAGS_OTHER}" PARENT_SCOPE)
    set(GEOTIFF_CFLAGS_OTHER "${GEOTIFF_CFLAGS_OTHER}" PARENT_SCOPE)
    message(STATUS "Using GeoTIFF include dir: ${GEOTIFF_INCLUDE_DIRS}")
    message(STATUS "Using GeoTIFF library: ${GEOTIFF_LIBRARIES}")
endif()

# Propagate _have_config to parent scope for test builds
set(_have_config "${_have_config}" PARENT_SCOPE)
set(_TIFF_TARGET "${_TIFF_TARGET}" PARENT_SCOPE)
set(_GEOTIFF_TARGET "${_GEOTIFF_TARGET}" PARENT_SCOPE)

# Build the GeoTIFF VOL connector
add_library (${GEOTIFF_VOL_NAME} SHARED geotiff_vol_connector.c)
set_target_properties (${GEOTIFF_VOL_NAME} PROPERTIES VERSION ${PROJECT_VERSION})
set_target_properties (${GEOTIFF_VOL_NAME} PROPERTIES SOVERSION 1)
set_target_properties (${GEOTIFF_VOL_NAME} PROPERTIES PUBLIC_HEADER "geotiff_vol_connector.h;geotiff_vol_err.h")

# Always add HDF5 include directories explicitly
target_include_directories(${GEOTIFF_VOL_NAME} PRIVATE ${HDF5_INCLUDE_DIRS})

# Link libraries
if (_have_config)
    target_link_libraries(${GEOTIFF_VOL_NAME} PRIVATE ${_TIFF_TARGET} ${_GEOTIFF_TARGET} HDF5::HDF5)
else()
    target_link_libraries(${GEOTIFF_VOL_NAME} PRIVATE ${TIFF_LIBRARIES} ${GEOTIFF_LIBRARIES} HDF5::HDF5)
    target_include_directories(${GEOTIFF_VOL_NAME} PRIVATE ${TIFF_INCLUDE_DIRS} ${GEOTIFF_INCLUDE_DIRS})
    target_compile_options(${GEOTIFF_VOL_NAME} PRIVATE ${TIFF_CFLAGS_OTHER} ${GEOTIFF_CFLAGS_OTHER})
endif()

# Install rules
install(TARGETS ${GEOTIFF_VOL_NAME}
        LIBRARY DESTINATION lib
        PUBLIC_HEADER DESTINATION include)
