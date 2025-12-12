cmake_minimum_required (VERSION 3.9)
project (geotiff_vol VERSION 1.0.0 DESCRIPTION "HDF5 VOL connector for GeoTIFF files" LANGUAGES C)

# Enable testing
enable_testing ()
include (CTest)

# Find HDF5 (prefer CONFIG mode). Try lowercase first (HDF5 develop installs 'hdf5'), then uppercase
find_package(hdf5 COMPONENTS C CONFIG QUIET)
find_package(HDF5 COMPONENTS C CONFIG QUIET)
if(NOT hdf5_FOUND AND NOT HDF5_FOUND)
    message(FATAL_ERROR "HDF5 CONFIG package not found. Set CMAKE_PREFIX_PATH or HDF5_DIR to your HDF5 install.")
endif()

# Normalize HDF5 target name so downstream can always link HDF5::HDF5
if(NOT TARGET HDF5::HDF5)
    foreach(hdf5_candidate IN ITEMS hdf5::hdf5 hdf5::hdf5-shared HDF5::HDF5_C hdf5-shared hdf5-static hdf5)
        if(TARGET ${hdf5_candidate})
            add_library(HDF5::HDF5 ALIAS ${hdf5_candidate})
            break()
        endif()
    endforeach()
    # Extract HDF5 include directories for use by targets/tests
    if(TARGET HDF5::HDF5)
        get_target_property(HDF5_INCLUDE_DIRS HDF5::HDF5 INTERFACE_INCLUDE_DIRECTORIES)
    endif()
endif()

# Find TIFF - try config first, then allow CMake's FindTIFF.cmake module
find_package(TIFF CONFIG QUIET)
if(NOT TIFF_FOUND)
    find_package(TIFF QUIET)  # Let CMake use FindTIFF module (works with Homebrew)
endif()

# Find GeoTIFF - only try config mode (no standard Find module for GeoTIFF)
find_package(GeoTIFF CONFIG QUIET)
if(NOT GeoTIFF_FOUND)
    find_package(geotiff CONFIG QUIET)
endif()

# Determine which TIFF target is available
set(_TIFF_TARGET "")
foreach(tiff_candidate IN ITEMS TIFF::TIFF TIFF::tiff)
    if(TARGET ${tiff_candidate})
        set(_TIFF_TARGET ${tiff_candidate})
        break()
    endif()
endforeach()

# Determine which GeoTIFF target is available
set(_GEOTIFF_TARGET "")
foreach(geotiff_candidate IN ITEMS geotiff::geotiff GeoTIFF::geotiff GeoTIFF::GeoTIFF geotiff_library)
    if(TARGET ${geotiff_candidate})
        set(_GEOTIFF_TARGET ${geotiff_candidate})
        message(STATUS "Found GeoTIFF target: ${_GEOTIFF_TARGET}")
        break()
    endif()
endforeach()
if(NOT _GEOTIFF_TARGET)
    message(STATUS "No GeoTIFF CMake target found")
endif()

if(_TIFF_TARGET AND _GEOTIFF_TARGET)
    set(_have_config TRUE)
    message(STATUS "Using CMake config mode for TIFF and GeoTIFF")
    # Extract and adjust include directories from the imported GeoTIFF target
    get_target_property(GEOTIFF_INCLUDE_DIRS ${_GEOTIFF_TARGET} INTERFACE_INCLUDE_DIRECTORIES)
    set(_GEOTIFF_ADJUSTED_INCLUDES "")
    foreach(_dir ${GEOTIFF_INCLUDE_DIRS})
        if(EXISTS "${_dir}/geotiff/geotiff.h")
            list(APPEND _GEOTIFF_ADJUSTED_INCLUDES "${_dir}/geotiff")
        else()
            list(APPEND _GEOTIFF_ADJUSTED_INCLUDES "${_dir}")
        endif()
    endforeach()
    set(GEOTIFF_INCLUDE_DIRS "${_GEOTIFF_ADJUSTED_INCLUDES}")
else()
    set(_have_config FALSE)
    # Try pkg-config for packages not yet found
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        if(NOT _GEOTIFF_TARGET)
            message(STATUS "Attempting pkg-config discovery of libgeotiff")
            pkg_check_modules(GEOTIFF QUIET IMPORTED_TARGET libgeotiff)
            if(GEOTIFF_FOUND)
                set(_GEOTIFF_TARGET "PkgConfig::GEOTIFF")
            endif()
        endif()

        if(NOT _TIFF_TARGET)
            message(STATUS "Attempting pkg-config discovery of libtiff")
            pkg_check_modules(TIFF QUIET IMPORTED_TARGET libtiff-4)
            if(TIFF_FOUND)
                set(_TIFF_TARGET "PkgConfig::TIFF")
            endif()
        endif()
    endif()

    if(_GEOTIFF_TARGET AND _TIFF_TARGET)
        set(_have_config TRUE)
        message(STATUS "Using TIFF and GeoTIFF targets: ${_TIFF_TARGET}, ${_GEOTIFF_TARGET}")
    else()
        # Manual discovery for packages in standard system locations without config files
        message(STATUS "Attempting manual discovery of libtiff/libgeotiff")
        find_path(TIFF_INCLUDE_DIR tiffio.h)
        find_library(TIFF_LIBRARY NAMES tiff)
        find_path(GEOTIFF_INCLUDE_DIR geotiff/geotiff.h
            PATHS /usr/include /opt/homebrew/include /usr/local/include)
        find_library(GEOTIFF_LIBRARY NAMES geotiff
            PATHS /usr/lib /usr/lib/x86_64-linux-gnu /opt/homebrew/lib /usr/local/lib)

        if(NOT GEOTIFF_INCLUDE_DIR OR NOT GEOTIFF_LIBRARY)
            message(FATAL_ERROR "libgeotiff not found. Ensure GeoTIFF is installed (e.g., via apt/brew) or provide headers/libs.")
        endif()

        set(TIFF_LIBRARIES ${TIFF_LIBRARY})
        set(GEOTIFF_LIBRARIES ${GEOTIFF_LIBRARY})
        set(GEOTIFF_INCLUDE_DIRS ${GEOTIFF_INCLUDE_DIR})
        # Add common geotiff subdirectory to include path if it exists
        if(EXISTS "${GEOTIFF_INCLUDE_DIR}/geotiff")
            list(APPEND GEOTIFF_INCLUDE_DIRS "${GEOTIFF_INCLUDE_DIR}/geotiff")
        endif()
    endif()
endif()

# Build options
option (GEOTIFF_BUILD_EXAMPLES "Build example programs" ON)

add_subdirectory (src)
add_subdirectory (test)

if (GEOTIFF_BUILD_EXAMPLES)
    add_subdirectory (examples)
endif()
