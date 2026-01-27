cmake_minimum_required (VERSION 3.10)
project (grib2_vol VERSION 1.0.0 DESCRIPTION "HDF5 VOL connector for GRIB2 files" LANGUAGES C)

# Location of sample GRIB2 files
set(GRIB2_VOL_FILES_DIR "${PROJECT_SOURCE_DIR}/files")

set(GRIB2_VOL_TEMP_GRIB2 "${GRIB2_VOL_FILES_DIR}/example-1.grib2")
if (NOT EXISTS "${GRIB2_VOL_TEMP_GRIB2}")
    message(WARNING "Expected GRIB2 sample not found: ${GRIB2_VOL_TEMP_GRIB2}")
endif()


# Enable testing
enable_testing ()
include (CTest)

# ------------------------------------------------------------
# Find HDF5 
# ------------------------------------------------------------

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

# ------------------------------------------------------------
# Find ecCodes and normalize target name to ECCODES::eccodes
# ------------------------------------------------------------

set(_ECCODES_FOUND FALSE)

# 1) Try CONFIG package first
find_package(eccodes CONFIG QUIET)

if (eccodes_FOUND)
    set(_ECCODES_FOUND TRUE)

    # ecCodes exports targets with varying names depending on packaging.
    # Normalize to ECCODES::eccodes for downstream use.
    if (NOT TARGET ECCODES::eccodes)
        if (TARGET eccodes::eccodes)
            add_library(ECCODES::eccodes ALIAS eccodes::eccodes)
        elseif (TARGET eccodes)
            add_library(ECCODES::eccodes ALIAS eccodes)
        endif()
    endif()
endif()

# 2) Fallback: manual search (for installs without a CMake config)
if (NOT _ECCODES_FOUND OR NOT TARGET ECCODES::eccodes)
    # Optional hint variable: ECCODES_ROOT
    if (DEFINED ECCODES_ROOT)
        list(PREPEND CMAKE_PREFIX_PATH "${ECCODES_ROOT}")
    endif()

    find_path(ECCODES_INCLUDE_DIR
        NAMES eccodes.h
        PATH_SUFFIXES include
    )

    find_library(ECCODES_LIBRARY
        NAMES eccodes
        PATH_SUFFIXES lib
    )

    if (ECCODES_INCLUDE_DIR AND ECCODES_LIBRARY)
        set(_ECCODES_FOUND TRUE)

        if (NOT TARGET ECCODES::eccodes)
            add_library(ECCODES::eccodes UNKNOWN IMPORTED)
            set_target_properties(ECCODES::eccodes PROPERTIES
                IMPORTED_LOCATION "${ECCODES_LIBRARY}"
                INTERFACE_INCLUDE_DIRECTORIES "${ECCODES_INCLUDE_DIR}"
            )
        endif()
    endif()
endif()

if (NOT _ECCODES_FOUND OR NOT TARGET ECCODES::eccodes)
    message(FATAL_ERROR
        "ecCodes not found or no usable target exported. "
        "Try setting -DCMAKE_PREFIX_PATH=\"...;/opt/homebrew\" or -DECCODES_ROOT=/path/to/eccodes."
    )
endif()

# Optional: export include dirs as a variable too (safe now)
get_target_property(ECCODES_INCLUDE_DIRS ECCODES::eccodes INTERFACE_INCLUDE_DIRECTORIES)


# Note: This connector targets the modern HDF5 VOL API (1.15+). Ensure your HDF5 matches.

# (moved aliasing section above)

# It's really easy to pick up the wrong HDF5 library if you set the path
# wrong. Turn this on for added confirmation that you got it right.
#message (DEPRECATION "Include: ${HDF5_INCLUDE_DIR}")

# Build options
option (GRIB2_VOL_BUILD_EXAMPLES "Build example programs" ON)

add_subdirectory (src)
add_subdirectory (test)

if (GRIB2_VOL_BUILD_EXAMPLES)
    add_subdirectory (examples)
endif()
