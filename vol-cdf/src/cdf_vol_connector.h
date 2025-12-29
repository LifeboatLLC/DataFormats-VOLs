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

/* Purpose:     HDF5 Virtual Object Layer (VOL) connector for CDF files
 *              Enables reading CDF files through HDF5 tools and netCDF-C
 */

#ifndef _cdf_vol_connector_H
#define _cdf_vol_connector_H

#include "cdf_vol_err.h" /* Error reporting macros */
#include <hdf5.h>
#include <cdf.h>
#include <stdint.h>

/* The value must be between 256 and 65535 (inclusive) */
#define CDF_VOL_CONNECTOR_VALUE ((H5VL_class_value_t) 12204)
#define CDF_VOL_CONNECTOR_NAME "cdf_vol_connector"

/* CDF VOL file object structure */
typedef struct cdf_file_t {
    CDFid id;           /* CDF file ID */
    char *filename;     /* File name */
    unsigned int flags; /* File access flags */
    hid_t plist_id;     /* Property list ID */
} cdf_file_t;

struct cdf_dataset_t {
    char *name;          /* Dataset name */
    long var_num;        /* CDF variable number */
    long num_records;    /* Number of records in the variable */
    long num_elements;   /* Number of elements in a record */
    long num_dims;       /* Number of dimensions */
    long dim_sizes[CDF_MAX_DIMS]; /* Sizes of each dimension */
    long rec_vary;        /* Does the variable have record variance */
    long dim_varys[CDF_MAX_DIMS]; /* Does each dimension vary */
    hid_t type_id;       /* HDF5 datatype */
    hid_t space_id;      /* HDF5 dataspace */
};

/* Forward declaration for unified object type */
typedef struct cdf_object_t cdf_object_t;

/* Unified CDF VOL object structure */
struct cdf_object_t {
    cdf_object_t *parent_file;     /* Parent file (never NULL after open) */
    H5I_type_t obj_type;           /* HDF5 object type identifier */
    size_t ref_count;              /* Reference count for child objects */
    union {
        cdf_file_t file;
        cdf_dataset_t dataset;
    } u;
};

/* Function prototypes (HDF5 develop expects hid_t vipl_id) */
herr_t cdf_init_connector(hid_t vipl_id);
herr_t cdf_term_connector(void);

/* File operations */
void *cdf_file_open(const char *name, unsigned flags, hid_t fapl_id, hid_t dxpl_id, void **req);
herr_t cdf_file_close(void *file, hid_t dxpl_id, void **req);

/* Dataset operations */
void *cdf_dataset_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                       hid_t dapl_id, hid_t dxpl_id, void **req);
herr_t cdf_dataset_read(size_t count, void *dset[], hid_t mem_type_id[], hid_t mem_space_id[],
                            hid_t file_space_id[], hid_t dxpl_id, void *buf[], void **req);
herr_t cdf_dataset_close(void *dset, hid_t dxpl_id, void **req);


herr_t cdf_introspect_opt_query(void *obj, H5VL_subclass_t subcls, int opt_type,
                                    uint64_t *flags);

herr_t cdf_introspect_get_conn_cls(void __attribute__((unused)) * obj,
                                       H5VL_get_conn_lvl_t __attribute__((unused)) lvl,
                                       const H5VL_class_t __attribute__((unused)) * *conn_cls);
#endif /* _cdf_vol_connector_H */
