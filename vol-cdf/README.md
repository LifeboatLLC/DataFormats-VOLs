This directory contains source and documentation for the CDF VOL connector for reading CDF file via HDF5 interface.

## Dependencies

The CDF VOL connector has the following dependencies:

- **HDF5 develop branch (1.14+/2.x)** with VOL support
- **CDF library
- **CMake 3.9 or later**

### Installing Dependencies

On Ubuntu/Debian (using HDF5 develop built from source):
```bash
git clone --depth 1 --branch develop https://github.com/HDFGroup/hdf5.git
cd hdf5 && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DHDF5_BUILD_TOOLS=ON -DHDF5_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=/opt/hdf5-develop
make -j$(nproc) && sudo make install
```

On CentOS/RHEL:
```bash
sudo yum install hdf5-devel pkgconfig
```

On macOS with Homebrew (HDF5 develop built from source is required):
```bash
brew install cmake pkg-config
# Build and install HDF5 develop similarly to the Linux instructions above
```

## Building

This project uses CMake as the build system.

### CMake Build
1. Create a build directory:
   ```bash
   mkdir build && cd build
   ```

2. Configure with CMake (point to HDF5 install).  But remember to modify the hard-coded include directory and library path for CDF in the CMakeLists.txt under vol-cdf/src and vol-cdf/test with your installed CDF location:
   ```bash
   cmake .. -DCMAKE_PREFIX_PATH=/opt/hdf5-develop/
   ```

   Or specify HDF5 location explicitly:
   ```bash
   cmake .. -DHDF5_DIR=/opt/hdf5-develop/install
   ```

3. Build the connector:
   ```bash
   make -j$(nproc)
   ```

4. (Optional) Install the connector:
   ```bash
   sudo make install
   ```

5. Run tests.  But first copy the CDF file called example1.cdf from vol-cdf/test to your build directory (build/test):
   ```bash
   make test
   ```

## Usage

### Programming Interface

```c
#include "cdf_vol_connector.h"
#include <hdf5.h>

int main() {
    hid_t vol_id, fapl_id, file_id, dset_id;

    // Tell the library where to find the CDF VOL connector library
    // (May be skipped if HDF5_VOL_CONNECTOR/HDF5_PLUGIN_PATH are defined in env)
    H5PLappend(CDF_VOL_PLUGIN_PATH);

    // Register the GeoTIFF VOL connector
    vol_id = H5VLregister_connector_by_name(CDF_VOL_CONNECTOR_NAME, H5P_DEFAULT);

    // Create file access property list and set VOL connector
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    H5Pset_vol(fapl_id, vol_id, NULL);

    // Open CDF file called example1.cdf without the .cdf extension
    file_id = H5Fopen("example1", H5F_ACC_RDONLY, fapl_id);

    // Cleanup
    H5Fclose(file_id);
    H5Pclose(fapl_id);
    H5VLunregister_connector(vol_id);

    return 0;
}
```
