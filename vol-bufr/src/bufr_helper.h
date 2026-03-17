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

#ifndef _BUFR_HELPER_H
#define _BUFR_HELPER_H

#include <stddef.h>
#include <stdbool.h>
#include <eccodes.h>

/* ------------------------------------------------------------------------- */
/* Inventory data structures                                                 */
/* ------------------------------------------------------------------------- */

typedef struct inv_attr_t {
    char   name[512];
    int    native_type;   /* CODES_TYPE_* */
    size_t size;          /* codes_get_size() result when known */
} inv_attr_t;

typedef struct inv_dset_attr_t {
    char   name[512];     /* attribute name from "base->attr" */
    int    native_type;
    size_t size;

    int    per_occurrence; /* true for "#n#base->attr" */
    int    rep_count;      /* maximum replication index seen */
} inv_dset_attr_t;

typedef struct inv_dataset_t {
    char   name[512];      /* HDF5-visible dataset name */
    char   ecc_key[768];   /* canonical ecCodes key used to resolve/read */

    int    native_type;
    size_t raw_size;

    size_t elem_size;
    size_t str_len;

    size_t num_subsets;
    size_t occ_size;
    size_t total_elems;
    size_t storage_size;

    int    rank;
    size_t dims[2];

    int    is_replicated;
    int    rep_count;
    int    is_meta_dataset;

    inv_dset_attr_t *attrs;
    size_t nattrs;
    size_t capattrs;
} inv_dataset_t;

typedef struct inv_t {
    size_t num_subsets;

    inv_attr_t *group_attrs;
    size_t ngroup_attrs;
    size_t capgroup_attrs;

    inv_dataset_t *datasets;
    size_t ndatasets;
    size_t capdatasets;
} inv_t;

/* ------------------------------------------------------------------------- */
/* Dataset-spec structure used by dataset-open/read helpers                  */
/* ------------------------------------------------------------------------- */

typedef struct bufr_dataset_spec_t {
    char hdf5_name[512];
    char ecc_key[512];

    int  is_meta_dataset;
    char meta_base[512];
    char meta_attr[256];

    int  is_replicated;
    int  rep_count;

    int    native_type;
    size_t occ_size;
} bufr_dataset_spec_t;

/* ------------------------------------------------------------------------- */
/* Public helper API                                                         */
/* ------------------------------------------------------------------------- */

void* xmalloc(size_t n);
void* xrealloc(void* p, size_t n);
int read_one_string(codes_handle* h, const char* key, char** out_s);
void bufr_safe_strcpy(char *dst, size_t cap, const char *src);

void bufr_inv_free(inv_t *inv);
void bufr_inv_print(const inv_t *inv);
int  bufr_build_group_inventory(codes_handle *h, inv_t *inv);

int bufr_read_double_dataset(codes_handle *h, const bufr_dataset_spec_t *spec,
                             double **out_buf, size_t *out_n);
int bufr_read_long_dataset(codes_handle *h, const bufr_dataset_spec_t *spec,
                           long **out_buf, size_t *out_n);
int bufr_read_string_dataset(codes_handle *h, const bufr_dataset_spec_t *spec,
                             char ***out_strings, size_t *out_nstrings);
int bufr_read_bytes_dataset(codes_handle *h, const bufr_dataset_spec_t *spec,
                            unsigned char **out_buf, size_t *out_nbytes);

int bufr_parse_hdf5_dataset_name(const char *hdf5_name, bufr_dataset_spec_t *spec);
int bufr_resolve_dataset_spec(codes_handle *h, bufr_dataset_spec_t *spec);
//hid_t bufr_make_hdf5_type_for_codes_type(int codes_type, bool is_vlen_string);

#endif /* _BUFR_HELPER_H */
