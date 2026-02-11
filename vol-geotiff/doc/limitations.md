# GeoTIFF VOL Connector: Limitations

This document is a non-exhaustive the major limitations and unsupported features of the GeoTIFF VOL connector.

## TIFF

### Read-Only

The connector is strictly read-only. GeoTIFF files cannot be created, modified, or written to through the HDF5 API.

### Photometric Interpretation

Only three photometric interpretations are supported:

- `PHOTOMETRIC_MINISBLACK`
- `PHOTOMETRIC_MINISWHITE`
- `PHOTOMETRIC_RGB`

Palette/indexed color (`PHOTOMETRIC_PALETTE`), YCbCr, CIE L\*a\*b\*, and other specialized color spaces are not supported.

### Planar Configuration

Only `PLANARCONFIG_CONTIG` (interleaved/chunky pixel data) is supported. `PLANARCONFIG_SEPARATE` is not supported.

### Bit Depth

Bits per sample must be a multiple of 8. Sub-byte formats (1-bit, 2-bit, 4-bit) and non-byte-aligned depths (e.g. 12-bit) are not supported.

### Samples Per Pixel

Only 1 (grayscale), 2 (grayscale + alpha), 3 (RGB), or 4 (RGBA) samples per pixel are supported. Images with 5+ samples (e.g. multispectral or hyperspectral data) are not supported.

### Extra Samples

Only a single extra sample interpreted as an alpha channel (associated or unassociated) is supported.

### Image Size Limits

Width and height must each be in the range 1–65535. Images exceeding 65,535 pixels in either dimension are rejected.

Similarly, the total decompressed image data is limited to 100 MB (defined as `SIZE_LIMIT_BYTES`). Images whose raw pixel data exceeds this threshold cannot be opened. See also the memory discussion in the Misc section.

### Multi-Resolution / Overviews

TIFF files may contain multiple resolution levels (overviews / image pyramids) stored as separate directories. The connector treats every directory as an independent dataset (`image0`, `image1`, ...) with no semantic distinction between full-resolution images and their overviews. There is no `SUBFILETYPE` inspection or hierarchical grouping.

### TIFF Tag Attributes

Several TIFF tags do not support access as HDF5 attributes:

- Color-space tags: `PRIMARYCHROMATICITIES`, `REFERENCEBLACKWHITE`, `WHITEPOINT`, `YCBCRCOEFFICIENTS`, `YCBCRPOSITIONING`, `YCBCRSUBSAMPLING`
- Fax/compression pseudo-tags: `BADFAXLINES`, `CLEANFAXDATA`, `CONSECUTIVEBADFAXLINES`, `FAXMODE`, `GROUP3OPTIONS`
- JPEG pseudo-tags: `JPEGCOLORMODE`, `JPEGTABLESMODE`
- Other: `EXTRASAMPLES`, `PREDICTOR`

### Compression Codecs

The connector delegates decompression entirely to libtiff. If libtiff on the host system lacks support for a given compression codec (e.g. a proprietary or rarely-built codec), the read will fail. The connector itself has no TIFF-compression awareness.

### GeoTIFF Keys

GeoTIFF coordinate transforms are handled by libgeotiff's `GTIFImageToPCS()`. Coordinate systems or projections that libgeotiff cannot interpret will produce NaN coordinates rather than an error.

## HDF5

### No Write / Create / Delete

No HDF5 write operations are implemented. File creation, dataset creation, attribute creation, group creation, link creation/copy/move/delete, and object copy are all unsupported.

### No Object Operations

`H5Oopen()`, `H5Oget_info()`, `H5Ocopy()`, and related object-level functions are entirely unimplemented (all callbacks are NULL).

### No Named Datatypes

The datatype VOL class is entirely NULL. Named (committed) datatypes cannot be created, opened, or queried.

### No VOL Connector Stacking

The wrap VOL class is entirely NULL. The connector cannot participate in a VOL passthrough/stacking chain.

### Limited File-Get Operations

`H5Fget_name()` works. All other file-get queries (`H5Fget_access_plist`, `H5Fget_create_plist`, `H5Fget_intent`, `H5Fget_obj_count`, `H5Fget_obj_ids`) are unsupported.

### Limited Dataset-Get Operations

`H5Dget_space()` and `H5Dget_type()` work. `H5Dget_create_plist()`, `H5Dget_access_plist()`, and `H5Dget_storage_size()` are unsupported.

### Limited Attribute Operations

- Attributes can be opened by name and read, but not created, written, renamed, or deleted.
- `H5Aget_space()` and `H5Aget_type()` work. `H5Aget_name()`, `H5Aget_info()`, `H5Aexists()`, `H5Aiterate()`, and `H5Aget_num_attrs()` are unsupported.
- Attributes can only be opened via `H5VL_OBJECT_BY_SELF` location; opening by index or creation order is not supported.

### Limited Group Operations

Only the root group `"/"` can be opened. No subgroups exist or can be created. `H5Gget_info()` works (reports `nlinks` as the number of images), but `H5Gget_create_plist()` does not.

### No HDF5 Filters

Since data is read through libtiff rather than the HDF5 I/O pipeline, HDF5-level filters (compression, shuffle, Fletcher32, etc.) are not applicable and not reported.

### Flat Namespace

The connector exposes a flat structure: a root group containing `imageN` datasets. There are no subgroups, no nested hierarchies, no soft/hard/external links beyond the implicit image links.

### Dataset Naming

Dataset names must exactly match the pattern `imageN` (e.g. `image0`, `image1`).

## Misc

### Full Image Loaded into Memory on Open

When a dataset is opened, the entire decompressed image is read into memory and cached. There is no lazy loading, memory-mapped I/O, or on-demand tile/scanline fetching. The 100 MB limit is a placeholder to prevent attempts to open excessively large images from crashing.

### Coordinates Computed Eagerly

The `coordinates` attribute computes lon/lat for every pixel in the image when read. For large images this can be slow and memory-intensive (16 bytes per pixel for the output, plus computation overhead).

### No Thread Safety

The TIFF file handle is shared across all datasets within a file. Concurrent dataset operations (e.g. reading two datasets from a multi-image file in parallel threads) may cause race conditions due to shared `TIFFSetDirectory()` state. 

### Shared Library Only

The build system links against `hdf5-shared`. Static linking is not supported.

### TIFF Directory Count

The number of TIFF directories is represented internally as `uint16_t`, limiting the connector to a maximum of 65,535 directories per file.

### Overstated Capability Flags

The connector reports `H5VL_CAP_FLAG_*_BASIC` for files, datasets, groups, and attributes. These flags imply basic creation support, but creation is not actually supported.
