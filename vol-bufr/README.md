# HDF5 VOL Connector for BUFR

This directory contains the HDF5 Virtual Object Layer (VOL) connector for reading BUFR files through the HDF5 interface.

## Features

- Read BUFR message metadata and selected data through the HDF5 API
- BUFR messages mapped as HDF5 groups with datasets and attributes
- Attribute iteration and dataset access via standard HDF5 calls

## Dependencies

- HDF5 2.0.0+ (or develop branch with VOL support)
- ecCodes library (https://confluence.ecmwf.int/display/ECC/ecCodes+Home)
- CMake 3.10+

## Quick Start

```bash
cd vol-bufr
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH="/path/to/hdf5;/path/to/eccodes"
make -j$(nproc)
make test
```

See `doc/usage.md` for full dependency installation and build instructions.