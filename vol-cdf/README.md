# HDF5 VOL Connector for CDF

This directory contains the HDF5 Virtual Object Layer (VOL) connector for reading NASA CDF (Common Data Format) files through the HDF5 interface.

## Features

- Read CDF variables and attributes through the HDF5 API
- CDF rVariables/zVariables mapped as HDF5 datasets
- Global and variable-scoped CDF attributes accessible as HDF5 attributes
- Support for CDF epoch types and multi-entry attributes

## Dependencies

- HDF5 2.0.0+ (or develop branch with VOL support)
- NASA CDF library (https://cdf.gsfc.nasa.gov/)
- CMake 3.9+

## Quick Start

```bash
cd vol-cdf
mkdir build && cd build
cmake .. -DCMAKE_PREFIX_PATH="/path/to/hdf5;/path/to/cdf"
make -j$(nproc)
make test
```

See `doc/usage.md` for full dependency installation and build instructions.