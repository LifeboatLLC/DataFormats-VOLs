# GRIB2 to HDF5 Mapping Specification

This document outlines the conceptual mapping between the **GRIB2 (Gridded Binary Edition 2)** format and the **HDF5 (Hierarchical Data Format, Version 5)**, often implemented via the **NetCDF-4** data model.

---

## Core Data and Structural Mapping

The key is translating GRIB's record-oriented, compressed format into HDF5's hierarchical, multi-dimensional array structure.

| GRIB2 Component | GRIB Section(s) | HDF5 Mapping | Implementation Details |
| :--- | :--- | :--- | :--- |
| **GRIB Message** (Single Field) | All Sections (0-7) | **HDF5 Dataset** | Multiple GRIB messages for the same variable (different times/levels) are combined into a single, multi-dimensional HDF5 Dataset. |
| **Data Section** (Field Values) | Section 7 | **Main Data Array (Dataset)** | The core multi-dimensional array, often structured as `(time, level, latitude, longitude)`. |
| **Data Representation** | Section 5 (DRS) | **Dataset Properties/Filters** | GRIB packing methods (e.g., Simple Packing, JPEG 2000) are translated into HDF5 **compression filters** (like `zlib` or `gzip`) and **chunking** for I/O optimization. |
| **File Structure** | Sequential Records | **Hierarchical Groups and Datasets** | GRIB's linear file becomes an HDF5 structure where data is organized into logical **Groups** (like folders) containing related **Datasets** (arrays). |

---

## Metadata Mapping (Attributes and Coordinates)

Metadata in GRIB's fixed sections is translated into flexible, self-describing HDF5 **Attributes** and **Coordinate Datasets**.

| GRIB2 Metadata | GRIB Section(s) | HDF5 Mapping | CF/Convention Name |
| :--- | :--- | :--- | :--- |
| **Meteorological Parameter** | Section 4 (PDS) | **Dataset Attributes** | `long_name`, `units`, `standard_name` |
| **Time/Forecast Info** | Section 4 (PDS) | **Dataset Attributes** / **Coordinate Dataset** | `valid_time`, `forecast_time`, `time` (Coordinate) |
| **Vertical Level Info** | Section 4 (PDS) | **Dataset Attributes** / **Coordinate Dataset** | `level_type`, `level` (Coordinate) |
| **Grid Definition** | Section 3 (GDS) | **Coordinate Datasets** | `latitude`, `longitude` (Coordinate Variables) |
| **Projection Parameters** | Section 3 (GDS) | **Dataset Attributes** | `grid_mapping` (Attributes describing the projection, e.g., Lambert Conformal parameters) |
| **Originating Center/Source**| Sections 1 (IDS) | **File/Group Attributes** | `institution`, `source`, `Conventions` |

---

## Key Conversion Principle

The goal of conversion is to move from a highly **bit-efficient, sequential** format (GRIB2) to a **self-describing, randomly accessible** format (HDF5/NetCDF-4), maximizing interoperability with scientific analysis tools.
