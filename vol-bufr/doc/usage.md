## Dependencies

The BUFR VOL connector has the following dependencies:

- **HDF5 develop branch (1.14+/2.x)** with VOL support. If building from source, make sure the CPP library is disabled, as attempts to open it during VOL registration can break parts of the test runner.
- **ecCodes library (https://confluence.ecmwf.int/display/ECC/ecCodes+Home)
- **CMake 3.10 or later**

### Installing Dependencies

On Ubuntu/Debian (using HDF5 develop built from source):
```bash
sudo apt install eccodes 
git clone --depth 1 --branch develop https://github.com/HDFGroup/hdf5.git
cd hdf5 && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DHDF5_BUILD_TOOLS=ON -DHDF5_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=/opt/hdf5-develop
make -j$(nproc) && sudo make install
```

On macOS with Homebrew (HDF5 develop built from source is required):

```bash
brew install eccodes
# Build and install HDF5 develop similarly to the Linux instructions above
```

## Building

This project uses CMake as the build system.

### CMake Build
1. Create a build directory:
   ```bash
   mkdir build && cd build
   ```

2. Configure with CMake (point to HDF5 install):
   ```bash
   cmake .. -DCMAKE_PREFIX_PATH=/opt/hdf5/
   ```

   On macOS with Homebrew, include the Homebrew prefix so pkg-config can find system packages:
   ```bash
   cmake .. -DCMAKE_PREFIX_PATH="/opt/hdf5/;$(brew --prefix)"
   ```

   For custom-built eccodes, add their paths:
   ```bash
   cmake .. -DCMAKE_PREFIX_PATH="/opt/hdf5/;/path/to/eccodes"
   ```

3. Build the connector:
   ```bash
   make -j$(nproc)
   ```

4. (Optional) Install the connector:
   ```bash
   sudo make install
   ```

5. Run tests:
   ```bash
   make test
   ```

### CMake Configuration Options

- **`BUFR_VOL_BUILD_EXAMPLES`** (default: `ON`): Build example programs

  To disable building examples:
  ```bash
  cmake .. -DBUFR_VOL_BUILD_EXAMPLES=OFF
  ```

**Note:** This project requires HDF5 develop (1.14+/2.x). Ensure your `CMAKE_PREFIX_PATH` points to the HDF5 develop installation. On macOS with Homebrew, also include `$(brew --prefix)` in CMAKE_PREFIX_PATH to enable pkg-config discovery of system packages.

### Example Programs

The example programs demonstrate how to use the BUFR VOL connector. They are built and run by default but can be disabled with `-DBUFR_VOL_BUILD_EXAMPLES=OFF`.

### Programming Interface

```c
#include "bufr_vol_connector.h"
#include <hdf5.h>

int main() {
    hid_t vol_id, fapl_id, file_id, dset_id;

    // Tell the library where to find the GeoTIFF VOL connector library
    // (May be skipped if HDF5_VOL_CONNECTOR/HDF5_PLUGIN_PATH are defined in env)
    // Define the path and uncomment the line if not using environment variable.
    // H5PLappend(GEOTIFF_VOL_PLUGIN_PATH);

    // Register the GeoTIFF VOL connector
    vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT);

    // Create file access property list and set VOL connector
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    H5Pset_vol(fapl_id, vol_id, NULL);

    // Open GeoTIFF file
    file_id = H5Fopen("temp.bufr", H5F_ACC_RDONLY, fapl_id);

    // Open latitude variable from the first message
    dset_id = H5Dopen2(file_id, "/1/latitude", H5P_DEFAULT);

    // Read data...

    // Cleanup
    H5Dclose(dset_id);
    H5Fclose(file_id);
    H5Pclose(fapl_id);
    H5VLunregister_connector(vol_id);

    return 0;
}
```
