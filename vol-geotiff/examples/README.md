# GeoTIFF VOL Connector Examples

This directory contains C examples demonstrating how to use the GeoTIFF VOL connector to read GeoTIFF files through the HDF5 API. These examples are built as part of the normal build process - see `doc/usage.md`.

## Basic Examples

### Iterate and Read Demo
Iterating through all images in a multi-image TIFF file and reading the full raster data from each, with summary statistics.

## Metadata Examples

### Metadata Demo
Reading standard TIFF tags as HDF5 attributes (dimensions, resolution, descriptive strings).


### Coordinates Demo
Reading geographic coordinates (lon/lat) from the compound "coordinates" attribute on georeferenced images.


## Visualization

### gnuplot Visualization Demo
The demo will:
- Create `demo_rings_gnuplot.tif` with colorful concentric rings
- Read it back through the HDF5 VOL connector
- Generate gnuplot data files (`gnuplot_rgb.dat`, `gnuplot_gray.dat`)
- Create a gnuplot script (`gnuplot_commands.gp`)
- Invoke gnuplot to generate visualizations (`demo_rgb.png`, `demo_gray.png`)

From the build directory:

To execute it, run the `gnuplot_demo` executable from the build directory.

## Running Examples

All examples should be run from the build directory:
```bash
cd build
./examples/<example_name>
```

## Troubleshooting

If the demo fails to find the VOL connector:
```bash
export HDF5_PLUGIN_PATH=/path/to/vol-geotiff/build/src
```
