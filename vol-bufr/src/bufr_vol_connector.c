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

/* Purpose:     HDF5 Virtual Object Layer (VOL) connector for BUFR files */

/* This connector's header */
#include "bufr_vol_connector.h"

#include <H5PLextern.h>
#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#ifndef strdup
#define strdup _strdup
#endif
/* MSVC doesn't support __attribute__ */
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

#define SUCCEED 0

static hbool_t H5_bufr_initialized_g = FALSE;

/* Identifiers for HDF5's error API */
hid_t H5_bufr_err_stack_g = H5I_INVALID_HID;
hid_t H5_bufr_err_class_g = H5I_INVALID_HID;
hid_t H5_bufr_obj_err_maj_g = H5I_INVALID_HID;

/* The VOL class struct */
static const H5VL_class_t bufr_class_g = {
    3,                        /* VOL class struct version */
    BUFR_VOL_CONNECTOR_VALUE, /* value                    */
    BUFR_VOL_CONNECTOR_NAME,  /* name                     */
    1,                        /* version                  */
    0,                        /* capability flags         */
    bufr_init_connector,      /* initialize               */
    bufr_term_connector,      /* terminate                */
    {
        /* info_cls */
        (size_t) 0, /* size    */
        NULL,       /* copy    */
        NULL,       /* compare */
        NULL,       /* free    */
        NULL,       /* to_str  */
        NULL,       /* from_str */
    },
    {
        /* wrap_cls */
        NULL, /* get_object   */
        NULL, /* get_wrap_ctx */
        NULL, /* wrap_object  */
        NULL, /* unwrap_object */
        NULL, /* free_wrap_ctx */
    },
    {
        /* attribute_cls */
        NULL,           /* create       */
        bufr_attr_open, /* open         */
        bufr_attr_read, /* read         */
        NULL,           /* write        */
        bufr_attr_get,  /* get          */
        NULL,           /* specific     */
        NULL,           /* optional     */
        bufr_attr_close /* close        */
    },
    {
        /* dataset_cls */
        NULL,              /* create       */
        bufr_dataset_open, /* open         */
        bufr_dataset_read, /* read         */
        NULL,              /* write        */
        bufr_dataset_get,  /* get          */
        NULL,              /* specific     */
        NULL,              /* optional     */
        bufr_dataset_close /* close        */
    },
    {
        /* datatype_cls */
        NULL, /* commit       */
        NULL, /* open         */
        NULL, /* get_size     */
        NULL, /* specific     */
        NULL, /* optional     */
        NULL  /* close        */
    },
    {
        /* file_cls */
        NULL,           /* create       */
        bufr_file_open, /* open         */
        bufr_file_get,  /* get          */
        NULL,           /* specific     */
        NULL,           /* optional     */
        bufr_file_close /* close        */
    },
    {
        /* group_cls */
        NULL, /* create       */
        NULL, /* open         */
        NULL, /* get          */
        NULL, /* specific     */
        NULL, /* optional     */
        NULL  /* close        */
    },
    {
        /* link_cls */
        NULL, /* create       */
        NULL, /* copy         */
        NULL, /* move         */
        NULL, /* get          */
        NULL, /* specific     */
        NULL  /* optional     */
    },
    {
        /* object_cls */
        NULL, /* open         */
        NULL, /* copy         */
        NULL, /* get          */
        NULL, /* specific     */
        NULL  /* optional     */
    },
    {
        /* introscpect_cls */
        bufr_introspect_get_conn_cls, /* get_conn_cls  */
        NULL,                         /* get_cap_flags */
        bufr_introspect_opt_query     /* opt_query     */
    },
    {
        /* request_cls */
        NULL, /* wait         */
        NULL, /* notify       */
        NULL, /* cancel       */
        NULL, /* specific     */
        NULL, /* optional     */
        NULL  /* free         */
    },
    {
        /* blob_cls */
        NULL, /* put          */
        NULL, /* get          */
        NULL, /* specific     */
        NULL  /* optional     */
    },
    {
        /* token_cls */
        NULL, /* cmp          */
        NULL, /* to_str       */
        NULL  /* from_str     */
    },
    NULL /* optional     */
};

/* Helper function to get HDF5 memory type from ecCodes type */
herr_t bufr_get_hdf5_type(int codes_type, hid_t *type_id)
{
    herr_t ret_value = SUCCEED;
    hid_t new_type = H5I_INVALID_HID;
    hid_t predef_type = H5I_INVALID_HID;
    assert(type_id);

    switch (codes_type) {
        case CODES_TYPE_LONG:
            predef_type = H5T_NATIVE_LONG;
            break;
        case CODES_TYPE_DOUBLE:
            predef_type = H5T_NATIVE_DOUBLE;
            break;
        case CODES_TYPE_STRING:
            // Add error checking
            predef_type = H5Tcopy(H5T_C_S1);
            H5Tset_size(predef_type, H5T_VARIABLE);
            break;
        case CODES_TYPE_BYTES:
            predef_type = H5T_NATIVE_UCHAR;
            break;
        default:
            predef_type = H5I_INVALID_HID;
            break;
    }
    if ((new_type = H5Tcopy(predef_type)) == H5I_INVALID_HID)
        FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_CANTCOPY, FAIL, "Failed to copy datatype");

    *type_id = new_type;

done:
    if (ret_value < 0 && new_type != H5I_INVALID_HID) {
        H5E_BEGIN_TRY
        {
            H5Tclose(new_type);
        }
        H5E_END_TRY;
    }
    return ret_value;
}

/* strdup() replacement */
static char *bufr_strdup(const char *s)
{
    if (!s)
        return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *) malloc(n);
    if (!p)
        return NULL;
    memcpy(p, s, n);
    return p;
}

/* Helper function to parse the name of the form /message_<N>/key_path" to extract message number
 * and key path */
static int parse_message_key_path(const char *name, long *msg_index, const char **key_view)
{
    //    if (!name || !msg_index || !key_view) return -1;
    assert(name);
    assert(msg_index);
    assert(key_view);

    const char *p = name;

    /* Skip leading slashes */
    while (*p == '/')
        p++;

    /* Expect "message_" prefix */
    static const char prefix[] = "message_";
    size_t prefix_len = sizeof(prefix) - 1;

    if (strncmp(p, prefix, prefix_len) != 0) {
        return -2; /* bad prefix */
    }
    p += prefix_len;

    /* Parse <N> */
    int errno = 0;
    char *end = NULL;
    long n = strtol(p, &end, 10);
    if (errno != 0 || end == p || n <= 0) {
        return -3; /* invalid number */
    }

    /* Expect '/' after the number */
    if (*end != '/') {
        return -4; /* missing key separator */
    }
    end++; /* move past '/' */

    if (*end == '\0') {
        return -5; /* empty key */
    }

    *msg_index = n;
    *key_view = end;
    return 0;
}

/* Helper function to find type and size for a key */
static int bufr_key_type_and_size(codes_handle *h, const char *key, int *out_type, size_t *out_size)
{
    int err;
    int t = 0;
    size_t n = 0;

    err = codes_get_native_type(h, key, &t);
    if (err != 0)
        return err;

    err = codes_get_size(h, key, &n);
    if (err != 0)
        return err;

    *out_type = t;
    *out_size = n;
    return 0;
}

/* Helper function to build the message offset index */
static int bufr_build_message_index(bufr_file_t *bf)
{
    //    if (!bf || !bf->bufr) return -1;
    assert(bf);
    assert(bf->bufr);

    /* Save/restore caller's file position */
    long saved = ftell(bf->bufr);
    if (saved < 0)
        saved = 0;

    if (fseek(bf->bufr, 0, SEEK_SET) != 0)
        return -2;

    free(bf->msg_offsets);
    bf->msg_offsets = NULL;
    bf->nmsgs = 0;

    size_t cap = 128;
    long *offs = (long *) malloc(cap * sizeof(long));
    if (!offs)
        return -3;

    int err = 0;
    while (1) {
        long off = ftell(bf->bufr);
        if (off < 0) {
            err = -4;
            break;
        }

        /* Try to read next BUFR message */
        codes_handle *h = codes_handle_new_from_file(NULL, bf->bufr, PRODUCT_BUFR, &err);

        if (!h || err != 0) {
            /* Normal end-of-file: ecCodes returns NULL at EOF */
            if (h)
                codes_handle_delete(h);
            break;
        }

        /* Record offset for this message */
        if (bf->nmsgs == cap) {
            cap *= 2;
            long *tmp = (long *) realloc(offs, cap * sizeof(long));
            if (!tmp) {
                codes_handle_delete(h);
                err = -5;
                break;
            }
            offs = tmp;
        }

        offs[bf->nmsgs++] = off;

        /* We only index offsets; discard handle */
        codes_handle_delete(h);
    }

    if (err < 0) {
        free(offs);
        bf->msg_offsets = NULL;
        bf->nmsgs = 0;
        (void) fseek(bf->bufr, saved, SEEK_SET);
        return err;
    }

    /* Shrink to fit */
    if (bf->nmsgs == 0) {
        free(offs);
        bf->msg_offsets = NULL;
    } else {
        long *tmp = (long *) realloc(offs, bf->nmsgs * sizeof(long));
        bf->msg_offsets = tmp ? tmp : offs;
    }

    (void) fseek(bf->bufr, saved, SEEK_SET);
    return 0;
}

/* Helper function to open the N-the message using the offset index */
static codes_handle *bufr_open_message_by_index(bufr_file_t *bf, size_t msg_index_1based)
{
    if (!bf || !bf->bufr || !bf->msg_offsets)
        return NULL;
    if (msg_index_1based == 0 || msg_index_1based > bf->nmsgs)
        return NULL;

    long saved = ftell(bf->bufr);
    if (saved < 0)
        saved = 0;

    long off = bf->msg_offsets[msg_index_1based - 1];
    if (fseek(bf->bufr, off, SEEK_SET) != 0)
        return NULL;

    int err = 0;
    codes_handle *h = codes_handle_new_from_file(NULL, bf->bufr, PRODUCT_BUFR, &err);

    (void) fseek(bf->bufr, saved, SEEK_SET);

    if (!h || err != 0) {
        if (h)
            codes_handle_delete(h);
        return NULL;
    }
    return h; /* caller owns */
}

/* Helper functionto read BUFR data into dset->data and set dset->data_size */
static herr_t bufr_read_data(bufr_dataset_t *dset)
{
    herr_t ret_value = SUCCEED;
    int err;
    assert(dset);
    assert(dset->msg);
    assert(dset->msg->h);

    codes_handle *h = dset->msg->h;
    const char *key = dset->name;

    /* Determine logical number of elements */
    size_t nvals = 0;
    err = codes_get_size(h, key, &nvals);
    if (err != 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to get logical number of elements");

    /* Empty dataset: nothing to read */
    if (nvals == 0) {
        dset->data = NULL;
        dset->data_size = 0;
        dset->nvals = 0;
        FUNC_GOTO_DONE(SUCCEED);
    }

    /* -----------------------------
     * STRING (VL)
     * ----------------------------- */
    if (dset->is_vlen_string) {

        /* Scalar string */
        if (nvals == 1) {
            size_t len = 0;

            err = codes_get_string(h, key, NULL, &len);
            if (err != 0)
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to get length for key string");

            char *s = (char *) H5allocate_memory(len, 0);
            if (!s)
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL,
                                "Failed to allocate buffer for a key string");

            err = codes_get_string(h, key, s, &len);
            if (err != 0) {
                H5free_memory(s);
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to read key string");
            }

            dset->data = s;
            dset->data_size = sizeof(char *);
            dset->nvals = 1;
            FUNC_GOTO_DONE(SUCCEED);
        }

        /* Array of strings (replicated) */
        char **arr = (char **) calloc(nvals, sizeof(char *));
        if (!arr)
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL, "Failed to allocate array of strings");

        for (size_t i = 0; i < nvals; i++) {
            char ikey[512];
            snprintf(ikey, sizeof(ikey), "%s#%zu", key, i + 1);

            size_t len = 0;
            err = codes_get_string(h, ikey, NULL, &len);
            if (err != 0)
                goto string_array_fail;

            arr[i] = (char *) H5allocate_memory(len, 0);
            if (!arr[i]) {
                // err = -2;
                goto string_array_fail;
            }

            err = codes_get_string(h, ikey, arr[i], &len);
            if (err != 0)
                goto string_array_fail;
        }

        dset->data = arr;
        dset->data_size = nvals * sizeof(char *);
        dset->nvals = nvals;
        FUNC_GOTO_DONE(SUCCEED);

    string_array_fail:
        for (size_t j = 0; j < nvals; j++) {
            if (arr[j])
                H5free_memory(arr[j]);
        }
        free(arr);
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to read array of strings");
    }

    /* -----------------------------
     * NUMERIC (LONG / DOUBLE)
     * ----------------------------- */

    if (dset->codes_type == CODES_TYPE_LONG) {
        long *buf = (long *) malloc(nvals * sizeof(long));
        if (!buf)
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL,
                            "Failed to allocate buffer for long values");

        err = codes_get_long_array(h, key, buf, &nvals);
        if (err != 0) {
            free(buf);
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to get long values");
        }

        dset->data = buf;
        dset->data_size = nvals * sizeof(long);
        dset->nvals = nvals;
        FUNC_GOTO_DONE(SUCCEED);
    }

    if (dset->codes_type == CODES_TYPE_DOUBLE) {
        double *buf = (double *) malloc(nvals * sizeof(double));
        if (!buf)
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL,
                            "Failed to allocate buffer for double values");

        err = codes_get_double_array(h, key, buf, &nvals);
        if (err != 0) {
            free(buf);
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to get double values");
        }

        dset->data = buf;
        dset->data_size = nvals * sizeof(double);
        dset->nvals = nvals;
        FUNC_GOTO_DONE(SUCCEED);
    }

    /* Unsupported type */
    FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "Discovered unknown datatype");
done:
    return ret_value;
}

/* Helper function to free cached data */
static void bufr_free_cached_data(bufr_dataset_t *dset)
{
    assert(dset);
    assert(dset->data);

    if (dset->is_vlen_string) {
        /* data is either char* (scalar) or char** (array); decide by dataspace */
        int ndims = H5Sget_simple_extent_ndims(dset->space_id);

        if (ndims <= 0) {
            /* scalar: data is char* */
            H5free_memory(dset->data);
        } else {
            /* 1-D array: data is char** of length dims[0] */
            hsize_t dims[1] = {0};
            H5Sget_simple_extent_dims(dset->space_id, dims, NULL);

            char **arr = (char **) dset->data;
            for (hsize_t i = 0; i < dims[0]; i++) {
                if (arr[i])
                    H5free_memory(arr[i]);
            }
            free(arr); /* pointer array allocated by us */
        }
    } else {
        /* numeric cache allocated by us */
        free(dset->data);
    }

    dset->data = NULL;
    dset->data_size = 0;
}

/* VOL file open callback */
void *bufr_file_open(const char *name, unsigned flags, hid_t fapl_id,
                     hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    bufr_object_t *file_obj = NULL;
    bufr_object_t *ret_value = NULL;

    bufr_file_t *file = NULL; /* Convenience pointer */

    /* We only support read-only access for BUFR files */
    if (flags != H5F_ACC_RDONLY)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL,
                        "BUFR VOL connector only supports read-only access");

    if ((file_obj = (bufr_object_t *) calloc(1, sizeof(bufr_object_t))) == NULL)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for BUFR file struct");

    file_obj->obj_type = H5I_FILE;
    file = &file_obj->u.file;
    /* Parent file pointers points to itself */
    file_obj->parent_file = file_obj;
    file_obj->ref_count = 1;

    if ((file->bufr = fopen(name, "rb")) == NULL)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTOPENFILE, NULL, "Failed to open BUFR file: %s", name);

    if (bufr_build_message_index(file) != 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL, "Failed to build message index");

    if ((file->filename = strdup(name)) == NULL)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL, "Failed to duplicate BUFR filename string");

    file->flags = flags;
    file->plist_id = fapl_id;

    ret_value = file_obj;

done:
    if (!ret_value) {
        if (file_obj) {
            H5E_BEGIN_TRY
            {
                bufr_file_close(file_obj, dxpl_id, req);
            }
            H5E_END_TRY;
        }
    }

    return ret_value;
}

/* VOL file get callback */

// cppcheck-suppress constParameterCallback
herr_t bufr_file_get(void *file, H5VL_file_get_args_t *args, hid_t __attribute__((unused)) dxpl_id,
                     void __attribute__((unused)) * *req)
{
    const bufr_object_t *o = (const bufr_object_t *) file;
    const bufr_file_t *f = &o->u.file;
    herr_t ret_value = SUCCEED;

    switch (args->op_type) {
        case H5VL_FILE_GET_NAME:
            /* HDF5 1.14+/develop uses buf, buf_size, buf_len */
            if (args->args.get_name.buf && args->args.get_name.buf_size > 0) {
                size_t ncopy = strlen(f->filename);
                if (ncopy >= args->args.get_name.buf_size)
                    ncopy = args->args.get_name.buf_size - 1;
                memcpy(args->args.get_name.buf, f->filename, ncopy);
                args->args.get_name.buf[ncopy] = '\0';
            }
            /* Some HDF5 versions may not provide buf_len. If available, setting it is optional. */
            break;
        default:
            FUNC_GOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unsupported file get operation");
    }
done:
    return ret_value;
}

/* VOL file close callback */
herr_t bufr_file_close(void *file, hid_t __attribute__((unused)) dxpl_id,
                       void __attribute__((unused)) * *req)
{
    int bufr_status;
    bufr_object_t *o = (bufr_object_t *) file;
    herr_t ret_value = SUCCEED;

    assert(o);

    if (o->ref_count == 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCLOSEFILE, FAIL,
                        "BUFR file already closed (ref_count is 0)");

    o->ref_count--;

    if (o->ref_count == 0) {
        if (o->u.file.bufr) {
            bufr_status = fclose(o->u.file.bufr);
            if (bufr_status == EOF)
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCLOSEFILE, FAIL, "Failed to close BUFR file");
            else
                o->u.file.bufr = NULL;
        }
        if (o->u.file.filename)
            free(o->u.file.filename);
        if (o->u.file.msg_offsets)
            free(o->u.file.msg_offsets);
        free(o);
    }

done:
    return ret_value;
}

/* VOL dataset open callback */
void *bufr_dataset_open(void *obj, const H5VL_loc_params_t __attribute__((unused)) * loc_params,
                        const char *name, hid_t __attribute__((unused)) dapl_id,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    bufr_object_t *file_obj = (bufr_object_t *) obj;
    bufr_object_t *dset_obj = NULL;
    bufr_object_t *ret_value = NULL;

    bufr_dataset_t *dset = NULL;           /* Convenience pointer */
    bufr_file_t *file = &file_obj->u.file; /* Convenience pointer */

    int parse_return = -1;
    hsize_t dims[1] = {0}; /* BUFR key values are always read as 1-dim datasets */

    long msg_index = 0;
    const char *key_view = NULL;
    codes_handle *h = NULL;
    bufr_message_t *msg = NULL;
    int message_type = 0;
    size_t len = 0;
    size_t string_len = 0;

    /* Parse "/message_<N>/<key>" */
    parse_return = parse_message_key_path(name, &msg_index, &key_view);

    if (!file_obj || (parse_return != 0)) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "Invalid BUFR file name or key path");
    }

    if ((dset_obj = (bufr_object_t *) malloc(sizeof(bufr_object_t))) == NULL) {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for BUFR dataset struct");
    }
    dset_obj->obj_type = H5I_DATASET;
    dset_obj->parent_file = file_obj->parent_file;
    dset_obj->ref_count = 1; /* Initialize dataset's own ref count */
    /* Increment file reference count since this dataset holds a reference */
    file_obj->ref_count++;

    dset = &dset_obj->u.dataset;

    dset->msg = NULL;
    dset->space_id = H5I_INVALID_HID;
    dset->type_id = H5I_INVALID_HID;
    dset->data = NULL;
    dset->is_vlen_string = 0;
    /* The value is set to the size of the data buffer; see bufr_read_data function below */
    dset->data_size = 0;

    /* Fast open using message offsets */
    h = bufr_open_message_by_index(file, (size_t) msg_index);
    if (!h) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "Failed to get BUFR message handle ");
    }
    /* Unpack the  message data */
    if (codes_set_long(h, "unpack", 1) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "Failed to unpack BUFR message handle");
    }

    msg = (bufr_message_t *) calloc(1, sizeof(*msg));
    if (!msg) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL,
                        "Failed to initialize message handle for dataset object");
    }
    msg->h = h;
    dset->msg = msg;

    /* Copy key to set up dataset name */
    if ((dset->name = bufr_strdup(key_view)) == NULL) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL, "Failed to duplicate dataset name string");
    }

    /* Copy key to set up dataset name */
    /* Find BUFR datatype and size (replication) for the key */
    // EP if (bufr_key_type_and_size(msg->h, dset->name, &message_type, &len) != 0) {
    if (bufr_key_type_and_size(h, key_view, &message_type, &len) != 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL,
                        "Failed to discover datatype and size for the key");
    }

    /* Find corresponding HDF5 native type; set a special flag if BUFR type is string */
    dset->codes_type = message_type;
    if (dset->codes_type == CODES_TYPE_STRING)
        dset->is_vlen_string = 1;
    if (bufr_get_hdf5_type(dset->codes_type, &dset->type_id) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL,
                        "Failed to covert BUFR datatype to native HDF5 type");
    }

    /* len = 0 means that key is present, the replication count is zero, and no values are encoded.
       We will use empty dataset. If len = 1 create a scalar dataset. Otherwise, create 1-dim
       with dimension size len */
    if (len == 0) {
        dset->space_id = H5Screate(H5S_NULL);
        if (dset->space_id == H5I_INVALID_HID) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL,
                            "Failed to create NULL dataspace for dataset");
        }
    } else if (len == 1) {
        dset->space_id = H5Screate(H5S_SCALAR);
        if (dset->space_id == H5I_INVALID_HID) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL,
                            "Failed to create SCALAR dataspace for dataset");
        }
    } else {
        hsize_t dims[1];
        dims[0] = (hsize_t) len;
        dset->space_id = H5Screate_simple(1, dims, NULL);
        if (dset->space_id == H5I_INVALID_HID) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL,
                            "Failed to create simple dataspace for dataset");
        }
    }
    /* Cache the key data */
    if (bufr_read_data(dset) != 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "Failed to read data for the key");

    return dset_obj;

done:
    if (!ret_value) {
        if (dset)
            H5E_BEGIN_TRY
            {
                bufr_dataset_close(dset_obj, dxpl_id, req);
            }
        H5E_END_TRY;
    }

    return ret_value;
}

/* Helper functions below are resused from the GeoTIFF connector.
 * Support for selections in BUFR are overkill since size of raw data is not large,
 * but we support it since HDF5 apps may use partial reads/writes. In most cases
 * H5S_ALL selectiosn will be used (and recommended). Datatype conversion is a valid
 * operation for numeric ecCodes types only.
 */

/* Struct for H5Dscatter's callback that allows it to scatter from a non-global response buffer */

typedef struct response_read_info {
    void *buffer;
    void *read_size;
} response_read_info;

static herr_t dataset_read_scatter_op(const void **src_buf, size_t *src_buf_bytes_used,
                                      void *op_data)
{
    response_read_info *resp_info = (response_read_info *) op_data;
    *src_buf = resp_info->buffer;
    *src_buf_bytes_used = *((size_t *) resp_info->read_size);

    return 0;
} /* end dataset_read_scatter_op() */

/* Helper function: Prepare a buffer with data in the requested memory type.
 * If conversion is needed, allocates a new buffer and performs conversion.
 * If no conversion needed, returns pointer to original data.
 */
static herr_t prepare_converted_buffer(const bufr_dataset_t *dset, hid_t mem_type_id,
                                       size_t num_elements, void **out_buffer,
                                       size_t *out_buffer_size, hbool_t *out_tconv_buf_allocated)
{
    herr_t ret_value = SUCCEED;
    htri_t types_equal = 0;
    size_t dataset_type_size = 0;
    size_t mem_type_size = 0;
    void *conversion_buf = NULL;

    if (!dset || !out_buffer || !out_buffer_size || !out_tconv_buf_allocated)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "invalid arguments");

    /* Check if types are equal */
    if ((types_equal = H5Tequal(mem_type_id, dset->type_id)) < 0)
        FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_CANTCOMPARE, FAIL, "failed to compare datatypes");

    if (types_equal) {
        /* No conversion needed - return borrowed pointer to cached data */
        *out_buffer = dset->data;
        *out_buffer_size = dset->data_size;
        *out_tconv_buf_allocated = FALSE;
        FUNC_GOTO_DONE(SUCCEED);
    }

    /* Conversion needed - allocate buffer and convert */
    if ((dataset_type_size = H5Tget_size(dset->type_id)) == 0)
        FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_CANTGET, FAIL, "failed to get dataset type size");

    if ((mem_type_size = H5Tget_size(mem_type_id)) == 0)
        FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_CANTGET, FAIL, "failed to get memory type size");

    /* Allocate buffer large enough for in-place conversion (max of src/dst) */
    size_t src_data_size = num_elements * dataset_type_size;
    size_t dst_data_size = num_elements * mem_type_size;
    size_t conversion_buf_size = (src_data_size > dst_data_size) ? src_data_size : dst_data_size;

    if ((conversion_buf = malloc(conversion_buf_size)) == NULL)
        FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL,
                        "failed to allocate memory for datatype conversion");

    /* Copy source data */
    memcpy(conversion_buf, dset->data, src_data_size);

    /* Perform in-place conversion */
    if (H5Tconvert(dset->type_id, mem_type_id, num_elements, conversion_buf, NULL, H5P_DEFAULT) < 0)
        FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_CANTCONVERT, FAIL,
                        "failed to convert data from dataset type to memory type");

    /* Return owned buffer */
    *out_buffer = conversion_buf;
    *out_buffer_size = dst_data_size;
    *out_tconv_buf_allocated = TRUE;
    conversion_buf = NULL; /* Transfer ownership */

done:
    if (conversion_buf)
        free(conversion_buf);

    return ret_value;
} /* end prepare_converted_buffer() */

/* Helper function: Transfer data from source buffer to user buffer.
 * Handles both simple memcpy (when both selections are ALL) and scatter operations.
 */
static herr_t transfer_data_to_user(const void *source_buf, size_t source_size, hid_t mem_type_id,
                                    hid_t mem_space_id, void *user_buf)
{
    herr_t ret_value = SUCCEED;
    H5S_sel_type mem_sel_type = H5S_SEL_ERROR;

    if (!source_buf || !user_buf)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid arguments");

    /* Determine if we can use simple memcpy or need scatter */
    if (mem_space_id == 0 || mem_space_id == H5S_ALL) {
        /* Simple case: copy entire buffer directly */
        memcpy(user_buf, source_buf, source_size);
        FUNC_GOTO_DONE(SUCCEED);
    }

    /* Check selection type */
    if ((mem_sel_type = H5Sget_select_type(mem_space_id)) < 0)
        FUNC_GOTO_ERROR(H5E_DATASPACE, H5E_CANTGET, FAIL,
                        "failed to get memory space selection type");

    if (mem_sel_type == H5S_SEL_ALL) {
        /* Simple case: copy entire buffer directly */
        memcpy(user_buf, source_buf, source_size);
    } else {
        /* Use scatter for non-trivial selections */
        response_read_info resp_info;
        resp_info.read_size = &source_size;
        resp_info.buffer = (void *) source_buf;

        if (H5Dscatter(dataset_read_scatter_op, &resp_info, mem_type_id, mem_space_id, user_buf) <
            0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_READERROR, FAIL, "can't scatter data to user buffer");
    }

done:
    return ret_value;
} /* end transfer_data_to_user() */

herr_t bufr_dataset_read(size_t __attribute__((unused)) count, void *dset[], hid_t mem_type_id[],
                         hid_t __attribute__((unused)) mem_space_id[], hid_t file_space_id[],
                         hid_t __attribute__((unused)) dxpl_id, void *buf[],
                         void __attribute__((unused)) * *req)
{
    const bufr_object_t *dset_obj = (const bufr_object_t *) dset[0];
    const bufr_dataset_t *d = NULL; /* Convenience pointer */

    herr_t ret_value = SUCCEED;
    H5S_sel_type file_sel_type = H5S_SEL_ERROR;
    H5S_sel_type mem_sel_type = H5S_SEL_ERROR;
    hssize_t num_elements = 0;
    void *source_buf = NULL;
    size_t source_size = 0;
    hbool_t tconv_buf_allocated = FALSE;
    /* To follow H5S_ALL semantics, we set up local vars for effective values of mem/filespace */
    hid_t effective_file_space_id = file_space_id[0];
    hid_t effective_mem_space_id = mem_space_id[0];

    void *gathered_buf = NULL;

    assert(dset_obj);
    d = (const bufr_dataset_t *) &dset_obj->u.dataset;

    if (!buf[0])
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "invalid dataset buffer");

    /* Set up dataspaces and element count */
    if (file_space_id[0] == 0) {
        file_sel_type = H5S_SEL_ALL;
    } else if ((file_sel_type = H5Sget_select_type(file_space_id[0])) < 0) {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL,
                        "failed to get file dataspace selection type");
    }

    if (mem_space_id[0] == 0) {
        mem_sel_type = H5S_SEL_ALL;
    } else if ((mem_sel_type = H5Sget_select_type(mem_space_id[0])) < 0) {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL,
                        "failed to get memory dataspace selection type");
    }

    if (file_sel_type == H5S_SEL_ALL && mem_sel_type == H5S_SEL_ALL) {
        num_elements = H5Sget_simple_extent_npoints(d->space_id);
        effective_file_space_id = d->space_id;
        effective_mem_space_id = d->space_id;
    } else if (file_sel_type == H5S_SEL_ALL && mem_sel_type != H5S_SEL_ALL) {
        num_elements = H5Sget_select_npoints(mem_space_id[0]);
        effective_file_space_id = d->space_id;
    } else if (file_sel_type != H5S_SEL_ALL && mem_sel_type == H5S_SEL_ALL) {
        num_elements = H5Sget_select_npoints(file_space_id[0]);
        effective_mem_space_id = d->space_id;
    } else {
        /* Both selections are provided - verify equivalence */
        num_elements = H5Sget_select_npoints(file_space_id[0]);
        if (num_elements != H5Sget_select_npoints(mem_space_id[0]))
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL,
                            "file and memory selections have different number of points");
    }

    /* Prepare source buffer with type conversion if needed.
     * If we have a non-trivial file selection, we need the full dataset in the source buffer
     * for H5Dgather to extract the selection from. Otherwise, we only need num_elements.
     */
    size_t prepare_num_elements;
    hssize_t temp_npoints;

    if (file_sel_type != H5S_SEL_ALL && effective_file_space_id != d->space_id) {
        /* Will need to gather - prepare full dataset */
        if ((temp_npoints = H5Sget_simple_extent_npoints(d->space_id)) < 0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "failed to get dataset extent");
        prepare_num_elements = (size_t) temp_npoints;
    } else {
        /* No gather needed - prepare only selected elements */
        if (num_elements < 0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "invalid number of elements");
        prepare_num_elements = (size_t) num_elements;
    }

    if (prepare_converted_buffer(d, mem_type_id[0], prepare_num_elements, &source_buf, &source_size,
                                 &tconv_buf_allocated) < 0)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "failed to prepare converted buffer");

    /* If file selection is non-trivial (hyperslab, points), gather selected data first */
    if (file_sel_type != H5S_SEL_ALL && effective_file_space_id != d->space_id) {
        /* Allocate buffer for gathered data */
        if (num_elements < 0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL,
                            "invalid number of elements for gather");
        size_t gathered_size = (size_t) num_elements * H5Tget_size(mem_type_id[0]);

        if ((gathered_buf = malloc(gathered_size)) == NULL)
            FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL,
                            "failed to allocate buffer for gathered data");

        /* Gather selected data from source buffer according to file space selection.
         * Note: We pass file_space_id[0] which has the selection, and source_buf which
         * must be sized according to the full extent described by the selection's dataspace.
         */
        if (H5Dgather(file_space_id[0], source_buf, mem_type_id[0], gathered_size, gathered_buf,
                      NULL, NULL) < 0) {
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_READERROR, FAIL, "failed to gather selected data");
        }

        /* Free original buffer if we allocated it for type conversion */
        if (tconv_buf_allocated && source_buf) {
            free(source_buf);
            source_buf = NULL;
        }

        /* Use gathered buffer as new source */
        source_buf = gathered_buf;
        source_size = gathered_size;
        tconv_buf_allocated = TRUE;
    }

    /* Transfer data to user buffer (handles selections via scatter if needed) */
    if (transfer_data_to_user(source_buf, source_size, mem_type_id[0], effective_mem_space_id,
                              buf[0]) < 0)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_READERROR, FAIL, "failed to transfer data to user buffer");

done:
    /* Clean up allocated buffer if needed */
    if (tconv_buf_allocated && source_buf)
        free(source_buf);

    if (ret_value < 0 && gathered_buf)
        free(gathered_buf);

    return ret_value;
}

/* VOL dataset get callback */

// cppcheck-suppress constParameterCallback
herr_t bufr_dataset_get(void *dset, H5VL_dataset_get_args_t *args,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    const bufr_object_t *o = (const bufr_object_t *) dset;
    const bufr_dataset_t *d = &o->u.dataset;

    herr_t ret_value = SUCCEED;

    switch (args->op_type) {
        case H5VL_DATASET_GET_SPACE:
            /* Return a copy of the dataspace */
            assert(d->space_id != H5I_INVALID_HID);

            args->args.get_space.space_id = H5Scopy(d->space_id);
            if (args->args.get_space.space_id < 0)
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to copy dataspace");
            break;

        case H5VL_DATASET_GET_TYPE:
            /* Return a copy of the datatype */
            args->args.get_type.type_id = H5Tcopy(d->type_id);
            if (args->args.get_type.type_id < 0)
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to copy datatype");
            break;

        default:
            FUNC_GOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "Unsupported dataset get operation");
            break;
    }
done:
    return ret_value;
}

/* VOL dataset close callback */
herr_t bufr_dataset_close(void *dset, hid_t dxpl_id, void **req)
{

    bufr_object_t *d = (bufr_object_t *) dset;
    herr_t ret_value = SUCCEED;

    assert(d);

    /* Use FUNC_DONE_ERROR to try to complete resource release after failure */
    if (!d->parent_file)
        FUNC_DONE_ERROR(H5E_DATASET, H5E_BADVALUE, FAIL,
                        "Dataset has no valid parent file reference");

    /* Decrement dataset's ref count */
    if (d->ref_count == 0)
        FUNC_DONE_ERROR(H5E_DATASET, H5E_CANTCLOSEOBJ, FAIL,
                        "Dataset already closed (ref_count is 0)");

    d->ref_count--;

    /* Only do the real close when ref_count reaches 0 */
    if (d->ref_count == 0) {

        /* Free cached data */
        bufr_free_cached_data(&d->u.dataset);

        if (d->u.dataset.name)
            free(d->u.dataset.name);
        if (d->u.dataset.space_id != H5I_INVALID_HID)
            if (H5Sclose(d->u.dataset.space_id) < 0)
                FUNC_DONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "Failed to close dataspace");
        /* Close datatype ONLY if we created it.
         * Typical pattern:
         * - LONG/DOUBLE: type_id is H5T_NATIVE_* (do NOT close)
         * - STRING: type_id was created by H5Tcopy(H5T_C_S1) (must close)
         */
        if (d->u.dataset.is_vlen_string && (d->u.dataset.type_id != H5I_INVALID_HID))
            if (H5Tclose(d->u.dataset.type_id) < 0)
                FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "Failed to close datatype");

        /* Delete BUFR message handle and free message object */
        if (d->u.dataset.msg) {
            if (d->u.dataset.msg->h) {
                codes_handle_delete(d->u.dataset.msg->h);
                d->u.dataset.msg->h = NULL;
            }
            free(d->u.dataset.msg);
            d->u.dataset.msg = NULL;
        }
        /* Decrement parent file's reference count */
        if (bufr_file_close(d->parent_file, dxpl_id, req) < 0)
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "Failed to close dataset file object");

        free(d);
    }
    return ret_value;
}
/* Helper functions to check valid attribute name */

/* ---------- helpers ---------- */

static bool contains_arrow(const char *key)
{
    return strstr(key, "->") != NULL;
}

static bool starts_with(const char *s, const char *p)
{
    return s && p && strncmp(s, p, strlen(p)) == 0;
}

static bool equals_any(const char *s, const char *const *list)
{
    for (size_t i = 0; list[i]; i++) {
        if (strcmp(s, list[i]) == 0)
            return true;
    }
    return false;
}

/* ---------- Per-subset keys checkers ---------- */

static bool varies_per_subset(const char *key)
{

    /* Exact canonical per-observation keys */
    static const char *const exact_keys[] = {"latitude", "longitude", "height",       "pressure",
                                             "depth",    "elevation", "subsetNumber", NULL};

    if (equals_any(key, exact_keys))
        return true;

    /* Common per-subset prefixes */
    static const char *const prefixes[] = {
        "time", /* time, timePeriod, timeIncrement */
        "year",   "month",   "day",        "hour", "minute",      "second",   "station",
        "sensor", "quality", "confidence", "wind", "temperature", "humidity", NULL};

    for (size_t i = 0; prefixes[i]; i++) {
        if (starts_with(key, prefixes[i]))
            return true;
    }

    return false;
}

/* ---------- Decision function ---------- */

static bool reject_key(const char *key)
{

    if (!key || !*key)
        return true;

    /* Rule 1: reject key attributes */
    if (contains_arrow(key))
        return true;

    /* Rule 2: reject per-subset varying keys */
    if (varies_per_subset(key))
        return true;

    return false;
}

/* Helper functionto read BUFR data into attr->data and set attr->data_size */
static herr_t bufr_read_attr_data(bufr_attr_t *attr)
{
    herr_t ret_value = SUCCEED;
    int err;
    assert(attr);
    assert(attr->msg);
    assert(attr->msg->h);
    assert(attr->nvals == 1);

    codes_handle *h = attr->msg->h;
    const char *key = attr->name;

    /* -----------------------------
     * STRING (VL)
     * ----------------------------- */
    if (attr->codes_type == CODES_TYPE_STRING) {

        size_t len = 0;

        err = codes_get_string(h, key, NULL, &len);
        if (err != 0)
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to get length for key string");

        char *s = (char *) malloc(len * sizeof(char));
        if (!s)
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL,
                            "Failed to allocate buffer for a key string");

        err = codes_get_string(h, key, s, &len);
        if (err != 0) {
            H5free_memory(s);
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to read key string");
        }

        attr->data = s;
        attr->data_size = len;
        FUNC_GOTO_DONE(SUCCEED);
    }

    /* -----------------------------
     * NUMERIC (LONG / DOUBLE)
     * ----------------------------- */

    if (attr->codes_type == CODES_TYPE_LONG) {
        long *buf = (long *) malloc(sizeof(long));
        if (!buf)
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL,
                            "Failed to allocate buffer for long attribute value");

        err = codes_get_long(h, key, buf);
        if (err != 0) {
            free(buf);
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to get long attribute value");
        }
        attr->data = buf;
        attr->data_size = sizeof(long);
        FUNC_GOTO_DONE(SUCCEED);
    }

    if (attr->codes_type == CODES_TYPE_DOUBLE) {
        double *buf = (double *) malloc(sizeof(double));
        if (!buf)
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL,
                            "Failed to allocate buffer for double attribute value");

        err = codes_get_double(h, key, buf);
        if (err != 0) {
            free(buf);
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to get double values");
        }

        attr->data = buf;
        attr->data_size = sizeof(double);
        FUNC_GOTO_DONE(SUCCEED);
    }

    /* Unsupported type */
    FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "Discovered unknown datatype");
done:
    return ret_value;
}

/* Helper function to free cached attribute data */
static void bufr_free_cached_attr_data(bufr_attr_t *attr)
{
    assert(attr);
    assert(attr->data);

    free(attr->data);

    attr->data = NULL;
    attr->data_size = 0;
}

/* Attribute operations */
void *bufr_attr_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                     hid_t __attribute__((unused)) aapl_id, hid_t __attribute__((unused)) dxpl_id,
                     void __attribute__((unused)) * *req)
{
    bufr_object_t *parent_obj = NULL;
    bufr_object_t *attr_obj = NULL;
    bufr_object_t *ret_value = NULL;

    bufr_attr_t *attr = NULL; /* Convenience pointer */

    bufr_object_t *file_obj = (bufr_object_t *) obj;

    bufr_file_t *file = &file_obj->u.file; /* Convenience pointer */

    const char *key_view = NULL;
    codes_handle *h = NULL;
    bufr_message_t *msg = NULL;
    int message_type = 0;
    long msg_index = 0;
    size_t len = 0;
    size_t string_len = 0;
    int parse_return = -1;

    if (!obj || !name || !loc_params)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, NULL, "Invalid object or attribute name");

    /* Currenlty the code only accepts canonical BUFR keys that can be treated as attributes.
     * <name> string should be in the form of /massage_<N>/<key_name>, where <key_name> doesn't
     * contain ->, and is one of a known per-subset varying values, or matches common
     * per-observation prefixes. Only file ID can be an object ID in the call to H5Aopen.
     * We will lift this restriction in the future.
     */

    parent_obj = (bufr_object_t *) obj;
    if (parent_obj->obj_type != H5I_FILE)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL,
                        "Unsupported object identifier for attribute open");

    /* Parse "/message_<N>/<key>" */
    parse_return = parse_message_key_path(name, &msg_index, &key_view);
    if (parse_return != 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "Invalid message number or  key path");
    }
    if (reject_key(key_view))
        FUNC_GOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL, "Unsupported name for attribute open");

    /* Determine the type of the parent object */
    if (loc_params->type != H5VL_OBJECT_BY_SELF)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_UNSUPPORTED, NULL,
                        "Unsupported location parameter type for attribute open");

    if ((attr_obj = (bufr_object_t *) calloc(1, sizeof(bufr_object_t))) == NULL)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for BUFR attribute struct");

    attr_obj->obj_type = H5I_ATTR;
    attr_obj->parent_file = parent_obj->parent_file;
    attr_obj->ref_count = 1; /* Initialize attribute's own ref count */
    /* Increment file reference count since this attribute holds a reference */
    attr_obj->parent_file->ref_count++;
    /* Increment parent object's reference count since this attribute holds a reference */
    parent_obj->ref_count++;
    attr = &attr_obj->u.attr;

    if ((attr->name = bufr_strdup(key_view)) == NULL)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTALLOC, NULL, "Failed to duplicate attribute name string");

    attr->parent = obj;
    attr->space_id = H5I_INVALID_HID;
    attr->type_id = H5I_INVALID_HID;
    attr->data = NULL;
    attr->data_size =
        0; /* The value is set to the size of the data buffer; see bufr_read_data function below */

    /* Fast open using message offsets */
    h = bufr_open_message_by_index(file, (size_t) msg_index);
    if (!h) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "Failed to get BUFR message handle ");
    }
    /* Unpack the  message data */
    if (codes_set_long(h, "unpack", 1) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "Failed to unpack BUFR message handle");
    }

    msg = (bufr_message_t *) calloc(1, sizeof(*msg));
    if (!msg) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL,
                        "Failed to initialize message handle for dataset object");
    }
    msg->h = h;
    attr->msg = msg;

    /* Find BUFR datatype and size (replication) for the key */
    if (bufr_key_type_and_size(h, key_view, &message_type, &len) != 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL,
                        "Failed to discover datatype and size for the key");
    }

    /* We allow to read as attributes only the keys with size 1 (not arrays) */
    if (len != 1) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL,
                        "Provided key is an array; use H5Dopen instead");
    }

    attr->nvals = 1;

    /* Find corresponding HDF5 native type; set a special flag if BUFR type is string */
    /* EP-later need to add for BYTE type too */
    attr->codes_type = message_type;
    if (attr->codes_type == CODES_TYPE_STRING)
        attr->is_vlen_string = 1;
    if (bufr_get_hdf5_type(attr->codes_type, &attr->type_id) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL,
                        "Failed to covert BUFR datatype to native HDF5 type");
    }

    attr->space_id = H5Screate(H5S_SCALAR);
    if (attr->space_id == H5I_INVALID_HID) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL,
                        "Failed to create SCALAR dataspace for attribute");
    }
    /* Cache the key data */
    if (bufr_read_attr_data(attr) != 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "Failed to read data for the key");

    ret_value = attr_obj;

done:
    if (!ret_value && attr_obj) {
        H5E_BEGIN_TRY
        {
            bufr_attr_close(attr_obj, dxpl_id, req);
        }
        H5E_END_TRY;
    }
    return ret_value;
}

herr_t bufr_attr_close(void *attr, hid_t dxpl_id, void **req)
{
    bufr_object_t *o = (bufr_object_t *) attr;
    bufr_attr_t *a = &o->u.attr;
    bufr_object_t *parent_obj = NULL;
    herr_t ret_value = SUCCEED;

    assert(a);

    /* Decrement attribute's ref count */
    if (o->ref_count == 0)
        FUNC_DONE_ERROR(H5E_ATTR, H5E_CANTCLOSEOBJ, FAIL,
                        "Attribute already closed (ref_count is 0)");

    o->ref_count--;

    /* Only do the real close when ref_count reaches 0 */
    if (o->ref_count == 0) {

        /* Free cached data */
        bufr_free_cached_attr_data(a);

        /* Use FUNC_DONE_ERROR to try to complete resource release after failure */
        if (a->name)
            free(a->name);
        if (a->space_id != H5I_INVALID_HID)
            if (H5Sclose(a->space_id) < 0)
                FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                "Failed to close attribute dataspace");
        /* Only close type_id if it's not a predefined type (like H5T_NATIVE_*) */
        if ((a->type_id != H5I_INVALID_HID) && a->is_vlen_string)
            if (H5Tclose(a->type_id) < 0)
                FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                "Failed to close attribute datatype");

        /* Close parent object (dataset, group, or file) */
        parent_obj = (bufr_object_t *) a->parent;
        if (parent_obj) {
            switch (parent_obj->obj_type) {
                case H5I_FILE:
                    if (bufr_file_close(parent_obj, dxpl_id, req) < 0)
                        FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                        "Failed to close attribute's parent file");
                    break;
                /* EP-later Not implemented yet
                case H5I_DATASET:
                    if (bufr_dataset_close(parent_obj, dxpl_id, req) < 0)
                        FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                        "Failed to close attribute's parent dataset");
                    break;
                case H5I_GROUP:
                    if (bufr_group_close(parent_obj, dxpl_id, req) < 0)
                        FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                        "Failed to close attribute's parent group");
                    break;
                */
                default:
                    FUNC_DONE_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Invalid parent object type");
            }
        }

        /* Also decrement the file reference count */
        if (bufr_file_close(o->parent_file, dxpl_id, req) < 0)
            FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                            "Failed to close attribute file object");

        free(o);
    }

    return ret_value;
}

herr_t bufr_attr_read(void *attr, hid_t __attribute__((unused)) mem_type_id, void *buf,
                      hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    const bufr_object_t *o = (const bufr_object_t *) attr;
    const bufr_attr_t *a = NULL; /* Convenience pointer */
    htri_t types_equal = 0;
    herr_t ret_value = SUCCEED;

    assert(o);

    a = &o->u.attr;

    if (!buf)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Invalid attribute or buffer");

    /* Check if memory datatype corresponds to BUFR datatype before transferring data */
    /* EP-later: add type conversion */
    if ((types_equal = H5Tequal(mem_type_id, a->type_id)) <= 0)
        FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_BADVALUE, FAIL,
                        "failed to compare datatypes or datatypes are different");
    memcpy(buf, a->data, a->data_size);

done:
    return ret_value;
}

// cppcheck-suppress constParameterCallback
herr_t bufr_attr_get(void *obj, H5VL_attr_get_args_t *args, hid_t __attribute__((unused)) dxpl_id,
                     void __attribute__((unused)) * *req)
{
    const bufr_object_t *o = (const bufr_object_t *) obj;
    const bufr_attr_t *a = &o->u.attr;

    herr_t ret_value = SUCCEED;

    switch (args->op_type) {
        case H5VL_ATTR_GET_SPACE:
            if ((args->args.get_space.space_id = H5Scopy(a->space_id)) < 0)
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTGET, FAIL, "Failed to copy attribute dataspace");
            break;
        case H5VL_ATTR_GET_TYPE:
            if ((args->args.get_type.type_id = H5Tcopy(a->type_id)) < 0)
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTGET, FAIL, "Failed to copy attribute datatype");
            break;
        default:
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_UNSUPPORTED, FAIL, "Unsupported attribute get operation");
            break;
    }

done:
    return ret_value;
}

/* These two functions are necessary to load this plugin using  the HDF5 library */
H5PL_type_t H5PLget_plugin_type(void)
{
    return H5PL_TYPE_VOL;
}
const void *H5PLget_plugin_info(void)
{
    return &bufr_class_g;
}

/*---------------------------------------------------------------------------
 * Function:    bufr_introspect_opt_query
 *
 * Purpose:     Query if an optional operation is supported by this connector
 *
 * Returns:     SUCCEED (Can't fail)
 *
 *---------------------------------------------------------------------------
 */
herr_t bufr_introspect_opt_query(void __attribute__((unused)) * obj, H5VL_subclass_t subcls,
                                 int opt_type, uint64_t __attribute__((unused)) * flags)
{
    /* We don't support any optional operations */
    (void) subcls;
    (void) opt_type;
    *flags = 0;
    return SUCCEED;
}

herr_t bufr_introspect_get_conn_cls(void __attribute__((unused)) * obj,
                                    H5VL_get_conn_lvl_t __attribute__((unused)) lvl,
                                    const H5VL_class_t __attribute__((unused)) * *conn_cls)
{
    herr_t ret_value = SUCCEED;

    assert(conn_cls);

    /* Retrieve the VOL connector class */
    *conn_cls = &bufr_class_g;

    return ret_value;
}

herr_t bufr_init_connector(hid_t __attribute__((unused)) vipl_id)
{
    herr_t ret_value = SUCCEED;

    /* Register the connector with HDF5's error reporting API */
    if ((H5_bufr_err_class_g = H5Eregister_class(HDF5_VOL_BUFR_ERR_CLS_NAME, HDF5_VOL_BUFR_LIB_NAME,
                                                 HDF5_VOL_BUFR_LIB_VER)) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "can't register with HDF5 error API");

    /* Create a separate error stack for the BUFR VOL to report errors with */
    if ((H5_bufr_err_stack_g = H5Ecreate_stack()) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "can't create error stack");

    /* Set up a few BUFR VOL-specific error API message classes */
    if ((H5_bufr_obj_err_maj_g =
             H5Ecreate_msg(H5_bufr_err_class_g, H5E_MAJOR, "Object interface")) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL,
                        "can't create error message for object interface");

    /* Initialized */
    H5_bufr_initialized_g = TRUE;

done:
    if (ret_value < 0)
        bufr_term_connector();

    return ret_value;
}

herr_t bufr_term_connector(void)
{
    herr_t ret_value = SUCCEED;

    /* Unregister from the HDF5 error API */
    if (H5_bufr_err_class_g >= 0) {
        if (H5_bufr_obj_err_maj_g >= 0 && H5Eclose_msg(H5_bufr_obj_err_maj_g) < 0)
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL,
                            "can't unregister error message for object interface");
        if (H5Eunregister_class(H5_bufr_err_class_g) < 0)
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "can't unregister from HDF5 error API");

        /* Print the current error stack before destroying it */
        PRINT_ERROR_STACK;

        /* Destroy the error stack */
        if (H5Eclose_stack(H5_bufr_err_stack_g) < 0) {
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "can't close error stack");
            PRINT_ERROR_STACK;
        }

        H5_bufr_err_stack_g = H5I_INVALID_HID;
        H5_bufr_err_class_g = H5I_INVALID_HID;
        H5_bufr_obj_err_maj_g = H5I_INVALID_HID;
    }

    return ret_value;
}
