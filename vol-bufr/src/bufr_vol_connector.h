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

/* Purpose:     HDF5 Virtual Object Layer (VOL) connector for BUFR files
 *              Enables reading BUFR files through HDF5 tools
 */

#ifndef _bufr_vol_connector_H
#define _bufr_vol_connector_H

#include "bufr_vol_err.h" /* Error reporting macros */
#include <hdf5.h>
#include <eccodes.h>
#include <stdint.h>

/* The value must be between 256 and 65535 (inclusive) */
#define BUFR_VOL_CONNECTOR_VALUE ((H5VL_class_value_t) 12210)
#define BUFR_VOL_CONNECTOR_NAME "bufr_vol_connector"

/* BUFR VOL file object structure */
typedef struct bufr_file_t {
    FILE *bufr;         /* BUFR file handle - shared across all groups and datasets */
    char *filename;     /* File name */
    unsigned int flags; /* File access flags */
    hid_t plist_id;     /* Property list ID */
} bufr_file_t;

/* Forward declaration for unified object type */
typedef struct bufr_object_t bufr_object_t;

/* Unified BUFR VOL object structure */
struct bufr_object_t {
    bufr_object_t *parent_file;    /* Parent file (never NULL after open) */
    H5I_type_t obj_type;           /* HDF5 object type identifier */
    size_t ref_count;              /* Reference count for child objects */
    union {
        bufr_file_t file;
    } u;
};

/* Function prototypes (HDF5 develop expects hid_t vipl_id) */
herr_t bufr_init_connector(hid_t vipl_id);
herr_t bufr_term_connector(void);

/* File operations */
void *bufr_file_open(const char *name, unsigned flags, hid_t fapl_id, hid_t dxpl_id, void **req);
herr_t bufr_file_close(void *file, hid_t dxpl_id, void **req);


herr_t bufr_introspect_opt_query(void *obj, H5VL_subclass_t subcls, int opt_type,
                                    uint64_t *flags);

herr_t bufr_introspect_get_conn_cls(void __attribute__((unused)) * obj,
                                       H5VL_get_conn_lvl_t __attribute__((unused)) lvl,
                                       const H5VL_class_t __attribute__((unused)) * *conn_cls);
#endif /* _bufr_vol_connector_H */
