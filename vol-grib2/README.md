# HDF5 VOL Connector for GRIB2

This directory contains the HDF5 Virtual Object Layer (VOL) connector for reading GRIB2 files through the HDF5 interface.

## Features

- Read GRIB2 messages through the HDF5 API via ecCodes key-based access
- GRIB2 messages mapped as HDF5 groups with lat/lon/values datasets
- ecCodes keys exposed as HDF5 attributes

## Dependencies

- HDF5 2.0.0+ (or develop branch with VOL support)
- ecCodes library (https://confluence.ecmwf.int/display/ECC/ecCodes+Home)
- CMake 3.9+

## Quick Start

```bash
cd vol-grib2
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH="/path/to/hdf5;/path/to/eccodes"
make -j$(nproc)
make test
```

See `doc/usage.md` for full dependency installation and build instructions.