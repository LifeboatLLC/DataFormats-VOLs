# An HDF5 Primer

This document is intended for users of the GeoTIFF VOL who are familiar with TIFF/GeoTIFF libraries and concepts, but not with HDF5. It explain basic HDF5 concepts and practices.

## Hierarchy and Objects

The HDF5 file format organizes data hierarchically, similar to a filesystem. Files contain groups (like directories) and datasets (like files containing arrays). Groups form a tree structure starting from the root group `/`, and datasets store multidimensional arrays with associated metadata called attributes. Unlike TIFF's flat tag-based metadata system, HDF5 provides a more structured approach where attributes attach to specific datasets or groups, and the type system explicitly describes data layout, endianness, and precision.

The GeoTIFF VOL connector transparently translates the TIFF structure into this HDF5 model, allowing you to use the HDF5 library inspect and read GeoTIFF files without writing TIFF-specific code. The underlying data is not modified, and no new files are created.

## Accessing TIFF Image Data

When you open a GeoTIFF file through the VOL connector, the raster data appears as HDF5 datasets named `/image0`, `/image1`, etc., with each dataset corresponding to a TIFF directory (sub-image). Single-image GeoTIFF files have just `/image0`. Rather than using `TIFFSetDirectory()` to navigate between images, in the HDF5 view each image is a separately addressable dataset path.

To read the image data, open the dataset and call `H5Dread()`. The connector automatically handles decompression and byte-order conversion, just as `TIFFReadScanline()` does. For grayscale images, you get a 2D array `[height, width]`. RGB and RGBA images become 3D arrays `[height, width, channels]`.

## Discovering Image Properties

Instead of calling `TIFFGetField(TIFFTAG_IMAGEWIDTH)` and `TIFFGetField(TIFFTAG_IMAGELENGTH)`, you query the HDF5 dataspace. Use `H5Dget_space()` to retrieve the dataspace, then `H5Sget_simple_extent_dims()` to get dimensions. For a grayscale image, you'll see dimensions like `[512, 512]`. For RGB, you'll see `[512, 512, 3]`.

Data types work similarly. Rather than checking `TIFFTAG_BITSPERSAMPLE` and `TIFFTAG_SAMPLEFORMAT`, call `H5Dget_type()` on the dataset. An 8-bit unsigned grayscale image maps to `H5T_NATIVE_UCHAR`, while a 16-bit unsigned image maps to the HDF5 datatype `H5T_NATIVE_USHORT`. Floating-point GeoTIFF data maps to `H5T_NATIVE_FLOAT` or `H5T_NATIVE_DOUBLE`.

## Reading TIFF Tag Metadata

TIFF tags become HDF5 attributes attached to the `/image0` dataset. To read what you'd normally access via `TIFFGetField(TIFFTAG_IMAGEDESCRIPTION)`, open the attribute named `"TIFFTAG_IMAGEDESCRIPTION"` and call `H5Aread()` on it. The connector exposes most standard TIFF tags this way.

For tags that return arrays (like `STRIPOFFSETS` or `STRIPBYTECOUNTS`), the attribute has an array datatype matching the tag's structure. String tags become HDF5 string attributes. You can list all available attributes using `H5Aget_num_attrs()` and `H5Aget_name_by_idx()`.

## Accessing Geospatial Metadata

GeoTIFF's coordinate reference system information appears as attributes on the image datasets. The standard GeoTIFF tags `GEOTIEPOINTS`, `GEOPIXELSCALE`, and `GEOTRANSFORMATION` are accessible as attributes with those names. These contain the same numeric data you'd retrieve from `TIFFGetField()`.

The connector also provides a computed `coordinates` attribute that performs coordinate transformation for you. Instead of manually calling `GTIFImageToPCS()` and projection library functions to convert pixel coordinates to longitude/latitude, you can read the `coordinates` attribute to get a 2D array of `{lon, lat}` structures, one for each pixel. This is stored as an HDF5 compound type with `lon` and `lat` fields. While convenient for small images, be aware this computes coordinates for every pixel on access, so it may be expensive for large rasters.

## Working with Multi-Image Files

TIFF files often contain multiple images (multi-page TIFFs or images with overviews). In traditional libtiff code, you'd loop through directories using `TIFFSetDirectory(i)` and check the return value. With the HDF5 VOL, each directory becomes a separate dataset: `/image0`, `/image1`, `/image2`, etc. You can check how many images exist via opening and reading the `num_images` attribute on the root group.
