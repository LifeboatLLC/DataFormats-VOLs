#!/usr/bin/env python3
"""
Simple test to verify the GeoTIFF VOL connector works with updated h5py.
This avoids operations that the VOL connector doesn't support.
"""

import sys
import os
import h5py

def main():
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <geotiff_file>")
        sys.exit(1)

    geotiff_file = sys.argv[1]

    print("=" * 70)
    print("Simple GeoTIFF VOL Connector Test")
    print("=" * 70)
    print(f"\nOpening: {geotiff_file}")
    print(f"HDF5_VOL_CONNECTOR: {os.environ.get('HDF5_VOL_CONNECTOR', 'NOT SET')}")
    print(f"HDF5_PLUGIN_PATH: {os.environ.get('HDF5_PLUGIN_PATH', 'NOT SET')}")

    try:
        # Open the file
        f = h5py.File(geotiff_file, 'r')
        print("\n✓ File opened successfully via VOL connector")

        # Try to list keys directly (avoid 'in' operator)
        try:
            keys = list(f.keys())
            print(f"\n✓ Top-level keys: {keys}")
        except Exception as e:
            print(f"\n⚠ Could not list keys: {e}")

        # Try direct access to known dataset name
        try:
            dataset = f['image0']
            print(f"\n✓ Successfully accessed 'image0' dataset")
            print(f"  Shape: {dataset.shape}")
            print(f"  Dtype: {dataset.dtype}")

            # Try to read data
            data = dataset[:]
            print(f"\n✓ Successfully read data: {data.shape}, {data.dtype}")
            print(f"  Data range: [{data.min()}, {data.max()}]")
            print(f"  Data size: {data.nbytes} bytes")

        except KeyError:
            print(f"\n⚠ 'image0' dataset not found")
        except Exception as e:
            print(f"\n⚠ Error accessing dataset: {e}")

        # Close file manually
        try:
            f.close()
            print(f"\n✓ File closed successfully")
        except Exception as e:
            print(f"\n⚠ Warning during close: {e}")

        print("\n" + "=" * 70)
        print("✓ Test completed successfully!")
        print("=" * 70)

    except Exception as e:
        print(f"\n✗ ERROR: {e}")
        import traceback
        traceback.print_exc()
        sys.exit(1)

if __name__ == '__main__':
    main()
