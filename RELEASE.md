## Summary
Initial public release of HDF5 VOL connectors enabling unified access to BUFR, GRIB2, CDF, and GeoTIFF through standard HDF5 and netCDF-4 APIs on Linux and macOS platforms.

---

## Overview
This release provides the first public version of the DataFormats VOL Connectors project. The software implements a set of HDF5 Virtual Object Layer (VOL) connectors that expose multiple scientific data formats through standard HDF5 APIs.

By mapping format-specific data structures to the HDF5 object model, the connectors enable applications to discover and read heterogeneous scientific data without requiring format-specific code. This approach supports interoperability and integration with existing HDF5- and netCDF-based workflows.

---

## Supported Formats

| Format  | Description                       | External Libraries|
| ------- | --------------------------------- |----------------|
| BUFR    | Meteorological observational data | [ECMWF, ecCodes Home](https://confluence.ecmwf.int/display/ECC/ecCodes+Home) |
| GRIB2   | Gridded meteorological data       |[ECMWF, ecCodes Home](https://confluence.ecmwf.int/display/ECC/ecCodes+Home) |
| CDF     | Space science data                | [NASA Common Data Format ](https://cdf.gsfc.nasa.gov/)|
| GeoTIFF | Geospatial raster imagery         | [GeoTIFF](https://github.com/OSGeo/libgeotiff) and [TIFF libraries](https://github.com/libsdl-org/libtiff) |

---

## Build Instructions
See `doc/usage.md` file in each `vol-<connector>` directory.

---

## Documentation and example programs
Documents describing mappings between data stored in the binary formats and HDF5, and connector implementations can be found in the `doc` subdirectories of each `vol-<connector>` direcory.
Examples can be found in the `examples` subdirectory of each `vol-<connector>` directory.

---

## Testing

This distribution was tested on [Ubuntu-24.04](https://github.com/actions/runner-images/blob/main/images/ubuntu/Ubuntu2404-Readme.md) and [macOS 15 Arm64](https://github.com/actions/runner-images/blob/main/images/macos/macos-15-arm64-Readme.md). More platforms will be avilable in the future releases.

## Acknowledgment
This work contributes to ongoing efforts to improve interoperability of scientific data through HDF5.

## What's Changed
* ci: test gh actions by @hyoklee in https://github.com/LifeboatLLC/DataFormats-VOLs/pull/49
* Resolve issue-32 fix merge conflicts by @mattjala in https://github.com/LifeboatLLC/DataFormats-VOLs/pull/50
* fix: support netcdf and cf conventions by @hyoklee in https://github.com/LifeboatLLC/DataFormats-VOLs/pull/45
* Update release process by @mattjala in https://github.com/LifeboatLLC/DataFormats-VOLs/pull/51
* Correct filepath in release process by @mattjala in https://github.com/LifeboatLLC/DataFormats-VOLs/pull/52

## New Contributors
* @hyoklee made their first contribution in https://github.com/LifeboatLLC/DataFormats-VOLs/pull/49

**Full Changelog**: https://github.com/LifeboatLLC/DataFormats-VOLs/compare/v0.1.0...v1.0.0
