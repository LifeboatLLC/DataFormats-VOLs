# CDF to HDF5 Mapping 

**NOTE**: This document is outdated and much of the information contained is incorrect. It will be updated in time.

This document specifies a practical, loss-minimizing mapping from **NASA CDF (Common Data Format)** objects to **HDF5** objects. It targets implementers of converters and readers who want predictable, interoperable HDF5 outputs that preserve CDF semantics.

> Scope: This is a *logical* mapping independent of any specific C/Fortran library. Optional sections document recommended conventions for portability (chunking, filters, dimension scales, and attribute encoding).

---

## 1. Terminology

* **CDF file**: a single container storing variables (rVariables and zVariables), file metadata (e.g., majority, encoding), and attributes at both the global level (attached to the file) and variable level (attached to specific variables).
* **rVariable**: variable that shares the global record dimensions of the CDF; all rVariables have the same number of dimensions and dimension sizes, and each record has the same shape. rVariables are logically converted to zVariables at file open in the VOL connector, so they are not supported in HDF5.
* **zVariable**: variable with independent per-variable dimensions; each zVariable defines its own shape and record storage, independent of the global record structure. 
* **Record variance**: whether a variable varies across records; if not, pad value applies.
* **Dim variance**: indicates whether elements within a single record vary along a given logical dimension (given the record is not a scalar).
* **Pad value**: a scalar used to fill unwritten elements; it must have the same data type as the variable.
* **Entry**: an indexed element within a CDF attribute, either at the global level or scoped to a particular variable.
* **Majority**: the logical element ordering of record-varying variables, either *ROW_MAJOR* or *COLUMN_MAJOR*, which affects the layout in memory.

---

## 2. HDF5 Constructs Used

* **File** → HDF5 file
* **Group** → only the root group (`/`) exists; datasets and global attributes are attached to this group.
* **Dataset** → rVariables and zVariables are represented as HDF5 datasets; rVariables are internally converted to zVariables at file open, so all datasets appear as zVariables with appropriate dimensions. 
* **Datatype** → signed/unsigned integers of 8, 16, 32, 64 bits; 32- and 64-bit floating point; high-precision timestamps (16-byte arrays of doubles); character types are converted to HDF5 string types.
* **Dataspace** → datasets have fixed dimensions corresponding to each variable's shape; record axes are represented explicitly but are not extendible in read-only mode.
* **Attributes** → metadata attached to datasets and the file/root group; includes global and variable-specific attributes.
* **Dimension Scales** → optional: label dimensions names/units and bind axes (NOTE: Not sure that this point should be here. Nothing like this is implemented in the VOL connector - Cody)
* **Filters** → optional compression (gzip) and chunking; not currently implemented in this VOL connector.

---

## 3. Top-Level Layout (Recommended)

```
/
├─ <rvar_or_zvar_name> : dataset       # Variable datasets (all rVars appear as zVars)
│   └─ (variable attribute entries)    # Attributes for this variable
└─ (file-level attributes)             # Global CDF attributes attached to root group
```

  <!-- Since this is the "Recommended" layout, I wasnt sure if I should completely remove and replace it -Cody -->
  <!-- ```
  /
    ├─ CDF/                         # Namespace for CDF content
    │   ├─ Metadata/                # File-wide metadata & options
    │   │   ├─ (attrs)              # majority, encoding, version, etc.
    │   │   └─ AttrIndex (optional) # helper to locate numbered entries
    │   ├─ Dimensions/              # Optional named dimension scales
    │   │   └─ <dimname> : dataset  # 1-D scale; size matches bound axis
    │   └─ Variables/
    │       ├─ rVars/
    │       │   └─ <rvar_name> : dataset (+attrs)
    │       └─ zVars/
    │           └─ <zvar_name> : dataset (+attrs)
    └─ (file-level attributes)      # Global CDF attributes (flattened)
  ```

  **Alternative minimal layout:** place `Variables` directly under root and encode everything with object attributes. Both styles are equivalent; the namespacing above helps multi-product repositories. -->
---

## 4. Dataset Shapes and Record Axis
Variance Overview:
 * **Record variance**: whether a variable’s values differ between records. If a variable is record-invariant (NOVARY), its values do not change across records, and only the first record is physically stored in the CDF regardless of the number of the variable's reported number of records.
 * **Dimension variance:** whether a variable’s values differ along each logical dimension. Each dimension can be varying (VARY) or non-varying (NOVARY). Non-varying dimensions occupy storage only once for a record, regardless of the dimension size.
Each Variable may have record variance and dimension variance independently. The library tracks this variance but does not impose constraints on the actual values — it only determines how data is stored and logically interpreted.
### 4.1 rVariables
<!-- I'm gonna leave the old explanation here in case we want to keep it -Cody -->
<!-- * **CDF:** share `rDimSizes = [D0, D1, ..., Dn-1]`; may have a *record* axis.
* **HDF5:** map to an N-D dataset with the **record axis as the first dimension**; make it **unlimited** if record-varying.

**Shape rule:**

* If record-varying: `shape = [UNLIMITED, D0, D1, ..., Dn-1]`
* If not record-varying: `shape = [1, D0, D1, ..., Dn-1]` (still keep a size-1 leading axis for uniformity) or drop the record axis if desired (note this loses the distinction).

### 4.2 zVariables

* **CDF:** each zVar has its own dim list, optionally with a record axis.
* **HDF5:** same rule as rVars: put record axis first and mark it **UNLIMITED** when varying.

> **Note:** Using the first dimension as the record axis aligns with C row-major storage and typical time-leading conventions. Store an attribute to identify which axis is the CDF record dimension if not first. -->

<!-- But this explanation is more accurate -Cody -->
* **CDF:** rVariables are variables that share the global record dimensions defined in the file. All rVariables have the same number of dimensions and the same size for each dimension.
* **HDF5:** read as zVariable and handled the same way as zVariables.

### 4.2 zVariables
* **CDF:** Each zVariable independently defines its number of dimensions and the size of each dimension. Like rVariables, zVariables can have record variance and dimension variance
* **HDF5:** Map to an N-D dataset with the **record axis as the first dimension**; make it **unlimited** if record-varying later when VOL connector supports write/extend.

**Shape rule**
| Property	         | CDF	                       | HDF5                                                                                          |
|--------------------|-----------------------------|-----------------------------------------------------------------------------------------------|
| Record axis        | N/A                         | Always included as the first dimension; full length of the variable’s records                 |
| Dimension variance | NOVARY / VARY per dimension | All dimensions included; NOVARY dimensions are stored fully but logically constant per record |
| Full dataset shape | [D0, D1, ..., Dn-1]         | [N_records, D0, D1, ..., Dn-1]                                                                |
---

## 5. Datatypes

Map CDF numeric/logical types to HDF5 native types of equal or greater precision.

| CDF Type      | HDF5 Datatype                             | Notes                      |
| ------------- | ----------------------------------------- | -------------------------- |
| CDF_BYTE      | H5T_STD_I8LE                              | or BE to match policy      |
| INT1/UINT1    | H5T_STD_I8LE / H5T_STD_U8LE               |                            |
| INT2/UINT2    | H5T_STD_I16LE / H5T_STD_U16LE             |                            |
| INT4/UINT4    | H5T_STD_I32LE / H5T_STD_U32LE             |                            |
| INT8/UINT8    | H5T_STD_I64LE / H5T_STD_U64LE             |                            |
| FLOAT/REAL4   | H5T_IEEE_F32LE                            |                            |
| DOUBLE/REAL8  | H5T_IEEE_F64LE                            |                            |
| CDF_EPOCH     | **compound** of 1× float64 or int64 epoch | See §10                    |
| CDF_EPOCH16   | **compound** of 2× float64                | t0 + dt, or reals          |
| TT2000        | H5T_STD_I64LE                             | ns since 2000-01-01        |
| CDF_CHAR/UCHAR| H5T_STRING (variable-length)              | set charset to ASCII/UTF-8 |


> Choose endianness consistently; LE is common. If preserving original endianness is a requirement, set per-dataset types accordingly.

---

## 6. Attributes (Global and Variable-Scoped)

CDF attributes can have **multiple numbered entries**; some are **GLOBAL** (apply to the file), others are **VARIABLE**-scoped (per variable).

### 6.1 Global Attributes → HDF5

<!-- Leaving the "Preferred" encoding in the comments in case we still need it -Cody -->
<!-- * **Preferred:** encode each CDF global attribute as a single HDF5 attribute on the **root group** (or `/CDF/Metadata`).
* If the CDF attribute has **multiple entries** (by entry number), store:

  * **Option A (compact):** an **array-valued** HDF5 attribute where index = entry number; use fill value/sentinel for missing.
  * **Option B (explicit):** create subgroup `/CDF/Attributes/Global/<attrName>` and store a 1-D dataset `Entries` with explicit `(entry, value)` pairs (compound type). This preserves sparsity. -->

<!-- This is how it is currently -->
* Encode each CDF global attribute (gAttribute) as a single 1D array of stringified gEntries (gEntries can have differing datatypes within a gAttribute)
* Allow user to read singular gEntry inside a gAttribute by specifying an index in the attribute name within the H5Aopen() call using '_#' suffix attached to gAttribute name. Returned attribute will have the same datatype as the original gEntry (converted to HDF5 datatype).
  * **Example:** to get gEntry 3 from gAttribute "UNITS", use attribute name "UNITS_3".
  
### 6.2 Variable Attributes → HDF5

* Variable Attributes (vAttributes) are stored globally. Each vAttribute has per-variable entries. A single variable may have at most one entry from any given vAttribute, but can have entries from multiple different vAttributes.
* Since a variable can have at most one entry per vAttribute, store each entry as a separate HDF5 attribute attached directly to the corresponding variable dataset.

---

## 7. Variance and Pad Values 
<!-- We dont do anything like what is described here. All records and dimensions are all phyiscally read into memory regardless of variance when the user reads the data.
We also do nothing with the pad value -->
* **Record variance** and **dim variance** flags: store as small scalar attributes on each dataset:
  * `cdf_record_variance` (BOOL)
  * `cdf_dim_variance` (1-D BOOL array, rank = ndims without record axis)
* **Pad value:** use the **dataset creation property** `H5Pset_fill_value` and set the **allocation time** as early/never as needed. Also write the **original CDF pad** to an attribute `cdf_pad_value` for explicit provenance.

---

## 8. Majority (Row/Column)
<!-- Currently we only cache the majority in the cdf_file_t object. Nothing about it is written to any attributes yet. No transposing is done. -->
* Store the original CDF **majority** as a string attribute, e.g., `cdf_majority = "ROW_MAJOR" | "COLUMN_MAJOR"` on `/CDF/Metadata`.
* HDF5 uses row-major indexing; do **not** transpose data by default. Document if your converter transposes when majority is COLUMN_MAJOR.

---

## 9. Compression, Chunking, and Performance
<!-- Currently we don't do anything described here either. CDF uncompresses the data for us when we read it into a buffer. No compression info is stored anywhere in the VOL -Cody -->
* Use **chunked layout** for any dataset with an **unlimited** record axis.
* Choose chunk sizes that tile one or a few records (e.g., `[Rchunk, dim1, dim2, ...]`).
* Compression: gzip level 4–6 is a good default; honor source CDF compression as a metadata attribute (`cdf_source_compressed = TRUE/FALSE`, `cdf_source_codec = "GZIP"|…`).
* Enable **shuffle** and **fletcher32** filters if desired for better compression and integrity checks.

---

## 10. Time Types

### CDF_EPOCH

* Commonly represented as milliseconds since 0000-01-01 or as a float64 real time. Choose one canonical representation and document:

  * **Option A:** `H5T_STD_I64LE` milliseconds since 0000-01-01, attribute `time_unit = "ms since 0000-01-01"`.
  * **Option B:** `H5T_IEEE_F64LE` seconds since a reference epoch, attribute `time_unit` accordingly.

### CDF_EPOCH16

* Represent as **compound** of two float64 fields `{t0, t1}`. In future: use CDF to allow decoding EPOCH16 data into string timestamp format when string conversion requested.
<!-- * Represent as **compound** of two float64 fields `{t0, dt}` or `{real, imag}` depending on usage. Add attribute `cdf_epoch16_semantics`. -->

### TT2000
<!-- Currently, we just map it to a H5T_NATIVE_INT64, and store no attributes -Cody -->
* Map to `H5T_STD_I64LE` with attribute `time_unit = "ns since 2000-01-01T00:00:00"`.

> Consider adopting **CF/UDUNITS**-style `units` attributes for consumer compatibility.

---

## 11. Dimension Names and Scales (Optional but Recommended)
<!-- We dont do anything like this currently. -Cody -->
* Create 1-D datasets under `/CDF/Dimensions/<dimname>` to hold coordinate vectors if present (e.g., time values, latitude, energy bins).
* Use **HDF5 Dimension Scales** to attach these to variable datasets. Add human-readable attributes like `long_name`, `units`.

---

## 12. Provenance and Compatibility Attributes
<!-- We dont do any of this currently -Cody -->
Attach to `/CDF/Metadata`:

* `source_format = "CDF"`
* `source_library_version = "<CDF lib version if known>"`
* `converter = "<tool-name>"`
* `converter_version = "<semver>"`
* `conversion_time = ISO-8601` (UTC)
* `cdf_majority`, `cdf_encoding`, `cdf_version`

Attach to each dataset:

* `cdf_var_class = "rVariable"|"zVariable"`
* `cdf_dims = [dim sizes w/o record axis]`
* `cdf_has_record_axis = TRUE/FALSE`
* `cdf_record_axis_index = 0` (if present)
* `cdf_pad_value` (scalar)
* `cdf_dim_variance` (BOOL array)

---

## 13. Example

Assume a CDF with `rDimSizes = [3, 4]`, record-varying, and one zVar `Z(time, level)`.

**Variables:**

* `B` (rVar): shape `[record, 3, 4]`, float32, pad = 0.0
* `Z` (zVar): dims `[record, level]` where `level = 10`

**HDF5 layout (tree):**

```
/
  ├─ B (zVariable dataset float32, chunks [32,3,4])
  ├─ V (zVariable dataset float32, chunks [64,10])
  └─ (root attributes)
      ├─ title = "..."
      └─ [global CDF attributes]
```

<!--  We dont create any groups for CDF currently. Everything is attached to root group -Cody
```
/
  ├─ CDF/
  │   ├─ Metadata (attrs)
  │   └─ Variables/
  │       ├─ rVars/
  │       │   └─ B   (dataset float32, chunks [32,3,4])
  │       └─ zVars/
  │           └─ Z   (dataset float32, chunks [64,10])
  └─ (root attributes)
      ├─ title = "..."
      └─ [global CDF attributes]
```

**Dataset attributes (B):**

```
: cdf_var_class = "rVariable"
: cdf_has_record_axis = 1
: cdf_record_axis_index = 0
: cdf_dim_variance = [1,1]       # varies along both dims
: cdf_pad_value = 0.0
``` -->

**Writes (append a record):** extend dataset along axis 0 by `+1` and write a hyperslab at `[new_record, :, :]`.
<!-- This will probably be correct when we eventually do add ability to write/extend CDF, but right now VOL connector is read only -Cody -->
---

## 14. Attribute Entry Numbering – Encoding Patterns

<!-- 
When preserving **CDF entry numbers** exactly:

**Dense (small N):** store an attribute as a **1-D array** and document `entry_indexing = "cdf_entry_number"`.

**Sparse (large N):** create a subgroup and a 1-D **compound** dataset with fields:

```
{ int32 entry; <H5-type value>; }
```

This allows arbitrary missing entries and multiple types per attribute name when required. -->

* **Global Attributes (gAttributes)**
  * **Indexed gEntry:** If a specific entry number is provided, return only that gEntry as raw data with its CDF datatype converted to HDF5.
    * **Example:** `"UNITS_2"` → returns the gEntry with index #2 of the gAttribute `"UNITS"`.
  * **All gEntries:** If no entry number is specified, return all gEntries as a 1-D array of strings. Each string encodes the entry number, CDF datatype, number of elements, and value in the format:  
    ```
    <entry_index> (<CDF_type>/<num_elements>): <value>
    ```
    * **Example for gAttribute `"TITLE"`:**
      ```
      0 (CDF_CHAR/9): "CDF title"
      1 (CDF_CHAR/11): "Author: CDF"
      ```
* **Variable Attributes (vAttributes)**
  * Always return the single entry corresponding to the variable, as raw data with its CDF datatype converted to HDF5.
  * Multiple entries per vAttribute are not allowed; each variable can have at most one entry from a given vAttribute.
  * The Variable's number is the same as the corresponding entry's number, so preserving CDF entry number is trivial.

This encoding pattern allows arbitrary missing entries.

  

---

## 15. Error Handling & Edge Cases
<!-- None of this is currently done in the VOL connector. It should be easy to set encoding to UTF-8 for string types though -Cody  -->
* **Non-varying variables**: Write as full-size datasets; optionally elide the record axis. Keep `cdf_record_variance=0`.
* **Missing pad values**: Use HDF5 fill values only if present in CDF; otherwise leave unset.
* **Strings**: Prefer variable-length HDF5 strings; if fixed, set explicit encoding (`ASCII` vs `UTF-8`).
* **Column-major sources**: If you transpose, set `transposed_from_column_major = TRUE` and record original shape/order.

---

## 16. Conformance Checklist (TL;DR)
<!-- This entire thing should be changed -Cody -->

* [ ] rVars/zVars → HDF5 datasets, record axis first, unlimited when varying
* [ ] Datatypes mapped per §5 (time types per §10)
* [ ] Global attrs → root (or `/CDF/Metadata`); variable attrs → datasets
* [ ] Multi-entry attrs encoded (dense array or sparse compound)
* [ ] Pad values → HDF5 fill value + attribute copy
* [ ] Majority stored, no implicit transpose (document if done)
* [ ] Chunking + compression set; filters documented
* [ ] Optional: dimension scales created and attached
* [ ] Provenance attributes recorded

---

## 17. Appendix: Minimal Naming Conventions
<!-- We don't really do any of this either. CDF should do its own name sanitization though -Cody -->
* Group names and dataset names mirror CDF var/attr names; sanitize to POSIX HDF5 link names (replace spaces with `_`, avoid `/`, leading `.`).
* Time variables include `units` and `calendar` (e.g., `proleptic_gregorian`) when compatible with downstream tools.

---

**Contact / Issues**: Please open an issue in the converter repository with a sample CDF demonstrating any mapping ambiguity.
