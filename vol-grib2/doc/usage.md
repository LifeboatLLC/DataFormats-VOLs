## Dependencies

The GRIB2 VOL connector has the following dependencies:

- **HDF5 develop branch (1.14+/2.x)** with VOL support. If building from source, make sure the CPP library is disabled, as attempts to open it during VOL registration can break parts of the test runner.
- **ecCodes** (ecCodes library https://confluence.ecmwf.int/display/ECC/Documentation)
- **CMake 3.9 or later**

### Installing Dependencies

On Ubuntu/Debian (using HDF5 develop built from source):
```bash
sudo apt-get install -y build-essential cmake libtiff5-dev libgeotiff-dev
git clone --depth 1 --branch develop https://github.com/HDFGroup/hdf5.git
cd hdf5 && mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DHDF5_BUILD_TOOLS=ON -DHDF5_BUILD_TESTS=OFF -DCMAKE_INSTALL_PREFIX=/opt/hdf5-develop
make -j$(nproc) && sudo make install
```

On macOS with Homebrew (HDF5 develop built from source is required):

```bash
brew install cmake eccodes
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

   For custom-built ecCodes, add their paths:
   ```bash
   cmake .. -DCMAKE_PREFIX_PATH="/opt/hdf5/;/path/to/eccodes;"
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

- **`GRIB2_VOL_BUILD_EXAMPLES`** (default: `ON`): Build example programs

  To disable building examples:
  ```bash
  cmake .. -DGRIB2_VOL_BUILD_EXAMPLES=OFF
  ```

**Note:** This project requires HDF5 develop (1.14+/2.x). Ensure your `CMAKE_PREFIX_PATH` points to the HDF5 develop installation. On macOS with Homebrew, also include `$(brew --prefix)` in CMAKE_PREFIX_PATH to enable pkg-config discovery of system packages.

### Example Program

### Environment Setup

Set the HDF5 plugin path to include the built connector:
```bash
export HDF5_PLUGIN_PATH=/path/to/DataFormats-VOLS/build/src
```

### Programming Interface

```c


```
