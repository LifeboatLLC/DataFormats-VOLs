# CDF VOL Connector HDF5 View: Object Model and Read Semantics

This document defines the read-only HDF5 object layout exposed by the CDF VOL connector and the semantics of `H5Dread` and `H5Aread` for that layout.

The connector exposes a flat HDF5 namespace:
 * CDF `zVariables` appear as HDF5 datasets
 * CDF attributes appear as HDF5 attributes

All objects are read-only.

## 1. HDF5 Object Model
**Namespace and groups**  

The HDF5 namespace is flat. Only the root group (`/`) is implemented. No subgroups are created or interpreted.

**Variables → Datasets**  
Each CDF `zVariable` is exposed as one HDF5 dataset directly under `/`.  
Dataset names must match the CDF variable name exactly and **must not include a leading slash**.

CDF `rVariables` and `rEntries` are not yet supported.

**Attributes**
 * `vAttributes` attach to the corresponding `zVariable` dataset and must be opened using the dataset ID.
 * `gAttributes` attach to the file object (not to datasets) and must be opened using the file ID.

## 2. Variables (CDF zVariables)
### 2.1 Dataset Opening

Datasets are opened by passing the dataset name (no leading slash) to `H5Dopen`.

**Example:**  
```
dset_id = H5Dopen(file_id, "Image", ...);
```
Using a leading slash (for example `"/Image"`) is not supported.

**Note:** H5Dopen can also accept the root group ID instead of the file ID. This works, but it is an extra step with no benefit because the dataset name is resolved directly from the root namespace, so the file ID is sufficient.

### 2.2 Dataset datatype and read semantics
Each dataset has an HDF5 datatype that corresponds to the CDF variable datatype.

If `H5Dread` is called with a memory datatype different from the dataset datatype, the connector delegates conversion to HDF5’s conversion engine (`H5Tconvert`). Conversion succeeds only when HDF5 supports it; otherwise `H5Dread` fails with the standard HDF5 conversion error.

### 2.3 Selection and data handling

The connector reads the full extent of the variable and returns the values provided by the CDF library.

Standard HDF5 selection mechanisms (e.g., hyperslabs) may be used to read subsets of the data. Data is generated or converted on demand during the read operation. No additional interpretation, reshaping, or reordering is performed.

## 3. Attributes
### 3.1 Variable Attributes (vAttributes)

CDF variable attributes (`vAttributes`) are exposed as HDF5 attributes attached to the corresponding dataset.

A single `vAttribute` may contain multiple `zEntries`. Each `zEntry` belongs to exactly one CDF variable. The connector exposes each `zEntry` as a separate HDF5 attribute on that variable’s dataset.

**For Example:** to open attribute `FIELDNAM` on dataset `Image`:
```
dset_id  = H5Dopen(file_id, "Image", ...);
attr_id  = H5Aopen(dset_id, "FIELDNAM", ...);
```

`vAttribute` datatypes correspond to the CDF attribute type. 

**Type conversion is not yet supported for any attributes.**

### 3.2 Global Attributes (gAttributes)

CDF global attributes (`gAttributes`) are exposed as HDF5 attributes attached to the file object (not to datasets). Unlike `vAttributes`, a single CDF `gAttribute` can contain multiple `gEntries`. Because HDF5 attributes do not natively support multiple entries under the same name, the connector provides two access methods:

**1. Open a specific gEntry by index**  
```
attr_id = H5Aopen(file_id, "NAME[1]", ...);
```
This opens the `gEntry` at index `1`. The HDF5 attribute datatype will correspond to the CDF datatype of the gEntry.

**2. Open the entire gAttribute as an array of strings:**  
```
attr_id = H5Aopen(file_id, "NAME", ...);
```
When opened without an index, the attribute is returned as a 1D fixed-length string array, where each element corresponds to one `gEntry`.

All strings in the array have the same fixed length. The string length is chosen to be large enough to hold the string representation of the largest `gEntry` in the attribute. This size is determined by examining every `gEntry` in the attribute and computing the maximum possible string length required to represent its value.

As a result, some strings may contain unused space if their corresponding `gEntry` has a shorter string representation. This behavior is expected and ensures that all `gEntries` can be safely represented without truncation.

### 4. Notes on Performance and Caching
Currently, when a subset of dataset data is requested, the connector reads the full dataset into memory and then extracts the subset using HDF5 selection. This will likely be changed in the future to only read the selected hyperslab directly from the CDF file, avoiding unnecessary memory use.