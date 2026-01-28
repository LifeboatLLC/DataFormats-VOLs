

# GRIB2 VOL Connector HDF5 View: Object Model and Read Semantics

This document defines the stable, read-only HDF5 object layout exposed by the GRIB2 VOL connector and the semantics of H5Dread vs H5Aread. The connector provides message-centric access under `/message_*` paths and supports both “materialized” data products and computed, on-demand views derived from the GRIB grid definition.

## 1. Naming and Scope

A GRIB2 file is presented as a collection of messages, each mapped to a corresponding HDF5 group:

```
/message_<N>/
```

where `<N>` is a 1-based, zero-padded message index (recommended width: 6, e.g., `/message_000001/`).

Unless explicitly noted, all objects are read-only.

---

## 2. Required Datasets

`/message_<N>/data`, `/message_<N>/lon`, `/message_<N>/lat`, and `/message_<N>/values` are the only datasets for message `<N>`. The VOL connector will not find datasets with a different name.
For simplicity, all other informatuion stored in the message is treated as attributes on the `/message_<N>` group.  

### 2.1 `/message_<N>/data` (required name, not implemented yet)

`/message_<N>/data` 

**Purpose**
Provides a row-wise list of `(longitude, latitude, value)` triplets for all grid points in the message.

**Shape**

```
[(numberOfPoints), 3]   (2-D)
```

* Column 0: lon
* Column 1: lat
* Column 2: value

**Datatype**
Floating-point (`H5T_IEEE_F64LE` recommended) for all three columns. The VOL connector will convert to the memory datatype specified in the H5Dread call.

**Semantics**

* Longitude and latitude are in degrees.
* The order of rows follows the ecCodes iterator order for that grid (implementation-defined but stable for a given ecCodes version + grid).
* Missing values follow GRIB semantics:

  * If the GRIB bitmap indicates missing points, the corresponding value entry uses the message’s missing-value convention (recommended: IEEE NaN for the exposed HDF5 view) and the message metadata records the bitmap/missing policy.

**API mapping**

```
H5Dread("/message_<N>/data", …)
```

returns the full triplet table.

Implementation uses ecCodes point iteration (e.g., GRIB iterator) or equivalent “get data” API to obtain lon/lat/value triplets.

### 2.2 `/message_<N>/lon`, `/message_<N>/lat`,  `/message_<N>/values` (required names)

`/message_<N>/lon`,  `/message_<N>/lat`,  `/message_<N>/values`

**Purpose**
Provide one-dimensional arrays of computed values for `longitude`, `latitude ` and corresponding grid values.

**Shape**

```
[numberOfPoints]   (1-D)
```

**Datatype**
Floating-point (`H5T_IEEE_F64LE` recommended). The VOL connector will convert to the memory datatype specified in the H5Dread call.

**Semantics**

* Longitude and latitude are in degrees.
* Missing values follow GRIB semantics:

  * If the GRIB bitmap indicates missing points, the corresponding value entry uses the message’s missing-value convention (recommended: IEEE NaN for the exposed HDF5 view) and the message metadata records the bitmap/missing policy.

**API mapping**

```
H5Dread(file_id, "/message_<N>/<name>", …)
```
or
```
H5Dread(message_N_group_id, "<name>", …)
```
returns the corresponding values for `<name>`, i.e., `lon`, 'lat`, or `values`.

Implementation uses ecCodes point iteration (e.g., GRIB iterator) or equivalent “get data” API to obtain one of the lon/lat/value triplets.
These objects are computed from the GRIB grid definition and values using ecCodes iteration APIs. They are not required to correspond to physically stored latitude/longitude arrays in the GRIB message.

---

## 3. Metadata as Attributes

All scalar and small metadata items are exposed as HDF5 attributes and are read using `H5Aread`.

### 3.1 Attribute placement

Attributes may be attached to:

* `/message_<N>/` (message group), and/or
* `/message_<N>/data` (primary dataset) (not implemnted yet)

**Recommended:** attach domain metadata to `/message_<N>/data` so the dataset is self-describing when copied.

### 3.2 Raw GRIB keys (lossless identifiers)

The connector exposes raw GRIB keys required to interpret and/or reproduce the message, including but not limited to:

* **Identification/time:** `editionNumber`, `centre`, `subCentre`, `dataDate`, `dataTime`, `stepUnits`, `forecastTime`, `typeOfProcessedData`, …
* **Parameter identity:** `discipline`, `parameterCategory`, `parameterNumber`, `productDefinitionTemplateNumber`, …
* **Grid definition:** `gridDefinitionTemplateNumber`, `Ni`, `Nj`, scan flags, `shapeOfTheEarth`, …
* **Packing/missing:** `packingType` (or raw template number), `bitsPerValue`, `bitMapIndicator`, …

These attributes are read via `H5Aread` on `/message_<N>/`.

### 3.3 Derived semantic attributes (“decoded” meanings)

Where ecCodes provides human-readable semantic companions (e.g., table-decoded descriptions), the connector SHOULD expose them as string attributes, for example:

* `shapeOfTheEarthName`
* `centreDescription`
* `parameterName`, `parameterUnits`
* `name`, `shortName`, `cfName`, `typeOfLevel`, `level`

These attributes are derived by ecCodes from raw GRIB codes and tables and are not guaranteed to be physically present in the GRIB bitstream.

---

## 4. Dataset vs Attribute Rule

The connector applies the following classification rule:

* Objects intended for `H5Dread` are triplet table, and lon/lat/value vectors.
* Objects intended for `H5Aread` are scalars, descriptive strings, and other possible arrays (pl, pv, packing tables, bitmap indices, etc.)

Concretely:

* Coordinate/value arrays → `H5Dread`
* GRIB keys and decoded meanings → `H5Aread`

---

## 5. User-facing Access Pattern

Users SHOULD use:

* `H5Dread(…,"/message_<N>/data", …)` to obtain a complete lon/lat/value representation.
* `H5Dread(…"/message_<N>/lon", …)` / `H5Dread(…,"/message_<N>/lat", …)` for coordinate vectors.
* `H5Dread(…"/message_<N>/values", …)` for 'values' vector.
* `H5Aread` on `/message_<N>/` for metadata such as parameter identity, units, level, grid info, and decoded descriptions.

---

## 6. Ordering and Consistency Guarantees

The row order in `/message_<N>/data` and the index order of `/message_<N>/lon`, `/message_<N>/lat`, `/message_<N>/value` are identical for a given message.

---

## 7. Notes on Performance and Caching

Currently, implementation doesn't cache computed `/message_<N>/data`, `/message_<N>/lon`,  `/message_<N>/lat`,  `/message_<N>/values` and computes only required subset of the value using HDF5 selection mechanism.

When only `/message_<N>/lon` is requested, implementations may compute lon-only via iteration to avoid allocating the full Nx3 table (implementation choice).

---
