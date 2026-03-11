# HDF5 CDF VOL Connector: Object Model and Read Semantics

This document defines the read-only HDF5 object layout exposed by the CDF VOL connector and the semantics of `H5Dread` and `H5Aread` for that layout.

The connector exposes a flat HDF5 namespace:
 * CDF file is apear as HDF5 file
 * CDF `zVariables` and `rVariables` appear as HDF5 datasets
 * CDF attributes appear as HDF5 attributes on file or datasets

All objects are read-only.

## 1. HDF5 Object Model for CDF 
**Namespace and groups**  

CDF file has flat HDF5 namespace and only contains the root group (`/`). 

**CDF Variables → HDF5 Datasets**  

Each CDF variable is exposed as an HDF5 dataset directly under the root group `/`.  
Dataset names must match the CDF variable name exactly and **must not include a leading slash**.

**Note:** CDF `rVariables` and `rEntries` are supported via opening the file with `zMode\2` (More on that in section 4).

**CDF Attributes → HDF5 Attributes**

 * `vAttributes` attach to the corresponding `zVariable` dataset and must be opened using the dataset ID.
 * `gAttributes` attach to the file object (not to datasets) and must be opened using the file or root group ID.

## 2. Opening CDF file

CDF files are accessed through the **HDF5 API** using the CDF VOL connector. Before opening a file with `H5Fopen`, the VOL connector must be registered and a file access property list (FAPL) must be created and configured to use that connector. Once that is done, the file can be opened in read-only mode as a standard HDF5 file. 

**Example:** 
```
/* Register the CDF VOL connector and create a FAPL */
hid_t vol_id  = H5VLregister_connector_by_name(CDF_VOL_CONNECTOR_NAME, H5P_DEFAULT);
/* Create FAPL */
hid_t fapl_id = H5Pcreate(H5P_FILE_ACCESS);
/* Configure FAPL to use CDF VOL connector */
H5Pset_vol(fapl_id, vol_id, NULL);

/* Open the CDF file */
hid_t file_id = H5Fopen("example.cdf", H5F_ACC_RDONLY, fapl_id);
```
It is also possible to open the file while excluding the `.cdf` file extension from the file name.

> **Note:** Currently, only **ROW_MAJOR** CDF files are fully supported. Files stored in **COLUMN_MAJOR** layout may open, but doing so can result in undefined internal errors. 

## 3. Links and Groups
The CDF VOL connector uses a very flat HDF5 layout:  

* **Root group (`/`)**  
  All variables and global attributes are attached directly to the root group or file object. There are no nested HDF5 groups for variables, and the root group serves as the single organizational container.  

* **Groups**  
  The connector does not currently create or use additional HDF5 groups beyond the root. All variable datasets appear as direct children of `/`. 

* **Links**  
  Each variable dataset is represented as a **hard link** under the root group. Functions such as `H5Lexists()` and `H5Literate()` can be used to check for and iterate over these links. There are no soft or symbolic links, and link deletion is not supported.

**Implications for users:**

* All variable datasets can be opened directly with `H5Dopen(root_group_id, "variable_name", ...)`  (explained more below).
* Global attributes are attached to the file or root group, and variable attributes are attached to individual datasets.  
* Link iteration or existence checks should be performed only at the root group level; no nested group hierarchy exists in this connector.

### 3.1 Link Existence checking

The `H5Lexists()` function can be used with the CDF VOL connector to check if a dataset with a given name exists on the root group.

**Key points:**
* Only **variable datasets** are represented as HDF5 links. Global attributes are **not** links and will not be detected by `H5Lexists()`.
* The parent object for `H5Lexists()` should be the **file ID or root group ID**.
* The `name` argument can be either a relative name (e.g., `"temperature"`) or an absolute path from the root (e.g., `"/temperature"`).
* The function returns a positive value if the variable exists, zero if it does not, and a negative value in case of errors.

**Example:**
```
htri_t exists = H5Lexists(root_group_id, "UNITS");
/* Then check 'exists' to see if that attribute exists on that dataset */
```

### 3.2 Link Iteration
The `H5Literate()` (or `H5Literate2()`) function can be used to iterate over variable datasets under the root group in a CDF VOL connector file.

**Key points:**
* Only **variable datasets** are represented as HDF5 links. Global attributes and variable attributes are **not** iterated as links.
* Iteration should be performed on the **file ID or root group ID**.
* Links are reported in **creation order**, which corresponds to the order of zVariables in the CDF file.
* Each link is a **hard link**; there are no soft or symbolic links in this connector.
* Deletion of links is **not supported**.

## 4. Reading CDF Variables

CDF Variables are opened and read as HDF5 Datasets. 
The CDF file is opened using **zMode/2**, which treats all rVariables as zVariables in all CDF functions. This means that the VOL connector reads all variables as zVariables (read more about zModes in CDF [here](https://spdf.gsfc.nasa.gov/pub/software/cdf/doc/cdf_User_Guide.pdf#page=35).

CDF 

### 4.1 CDF Variable (Dataset) Opening

Datasets are opened by passing the dataset name to `H5Dopen2`.

**Example:**  
```
dset_id = H5Dopen2(file_id, "Image", H5P_DEFAULT);
```
Using a leading slash (for example `"/Image"`) is also supported - Datasets are considered to be attached to root group.

**Note:** H5Dopen can also accept the root group ID instead of the file ID. This works, but it is an extra step with no benefit because the dataset name is resolved directly from the root namespace, so the file ID is sufficient.

### 4.2 Dataset datatype and read semantics
Each dataset has an HDF5 datatype that corresponds to the CDF variable datatype.

If `H5Dread` is called with a memory datatype different from the dataset datatype, the connector delegates conversion to HDF5’s conversion engine (`H5Tconvert`). Conversion succeeds only when HDF5 supports it; otherwise `H5Dread` fails with the standard HDF5 conversion error.

### 4.3 Selection and data handling

The connector reads the full extent of the variable and returns the values provided by the CDF library.

Standard HDF5 selection mechanisms (e.g., hyperslabs) may be used to read subsets of the data. Data is generated or converted on demand during the read operation. No additional interpretation, reshaping, or reordering is performed.

## 5. Attributes

All CDF global and variable attributes are exposed as HDF5 attributes and can be accessed using standard HDF5 attribute APIs such as `H5Aread`.

### 5.1 Variable Attributes (vAttributes)

CDF variable attributes (`vAttributes`) are exposed as HDF5 attributes attached to the corresponding dataset of the variable.

A single `vAttribute` may contain multiple `zEntries`. Each `zEntry` belongs to exactly one CDF variable. The connector exposes each `zEntry` as a separate HDF5 attribute on that variable’s dataset.

**For Example:** to open attribute `FIELDNAM` on dataset `Image`:
```
dset_id  = H5Dopen2(file_id, "Image", H5P_DEFAULT);
attr_id  = H5Aopen(dset_id, "FIELDNAM", H5P_DEFAULT);
```

`vAttribute` datatype corresponds to the CDF attribute type. 

**Type conversion is minimally supported but not yet tested for any attributes.**  
The corresponding HDF5 datatype and size of the attribute should be found by using `H5Aget_type` and `H5Aget_size` calls.

### 5.2 Global Attributes (gAttributes)

CDF global attributes (`gAttributes`) are exposed as HDF5 attributes attached to the file object (not to datasets). Like `vAttributes`, which store a single entry per variable, a single `gAttribute` can contain multiple `gEntries` (each with a different datatype) for the file as a whole. Because HDF5 attributes do not natively support multiple entries under the same name, the connector provides two access methods:

**1. Open a specific gEntry by index**  
```
attr_id = H5Aopen(file_id, "TITLE_k", H5P_DEFAULT);
```
This opens the `gEntry` with index `k` from `gAttribute` "TITLE", `0` \=\< `k` \<\= `M`, where `M` is the maximum `gEntry` index in the `gAttribute`. The HDF5 attribute datatype will correspond to the CDF datatype of the gEntry at index `k`.

**Note:** We specifically state that `k` must be less than or equal to the max `gEntry` index because `gAttributes` can have sparse `gEntry` lists, meaning that some intermediate indices may not actually exist, so attempting to open a `gEntry` at a non-existent index will result in an error.

**2. Open the entire gAttribute as an array of strings:**  
```
attr_id = H5Aopen(file_id, "UNITS", H5P_DEFAULT);
```
When opened without an index, the attribute is returned as a 1D fixed-length string array, where each element corresponds to one `gEntry`.

All strings in the array have the same fixed length. The string length is chosen to be large enough to hold the string representation of the largest `gEntry` in the attribute. This size is determined by examining every `gEntry` in the attribute and computing the maximum possible string length required to represent its value.

As a result, some strings may contain unused space if their corresponding `gEntry` has a shorter string representation. This behavior is expected and ensures that all `gEntries` can be safely represented without truncation.

### 5.3 Attribute Existence Checking
The `H5Aexists()` function can be used with the CDF VOL connector to check if an attribute with a given name exists on an object.

**Key Points:**
* **Global attributes:** reside on the file or root group. Calling H5Aexists() with the file ID or root group ID will only detect global attribute names.
* **Variable attributes:** are tied to individual variable datasets. Use the dataset ID to check for a specific attribute on that variable.
* The function returns a positive value if the variable exists, zero if it does not, and a negative value in case of errors.

**Example:**
```
htri_t exists = H5Aexists(dset_id, "UNITS");
/* Then check 'exists' to see if that attribute exists on that dataset */
```

### 5.4 Attribute Iteration
HDF5 provides the `H5Aiterate2()` API to walk through attributes attached to a given object. In the context of the CDF VOL connector, this can be used to iterate over **global** or **variable attributes**.

**Key Points:**
* **Global attributes** are attached to the **file** or **root group**. Using `H5Aiterate2()` on the file ID or root group ID will visit each global attribute once.
* **Variable attributes** are attached to individual variable datasets. Use the dataset ID of the variable to iterate over its attributes.
* **Indexed gEntries** (e.g., 'TITLE_0') are not returned during iteration; only the top-level attribute names (gAttribute or vAttribute) are visited.
* Deletion of attributes is **not supported**.

**Example:**
```
hsize_t idx = 0;
H5Aiterate2(file_id, H5_INDEX_NAME, H5_ITER_INC, &idx, attr_iteration_cb, &op_data);
```
* `attr_iteration_cb` is the callback invoked for each attribute.
* `op_data` is a user-defined structure for passing context to the callback.

This method allows the user to discover all *available attributes* without needing to know their names in advance. For indexed access to individual attribute entries, use `H5Aopen()` with the appropriate suffix (e.g., 'TITLE_0').

## 6. Ordering and Consistency Guarantees

The CDF VOL connector maintains **contiguous numbering** and **consistent creation order** for variables and attributes, while respecting the natural sparsity of attribute entries.

* **Variables (zVars/rVars)**  
  Variable indices are always contiguous. When a variable is deleted, all higher-numbered variables are renumbered downward to fill the gap. This guarantees that a variable’s index always reflects its creation order.

* **Variable Attributes (vAttributes) and their entries**  
  Each `vAttribute` entry is associated with a specific variable via the variable’s index. When variable IDs change due to deletion, the corresponding `vAttribute` entries are automatically updated to maintain the correct association. The entries themselves may be sparse; there is no requirement that they be contiguous.

* **Global Attributes (gAttributes) and attribute numbers**  
  The identifiers for both `vAttributes` and `gAttributes` (the attributes themselves, not their entries) are maintained as contiguous sequences. Each attribute retains a creation-order mapping consistent with its attribute number, ensuring predictable access when attributes are inserted or deleted.  

**Key point:** Contiguity and creation-order guarantees apply to the **variables themselves** and the **attributes themselves**, but **not** to the individual entries within each attribute.


## 7. Notes on Performance and Caching
Currently, when a subset of dataset data is requested, the connector reads the full dataset into memory and then extracts the subset intousing HDF5 selection. This will likely be changed in the future to only read the selected hyperslab directly from the CDF file, avoiding unnecessary memory use.

Much of the CDF attribute information is cached upon file opening to simplify attribute access in other parts of the VOL connector. This includes, for each global and variable attribute, its name, CDF attribute number, the maximum entry index, and the number of entries. The file object also stores counts of all attributes, as well as separate counts for global and variable attributes. In addition, file-level properties such as the CDF encoding, majority (row- or column-major ordering), the maximum zVariable record number, and the total number of zVariables are cached for quick reference.
