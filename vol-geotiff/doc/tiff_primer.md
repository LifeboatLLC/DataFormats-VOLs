# A TIFF Primer

This document is intended for users of the GeoTIFF VOL who are familiar with the HDF5 library and associated concepts, but not with the TIFF/GeoTIFF file formats, or the libtiff library. It explains the basics of TIFF/GeoTIFF and goes over the major limitations of the GeoTIFF VOL connector compared to typical HDF5 library behavior.

## Understanding TIFF and GeoTIFF Basics

TIFF (Tagged Image File Format) is a flexible raster image format widely used for storing photographs, scanned documents, and scientific imagery. Unlike HDF5's hierarchical structure, TIFF organizes data as a flat sequence of numbered directories, each containing an image and its metadata tags. GeoTIFF extends TIFF by adding standardized tags for geospatial information—coordinate reference systems, transformations, and projection parameters.

The GeoTIFF VOL connector presents TIFF files through an HDF5 interface. There are some semantic differences from typical HDF5 files because TIFF's data model doesn't map one-to-one with HDF5's capabilities.

## Read-Only Access and File Structure

The connector provides read-only access. You cannot create new GeoTIFF files or modify existing ones. The VOL connector exists to enable reading and analysis of existing GeoTIFF data through the HDF5 library.

When you open a GeoTIFF file, you'll see a minimal group hierarchy. The root group `/` contains datasets named `/image0`, `/image1`, etc., corresponding to TIFF directories (sub-images). There are no nested groups.

## Dataset Structure and Dimensionality

TIFF image datasets map to 2D or 3D HDF5 datasets depending on the image type. Grayscale images become 2D `[height, width]` datasets. Multi-channel images (RGB, RGBA) become 3D `[height, width, channels]` datasets with interleaved channel data.

## Attributes Represent TIFF Tags

Attributes on image datasets correspond to TIFF tags, not arbitrary user-defined metadata. You'll see attributes like `IMAGEWIDTH`, `IMAGELENGTH`, `COMPRESSION`, `PHOTOMETRIC`, `XRESOLUTION`, `IMAGEDESCRIPTION`, and geospatial tags like `GEOTIEPOINTS` and `GEOPIXELSCALE`. Most, but not all TIFF tags are exposed this way - see `limitations.md` for a list of unsupported tags.

## Chunking, Compression, and Storage Layout

TIFF files can use strips (horizontal bands) or tiles (rectangular blocks) to organize image data. Many GeoTIFF files use tiling for efficient random access, similar to HDF5's chunked storage. However, the VOL connector does not expose TIFF tiles as HDF5 chunks.

Compression is fixed by the TIFF file. Common compression methods include LZW, DEFLATE (zlib), JPEG, and PackBits. The connector transparently decompresses data when you call `H5Dread()`, but you cannot change the compression level or method through HDF5 APIs. Compression happens at the TIFF level instead of through HDF5's filter pipeline.

## Memory Caching and Performance

The connector currently caches entire image datasets in memory when opened. This design choice optimizes for repeated access patterns common in image processing, but differs from HDF5's typical lazy-loading behavior where datasets are read on demand. Larger images where this may prove problematic are currently unsupported.

If you're working with large GeoTIFF files in memory-constrained environments, consider reading hyperslabs of the data rather than loading the full dataset. The connector supports `H5Sselect_hyperslab()` and will only transfer the selected region to your buffer, even though the full image is cached internally. Future versions may implement more sophisticated caching strategies or lazy loading to better match HDF5's typical behavior.

## Geospatial Metadata and Coordinates

The `coordinates` attribute on image datasets is a special computed attribute that doesn't exist in the TIFF file itself. When you read it, the connector performs coordinate transformations to convert pixel locations to geographic coordinates (longitude, latitude). This attribute has a compound datatype with `lon` and `lat` fields and dimensions `[height, width]`, providing one coordinate pair per pixel.

This computed attribute is expensive for large images because it transforms every pixel location. Unlike typical HDF5 attributes (which are small, cached metadata), reading `coordinates` may take significant time and memory. If you only need a few points, it's more efficient to read the `GEOTIEPOINTS` and `GEOPIXELSCALE` attributes and perform the transformation yourself for specific pixels of interest.
