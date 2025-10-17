#!/usr/bin/env python3
"""
Demonstrate reading a GeoTIFF file through the HDF5 VOL connector using h5py,
then visualizing it with xarray and matplotlib.

This script:
1. Opens a GeoTIFF file using h5py (which uses the GeoTIFF VOL connector)
2. Loads the image data into an xarray DataArray
3. Visualizes the RGB image using matplotlib
4. Saves the visualization to disk and displays it

Environment variables required:
- HDF5_PLUGIN_PATH: Path to the GeoTIFF VOL connector shared library
- HDF5_VOL_CONNECTOR: Should be set to "geotiff_vol_connector"
"""

import sys
import os
import numpy as np
import h5py
import xarray as xr
import matplotlib.pyplot as plt
from pathlib import Path

def check_environment():
    """Check that required environment variables are set."""
    required_vars = ['HDF5_PLUGIN_PATH', 'HDF5_VOL_CONNECTOR']
    missing = []

    for var in required_vars:
        if var not in os.environ:
            missing.append(var)

    if missing:
        print(f"ERROR: Missing required environment variables: {', '.join(missing)}")
        print("\nRequired settings:")
        print("  export HDF5_PLUGIN_PATH=/path/to/vol-geotiff/build/src")
        print("  export HDF5_VOL_CONNECTOR=geotiff_vol_connector")
        return False

    print("✓ Environment variables configured:")
    print(f"  HDF5_PLUGIN_PATH: {os.environ['HDF5_PLUGIN_PATH']}")
    print(f"  HDF5_VOL_CONNECTOR: {os.environ['HDF5_VOL_CONNECTOR']}")
    return True

def read_geotiff_via_hdf5(filename):
    """
    Read a GeoTIFF file using h5py through the GeoTIFF VOL connector.

    Args:
        filename: Path to the GeoTIFF file

    Returns:
        numpy array containing the image data
    """
    print(f"\nOpening GeoTIFF file via HDF5 VOL connector: {filename}")

    try:
        # Open the file using h5py - this will use the GeoTIFF VOL connector
        with h5py.File(filename, 'r') as f:
            print(f"✓ Successfully opened file via VOL connector")

            # List available datasets
            print(f"\nAvailable datasets in HDF5 view:")
            def print_items(name, obj):
                if isinstance(obj, h5py.Dataset):
                    print(f"  {name}: shape={obj.shape}, dtype={obj.dtype}")
            f.visititems(print_items)

            # Read the image dataset
            # The GeoTIFF VOL connector exposes the image as "image0"
            if 'image0' in f:
                dataset = f['image0']
                print(f"\nReading dataset 'image0':")
                print(f"  Shape: {dataset.shape}")
                print(f"  Dtype: {dataset.dtype}")

                # Read the full dataset
                data = dataset[:]
                print(f"✓ Successfully read {data.nbytes} bytes")

                return data
            else:
                print("ERROR: 'image0' dataset not found in file")
                return None

    except Exception as e:
        print(f"ERROR reading file: {e}")
        import traceback
        traceback.print_exc()
        return None

def create_xarray_dataarray(data):
    """
    Create an xarray DataArray from the numpy array.

    Args:
        data: numpy array with shape (height, width, channels) for RGB

    Returns:
        xarray.DataArray with proper dimensions and coordinates
    """
    print(f"\nCreating xarray DataArray from data...")

    if data.ndim == 3:
        # RGB image: (height, width, channels)
        height, width, channels = data.shape

        da = xr.DataArray(
            data,
            dims=['y', 'x', 'band'],
            coords={
                'y': np.arange(height),
                'x': np.arange(width),
                'band': ['red', 'green', 'blue']
            },
            attrs={
                'description': 'RGB image from GeoTIFF via HDF5 VOL connector',
                'source': 'GeoTIFF VOL connector demo'
            }
        )
        print(f"✓ Created DataArray with dimensions: {dict(da.sizes)}")
        return da

    elif data.ndim == 2:
        # Grayscale image: (height, width)
        height, width = data.shape

        da = xr.DataArray(
            data,
            dims=['y', 'x'],
            coords={
                'y': np.arange(height),
                'x': np.arange(width)
            },
            attrs={
                'description': 'Grayscale image from GeoTIFF via HDF5 VOL connector',
                'source': 'GeoTIFF VOL connector demo'
            }
        )
        print(f"✓ Created DataArray with dimensions: {dict(da.sizes)}")
        return da
    else:
        print(f"ERROR: Unexpected data dimensions: {data.shape}")
        return None

def visualize_and_save(da, output_file='demo_visualization.png'):
    """
    Visualize the xarray DataArray and save to file.

    Args:
        da: xarray.DataArray containing image data
        output_file: Path to save the visualization
    """
    print(f"\nVisualizing data...")

    fig, axes = plt.subplots(1, 2, figsize=(14, 6))

    # Plot 1: Full RGB image
    if 'band' in da.dims:
        # RGB image
        rgb_data = da.values
        axes[0].imshow(rgb_data)
        axes[0].set_title('RGB Image via GeoTIFF VOL Connector', fontsize=14, fontweight='bold')
        axes[0].set_xlabel('X (pixels)')
        axes[0].set_ylabel('Y (pixels)')
        axes[0].grid(True, alpha=0.3)

        # Plot 2: Individual channel intensity plot
        # Show the red channel as an example
        red_channel = da.sel(band='red')
        im = axes[1].imshow(red_channel, cmap='Reds')
        axes[1].set_title('Red Channel Intensity', fontsize=14, fontweight='bold')
        axes[1].set_xlabel('X (pixels)')
        axes[1].set_ylabel('Y (pixels)')
        plt.colorbar(im, ax=axes[1], label='Intensity (0-255)')

    else:
        # Grayscale image
        axes[0].imshow(da, cmap='gray')
        axes[0].set_title('Grayscale Image via GeoTIFF VOL Connector', fontsize=14, fontweight='bold')
        axes[0].set_xlabel('X (pixels)')
        axes[0].set_ylabel('Y (pixels)')

        # Hide second subplot for grayscale
        axes[1].axis('off')

    plt.suptitle('GeoTIFF accessed via HDF5 VOL Connector → h5py → xarray',
                 fontsize=16, fontweight='bold', y=1.02)
    plt.tight_layout()

    # Save to file
    plt.savefig(output_file, dpi=150, bbox_inches='tight')
    print(f"✓ Saved visualization to: {output_file}")

    # Display the plot
    print(f"✓ Displaying visualization window...")
    plt.show()

def main():
    """Main execution function."""
    print("=" * 70)
    print("GeoTIFF VOL Connector Visualization Demo")
    print("=" * 70)

    # Check command line arguments
    if len(sys.argv) != 2:
        print(f"Usage: {sys.argv[0]} <geotiff_file>")
        print(f"\nExample:")
        print(f"  {sys.argv[0]} demo_rings.tif")
        sys.exit(1)

    geotiff_file = sys.argv[1]

    # Check that file exists
    if not Path(geotiff_file).exists():
        print(f"ERROR: File not found: {geotiff_file}")
        sys.exit(1)

    # Check environment
    if not check_environment():
        sys.exit(1)

    # Read GeoTIFF via h5py using VOL connector
    data = read_geotiff_via_hdf5(geotiff_file)
    if data is None:
        print("\nFailed to read GeoTIFF file")
        sys.exit(1)

    # Create xarray DataArray
    da = create_xarray_dataarray(data)
    if da is None:
        print("\nFailed to create xarray DataArray")
        sys.exit(1)

    # Print some statistics
    print(f"\nData statistics:")
    print(f"  Min value: {da.min().values}")
    print(f"  Max value: {da.max().values}")
    print(f"  Mean value: {da.mean().values:.2f}")

    # Visualize and save
    output_file = 'demo_visualization.png'
    visualize_and_save(da, output_file)

    print("\n" + "=" * 70)
    print("Demo completed successfully!")
    print("=" * 70)
    print(f"\nWorkflow summary:")
    print(f"  1. ✓ GeoTIFF file opened via HDF5 VOL connector")
    print(f"  2. ✓ Data read through h5py")
    print(f"  3. ✓ Data loaded into xarray DataArray")
    print(f"  4. ✓ Visualization created with matplotlib")
    print(f"  5. ✓ Output saved to {output_file}")

if __name__ == '__main__':
    main()
