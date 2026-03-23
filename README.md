# HDF5 VOL Connectors to Earth Sciences Binary Data Formats

[![GeoTIFF VOL CI](https://github.com/LifeboatLLC/DataFormats-VOLs/actions/workflows/vol-geotiff-ci.yml/badge.svg?branch=main)](https://github.com/LifeboatLLC/DataFormats-VOLs/actions/workflows/vol-geotiff-ci.yml)
[![CDF VOL CI](https://github.com/LifeboatLLC/DataFormats-VOLs/actions/workflows/vol-cdf-ci.yml/badge.svg?branch=main)](https://github.com/LifeboatLLC/DataFormats-VOLs/actions/workflows/vol-cdf-ci.yml)
[![BUFR VOL CI](https://github.com/LifeboatLLC/DataFormats-VOLs/actions/workflows/vol-bufr-ci.yml/badge.svg?branch=main)](https://github.com/LifeboatLLC/DataFormats-VOLs/actions/workflows/vol-bufr-ci.yml)
[![GRIB2 VOL CI](https://github.com/LifeboatLLC/DataFormats-VOLs/actions/workflows/vol-grib2-ci.yml/badge.svg?branch=main)](https://github.com/LifeboatLLC/DataFormats-VOLs/actions/workflows/vol-grib2-ci.yml)


<!--
[![GitHub release](https://img.shields.io/github/v/release/LifeboatLLC/DataFormats-VOLs)](https://github.com/LifeboatLLC/DataFormats-VOLs/releases)
[![License](https://img.shields.io/github/license/LifeboatLLC/DataFormats-VOLs)](https://github.com/LifeboatLLC/DataFormats-VOLs/blob/main/LICENSE)
-->
---

## Overview

The **HDF5 VOL Connectors** project provides a set of HDF5 Virtual Object Layer (VOL) connectors that enable unified access to multiple scientific data formats through standard HDF5 APIs.

Supported formats include:

* BUFR
* GRIB2
* CDF
* GeoTIFF

By mapping format-specific data structures to the HDF5 object model, this project allows applications to **discover and read heterogeneous data using a single interface**, eliminating the need for format-specific application logic.

---

## Architecture

```mermaid
flowchart TD
    A["Applications (HDF5 / netCDF APIs)"] --> B["HDF5 Library"]
    B --> C["VOL Layer"]

    C --> D1["BUFR VOL"]
    C --> D2["GRIB2 VOL"]
    C --> D3["CDF VOL"]
    C --> D4["GeoTIFF VOL"]

    D1 --> E1["ecCodes"]
    D2 --> E2["ecCodes"]
    D3 --> E3["CDF Library"]
    D4 --> E4["libtiff / GeoTIFF"]
```

---

## Key Features

* Unified access to multiple scientific data formats via HDF5 APIs
* Transparent mapping of heterogeneous data to HDF5 objects
* Cross-platform build system (CMake)
* Integration with external libraries:

  * ecCodes (BUFR, GRIB2)
  * NASA CDF library
  * libtiff / GeoTIFF

---

## Supported Formats

| Format  | Description                       |
| ------- | --------------------------------- |
| BUFR    | Meteorological observational data |
| GRIB2   | Gridded meteorological data       |
| CDF     | Space science data                |
| GeoTIFF | Geospatial raster imagery         |

---

## Quick Start

### Build
Each connector is built separately. See `doc/usage.md` file for building instructions.

### Examples
Each connector comes with an example that shows how to access data via HDF5 APIs. `C` programs can be found in the `examples` directory under each VOL folder. 

---


## Project Status

This project is an **initial release** intended for:

* evaluation
* integration testing
* research workflows

Functionality and mappings will continue to evolve. For more details check documentation in the `doc` subdirectories of each connector repo.

---

## Known Limitations

* Read-only support (no write functionality yet)
* Partial support for complex structures (e.g., BUFR replication)
* Limited selection and datatype conversion support
* Performance optimizations ongoing
* For CDF, BUFR and GRIB2 connector testing was performed only on a limited number of binary files. Testing is ongoing.

---
<!--
## Documentation

See the [Releases](https://github.com/LifeboatLLC/DataFormats-VOLs/releases) page for detailed release notes.

---
-->

## Contributing

Contributions, issues, and feedback are welcome.

---

## License

Connectors are open-source and come with 3 clause BSD license. See COPYING file in each VOL folder.

---

