This work-in-progress document describes the mapping between GeoTIFF file data and metadata and HDF5-level objects.

## Files

The top-level GeoTIFF file maps to an HDF5 file.

## Images

Each distinct image within the GeoTIFF file is considered an HDF5 dataset. Each image is assigned an HDF5 native datatype that matches the bits-per-sample and signed/unsigned qualities of the image's sample format.

### Dataset Naming

Images are exposed as HDF5 datasets using a zero-indexed naming convention: `image0`, `image1`, `image2`, etc. Dataset names provided to `H5Dopen` and other operations must be of the form `imageN`. Any other name will fail due to not precisely specifying an image within the GeoTIFF file.

This system will need to be expanded if support for multi-resolution images within GeoTIFF is added. This will likely be done by replacing the single name for a multi-resolution image with multiple dataset names of the form `imageN_<resolution_information>`.

### RGB(A) Images

Grayscale GeoTIFF images (1 sample per pixel) are represented in HDF5 as two-dimensional datasets with shape `[height, width]`.

GeoTIFF images with RGB color data (3 samples per pixel) are represented in HDF5 as three-dimensional datasets with shape `[height, width, 3]`, where the third dimension distinguishes among the three RGB color channels (red, green, blue).

Similarly, GeoTIFF images with RGBA color data (4 samples per pixel) are represented in HDF5 as three-dimensional datasets with shape `[height, width, 4]`, where the third dimension distinguishes among the four color channels (red, green, blue, alpha).

## (TBD) Metadata & Tags
