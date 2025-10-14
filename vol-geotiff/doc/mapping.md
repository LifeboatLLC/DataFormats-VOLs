This work-in-progress document describes the mapping between GeoTIFF file data and metadata and HDF5-level objects.

## Files

The top-level GeoTIFF file maps to an HDF5 file.

## Images

Each distinct image within the GeoTIFF file is considered an HDF5 dataset. Each image is assigned an HDF5 native datatype that matches the bits-per-sample and signed/unsigned qualities of the image's sample format.

### Dataset Naming

Images are exposed as HDF5 datasets using a zero-indexed naming convention: `image0`, `image1`, `image2`, etc. The first image in a GeoTIFF file is accessible as the dataset named `"image0"`. This naming scheme is designed to support multi-image TIFF files in the future, where each image directory would correspond to a sequentially numbered dataset. Currently, only `image0` is implemented.

### RGB(A) Images

Grayscale GeoTIFF images (1 sample per pixel) are represented in HDF5 as two-dimensional datasets with shape `[height, width]`.

GeoTIFF images with RGB color data (3 samples per pixel) are represented in HDF5 as three-dimensional datasets with shape `[height, width, 3]`, where the third dimension distinguishes among the three RGB color channels (red, green, blue).

Similarly, GeoTIFF images with RGBA color data (4 samples per pixel) are represented in HDF5 as three-dimensional datasets with shape `[height, width, 4]`, where the third dimension distinguishes among the four color channels (red, green, blue, alpha).

## (TBD) Metadata & Tags
