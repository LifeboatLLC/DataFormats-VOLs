# GRIB2 VOL Connector HDF5: Object Model and Read Semantics

This document defines the read-only HDF5 object layout exposed by the GRIB2 VOL connector and the semantics of implemented HDF5 APIs (e.g., `H5Dread, H5Aread`) for that layout.

The connector exposes HDF5 namespace as follows:
 * GRIB2 file is apear as HDF5 file with a number of groups (GRIB2 messages)
 * GRIB2 keys and their values appear as HDF5 datasets or attributes on groups
   
**Dataset vs Attribute Rule**

The connector applies the following classification rule:

* Objects intended for `H5Dread` are lon/lat/value vectors.
* Objects intended for `H5Aread` are scalars, descriptive strings, and other possible arrays (pl, pv, packing tables, bitmap indices, etc.)

Concretely:

* Coordinate/value arrays → `H5Dread`
* GRIB keys and decoded meanings → `H5Aread`
---

## 1. HDF5 Object Model for GRIB2
**Namespace and groups**  

GRIB2 file has HDF5 namespace and contains multiple groups with the predefined names 

`message_1, message_1..., message_<N>`

where `<N>` is a 1-based, zero-padded message index (recommended width: 6, e.g., `/message_000001/`). Group name may contain leading `/`, i.e., here are examples of valid group names:

`/message_1, message_1, message_000001, /message_1`.

**GRIB2 Keys → HDF5 Datasets**

`lon`, `lat`, and `values` are the only datasets under any group `message_K`. The VOL connector will not find datasets with a different name. All other informatuion stored in a message is treated as attributes on the `message_K` group.  All datasets are 1-dimensional, and `lon` and `lat`  values are computed datasets based on GRIB2 grid definitions.

**Semantics**
- Longitude and latitude are in degrees.
- The order of rows follows the ecCodes iterator order for that grid (implementation-defined but stable for a given ecCodes version + grid).
- Missing values follow GRIB semantics: If the GRIB bitmap indicates missing points, the corresponding value entry uses the message’s missing-value convention (recommended: `IEEE NaN` for the exposed HDF5 view) and the message metadata records the bitmap/missing policy.

**GRIB2 Keys → HDF5 Attributes**

Metadata associated with GRIB2 messages is exposed through HDF5 attributes attached to groups.

**Raw GRIB2 Keys or Losselss Identifiers** 
 
 The connector exposes raw GRIB keys required to interpret and/or reproduce the message, including but not limited to:
- **Identification/time**: `editionNumber, centre, subCentre, dataDate, dataTime, stepUnits, forecastTime, typeOfProcessedData, …`
- **Parameter identity**: `discipline, parameterCategory, parameterNumber, productDefinitionTemplateNumber, …`
- **Grid definition**: `gridDefinitionTemplateNumber, Ni, Nj, scan flags, shapeOfTheEarth, …`
- **Packing/missing**: `packingType (or raw template number), bitsPerValue, bitMapIndicator, …`

**Derived semantic attributes (“decoded” meanings)**

Where ecCodes provides human-readable semantic companions (e.g., table-decoded descriptions), the connecto exposes them as string attributes, for example:

* `shapeOfTheEarthName`
* `centreDescription`
* `parameterName`, `parameterUnits`
* `name`, `shortName`, `cfName`, `typeOfLevel`, `level`

These attributes are derived by ecCodes from raw GRIB codes and tables and are not guaranteed to be physically present in the GRIB bitstream. 

**GRIB2 type -> HDF5 Datatype**

GRIB2 element values are mapped to HDF5 datatypes according to their decoded representation. Integer values are mapped to appropriate HDF5 integer types, while floating-point values are mapped to HDF5 floating-point types.
Missing GRIB2 values are represented using standard missing-value conventions supported by the decoding library.

| ecCodes Type           | Meaning                                | HDF5 Datatype                 |
| ---------------------- | -------------------------------------- | ----------------------------- |
| `CODES_TYPE_LONG`      | Integer (scaled/packed)                | `H5T_NATIVE_LONG`             |
| `CODES_TYPE_DOUBLE`    | Floating point (scaled physical value) | `H5T_NATIVE_DOUBLE`           |
| `CODES_TYPE_STRING`    | Text (code tables, descriptors, units) | `H5T_NATIVE_C1 (fixed-length) |
| `CODES_TYPE_BYTES`     | Raw binary blob                        | `H5T_NATIVE_UCHAR` (array)    |
| `CODES_TYPE_UNDEFINED` | Missing/invalid                        | skip or map to attribute flag |


The VOL connector will convert to the memory datatype specified in the H5Dread call. Only HDF5 native types specified above are currently supported, i.e., no HDF5 datatype conversion is implemented.

---
## 2. HDF5 Programming Model for GRIB2

### 2.1 Opening GRIB2 file

GRIB2 files are accessed through the **HDF5 API** using the GRIB2 VOL connector. Before opening a file with `H5Fopen`, the VOL connector must be registered and a file access property list (FAPL) must be created and configured to use that connector. Once that is done, the file can be opened in read-only mode as a standard HDF5 file. 

**Example:** 
```
/* Register the GRIB2 VOL connector and create a FAPL */
hid_t vol_id  = H5VLregister_connector_by_name(GRIB2_VOL_CONNECTOR_NAME, H5P_DEFAULT);
/* Create FAPL */
hid_t fapl_id = H5Pcreate(H5P_FILE_ACCESS);
/* Configure FAPL to use GRIB2 VOL connector */
H5Pset_vol(fapl_id, vol_id, NULL);

/* Open the GRIB2 file */
hid_t file_id = H5Fopen("example.grib2", H5F_ACC_RDONLY, fapl_id);
```

### 2.3 Groups and Links

The GRIB2 VOL connector uses the following HDF5 layout:  

**Groups**  

GRIB2 file contais groups (GRIB2 messages). Each group may have attributes and datasets `lon`, `lat`, and `values`; groups are not nested. To open the group use `H5Gopen()`.
```
group_id = H5Gopen2(file_id, "message_1", H5P_DEFAULT);
```

**Links**  

Name of each group corresponds to an HDF5 link in the root group (file). Functions `H5Lexists()` and `H5Literate2()` can be used to check if group exists and iterate over the groups to find their names and their total number.
  
```
H5Lexists(file_id, "message_1");
...
H5Literate2(file_id, H5_INDEX_NAME, H5_ITER_INC, &idx, file_info, &num_groups);
```  
For detailed example see `usage.md` file.

### 2.4 Reading GRIB2 Variables

GRIB2 variables `lon`, `lat`, and `values` are opened and read as HDF5 Datasets. Datasets are always one-dimensional. 

### 2.5 BUFR Variable (Dataset) Opening

Datasets are opened by passing the dataset name to `H5Dopen2` and using file or group identifier as a location identifier. When file identifier is provided, dataset name should be a full path containing group name.

**Example:**  
```
dset_id = H5Dopen2(file_id, "/message_1/value", H5P_DEFAULT);
``` 
```
dset_id = H5Dopen2(group_id, "value", H5P_DEFAULT);
```

### 2.6 Dataset datatype and read semantics

Each dataset has an HDF5 datatype that corresponds to the GRIB2 key type.

If `H5Dread` is called with a memory datatype different from the dataset datatype, the connector will fail. Datatype conversion will be implemented in the next version of the connector. To discover dataset's datatype and dimensionality use `H5Dget_type()` and `H5Dget_space` functions. See examples in `usage.md` file for more details.

### 2.7 Selection and data handling

The connector reads all data provided by the GRIB2 library. No spacial subsetting is implemented yet.

### 2.8 Attributes

GRIB2 keys are exposed as HDF5 attributes and can be accessed using standard HDF5 attribute APIs such as `H5Aread`.
Each attribute has an HDF5 datatype that corresponds to the GRIB2 key type. The datatype and size of the attribute should be found by using `H5Aget_type` and `H5Aget_size` calls. See example in the `usage.md` for more details.

 **Discovery of group attributes**

Use `H5Aiterate` function on group location identier to discover names and total number of dataset attributes as shown:
```
H5Aiterate2(group_id, H5_INDEX_NAME, H5_ITER_INC, &idx, attr_info, &num_attrs);
```
For detailed example see `usage.md` file.

To open attribute `maximum` on a group and to find its type and dimensionality the following calls are used:
```
attr_id  = H5Aopen(group_id, "maximum", H5P_DEFAULT);
type_id  = H5Aget_type(attr_id);
space_id = H5Aget_space(attr_id);
hsize_t adims[1] = {0};
H5Sget_simple_extent_dims(space_id, adims, NULL);

```

