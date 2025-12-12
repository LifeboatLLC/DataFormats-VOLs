cmake_minimum_required (VERSION 3.9)
project (geotiff_vol VERSION 1.0.0 DESCRIPTION "HDF5 VOL connector for GeoTIFF files" LANGUAGES C)

# Enable testing
enable_testing ()
include (CTest)

# Find HDF5 (prefer CONFIG mode). Try lowercase first (HDF5 develop installs 'hdf5'), then uppercase
set(_HDF5_FOUND FALSE)
find_package (hdf5 COMPONENTS C CONFIG QUIET)
if (hdf5_FOUND)
    set(_HDF5_FOUND TRUE)
else()
    find_package (HDF5 COMPONENTS C CONFIG QUIET)
    if (HDF5_FOUND)
        set(_HDF5_FOUND TRUE)
    endif()
endif()

# Ensure HDF5 target alias exists before extracting include dirs
if (NOT _HDF5_FOUND)
    message(FATAL_ERROR "HDF5 CONFIG package not found. Set CMAKE_PREFIX_PATH or HDF5_DIR to your HDF5 install.")
endif()

# Normalize HDF5 target name so downstream can always link HDF5::HDF5
if (NOT TARGET HDF5::HDF5)
    if (TARGET hdf5::hdf5)
        add_library(HDF5::HDF5 ALIAS hdf5::hdf5)
    elseif (TARGET hdf5::hdf5-shared)
        add_library(HDF5::HDF5 ALIAS hdf5::hdf5-shared)
    elseif (TARGET HDF5::HDF5_C)
        # Some configs expose HDF5_C as the C library target
        add_library(HDF5::HDF5 ALIAS HDF5::HDF5_C)
    elseif (TARGET hdf5-shared)
        add_library(HDF5::HDF5 ALIAS hdf5-shared)
    elseif (TARGET hdf5-static)
        add_library(HDF5::HDF5 ALIAS hdf5-static)
    elseif (TARGET hdf5)
        add_library(HDF5::HDF5 ALIAS hdf5)
    endif()
endif()

# Extract HDF5 include directories for use by targets/tests
if (TARGET HDF5::HDF5)
    get_target_property(HDF5_INCLUDE_DIRS HDF5::HDF5 INTERFACE_INCLUDE_DIRECTORIES)
endif()

# Find TIFF and create alias if needed (before libgeotiff is found)
find_package(TIFF CONFIG QUIET)
if(NOT TIFF_FOUND)
    find_package(Tiff CONFIG QUIET)
endif()

# Normalize TIFF target name  create TIFF::TIFF alias if needed
if (TARGET TIFF::tiff AND NOT TARGET TIFF::TIFF)
    add_library(TIFF::TIFF ALIAS TIFF::tiff)
    message(STATUS "Created TIFF alias: TIFF::TIFF -> TIFF::tiff")
endif()

# Find GeoTIFF (after TIFF alias is created, since it may depend on TIFF::TIFF)
find_package(GeoTIFF CONFIG QUIET)
if (NOT GeoTIFF_FOUND)
    find_package(geotiff CONFIG QUIET)
endif()


# Note: TIFF and GeoTIFF are now found in the top-level CMakeLists.txt
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
        message(FATAL_ERROR "libgeotiff not found. Ensure GeoTIFF is installed (e.g., via apt) or provide headers/libs.")
    endif()

    set(GEOTIFF_LIBRARIES ${GEOTIFF_LIBRARY})
    set(GEOTIFF_INCLUDE_DIRS ${GEOTIFF_INCLUDE_DIR} "/usr/include/geotiff")
endif()

# Build options
option (GEOTIFF_BUILD_EXAMPLES "Build example programs" ON)

add_subdirectory (src)
add_subdirectory (test)

if (GEOTIFF_BUILD_EXAMPLES)
    add_subdirectory (examples)
endif()
