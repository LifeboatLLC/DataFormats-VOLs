# HDF5 BUFR VOL Connector: Object Model and Read Semantics

This document defines the read-only HDF5 object layout exposed by the BUFR VOL connector and the semantics of implemented HDF5 APIS for that layout.

The connector exposes HDF5 namespace as follows:
 * BUFR file is apear as HDF5 file with a number of groups (BUFR messages)
 * BUFR keys and their values appear as HDF5 datasets or attributes on groups or datasets
   
All objects are read-only.

## 1. HDF5 Object Model for BUFR
**Namespace and groups**  

BUFR file has HDF5 namespace and contains multiple groups with the predefined names 

`message_0, message_1,..., message_<N-1>`

where `N` is a number of messages in the BUFR file. 

**BUFR Variables → HDF5 Datasets**  

Decoded BUFR data elements are represented as HDF5 datasets. The datasets correspond to the logical variables described by the BUFR descriptors 
and contain the values extracted from the message. The dimensionality of datasets depends on the number of subsets and occurrences of each element within the message.
Elements that repeat across subsets are typically represented as one-dimensional or two-dimensional arrays, depending on the structure of the BUFR data.

Datasets are named according to the BUFR element key names provided by the decoding library. 
These names correspond to the semantic identifiers defined in the BUFR descriptor tables.

**BUFR Data → HDF5 Attributes**

Metadata associated with BUFR messages is exposed through HDF5 attributes attached to groups and datasets. Group attributes represent message-level metadata, including information such as:
 - message type
 - center and subcenter identifiers
 - reference time
 - other message-level metadata
Dataset attributes represent metadata derived from the portion of a BUFR key that follows the “->” separator. For example, the attribute `percentConfidence`
is associated with the dataset `windSpeed`, corresponding to the BUFR key `windSpeed->percentConfidence`.
Representing this information as HDF5 attributes allows applications to access message metadata directly through standard HDF5 APIs without requiring explicit parsing of the internal BUFR message structure.

**BUFR Datatypes -> HDF5 Datatypes**

BUFR element values are mapped to HDF5 datatypes according to their decoded representation. Integer values are mapped to appropriate HDF5 integer types, while floating-point values are mapped to HDF5 floating-point types.
Missing BUFR values are represented using standard missing-value conventions supported by the decoding library.

| ecCodes Type           | Meaning                                | Typical HDF5 Mapping          |
| ---------------------- | -------------------------------------- | ----------------------------- |
| `CODES_TYPE_LONG`      | Integer (scaled/packed)                | `H5T_NATIVE_LONG`             |
| `CODES_TYPE_DOUBLE`    | Floating point (scaled physical value) | `H5T_NATIVE_DOUBLE`           |
| `CODES_TYPE_STRING`    | Text (code tables, descriptors, units) | VL string (`H5T_VARIABLE`)    |
| `CODES_TYPE_BYTES`     | Raw binary blob                        | `H5T_NATIVE_UCHAR` (array)    |
| `CODES_TYPE_UNDEFINED` | Missing/invalid                        | skip or map to attribute flag |



## 2. Opening BUFR file

BUFR files are accessed through the **HDF5 API** using the BUFR VOL connector. Before opening a file with `H5Fopen`, the VOL connector must be registered and a file access property list (FAPL) must be created and configured to use that connector. Once that is done, the file can be opened in read-only mode as a standard HDF5 file. 

**Example:** 
```
/* Register the BUFR VOL connector and create a FAPL */
hid_t vol_id  = H5VLregister_connector_by_name(BUFR_VOL_CONNECTOR_NAME, H5P_DEFAULT);
/* Create FAPL */
hid_t fapl_id = H5Pcreate(H5P_FILE_ACCESS);
/* Configure FAPL to use BUFR VOL connector */
H5Pset_vol(fapl_id, vol_id, NULL);

/* Open the BUFR file */
hid_t file_id = H5Fopen("temp.bufr", H5F_ACC_RDONLY, fapl_id);
```

## 3. Groups and Links

The BUFR VOL connector uses the following HDF5 layout:  

**Groups**  

BUFR file contais groups (BUFR messages). Each group may have attributes and datasets; they are not nested. To open the group use `H5Gopen()`.
```
group_id = H5Gopen2(file_id, "message_10", H5P_DEFAULT);
```

**Links**  

Name of each group corresponds to an HDF5 link in the root group (file). Functions `H5Lexists()` and `H5Literate()` can be used to check if group exists
and iterate over the groups to find their names and their total number.
  
```
H5Lexists(file_id, "message_10");
...
H5Literate2(file_id, H5_INDEX_NAME, H5_ITER_INC, &idx, file_info, &num_groups);
```  
For detailed example see `usage.md` file.

## 4. Reading BUFR Variables

BUFR variables are opened and read as HDF5 Datasets. Dataset name is a BUFR key name, where the key has replication count bigger than 1 or the size of the key bigger than 1. 
Currently, datasets are always one-dimensional. In the future, we may create two-dimensional datasets for replicated keys with the sizes bigger than 1. 

### 4.1 BUFR Variable (Dataset) Opening

Datasets are opened by passing the dataset name to `H5Dopen2` and using **group identifier** as a location identifier.

**Example:**  
```
dset_id = H5Dopen2(group_id, "pressure", H5P_DEFAULT);
```

### 4.2 Dataset datatype and read semantics

Each dataset has an HDF5 datatype that corresponds to the BUFR key type.

If `H5Dread` is called with a memory datatype different from the dataset datatype, the connector will fail. Datatype conversion will be implemented in the 
next version of the connector. To discover dataset's datatype and dimensionality use `H5Dget_tyep()` and `H5Dget_space` functions. See examples in `usage.md` 
file for more details.

### 4.3 Selection and data handling

The connector reads all data provided by the BUFR library. No spacial subsetting is implemented yet.

## 5. Attributes

BUFR keys are exposed as HDF5 attributes and can be accessed using standard HDF5 attribute APIs such as `H5Aread`.

**Type conversion is not yet tested for any attributes.**  

The corresponding HDF5 datatype and size of the attribute should be found by using `H5Aget_type` and `H5Aget_size` calls. See example in the `usage.md` for more details.

### 5.1 Discovery of datasets attributes

Use `H5Aiterate` function on dataset location identier to discover names and total number of dataset attributes as shown:
```
H5Aiterate2(obj_id, H5_INDEX_NAME, H5_ITER_INC, &idx, attr_info, &num_attrs);
```
For detailed example see `usage.md` file.

### 5.2 Dataset Attributes 

To open attribute `units` on dataset `pressure` and find its type and dimensionality the following calls are used:
```
dset_id  = H5Dopen2(group_id, "pressure", H5P_DEFAULT);
attr_id  = H5Aopen(dset_id, "units", H5P_DEFAULT);
type_id  = H5Aget_type(attr_id);
space_id = H5Aget_space(attr_id);
hsize_t adims[1] = {0};
H5Sget_simple_extent_dims(space_id, adims, NULL);

```
### 5.3 Discovery of group attributes

Use `H5Aiterate` function on group location identier to discover names and total number of group attributes as shown:
```
H5Aiterate2(group_id, H5_INDEX_NAME, H5_ITER_INC, &idx, group_info, &num_attrs);
```
For detailed example see `usage.md` file.

### 5.4 Group Attributes 

To open group attributes use group identifier and attribute name.  
```
hid_t attr_id = H5Aopen(group_id, "typicalDate", H5P_DEFAULT);
hid_t type_id = H5Aget_type(attr_id);
hid_t space_id = H5Aget_space(attr_id);

```

## 7. Handling BUFR STRING type for attributes and datasets

BUFR `CODES_TYPE_STRING` type is mapped to the HDF5 VL string. To avoid resource leaks use `H5Treclaim()" for scalar values or `H5Dvlen_reclaim` for array 
values to release resources as shown below. For complete example see `usage.md` file.

```
H5Treclaim(type_id, space_id, H5P_DEFAULT, &typical_date);
```
```
H5Dvlen_reclaim(type_id, space_id, H5P_DEFAULT, data);
```


