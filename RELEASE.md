## Summary
Initial public release of HDF5 VOL connectors enabling unified access to BUFR, GRIB2, CDF, and GeoTIFF through standard HDF5 and netCDF-4 APIs on Linux and macOS platforms.

---

## Overview
This release provides the first public version of the DataFormats VOL Connectors project. The software implements a set of HDF5 Virtual Object Layer (VOL) connectors that expose multiple scientific data formats through standard HDF5 APIs.

By mapping format-specific data structures to the HDF5 object model, the connectors enable applications to discover and read heterogeneous scientific data without requiring format-specific code. This approach supports interoperability and integration with existing HDF5- and netCDF-based workflows.

---

## Supported Formats

| Format   | Notes |
|----------|------|
| BUFR     | Metadata + selected data access |
| GRIB2    | Key-based access |
| CDF      | Variables and attributes |
| GeoTIFF  | Image data + metadata |

---

## Build Instructions
See `doc/usage.md` file in each `vol-<connector>` directory.

---

## Documentation and example programs
Documents describing mappings between data stored in the binary formats and HDF5, and connector implementations can be found in the `doc` subdirectories of each `vol-<connector>` folder.
Examples can be found in the `examples` subdirectory of each `vol-<connector>` directory.

---

## Acknowledgment
This work contributes to ongoing efforts to improve interoperability of scientific data through HDF5.
