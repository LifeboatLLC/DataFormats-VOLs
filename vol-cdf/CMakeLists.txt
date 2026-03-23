cmake_minimum_required (VERSION 3.9)
project (cdf_vol VERSION 1.0.0 DESCRIPTION "HDF5 VOL connector for CDF files" LANGUAGES C)

# Enable testing
enable_testing ()
include (CTest)

# --------------------------
# Find HDF5 (prefer CONFIG mode). Try lowercase first (HDF5 develop installs 'hdf5'), then uppercase
# --------------------------
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

# --------------------------
# Find CDF library
# --------------------------
set(_CDF_FOUND FALSE)

find_library(CDF_LIBRARY NAMES cdf HINTS ${CMAKE_PREFIX_PATH} PATH_SUFFIXES lib)
find_path(CDF_INCLUDE_DIR NAMES cdf.h HINTS ${CMAKE_PREFIX_PATH} PATH_SUFFIXES include)

if (CDF_INCLUDE_DIR AND CDF_LIBRARY)
    set(_CDF_FOUND TRUE)

    if (NOT TARGET CDF::cdf)
        add_library(CDF::cdf UNKNOWN IMPORTED)
        set_target_properties(CDF::cdf PROPERTIES
            IMPORTED_LOCATION "${CDF_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${CDF_INCLUDE_DIR}"
        )
    endif()
endif()

if (NOT _CDF_FOUND)
    message(FATAL_ERROR "CDF library or headers not found. Set CMAKE_PREFIX_PATH if needed.")
else()
    message(STATUS "Found CDF library at: ${CDF_LIBRARY}")
    message(STATUS "Found CDF header at: ${CDF_INCLUDE_DIR}")
endif()


# --------------------------
# Add subdirectories
# --------------------------
add_subdirectory(src)
add_subdirectory(tools)
add_subdirectory(test)

option (CDF_VOL_BUILD_EXAMPLES "Build example programs" ON)

if (CDF_VOL_BUILD_EXAMPLES)
    add_subdirectory (examples)
endif()
