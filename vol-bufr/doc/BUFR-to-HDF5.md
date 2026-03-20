
# BUFR to HDF5 Mapping Specification 

This document gives an overview of conceptual mapping between the **BUFR (Binary Universal Form for the Representation of meteorological data)** format and the **HDF5 (Hierarchical Data Format, Version 5)** and is subject to change. In the future it will be used to covert BUFR files to HDF5 format.

## Structural and Data Mapping

BUFR's sequence-oriented, table-driven structure is translated into HDF5's flexible, array-based hierarchy.

| BUFR Component | Description | HDF5 Mapping | Implementation Details |
| :--- | :--- | :--- | :--- |
| **BUFR Message** | A single observational report (e.g., one SYNOP, one sounding). | **HDF5 Group** or **Root Level** | Acts as the container for all decoded data elements and metadata associated with that report or collection of reports. |
| **Data Element** | A single measured or derived value (e.g., Air Temperature). | **HDF5 Dataset** | Each decoded variable becomes a named **Dataset** within the Group, typically a 1D or 2D array. |
| **Replication** | The mechanism for encoding repeating data sequences (e.g., multiple atmospheric levels). | **Dataset Dimension** | The repeating sequence defines a **dimension** of the HDF5 Dataset (e.g., the "level" dimension for sounding data). |
| **Quality Mark** | A flag indicating the status or quality of a data element. | **Auxiliary Dataset/Attribute** | Stored as a parallel **Dataset** (e.g., `variable_qc`) or a dedicated **Attribute** on the main data variable. |
| **BUFR Subset** | A logical grouping of data within a message. | **HDF5 Group** or **Dataset Dimension** | Can define a nested Group or a specific coordinate dimension (e.g., "station"). |

---

## Metadata Mapping (Attributes)

BUFR's table-driven metadata is translated into self-describing HDF5 attributes, improving discoverability and usability.

| BUFR Metadata | BUFR Origin | HDF5 Mapping | CF/Convention Name |
| :--- | :--- | :--- | :--- |
| **Data Element Name & Units** | BUFR Tables B and D | **Dataset Attributes** | `long_name`, `units`, `standard_name` |
| **Descriptor F-X-Y** | The encoding code for the data element. | **Dataset Attribute** | `BUFR_descriptor` (for traceability) |
| **Originating Center/Source** | BUFR Section 1 | **File/Group Attributes** | `institution`, `source`, `Conventions` (stored at the top level). |
| **BUFR Table Version** | BUFR Section 1 | **File/Group Attributes** | `BUFR_master_table_version`, `local_table_version` |

---

## Example: Sounding Report Structure

For a single HDF5 file containing multiple BUFR sounding reports, the structure aligns data along common dimensions:

| HDF5 Object | Example Name | Dimension(s) | BUFR Mapping Rationale |
| :--- | :--- | :--- | :--- |
| **Dimension Coordinate** | `station_id` | (N\_station) | Maps fixed station information to a primary identifier. |
| **Dimension Coordinate** | `level` | (N\_level) | Maps the replication count to a defined vertical level index. |
| **Data Variable (Fixed)** | `latitude` | (N\_station) | Fixed data indexed only by station. |
| **Data Variable (Replicated)** | `air_temperature`| (N\_station, N\_level) | Replicated data indexed by both station and level. |





