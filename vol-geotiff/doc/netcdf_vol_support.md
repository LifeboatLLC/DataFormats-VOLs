# NetCDF Support via HDF5 VOL Connectors

## Basic ncdump Support

- **netCDF magic number patch** — Before any VOL callbacks matter, netCDF-C itself must recognize the file format. For GeoTIFF, `NC_interpret_magic_number()` in `libdispatch/dinfermodel.c` was patched to detect TIFF/BigTIFF byte signatures and route them through `NC_FORMATX_NC4` (the HDF5/VOL code path).

- **`file_get`** — netCDF calls `H5Fget_create_plist()` and `H5Fget_access_plist()` immediately after opening. The VOL must handle `H5VL_FILE_GET_FCPL` and `H5VL_FILE_GET_FAPL`; returning default property lists via `H5Pcreate()` is sufficient.

- **`group_open` / `group_get`** — netCDF opens the root group `"/"` and calls `H5Gget_create_plist()` on it. The VOL must handle `H5VL_GROUP_GET_GCPL`. As with the FCPL and FAPL, a default property list suffices.

- **`link_specific(H5VL_LINK_ITER)`** — netCDF calls `H5Literate2()` on the root group to discover its contents. The VOL must iterate over the "fake" links it uses to expose TIFF images (e.g., `"image0"`, `"image1"`, ...) and invoke the caller's callback for each one. The callback's first argument is an `hid_t` for the group — the VOL must produce this via `H5VLwrap_register(obj, H5I_GROUP)`, since the VOL only has raw `void*` pointers internally. The object's reference count should be incremented before wrapping so that the subsequent `H5Idec_ref()` doesn't free the group out from under the caller. Note also that `obj` may be a group rather than a file, so the implementation must follow `parent_file` to reach the file struct (for the TIFF handle, directory count, etc.) rather than assuming `obj` is always a file.

- **`object_open`** — Inside its link iteration callback, netCDF calls `H5Oopen(grpid, "image0", ...)` to generically open whatever the link points to. The VOL must accept a name via `H5VL_OBJECT_BY_NAME`, determine what kind of object it refers to, delegate to the appropriate open function (e.g., `dataset_open` or `group_open`), and set `*opened_type` so HDF5 knows whether it got back a dataset, group, etc. If `obj` is a group rather than a file, the VOL must follow `parent_file` to obtain the file struct before delegating to `dataset_open`, since dataset open needs the file's TIFF handle.

- **`object_get`** — NetCDF calls `H5Oget_info3()` to determine the object type. The VOL must handle `H5VL_OBJECT_GET_INFO` and populate `H5O_info2_t` with at minimum the `type` field (`H5O_TYPE_GROUP` or `H5O_TYPE_DATASET`). For the GeoTIFF connector, when called by name, the type can be inferred from the name (e.g., `"imageN"` → dataset, `"/"` → group); when called on self (`H5VL_OBJECT_BY_SELF`), the type comes from the object's `obj_type` field.

- **`dataset_get`** — Beyond `H5VL_DATASET_GET_SPACE` and `H5VL_DATASET_GET_TYPE`, netCDF also calls `H5Dget_access_plist()` and `H5Dget_create_plist()` on every dataset it discovers. The VOL must handle `H5VL_DATASET_GET_DAPL` and `H5VL_DATASET_GET_DCPL`; default property lists are sufficient.

- **`attr_specific(H5VL_ATTR_EXISTS)`** — netCDF checks for the existence of several special attributes: `_nc3_strict`, `_NCProperties`, and others. The VOL must handle `H5VL_ATTR_EXISTS` and return a boolean. A try-open-then-close approach suffices: attempt `attr_open`, and if it succeeds, the attribute exists.

- **`attr_specific(H5VL_ATTR_ITER)`** — netCDF iterates attributes on both the root group and every dataset. Even if no attributes are exposed yet, the VOL must accept this call without error on all object types (groups, datasets, files). For a minimal implementation, a no-op which returns SUCCESS is valid and tells netCDF there are no attributes to enumerate.

---

## Towards CF-Compliant NetCDF Representation for GeoTIFF

A well-formed netCDF-4/CF file has a specific structure. The root group carries a `Conventions = "CF-1.8"` attribute. Coordinate axes are stored as 1-D datasets named `"x"` and `"y"` (or `"lon"` and `"lat"`) marked with `CLASS = "DIMENSION_SCALE"` attributes. A scalar `"crs"` (coordinate reference system) dataset (without data, just attributes) describes the projection via CF-standard attribute names like `grid_mapping_name`, `false_easting`, `semi_major_axis`, etc. The actual raster data lives in a dataset with a `grid_mapping = "crs"` attribute and a `DIMENSION_LIST` attribute that references the coordinate datasets.

- **CF concepts from GeoTIFF files**

  - **Raster data** — Maps directly from TIFF pixel data to a netCDF variable.
  - **Image dimensions** — `ImageLength` and `ImageWidth` tags provide the dimensions directly; the VOL can synthesize `"y"` and `"x"` names for them.
  - **Coordinate arrays** — CF expects explicit 1-D arrays of coordinate values, but GeoTIFF stores only an affine transform (ModelTiepoint + ModelPixelScale). The VOL must compute coordinate arrays from the transform parameters.
  - **CRS / grid mapping** — CF wants individual projection parameters spelled out as attributes (`false_easting`, `semi_major_axis`, etc.). Most GeoTIFF files store only an EPSG code (a numeric ID referencing a coordinate reference system definition), so the VOL would need to expand that code into its constituent parameters via a lookup table or library call. Only user-defined CRS entries store the individual GeoKeys directly.
  - **Projection parameters** — GeoKeys like `ProjFalseEastingGeoKey` are only present when the CRS is user-defined rather than referenced by EPSG code, so coverage is partial.
  - **Units** — Not stored in GeoTIFF. Must be inferred from the CRS type: meters for projected coordinate systems, degrees for geographic ones.
  - **`standard_name`** — Not stored in GeoTIFF. Must be inferred from context (e.g., `"projection_x_coordinate"` for projected CRS vs. `"longitude"` for geographic).
  - **`_FillValue` / NoData** — Sometimes available via `TIFFTAG_GDAL_NODATA` (tag 42113), but this is a GDAL convention, not part of the baseline TIFF spec.
  - **Band names** — Rarely available. Some files use `IMAGEDESCRIPTION` or GDAL-specific metadata, but there is no standard mechanism in TIFF.
  - **Time dimension** — Not available.
  - **Vertical levels** — Not available.

- **Getting past `phony_dim` to real dimension names** — netCDF assigns `phony_dim_N` when a dataset's dimensions aren't associated with HDF5 Dimension Scales. There are three approaches which could be taken for a VOL connector to provide real dimension names:

  - **Synthesize dimension scale datasets.** Present virtual `"x"` and `"y"` datasets during link iteration, with `CLASS = "DIMENSION_SCALE"` and `NAME` attributes, containing coordinate values computed from the affine transform. The image dataset would carry a `DIMENSION_LIST` attribute referencing them. This is the fully correct approach but somewhat substantial to implement, as `DIMENSION_LIST` uses HDF5 object references (`H5R_ref_t`), which means the VOL needs to support reference creation and resolution.

  - **Synthesize `_Netcdf4Dimid` attributes.** netCDF-4 uses these to associate datasets with named dimensions without full dimension scale machinery. The VOL would report dimension names via group info and attach `_Netcdf4Dimid` attributes to datasets. Lighter-weight than full dimension scales.

  - **Use the CF `coordinates` attribute.** Adding `coordinates = "y x"` to the image dataset tells CF-aware tools where to find coordinate data. This doesn't fix the `phony_dim` names in ncdump itself, but downstream CF tools will know to look at the `y` and `x` datasets for georeferencing. However, this would conflict with the way that computed coordinates for a TIFF image are currently exposed via the computed `coordinates` attribute.

- **Exposing CRS as a CF grid mapping variable** — In the CF convention, a "grid mapping variable" is a scalar dataset that contains no actual data, which exists solely as a container for attributes that describe a map projection (projection type, false easting, semi-major axis, etc.). The raster dataset references it by name via a `grid_mapping = "crs"` attribute, and CF-aware tools read the grid mapping variable's attributes to interpret the raster's coordinates.

  A grid mapping variable is not always required. Files using plain lat/lon (unprojected, geographic coordinates) don't need one. It becomes necessary when data uses a projected coordinate system (UTM, Lambert Conformal, Mercator, etc.), because the x/y coordinates aren't directly interpretable as lat/lon without knowing the projection parameters. Since GeoTIFF files almost always use a projected or geographic CRS, synthesizing a grid mapping variable is relevant for most files.

  The VOL would need to synthesize a scalar `"crs"` dataset with projection attributes derived from GeoTIFF GeoKeys. The key mappings are:

  - `ProjCoordTransGeoKey` → `grid_mapping_name`
  - `ProjFalseEastingGeoKey` → `false_easting`
  - `ProjFalseNorthingGeoKey` → `false_northing`
  - `ProjNatOriginLongGeoKey` → `longitude_of_central_meridian`
  - `ProjNatOriginLatGeoKey` → `latitude_of_projection_origin`
  - `ProjScaleAtNatOriginGeoKey` → `scale_factor_at_central_meridian`
  - `GeogSemiMajorAxisGeoKey` → `semi_major_axis`
  - `GeogInvFlatteningGeoKey` → `inverse_flattening`
  - `GeoAsciiParamsTag` → `crs_wkt`

  The main difficulty is that, as mentioned above, many files only store an EPSG code rather than individual projection parameters. Resolving EPSG to CF attributes would require either an embedded lookup table, using libgeotiff's `GTIFGetDefn()` to extract the definition (already linked), or just exposing `crs_wkt` and `epsg_code` and letting downstream tools handle resolution.
