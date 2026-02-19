/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by Lifeboat, LLC                                                *
 * All rights reserved.                                                      *
 *                                                                           *
 * The full copyright notice, including terms governing use, modification,   *
 * and redistribution, is contained in the COPYING file, which can be found  *
 * at the root of the source code distribution tree.                         *
 * If you do not have access to either file, you may request a copy from     *
 * help@lifeboat.llc                                                         *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* Purpose:     HDF5 Virtual Object Layer (VOL) connector for GRIB2 files
 *              Enables reading GRIB2 files through HDF5 tools
 */

#ifndef _grib2_vol_connector_H
#define _grib2_vol_connector_H

#include "grib2_vol_err.h" /* Error reporting macros */
#include <hdf5.h>
#include <eccodes.h>
#include <stdint.h>

/* The value must be between 256 and 65535 (inclusive) */
#define GRIB2_VOL_CONNECTOR_VALUE ((H5VL_class_value_t) 12211)
#define GRIB2_VOL_CONNECTOR_NAME "grib2_vol_connector"

/* GRIB2 VOL file object structure */
typedef struct grib2_file_t {
    FILE *grib2;         /* GRIB2 file handle - shared across all groups and datasets */
    char *filename;     /* File name */
    unsigned int flags; /* File access flags */
    hid_t plist_id;     /* Property list ID */

    /* Message index (offsets only) */
    off_t   *msg_offsets;   /* byte offsets of each GRIB2 message start */
    size_t  nmsgs;         /* number of messages */
} grib2_file_t;

/* Forward declaration for unified object type */
typedef struct grib2_object_t grib2_object_t;

/* GRIB2 handle for a specified message */ 
typedef struct grib2_message_t {
    codes_handle *h;
} grib2_message_t;

/* GRIB2 VOL group object structure */
typedef struct grib2_group_t {
    char *name;              /* Group name */
    grib2_message_t *msg;    /* GRIB2 message handle */
    size_t msg_num;          /* GRIB2 message  number*/  
    size_t num_attrs;        /* The number of non-grid keys in the GRIB2 message */
    size_t num_grids;        /* The number of grid keys in the GRIB2 message */
} grib2_group_t;

/* GRIB2 VOL dataset object structure */
typedef struct grib2_dataset_t {
    char *name;              /* Dataset (key) name */
    grib2_message_t *msg;     /* GRIB2 message handle */
    int   codes_type;        /* ecCodes datatype */
    hid_t type_id;           /* HDF5 datatype */
    hid_t space_id;          /* HDF5 dataspace */
    void *data;              /* Cached data for the key */
    size_t data_size;        /* Data size in bytes */
    size_t nvals;            /* Number of values (key replication is a message) */  
    bool is_vlen_string;     /* True if type_id/data use HDF5 VL-string semantics */
} grib2_dataset_t;

/* GRIB2 VOL attribute object structure */
typedef struct grib2_attr_t {
    void *parent;            /* Parent object (currently only group) */
    char *name;              /* Attribute (key) name */
    grib2_message_t *msg;     /* GRIB2 message handle */
    int   codes_type;        /* ecCodes datatype */
    hid_t type_id;           /* HDF5 datatype */
    hid_t space_id;          /* HDF5 dataspace */
    void *data;              /* Cached data for the key */
    size_t data_size;        /* Data size in bytes */
    size_t nvals;            /* Number of values - should be 1 for an attribute */
    bool is_vlen_string;     /* True if type_id/data use HDF5 VL-string semantics */
} grib2_attr_t;

/* Unified GRIB2 VOL object structure */
struct grib2_object_t {
    grib2_object_t *parent_file;    /* Parent file (never NULL after open) */
    H5I_type_t obj_type;           /* HDF5 object type identifier */
    size_t ref_count;              /* Reference count for child objects */
    union {
        grib2_file_t file;
        grib2_group_t group;
        grib2_dataset_t dataset;
        grib2_attr_t attr;
    } u;
};

/* Function prototypes (HDF5 develop expects hid_t vipl_id) */
herr_t grib2_init_connector(hid_t vipl_id);
herr_t grib2_term_connector(void);

/* File operations */
void *grib2_file_open(const char *name, unsigned flags, hid_t fapl_id, hid_t dxpl_id, void **req);
herr_t grib2_file_get(void *file, H5VL_file_get_args_t *args, hid_t dxpl_id, void **req);
herr_t grib2_file_close(void *file, hid_t dxpl_id, void **req);

/* Dataset operations */
void *grib2_dataset_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                           hid_t dapl_id, hid_t dxpl_id, void **req);
herr_t grib2_dataset_read(size_t count, void *dset[], hid_t mem_type_id[], hid_t mem_space_id[],
                            hid_t file_space_id[], hid_t dxpl_id, void *buf[], void **req);
herr_t grib2_dataset_get(void *dset, H5VL_dataset_get_args_t *args, hid_t dxpl_id, void **req);
herr_t grib2_dataset_close(void *dset, hid_t dxpl_id, void **req);

/* Group operations */
void *grib2_group_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                         hid_t gapl_id, hid_t dxpl_id, void **req);
herr_t grib2_group_get(void *obj, H5VL_group_get_args_t *args, hid_t dxpl_id, void **req);
herr_t grib2_group_close(void *grp, hid_t dxpl_id, void **req);

/* Attribute operations */
void *grib2_attr_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                        hid_t aapl_id, hid_t dxpl_id, void **req);
herr_t grib2_attr_read(void *attr, hid_t mem_type_id, void *buf, hid_t dxpl_id, void **req);
herr_t grib2_attr_get(void *obj, H5VL_attr_get_args_t *args, hid_t dxpl_id, void **req);
herr_t grib2_attr_specific(void *obj, const H5VL_loc_params_t *loc_params,
                             H5VL_attr_specific_args_t *args, hid_t dxpl_id, void **req);
herr_t grib2_attr_close(void *attr, hid_t dxpl_id, void **req);

/* Link operations */
herr_t grib2_link_specific(void *obj, const H5VL_loc_params_t *loc_params,
                             H5VL_link_specific_args_t *args, hid_t dxpl_id, void **req);

herr_t grib2_introspect_opt_query(void *obj, H5VL_subclass_t subcls, int opt_type,
                                    uint64_t *flags);

herr_t grib2_introspect_get_conn_cls(void __attribute__((unused)) * obj,
                                       H5VL_get_conn_lvl_t __attribute__((unused)) lvl,
                                       const H5VL_class_t __attribute__((unused)) * *conn_cls);
#endif /* _grib2_vol_connector_H */
