#!/usr/bin/env python3
"""
Test if netCDF4-python can access GeoTIFF files through the GeoTIFF VOL connector.

Pipeline: netCDF4-python -> netCDF-C library -> HDF5 library -> GeoTIFF VOL connector

Prerequisites:
- netCDF4-python installed (pip install netCDF4)
- netCDF-C library compiled with HDF5 support
- GeoTIFF VOL connector compiled and accessible
- Environment variables set:
  * HDF5_VOL_CONNECTOR=geotiff_vol_connector
  * HDF5_PLUGIN_PATH=/path/to/vol-geotiff/build/src
"""

import sys
import os
import netCDF4

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <geotiff_file>")
        sys.exit(1)

    geotiff_file = sys.argv[1]

    print("=" * 70)
    print("netCDF4-python → GeoTIFF VOL Connector Test")
    print("=" * 70)
    print(f"\nnetCDF4 version: {netCDF4.__version__}")
    print(f"netCDF-C library version: {netCDF4.getlibversion()}")
    print(f"\nTarget file: {geotiff_file}")
    print(f"File exists: {os.path.exists(geotiff_file)}")

    print(f"\nEnvironment variables:")
    print(f"  HDF5_VOL_CONNECTOR: {os.environ.get('HDF5_VOL_CONNECTOR', 'NOT SET')}")
    print(f"  HDF5_PLUGIN_PATH: {os.environ.get('HDF5_PLUGIN_PATH', 'NOT SET')}")

    print("\n" + "-" * 70)
    print("Test 1: Opening GeoTIFF with netCDF4.Dataset")
    print("-" * 70)

    try:
        # Try to open the GeoTIFF file as if it were a netCDF/HDF5 file
        # The netCDF-C library should use HDF5, which should use our VOL connector
        ds = netCDF4.Dataset(geotiff_file, 'r')
        print("✓ Successfully opened file with netCDF4.Dataset!")

        print(f"\nDataset info:")
        print(f"  Format: {ds.data_model}")
        print(f"  Dimensions: {list(ds.dimensions.keys())}")
        print(f"  Variables: {list(ds.variables.keys())}")
        print(f"  Global attributes: {list(ds.ncattrs())}")

        # Try to read some data
        if ds.variables:
            var_name = list(ds.variables.keys())[0]
            var = ds.variables[var_name]
            print(f"\nFirst variable '{var_name}':")
            print(f"  Shape: {var.shape}")
            print(f"  Dtype: {var.dtype}")
            print(f"  Dimensions: {var.dimensions}")

            # Try to read actual data
            try:
                data = var[:]
                print(f"  Successfully read data!")
                print(f"  Data shape: {data.shape}")
                print(f"  Data dtype: {data.dtype}")
                if data.size <= 100:
                    print(f"  Data sample: {data.flat[:10]}")
            except Exception as e:
                print(f"  ⚠ Could not read data: {e}")

        ds.close()
        print("\n✓ Dataset closed successfully")

        print("\n" + "=" * 70)
        print("✓ SUCCESS: netCDF4-python can access GeoTIFF via VOL connector!")
        print("=" * 70)
        return 0

    except Exception as e:
        print(f"\n✗ FAILED to open with netCDF4.Dataset")
        print(f"Error: {e}")
        print(f"Error type: {type(e).__name__}")

        import traceback
        print("\nFull traceback:")
        traceback.print_exc()

        print("\n" + "=" * 70)
        print("Analysis:")
        print("=" * 70)

        error_msg = str(e).lower()
        if 'not a valid netcdf' in error_msg or 'hdf error' in error_msg:
            print("The netCDF-C library may not be recognizing the VOL connector.")
            print("\nPossible issues:")
            print("1. netCDF-C may not be using the same HDF5 library as the VOL")
            print("2. VOL connector registration might not work with netCDF-C wrapper")
            print("3. netCDF-C might be checking file format before passing to HDF5")
            print("4. Environment variables might not be propagating to netCDF-C")

        print("\nNext steps:")
        print("- Verify netCDF-C was compiled with HDF5 support")
        print("- Check if netCDF-C supports HDF5 VOL connectors")
        print("- Consider alternative approaches (ctypes wrapper, etc.)")

        return 1

if __name__ == '__main__':
    sys.exit(main())
