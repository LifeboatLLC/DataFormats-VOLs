# CDF to HDF5 Mapping 

This document specifies a practical, loss-minimizing mapping from **NASA CDF (Common Data Format)** objects to **HDF5** objects. It targets implementers of converters and readers who want predictable, interoperable HDF5 outputs that preserve CDF semantics.

> Scope: This is a *logical* mapping independent of any specific C/Fortran library. Optional sections document recommended conventions for portability (chunking, filters, dimension scales, and attribute encoding).

---

## 1. Terminology

* **CDF file**: a single CDF container with attributes, rVariables, zVariables, and metadata (e.g., majority, encoding).
* **rVariable**: variable sharing the CDF’s global r-dimensions (`rDimSizes`). Last dimension is the **record** dimension (potentially unlimited).
* **zVariable**: variable with its own per-variable dimension list (may have a record dimension).
* **Record variance**: whether a variable varies across records; if not, pad value applies.
* **Dim variance**: whether a variable varies along a logical dimension.
* **Pad value**: scalar used for unwritten elements.
* **Entry**: numbered value within a CDF attribute (global or variable-scoped).
* **Majority**: *ROW_MAJOR* or *COLUMN_MAJOR* logical element ordering.

---

## 2. HDF5 Constructs Used

* **File** → HDF5 file
* **Group** → used for organization (/), `/CDF`, `/CDF/Variables`, `/CDF/Attributes`, etc.
* **Dataset** → rVars/zVars (N-D datasets), attribute backing stores (when needed)
* **Datatype** → native integers/floats, compound types for complex CDF types
* **Dataspace** → use of unlimited (extendible) dimensions for record axes
* **Attributes** → attached to groups/datasets for metadata
* **Dimension Scales** → optional: label dimensions names/units and bind axes
* **Filters** → optional compression (gzip) and chunking

---

## 3. Top-Level Layout (Recommended)

```
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

**Alternative minimal layout:** place `Variables` directly under root and encode everything with object attributes. Both styles are equivalent; the namespacing above helps multi-product repositories.

---

## 4. Dataset Shapes and Record Axis

### 4.1 rVariables

* **CDF:** share `rDimSizes = [D0, D1, ..., Dn-1]`; may have a *record* axis.
* **HDF5:** map to an N-D dataset with the **record axis as the first dimension**; make it **unlimited** if record-varying.

**Shape rule:**

* If record-varying: `shape = [UNLIMITED, D0, D1, ..., Dn-1]`
* If not record-varying: `shape = [1, D0, D1, ..., Dn-1]` (still keep a size-1 leading axis for uniformity) or drop the record axis if desired (note this loses the distinction).

### 4.2 zVariables

* **CDF:** each zVar has its own dim list, optionally with a record axis.
* **HDF5:** same rule as rVars: put record axis first and mark it **UNLIMITED** when varying.

> **Note:** Using the first dimension as the record axis aligns with C row-major storage and typical time-leading conventions. Store an attribute to identify which axis is the CDF record dimension if not first.

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

* **Preferred:** encode each CDF global attribute as a single HDF5 attribute on the **root group** (or `/CDF/Metadata`).
* If the CDF attribute has **multiple entries** (by entry number), store:

  * **Option A (compact):** an **array-valued** HDF5 attribute where index = entry number; use fill value/sentinel for missing.
  * **Option B (explicit):** create subgroup `/CDF/Attributes/Global/<attrName>` and store a 1-D dataset `Entries` with explicit `(entry, value)` pairs (compound type). This preserves sparsity.

### 6.2 Variable Attributes → HDF5

* Attach HDF5 attributes directly to each **variable dataset**.
* For **multi-entry** attributes:

  * Option A: a 1-D attribute with size = max entry + 1.
  * Option B: subgroup `/CDF/Attributes/Variable/<varName>/<attrName>` with a 1-D dataset `Entries` (compound) listing only present entries.

> **Recommendation:** Prefer Option A for dense/packed entries, Option B for sparse, very large or string-heavy attributes.

---

## 7. Variance and Pad Values

* **Record variance** and **dim variance** flags: store as small scalar attributes on each dataset:

  * `cdf_record_variance` (BOOL)
  * `cdf_dim_variance` (1-D BOOL array, rank = ndims without record axis)
* **Pad value:** use the **dataset creation property** `H5Pset_fill_value` and set the **allocation time** as early/never as needed. Also write the **original CDF pad** to an attribute `cdf_pad_value` for explicit provenance.

---

## 8. Majority (Row/Column)

* Store the original CDF **majority** as a string attribute, e.g., `cdf_majority = "ROW_MAJOR" | "COLUMN_MAJOR"` on `/CDF/Metadata`.
* HDF5 uses row-major indexing; do **not** transpose data by default. Document if your converter transposes when majority is COLUMN_MAJOR.

---

## 9. Compression, Chunking, and Performance

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

* Represent as **compound** of two float64 fields `{t0, dt}` or `{real, imag}` depending on usage. Add attribute `cdf_epoch16_semantics`.

### TT2000

* Map to `H5T_STD_I64LE` with attribute `time_unit = "ns since 2000-01-01T00:00:00"`.

> Consider adopting **CF/UDUNITS**-style `units` attributes for consumer compatibility.

---

## 11. Dimension Names and Scales (Optional but Recommended)

* Create 1-D datasets under `/CDF/Dimensions/<dimname>` to hold coordinate vectors if present (e.g., time values, latitude, energy bins).
* Use **HDF5 Dimension Scales** to attach these to variable datasets. Add human-readable attributes like `long_name`, `units`.

---

## 12. Provenance and Compatibility Attributes

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
```

**Writes (append a record):** extend dataset along axis 0 by `+1` and write a hyperslab at `[new_record, :, :]`.

---

## 14. Attribute Entry Numbering – Encoding Patterns

When preserving **CDF entry numbers** exactly:

**Dense (small N):** store an attribute as a **1-D array** and document `entry_indexing = "cdf_entry_number"`.

**Sparse (large N):** create a subgroup and a 1-D **compound** dataset with fields:

```
{ int32 entry; <H5-type value>; }
```

This allows arbitrary missing entries and multiple types per attribute name when required.

---

## 15. Error Handling & Edge Cases

* **Non-varying variables**: Write as full-size datasets; optionally elide the record axis. Keep `cdf_record_variance=0`.
* **Missing pad values**: Use HDF5 fill values only if present in CDF; otherwise leave unset.
* **Strings**: Prefer variable-length HDF5 strings; if fixed, set explicit encoding (`ASCII` vs `UTF-8`).
* **Column-major sources**: If you transpose, set `transposed_from_column_major = TRUE` and record original shape/order.

---

## 16. Conformance Checklist (TL;DR)

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

* Group names and dataset names mirror CDF var/attr names; sanitize to POSIX HDF5 link names (replace spaces with `_`, avoid `/`, leading `.`).
* Time variables include `units` and `calendar` (e.g., `proleptic_gregorian`) when compatible with downstream tools.

---

**Contact / Issues**: Please open an issue in the converter repository with a sample CDF demonstrating any mapping ambiguity.
