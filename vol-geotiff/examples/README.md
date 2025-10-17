# GeoTIFF VOL Connector Examples

This directory contains examples demonstrating how to use the GeoTIFF VOL connector with Python's h5py and xarray libraries for visualization.

## Overview

The demo workflow:
1. Creates a visually distinctive 256×256 RGB GeoTIFF image with colorful concentric rings
2. Configures environment variables to enable the HDF5 VOL connector
3. Uses h5py to read the GeoTIFF file through the VOL connector (as if it were an HDF5 file)
4. Loads the data into an xarray DataArray
5. Visualizes the result with matplotlib
6. Saves and displays the visualization

## Files

- **`visualize_demo.py`**: Python script that reads the GeoTIFF via h5py and visualizes it
- **`run_demo.sh`**: Bash script that orchestrates the entire workflow (recommended)
- **`CMakeLists.txt`**: Build configuration for the demo programs

## Quick Start

### Automated Demo (Recommended)

Simply run the demo script:

```bash
cd examples
./run_demo.sh
```

The script will:
- Check that the project is built
- Build the image creator if needed
- Generate the demo GeoTIFF
- Check for Python dependencies (numpy, h5py, xarray, matplotlib)
- Install missing dependencies using pip
- Set up environment variables
- Run the visualization
- Display the result and save to `demo_visualization.png`

### Manual Execution

If you prefer to run each step manually:

```bash
# 1. Build the demo image creator
cd build
make create_demo_image

# 2. Create the demo image
cd examples
../build/examples/create_demo_image

# 3. Set environment variables
export HDF5_PLUGIN_PATH=/path/to/vol-geotiff/build/src
export HDF5_VOL_CONNECTOR=geotiff_vol_connector

# 4. Run the Python visualization
python3 visualize_demo.py demo_rings.tif
```

## Requirements

### C Dependencies (for building)
- GCC or Clang compiler
- CMake 3.9+
- libtiff
- libgeotiff
- HDF5 develop branch (1.15+)

### Python Dependencies (auto-installed by run_demo.sh)
- Python 3.6+
- numpy
- h5py (built against HDF5 develop branch)
- xarray
- matplotlib

## Output

The demo creates two files:

1. **`demo_rings.tif`**: 256×256 RGB GeoTIFF with colorful concentric rings
   - Format: RGB (3 channels, 8-bit per channel)
   - Pattern: Rainbow-colored concentric rings from center
   - Geographic metadata: WGS84, California area coordinates
   - Pixel scale: 0.01 degrees/pixel

2. **`demo_visualization.png`**: Matplotlib visualization showing:
   - Full RGB image
   - Red channel intensity map
