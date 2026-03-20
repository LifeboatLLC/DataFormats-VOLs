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
#include <ctype.h>
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

/* Identifier for HDF5's error API */
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
        NULL,               /* create       */
        bufr_attr_open,     /* open         */
        bufr_attr_read,     /* read         */
        NULL,               /* write        */
        bufr_attr_get,      /* get          */
        bufr_attr_specific, /* specific     */
        NULL,               /* optional     */
        bufr_attr_close     /* close        */
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
        NULL,            /* create       */
        bufr_group_open, /* open         */
        bufr_group_get,  /* get          */
        NULL,            /* specific     */
        NULL,            /* optional     */
        bufr_group_close /* close        */
    },
    {
        /* link_cls */
        NULL,               /* create       */
        NULL,               /* copy         */
        NULL,               /* move         */
        NULL,               /* get          */
        bufr_link_specific, /* specific     */
        NULL                /* optional     */
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
    if (errno != 0 || end == p || n < 0) {
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
    printf("From parse_message_key_path message index is %ld \n", n);

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
static codes_handle *bufr_open_message_by_index(bufr_file_t *bf, size_t msg_index_0based)
{
    if (!bf || !bf->bufr || !bf->msg_offsets)
        return NULL;
    if (msg_index_0based < 0 || msg_index_0based > (bf->nmsgs - 1))
        return NULL;

    long saved = ftell(bf->bufr);
    if (saved < 0)
        saved = 0;

    long off = bf->msg_offsets[msg_index_0based];
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

    printf("In bufr_read_data \n");
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
    printf("From dataset read nvals = %zu codes_type is %d \n", nvals, dset->codes_type);
    return ret_value;
}

/* Helper function to free cached data */
static void bufr_free_cached_data(bufr_dataset_t *dset)
{
    assert(dset);

    if (!dset->data)
        return;

    if (dset->is_vlen_string) {
        if (dset->nvals <= 1) {
            /* scalar string: data is char * */
            H5free_memory(dset->data);
        } else {
            /* array of strings: data is char ** */
            char **arr = (char **) dset->data;

            for (size_t i = 0; i < dset->nvals; i++) {
                if (arr[i])
                    H5free_memory(arr[i]);
            }
            free(arr);
        }
    } else {
        free(dset->data);
    }

    dset->data = NULL;
    dset->data_size = 0;
    dset->nvals = 0;
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
/*
 * Helper function to check valid group names
 *
 * Accepts:
 *    "/message_<N>"
 *    "message_<N>"
 *    "//message_<N>"   (also allowed)
 *
 * Returns:
 *    true if  N   0 <= N <= K-1
 *    false  otherwise
 */
static bool is_group_name(const char *s, size_t K, size_t *idx_out)
{
    static const char prefix[] = "message_";
    const size_t prefix_len = sizeof(prefix) - 1;
    const char *p = s;
    size_t n = 0;

    if (!s || !idx_out || K == 0)
        return false;

    while (*p == '/')
        p++;

    if (strncmp(p, prefix, prefix_len) != 0)
        return false;

    p += prefix_len;

    if (!isdigit((unsigned char) *p))
        return false;

    while (isdigit((unsigned char) *p)) {
        size_t digit = (size_t) (*p - '0');

        if (n > (SIZE_MAX - digit) / 10)
            return false;

        n = n * 10 + digit;

        if (n >= K)
            return false;

        p++;
    }

    if (*p != '\0')
        return false;

    *idx_out = n;
    return true;
}

/*-------------------------------------------------------------------------
 * Function:    bufr_make_hdf5_type_for_codes_type
 *
 * Purpose:     Create an HDF5 memory datatype corresponding to an ecCodes
 *              datatype.
 *
 * Parameters:
 *              codes_type      ecCodes datatype (CODES_TYPE_*)
 *              is_vlen_string  whether string should use VL semantics
 *
 * Return:      HDF5 datatype ID on success
 *              H5I_INVALID_HID on failure
 *
 *-------------------------------------------------------------------------
 */
static hid_t bufr_make_hdf5_type_for_codes_type(int codes_type, bool is_vlen_string)
{
    hid_t new_type = H5I_INVALID_HID;

    switch (codes_type) {

        case CODES_TYPE_LONG:
            if ((new_type = H5Tcopy(H5T_NATIVE_LONG)) == H5I_INVALID_HID)
                goto error;
            break;

        case CODES_TYPE_DOUBLE:
            if ((new_type = H5Tcopy(H5T_NATIVE_DOUBLE)) == H5I_INVALID_HID)
                goto error;
            break;

        case CODES_TYPE_BYTES:
            if ((new_type = H5Tcopy(H5T_NATIVE_UCHAR)) == H5I_INVALID_HID)
                goto error;
            break;

        case CODES_TYPE_STRING:
            if ((new_type = H5Tcopy(H5T_C_S1)) == H5I_INVALID_HID)
                goto error;

            if (is_vlen_string) {
                if (H5Tset_size(new_type, H5T_VARIABLE) < 0)
                    goto error;
            }
            break;

        default:
            goto error;
    }

    return new_type;

error:

    if (new_type != H5I_INVALID_HID) {
        H5E_BEGIN_TRY
        {
            H5Tclose(new_type);
        }
        H5E_END_TRY;
    }

    return H5I_INVALID_HID;
}

/* Group operations */
void *bufr_group_open(void *obj, const H5VL_loc_params_t __attribute__((unused)) * loc_params,
                      const char *name, hid_t __attribute__((unused)) gapl_id,
                      hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    bufr_object_t *o = (bufr_object_t *) obj;
    bufr_object_t *file = (bufr_object_t *) obj;
    bufr_object_t *grp_obj = NULL;
    bufr_object_t *ret_value = NULL;
    bufr_group_t *grp = NULL; /* Convenience pointer */

    size_t n = 0;
    inv_t *inv; /* Group inventory structure */
    codes_handle *h = NULL;
    bufr_message_t *msg = NULL;

    if (!o || !name)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "Invalid file identifier or group name");
    if (o->obj_type != H5I_FILE)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "Invalid file identifier");

    /* Check for valid group name; it can be 'message_<N>' or '/message_<N>', where N is between
       0 and (number of messages - 1) in the BUFR file stored in file->nmsgs.
     */
    if (!(is_group_name(name, file->u.file.nmsgs, &n)))
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_UNSUPPORTED, NULL, "Invalid group name");

    if ((grp_obj = (bufr_object_t *) calloc(1, sizeof(bufr_object_t))) == NULL)
        FUNC_GOTO_ERROR(H5E_SYM, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for BUFR group struct");

    grp_obj->obj_type = H5I_GROUP;
    grp_obj->parent_file = file->parent_file;
    grp_obj->ref_count = 1; /* Initialize group's own ref count */
    /* Increment file reference count since this group holds a reference */
    grp_obj->parent_file->ref_count++;
    grp_obj->u.group.msg_num = n;

    /* Fast open using message offsets to get message handle */
    h = bufr_open_message_by_index(&file->u.file, n);
    if (!h) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "Failed to get BUFR message handle ");
    }
    /* Unpack the  message data */
    if (codes_set_long(h, "unpack", 1) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "Failed to unpack BUFR message handle");
    }

    msg = (bufr_message_t *) calloc(1, sizeof(*msg));
    if (!msg) {
        FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, NULL,
                        "Failed to allocate message structure for group object");
    }
    msg->h = h;
    grp_obj->u.group.msg = msg;

    grp = &grp_obj->u.group;
    if ((grp->name = strdup(name)) == NULL)
        FUNC_GOTO_ERROR(H5E_SYM, H5E_CANTALLOC, NULL, "Failed to duplicate group name string");

    /* Build group inventory */
    inv = (inv_t *) calloc(1, sizeof(*inv));
    if (!inv) {
        FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, NULL,
                        "Failed to initialize inventory structure for group object");
    }
    if (bufr_build_group_inventory(h, inv) != 0)
        FUNC_GOTO_ERROR(H5E_SYM, H5E_CANTALLOC, NULL, "Failed to create group inventory");

#ifdef BUFR_DEBUG
    bufr_inv_print(inv);
#endif

    grp->inv = inv;

    ret_value = grp_obj;
done:
    if (!ret_value && grp) {
        H5E_BEGIN_TRY
        {
            bufr_group_close(grp_obj, dxpl_id, req);
        }
        H5E_END_TRY;
    }

    return ret_value;
}

herr_t bufr_group_get(void *obj, H5VL_group_get_args_t *args, hid_t __attribute__((unused)) dxpl_id,
                      void __attribute__((unused)) * *req)
{
    bufr_object_t *o = (bufr_object_t *) obj;
    const bufr_group_t *grp = (const bufr_group_t *) &o->u.group; /* Convenience pointer */
    herr_t ret_value = SUCCEED;

    if (!args)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid arguments");

    switch (args->op_type) {
        case H5VL_GROUP_GET_INFO: {
            H5G_info_t *ginfo = args->args.get_info.ginfo;

            if (!grp || !ginfo)
                FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid group or info pointer");

            if (!o->parent_file || !o->parent_file->u.file.bufr)
                FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid file object");

            /* Fill in group info structure */
            ginfo->storage_type = H5G_STORAGE_TYPE_COMPACT;
            ginfo->nlinks = grp->inv->ndatasets;
            break;
        }

        case H5VL_GROUP_GET_GCPL: {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "GCPL get operation not supported");
        }

        default: {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL, "unknown group get operation");
        }
    }

done:
    return ret_value;
}

herr_t bufr_group_close(void *grp, hid_t dxpl_id, void **req)
{
    bufr_object_t *o = (bufr_object_t *) grp;
    bufr_group_t *g = &o->u.group; /* Convenience pointer */
    herr_t ret_value = SUCCEED;

    assert(g);

    /* Use FUNC_DONE_ERROR to try to complete resource release after failure */

    /* Decrement group's ref count */
    if (o->ref_count == 0)
        FUNC_DONE_ERROR(H5E_SYM, H5E_CANTCLOSEOBJ, FAIL, "Group already closed (ref_count is 0)");

    o->ref_count--;

    /* Only do the real close when ref_count reaches 0 */
    if (o->ref_count == 0) {
        if (g->name)
            free(g->name);

        /* Delete BUFR message handle and free message object */
        if (g->msg) {
            if (g->msg->h) {
                codes_handle_delete(g->msg->h);
                g->msg->h = NULL;
            }
            free(g->msg);
            g->msg = NULL;
        }
        if (g->inv)
            bufr_inv_free(g->inv);

        /* Decrement parent file's reference count */
        if (bufr_file_close(o->parent_file, dxpl_id, req) < 0)
            FUNC_DONE_ERROR(H5E_SYM, H5E_CLOSEERROR, FAIL, "Failed to close group object");

        free(o);
    }
done:
    return ret_value;
}

/* Helper function to read dataset data from resolved spec */
static herr_t bufr_read_data_spec(bufr_dataset_t *dset, bufr_dataset_spec_t *spec)
{
    herr_t ret_value = SUCCEED;
    codes_handle *h = NULL;

    assert(dset);
    assert(spec);
    assert(dset->msg);
    assert(dset->msg->h);

    h = dset->msg->h;

    dset->data = NULL;
    dset->nvals = 0;
    dset->data_size = 0;

    if (dset->codes_type == CODES_TYPE_DOUBLE) {
        double *buf = NULL;
        size_t n = 0;

        if (bufr_read_double_dataset(h, spec, &buf, &n) != 0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "Failed to read dataset of doubles");

        dset->data = buf;
        dset->nvals = n;
        dset->data_size = n * sizeof(double);
    } else if (dset->codes_type == CODES_TYPE_LONG) {
        long *buf = NULL;
        size_t n = 0;

        if (bufr_read_long_dataset(h, spec, &buf, &n) != 0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "Failed to read dataset of longs");

        dset->data = buf;
        dset->nvals = n;
        dset->data_size = n * sizeof(long);
    } else if (dset->codes_type == CODES_TYPE_STRING) {
        char **arr = NULL;
        size_t n = 0;

        if (bufr_read_string_dataset(h, spec, &arr, &n) != 0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "Failed to read dataset of strings");

        dset->data = arr;
        dset->nvals = n;
        dset->data_size = n * sizeof(char *);
    } else if (dset->codes_type == CODES_TYPE_BYTES) {
        unsigned char *buf = NULL;
        size_t n = 0;

        if (bufr_read_bytes_dataset(h, spec, &buf, &n) != 0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "Failed to read dataset of bytes");

        dset->data = buf;
        dset->nvals = n;
        dset->data_size = n * sizeof(unsigned char);
    } else {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_BADVALUE, FAIL,
                        "Unsupported ecCodes datatype for dataset");
    }

done:
    return ret_value;
}

/* VOL dataset open callback */
void *bufr_dataset_open(void *obj, const H5VL_loc_params_t __attribute__((unused)) * loc_params,
                        const char *name, hid_t __attribute__((unused)) dapl_id,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    bufr_object_t *o = (bufr_object_t *) obj;
    bufr_object_t *dset_obj = NULL;
    bufr_object_t *ret_value = NULL;
    bufr_dataset_t *dset = NULL;
    bufr_group_t *g = NULL;
    bufr_dataset_spec_t spec;
    codes_handle *h = NULL;

    if (!o || !name)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "Invalid arguments to dataset open");

    if (o->obj_type != H5I_GROUP)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "Dataset open location must be a group");

    g = &o->u.group;

    if ((dset_obj = (bufr_object_t *) calloc(1, sizeof(bufr_object_t))) == NULL)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTALLOC, NULL, "Failed to allocate BUFR dataset object");

    dset = &dset_obj->u.dataset;
    memset(&spec, 0, sizeof(spec));

    if (!g->inv)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_BADVALUE, NULL, "Failed to access group inventory");

    if (!g->msg || !g->msg->h)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_BADVALUE, NULL, "Failed to access ecCodes handle");

    h = g->msg->h;

    if (bufr_parse_hdf5_dataset_name(name, &spec) != 0)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_BADVALUE, NULL, "bufr_parse_hdf5_dataset_name failed");

    if (bufr_resolve_dataset_spec(h, &spec) != 0)
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_BADVALUE, NULL, "bufr_resolve_dataset_spec failed");

    dset_obj->parent_file = o->parent_file;
    dset_obj->obj_type = H5I_DATASET;
    dset_obj->ref_count = 1;

    dset->parent = o; /* remember parent group */
    o->ref_count++;   /* dataset holds one ref on group */

    dset->msg = g->msg;
    dset->inv = g->inv;

    dset->space_id = H5I_INVALID_HID;
    dset->type_id = H5I_INVALID_HID;
    dset->data = NULL;
    dset->data_size = 0; /* This value will be set by bufr_read_data function */
    dset->nvals = 0;     /* This value will be set by bufr_read_data function */
    dset->is_vlen_string = false;

    if ((dset->name = bufr_strdup(name)) == NULL)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL, "Failed to duplicate dataset name");

    dset->codes_type = spec.native_type;
    if (dset->codes_type == CODES_TYPE_STRING)
        dset->is_vlen_string = true;

    dset->type_id = bufr_make_hdf5_type_for_codes_type(dset->codes_type, dset->is_vlen_string);
    if (dset->type_id == H5I_INVALID_HID)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL,
                        "Failed to convert BUFR datatype to HDF5 datatype");

    if (bufr_read_data_spec(dset, &spec) != 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "Failed to cache dataset data");

    if (dset->nvals == 1) {
        dset->space_id = H5Screate(H5S_SCALAR);
        if (dset->space_id == H5I_INVALID_HID)
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL,
                            "Failed to create scalar dataspace for dataset");
    } else {
        hsize_t dims[1];
        dims[0] = (hsize_t) dset->nvals;
        dset->space_id = H5Screate_simple(1, dims, NULL);
        if (dset->space_id == H5I_INVALID_HID)
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL,
                            "Failed to create simple dataspace for dataset");
    }

    ret_value = dset_obj;

done:
    if (!ret_value) {
        if (dset_obj) {
            H5E_BEGIN_TRY
            {
                bufr_dataset_close(dset_obj, dxpl_id, req);
            }
            H5E_END_TRY;
        }
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
    if (d->is_vlen_string) {
        char **dst = (char **) buf[0];
        char **src = (char **) d->data;
        size_t i;

        if (H5Tequal(mem_type_id[0], d->type_id) <= 0)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_UNSUPPORTED, FAIL,
                            "VL string datatype conversion not supported");

        if (!src)
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTGET, FAIL, "no cached VL string data");

        for (i = 0; i < (size_t) num_elements; i++) {
            if (src[i]) {
                size_t len = strlen(src[i]) + 1;
                dst[i] = (char *) H5allocate_memory(len, 0);
                memcpy(dst[i], src[i], len);
            } else {
                dst[i] = NULL;
            }
        }

        FUNC_GOTO_DONE(SUCCEED);
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
    bufr_object_t *parent_obj = NULL;
    herr_t ret_value = SUCCEED;

    assert(d);

    if (d->obj_type != H5I_DATASET)
        FUNC_DONE_ERROR(H5E_DATASET, H5E_BADTYPE, FAIL, "Object is not a dataset");

    if (d->ref_count == 0)
        FUNC_DONE_ERROR(H5E_DATASET, H5E_CANTCLOSEOBJ, FAIL,
                        "Dataset already closed (ref_count is 0)");

    d->ref_count--;

    if (d->ref_count == 0) {
        /* Free cached dataset data */
        bufr_free_cached_data(&d->u.dataset);

        if (d->u.dataset.name) {
            free(d->u.dataset.name);
            d->u.dataset.name = NULL;
        }

        if (d->u.dataset.space_id != H5I_INVALID_HID) {
            if (H5Sclose(d->u.dataset.space_id) < 0)
                FUNC_DONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL,
                                "Failed to close dataset dataspace");
            d->u.dataset.space_id = H5I_INVALID_HID;
        }

        if (d->u.dataset.type_id != H5I_INVALID_HID) {
            if (H5Tclose(d->u.dataset.type_id) < 0)
                FUNC_DONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL,
                                "Failed to close dataset datatype");
            d->u.dataset.type_id = H5I_INVALID_HID;
        }

        /* borrowed from parent group */
        d->u.dataset.msg = NULL;
        d->u.dataset.inv = NULL;

        /* Release parent group, not parent file */
        parent_obj = (bufr_object_t *) d->u.dataset.parent;
        d->u.dataset.parent = NULL;

        if (parent_obj) {
            if (bufr_group_close(parent_obj, dxpl_id, req) < 0)
                FUNC_DONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL,
                                "Failed to close dataset parent group");
        }

        free(d);
    }

done:
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
/* Attribute operations */

/* --------------------------------------------------------------------------
 * Helpers for reading attributes data (see VOL bufr_attr_open callback )
 * -------------------------------------------------------------------------- */

static size_t bufr_codes_type_elem_size(int codes_type)
{
    switch (codes_type) {
        case CODES_TYPE_LONG:
            return sizeof(long);
        case CODES_TYPE_DOUBLE:
            return sizeof(double);
        case CODES_TYPE_BYTES:
            return sizeof(unsigned char);
        default:
            return 0;
    }
}

static void bufr_free_attr_cached_data(bufr_attr_t *attr)
{
    if (!attr || !attr->data)
        return;

    if (attr->is_vlen_string) {
        if (attr->nvals <= 1) {
            free(attr->data);
        } else {
            char **arr = (char **) attr->data;
            for (size_t i = 0; i < attr->nvals; i++)
                free(arr[i]);
            free(arr);
        }
    } else {
        free(attr->data);
    }

    attr->data = NULL;
    attr->data_size = 0;
    attr->nvals = 0;
}

static void bufr_attr_reset_cache_fields(bufr_attr_t *attr)
{
    if (!attr)
        return;

    attr->data = NULL;
    attr->data_size = 0;
    attr->nvals = 0;
    attr->is_vlen_string = false;
}

static int bufr_read_one_numeric_or_bytes(codes_handle *h, const char *key, int codes_type,
                                          size_t hinted_nvals, void **out_data, size_t *out_nvals,
                                          size_t *out_nbytes)
{
    int err = 0;
    size_t n = hinted_nvals;
    void *tmp = NULL;
    size_t elem_size;

    if (!h || !key || !out_data || !out_nvals || !out_nbytes)
        return -1;

    *out_data = NULL;
    *out_nvals = 0;
    *out_nbytes = 0;

    if (n == 0) {
        err = codes_get_size(h, key, &n);
        if (err != CODES_SUCCESS)
            return err;
    }

    if (n == 0)
        n = 1;

    elem_size = bufr_codes_type_elem_size(codes_type);
    if (elem_size == 0)
        return -1;

    switch (codes_type) {
        case CODES_TYPE_LONG:
            tmp = xmalloc(n * sizeof(long));
            err = codes_get_long_array(h, key, (long *) tmp, &n);
            break;

        case CODES_TYPE_DOUBLE:
            tmp = xmalloc(n * sizeof(double));
            err = codes_get_double_array(h, key, (double *) tmp, &n);
            break;

        case CODES_TYPE_BYTES:
            tmp = xmalloc(n * sizeof(unsigned char));
            err = codes_get_bytes(h, key, (unsigned char *) tmp, &n);
            break;

        default:
            return -1;
    }

    if (err != CODES_SUCCESS) {
        free(tmp);
        return err;
    }

    *out_data = tmp;
    *out_nvals = n;
    *out_nbytes = n * elem_size;

    return CODES_SUCCESS;
}

static int bufr_read_one_string_attr(codes_handle *h, const char *key, void **out_data,
                                     size_t *out_nvals, size_t *out_nbytes)
{
    int err = 0;
    char *s = NULL;

    if (!h || !key || !out_data || !out_nvals || !out_nbytes)
        return -1;

    *out_data = NULL;
    *out_nvals = 0;
    *out_nbytes = 0;

    err = read_one_string(h, key, &s);
    if (err != CODES_SUCCESS) {
        free(s);
        return err;
    }

    *out_data = s;
    *out_nvals = 1;
    *out_nbytes = sizeof(char *);

    return CODES_SUCCESS;
}

static int bufr_count_replicated_attr_values(codes_handle *h, inv_attr_t *a, inv_dataset_t *d,
                                             size_t *total_nvals, size_t *num_defined_keys)
{
    int err = 0;

    if (!h || !a || !d || !total_nvals || !num_defined_keys)
        return -1;

    *total_nvals = 0;
    *num_defined_keys = 0;

    for (int i = 1; i <= d->rep_count; i++) {
        char key[1024];
        size_t n = a->size;

        snprintf(key, sizeof(key), "#%d#%s->%s", i, d->name, a->name);

        if (!codes_is_defined(h, key))
            continue;

        (*num_defined_keys)++;

        if (a->native_type == CODES_TYPE_STRING) {
            /* One string per defined occurrence */
            (*total_nvals)++;
        } else {
            if (n == 0) {
                err = codes_get_size(h, key, &n);
                if (err != CODES_SUCCESS)
                    return err;
            }

            if (n == 0)
                n = 1;

            *total_nvals += n;
        }
    }

    return CODES_SUCCESS;
}

static void bufr_free_partial_string_array(char **arr, size_t used)
{
    if (!arr)
        return;

    for (size_t i = 0; i < used; i++)
        free(arr[i]);

    free(arr);
}

static int bufr_read_replicated_numeric_or_bytes(codes_handle *h, inv_attr_t *a, inv_dataset_t *d,
                                                 void **out_data, size_t *out_nvals,
                                                 size_t *out_nbytes)
{
    int err = 0;
    size_t total = 0, ndefined = 0, offset = 0, elem_size = 0;
    void *buf = NULL;

    if (!h || !a || !d || !out_data || !out_nvals || !out_nbytes)
        return -1;

    *out_data = NULL;
    *out_nvals = 0;
    *out_nbytes = 0;

    err = bufr_count_replicated_attr_values(h, a, d, &total, &ndefined);
    if (err != CODES_SUCCESS)
        return err;

    if (ndefined == 0 || total == 0)
        return CODES_NOT_FOUND;

    elem_size = bufr_codes_type_elem_size(a->native_type);
    if (elem_size == 0)
        return -1;

    buf = xmalloc(total * elem_size);

    for (int i = 1; i <= d->rep_count; i++) {
        char key[1024];
        size_t n = a->size;

        snprintf(key, sizeof(key), "#%d#%s->%s", i, d->name, a->name);

        if (!codes_is_defined(h, key))
            continue;

        if (n == 0) {
            err = codes_get_size(h, key, &n);
            if (err != CODES_SUCCESS) {
                free(buf);
                return err;
            }
        }

        if (n == 0)
            n = 1;

        switch (a->native_type) {
            case CODES_TYPE_LONG:
                err = codes_get_long_array(h, key, ((long *) buf) + offset, &n);
                break;

            case CODES_TYPE_DOUBLE:
                err = codes_get_double_array(h, key, ((double *) buf) + offset, &n);
                break;

            case CODES_TYPE_BYTES:
                err = codes_get_bytes(h, key, ((unsigned char *) buf) + offset, &n);
                break;

            default:
                free(buf);
                return -1;
        }

        if (err != CODES_SUCCESS) {
            free(buf);
            return err;
        }

        offset += n;
    }

    *out_data = buf;
    *out_nvals = total;
    *out_nbytes = total * elem_size;

    return CODES_SUCCESS;
}

static int bufr_read_replicated_strings(codes_handle *h, inv_attr_t *a, inv_dataset_t *d,
                                        void **out_data, size_t *out_nvals, size_t *out_nbytes)
{
    int err = 0;
    size_t total = 0, ndefined = 0, used = 0;
    char **arr = NULL;

    if (!h || !a || !d || !out_data || !out_nvals || !out_nbytes)
        return -1;

    *out_data = NULL;
    *out_nvals = 0;
    *out_nbytes = 0;

    err = bufr_count_replicated_attr_values(h, a, d, &total, &ndefined);
    if (err != CODES_SUCCESS)
        return err;

    if (ndefined == 0 || total == 0)
        return CODES_NOT_FOUND;

    arr = (char **) xmalloc(total * sizeof(char *));
    memset(arr, 0, total * sizeof(char *));

    for (int i = 1; i <= d->rep_count; i++) {
        char key[1024];
        char *s = NULL;

        snprintf(key, sizeof(key), "#%d#%s->%s", i, d->name, a->name);

        if (!codes_is_defined(h, key))
            continue;

        err = read_one_string(h, key, &s);
        if (err != CODES_SUCCESS) {
            bufr_free_partial_string_array(arr, used);
            return err;
        }

        arr[used++] = s;
    }

    *out_data = arr;
    *out_nvals = used;
    *out_nbytes = used * sizeof(char *);

    return CODES_SUCCESS;
}

/* --------------------------------------------------------------------------
 * Read one group attribute from BUFR/ecCodes into attr cache.
 * -------------------------------------------------------------------------- */
static int bufr_read_group_attr_inv(codes_handle *h, const char *key, bufr_attr_t *attr)
{
    int err = 0;
    void *tmp_data = NULL;
    size_t tmp_nvals = 0;
    size_t tmp_nbytes = 0;

    if (!h || !key || !attr)
        return -1;

    bufr_attr_reset_cache_fields(attr);
    attr->is_vlen_string = (attr->codes_type == CODES_TYPE_STRING);

    if (attr->codes_type == CODES_TYPE_STRING) {
        err = bufr_read_one_string_attr(h, key, &tmp_data, &tmp_nvals, &tmp_nbytes);
        if (err != CODES_SUCCESS)
            return err;
    } else {
        err = bufr_read_one_numeric_or_bytes(h, key, attr->codes_type, 0, &tmp_data, &tmp_nvals,
                                             &tmp_nbytes);
        if (err != CODES_SUCCESS)
            return err;
    }

    attr->data = tmp_data;
    attr->nvals = tmp_nvals;
    attr->data_size = tmp_nbytes;

    return CODES_SUCCESS;
}

/* --------------------------------------------------------------------------
 * Read one dataset attribute from inventory/ecCodes into attr cache.
 * For replicated datasets, flatten all defined #i#dataset->attr values.
 * -------------------------------------------------------------------------- */
static int bufr_read_dataset_attr_inv(codes_handle *h, inv_attr_t *a, inv_dataset_t *d,
                                      bufr_attr_t *attr)
{
    int err = 0;
    char key[1024];
    void *tmp_data = NULL;
    size_t tmp_nvals = 0;
    size_t tmp_nbytes = 0;

    if (!h || !a || !d || !attr)
        return -1;

    bufr_attr_reset_cache_fields(attr);
    attr->codes_type = a->native_type;
    attr->is_vlen_string = (a->native_type == CODES_TYPE_STRING);

    if (d->is_replicated) {
        if (a->native_type == CODES_TYPE_STRING) {
            err = bufr_read_replicated_strings(h, a, d, &tmp_data, &tmp_nvals, &tmp_nbytes);
            if (err != CODES_SUCCESS)
                return err;
        } else {
            err =
                bufr_read_replicated_numeric_or_bytes(h, a, d, &tmp_data, &tmp_nvals, &tmp_nbytes);
            if (err != CODES_SUCCESS)
                return err;
        }
    } else {
        snprintf(key, sizeof(key), "%s->%s", d->name, a->name);

        if (!codes_is_defined(h, key))
            return CODES_NOT_FOUND;

        if (a->native_type == CODES_TYPE_STRING) {
            err = bufr_read_one_string_attr(h, key, &tmp_data, &tmp_nvals, &tmp_nbytes);
            if (err != CODES_SUCCESS)
                return err;
        } else {
            err = bufr_read_one_numeric_or_bytes(h, key, a->native_type, a->size, &tmp_data,
                                                 &tmp_nvals, &tmp_nbytes);
            if (err != CODES_SUCCESS)
                return err;
        }
    }

    attr->data = tmp_data;
    attr->nvals = tmp_nvals;
    attr->data_size = tmp_nbytes;

    return CODES_SUCCESS;
}

/* --------------------------------------------------------------------------
 * Open BUFR attribute object and cache its contents.
 * Assumes:
 *   - group object:   o->u.group->inv, o->u.group->msg->h
 *   - dataset object: o->u.dataset->inv, o->u.dataset->msg->h, o->u.dataset->name
 * -------------------------------------------------------------------------- */
void *bufr_attr_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                     hid_t __attribute__((unused)) aapl_id, hid_t __attribute__((unused)) dxpl_id,
                     void __attribute__((unused)) * *req)
{
    bufr_object_t *o = (bufr_object_t *) obj;
    bufr_object_t *attr_obj = NULL;
    bufr_object_t *ret_value = NULL;
    bufr_attr_t *attr = NULL;
    codes_handle *h = NULL;
    inv_t *inv = NULL;

    if (!o || !loc_params || !name)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, NULL, "invalid argument to attribute open");

    if (loc_params->type != H5VL_OBJECT_BY_SELF)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_UNSUPPORTED, NULL, "unsupported attribute open location");

    if ((o->obj_type != H5I_GROUP) && (o->obj_type != H5I_DATASET))
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_UNSUPPORTED, NULL,
                        "attributes are supported only on BUFR groups and datasets");

    if (NULL == (attr_obj = (bufr_object_t *) calloc(1, sizeof(bufr_object_t))))
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTALLOC, NULL, "failed to allocate BUFR attribute object");

    attr_obj->obj_type = H5I_ATTR;
    attr_obj->ref_count = 1;
    attr_obj->parent_file = o->parent_file;

    attr = &attr_obj->u.attr;
    memset(attr, 0, sizeof(*attr));

    attr->name = strdup(name);
    attr->parent = obj;
    if (!attr->name)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTALLOC, NULL, "failed to copy attribute name");

    /* Keep parent object alive while attribute object exists */
    o->ref_count++;

    if (o->obj_type == H5I_GROUP) {
        const inv_attr_t *a = NULL;

        bufr_group_t *g = NULL;
        g = &o->u.group;

        if (!g || !g->msg || !g->msg->h || !g->inv)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, NULL, "invalid BUFR group object");

        inv = g->inv;
        h = g->msg->h;

        for (size_t i = 0; i < inv->ngroup_attrs; i++) {
            if (strcmp(inv->group_attrs[i].name, name) == 0) {
                a = &inv->group_attrs[i];
                break;
            }
        }

        if (!a)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_NOTFOUND, NULL, "group attribute not found in inventory");

        if (!codes_is_defined(h, name))
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_NOTFOUND, NULL, "group attribute key not defined");

        attr->codes_type = a->native_type;
        attr->is_vlen_string = (a->native_type == CODES_TYPE_STRING);

        if (bufr_read_group_attr_inv(h, name, attr) != CODES_SUCCESS)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTGET, NULL, "failed to read group attribute");
    } else if (o->obj_type == H5I_DATASET) {
        const inv_dataset_t *d = NULL;
        const char *dataset_name = NULL;
        const inv_dset_attr_t *a = NULL;

        bufr_dataset_t *ds = NULL;
        ds = &o->u.dataset;

        if (!ds || !ds->msg || !ds->msg->h || !ds->inv || !ds->name)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, NULL, "invalid BUFR dataset object");

        inv = ds->inv;
        h = ds->msg->h;
        dataset_name = ds->name;

        for (size_t i = 0; i < inv->ndatasets; i++) {
            if (strcmp(inv->datasets[i].name, dataset_name) == 0) {
                d = &inv->datasets[i];
                break;
            }
        }

        if (!d)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_NOTFOUND, NULL, "parent dataset not found in inventory");

        for (size_t i = 0; i < d->nattrs; i++) {
            if (strcmp(d->attrs[i].name, name) == 0) {
                a = &d->attrs[i];
                break;
            }
        }

        if (!a)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_NOTFOUND, NULL,
                            "dataset attribute not found in inventory");

        if (bufr_read_dataset_attr_inv(h, (inv_attr_t *) a, (inv_dataset_t *) d, attr) !=
            CODES_SUCCESS)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTGET, NULL, "failed to read dataset attribute");
    }

    attr->type_id = bufr_make_hdf5_type_for_codes_type(attr->codes_type, attr->is_vlen_string);
    if (attr->type_id == H5I_INVALID_HID)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTCREATE, NULL,
                        "failed to create HDF5 datatype for attribute");

    if (attr->nvals <= 1) {
        attr->space_id = H5Screate(H5S_SCALAR);
        if (attr->space_id == H5I_INVALID_HID)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTCREATE, NULL, "failed to create scalar dataspace");
    } else {
        hsize_t dims[1];
        dims[0] = (hsize_t) attr->nvals;

        attr->space_id = H5Screate_simple(1, dims, NULL);
        if (attr->space_id == H5I_INVALID_HID)
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTCREATE, NULL, "failed to create simple dataspace");
    }

    ret_value = attr_obj;

done:
    if (!ret_value) {
        if (attr_obj) {
            if (attr_obj->u.attr.space_id > 0)
                H5Sclose(attr_obj->u.attr.space_id);
            if (attr_obj->u.attr.type_id > 0)
                H5Tclose(attr_obj->u.attr.type_id);

            free(attr_obj);
        }

        if (o && o->ref_count > 0)
            o->ref_count--;
    }

    return ret_value;
}

/* --------------------------------------------------------------------------
 * Local helpers for attr_specific function
 * -------------------------------------------------------------------------- */

/* Best-effort size to report through H5A_info_t::data_size from inventory.
 * For VL strings we report pointer payload size, not character heap bytes.
 */
static size_t bufr_attr_info_data_size_from_group_attr(const inv_attr_t *a)
{
    size_t n = 0;

    if (!a)
        return 0;

    n = (a->size > 0) ? a->size : 1;

    switch (a->native_type) {
        case CODES_TYPE_LONG:
            return n * sizeof(long);

        case CODES_TYPE_DOUBLE:
            return n * sizeof(double);

        case CODES_TYPE_BYTES:
            return n * sizeof(unsigned char);

        case CODES_TYPE_STRING:
            return sizeof(char *);

        default:
            return 0;
    }
}

static size_t bufr_attr_info_data_size_from_dset_attr(const inv_dset_attr_t *a,
                                                      const inv_dataset_t *d)
{
    size_t n = 0;

    if (!a)
        return 0;

    if (a->native_type == CODES_TYPE_STRING) {
        if (a->per_occurrence && d && d->is_replicated)
            return ((a->rep_count > 0) ? (size_t) a->rep_count : (size_t) 1) * sizeof(char *);
        else
            return sizeof(char *);
    }

    n = (a->size > 0) ? a->size : 1;

    if (a->per_occurrence && d && d->is_replicated) {
        size_t rep = (a->rep_count > 0) ? (size_t) a->rep_count : (size_t) 1;
        n *= rep;
    }

    switch (a->native_type) {
        case CODES_TYPE_LONG:
            return n * sizeof(long);

        case CODES_TYPE_DOUBLE:
            return n * sizeof(double);

        case CODES_TYPE_BYTES:
            return n * sizeof(unsigned char);

        default:
            return 0;
    }
}

static const inv_dataset_t *bufr_find_inventory_dataset(const inv_t *inv, const char *dataset_name)
{
    if (!inv || !dataset_name)
        return NULL;

    for (size_t i = 0; i < inv->ndatasets; i++) {
        if (strcmp(inv->datasets[i].name, dataset_name) == 0)
            return &inv->datasets[i];
    }

    return NULL;
}

static bool bufr_group_attr_exists_in_inventory(const inv_t *inv, const char *attr_name)
{
    if (!inv || !attr_name)
        return false;

    for (size_t i = 0; i < inv->ngroup_attrs; i++) {
        if (strcmp(inv->group_attrs[i].name, attr_name) == 0)
            return true;
    }

    return false;
}

static bool bufr_dataset_attr_exists_in_inventory(const inv_dataset_t *d, const char *attr_name)
{
    if (!d || !attr_name)
        return false;

    for (size_t i = 0; i < d->nattrs; i++) {
        if (strcmp(d->attrs[i].name, attr_name) == 0)
            return true;
    }

    return false;
}

/* --------------------------------------------------------------------------
 * VOL attribute specific callback
 * -------------------------------------------------------------------------- */
/* cppcheck-suppress constParameterCallback */
herr_t bufr_attr_specific(void *obj, const H5VL_loc_params_t *loc_params,
                          H5VL_attr_specific_args_t *args, hid_t __attribute__((unused)) dxpl_id,
                          void __attribute__((unused)) * *req)
{
    bufr_object_t *o = (bufr_object_t *) obj;
    herr_t ret_value = SUCCEED;
    hid_t loc_id = H5I_INVALID_HID;

    if (!obj || !loc_params || !args)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "Invalid arguments to attr_specific");

    if ((o->obj_type != H5I_GROUP) && (o->obj_type != H5I_DATASET))
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_UNSUPPORTED, FAIL,
                        "Attributes are supported only on BUFR groups and datasets");

    switch (args->op_type) {
        case H5VL_ATTR_EXISTS: {
            const char *attr_name = args->args.exists.name;

            if (!args->args.exists.exists)
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "NULL exists output pointer");

            *args->args.exists.exists = false;

            if (!attr_name)
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "No attribute name provided");

            if (loc_params->type != H5VL_OBJECT_BY_SELF)
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL,
                                "Unexpected location type for attribute existence check");

            if (o->obj_type == H5I_GROUP) {
                bufr_group_t *g = &o->u.group;
                if (!g || !g->inv)
                    FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL,
                                    "Invalid group object or missing inventory");

                *args->args.exists.exists = bufr_group_attr_exists_in_inventory(g->inv, attr_name);
            } else if (o->obj_type == H5I_DATASET) {
                const inv_dataset_t *d = NULL;
                bufr_dataset_t *ds = &o->u.dataset;

                if (!ds || !ds->inv || !ds->name)
                    FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL,
                                    "Invalid dataset object or missing inventory");

                d = bufr_find_inventory_dataset(ds->inv, ds->name);
                if (!d)
                    FUNC_GOTO_ERROR(H5E_ATTR, H5E_NOTFOUND, FAIL,
                                    "Failed to find dataset in inventory");

                *args->args.exists.exists = bufr_dataset_attr_exists_in_inventory(d, attr_name);
            } else {
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Invalid object type");
            }

            break;
        }

        case H5VL_ATTR_ITER: {
            H5VL_attr_iterate_args_t *iter_args = &args->args.iterate;
            H5A_info_t attr_info;
            int cb_ret = 0;
            hsize_t start_idx = 0;

            if (!iter_args || !iter_args->op || !iter_args->idx)
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Invalid iterate arguments");

            if (loc_params->type != H5VL_OBJECT_BY_SELF)
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL,
                                "Unexpected location type for attribute iteration");

            loc_id = H5VLwrap_register(obj, o->obj_type);
            if (loc_id == H5I_INVALID_HID)
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, FAIL,
                                "Could not register object for attribute iteration");

            start_idx = *iter_args->idx;

            if (o->obj_type == H5I_GROUP) {
                inv_t *inv = NULL;
                bufr_group_t *g = &o->u.group;

                if (!g || !g->inv)
                    FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL,
                                    "Invalid group object or missing inventory");

                inv = g->inv;

                for (size_t i = (size_t) start_idx; i < inv->ngroup_attrs; i++) {
                    const inv_attr_t *a = &inv->group_attrs[i];

                    memset(&attr_info, 0, sizeof(attr_info));
                    attr_info.corder_valid = true;
                    attr_info.corder = (int64_t) i;
                    attr_info.cset = H5T_CSET_ASCII;
                    attr_info.data_size = bufr_attr_info_data_size_from_group_attr(a);

                    cb_ret = iter_args->op(loc_id, a->name, &attr_info, iter_args->op_data);
                    *iter_args->idx = (hsize_t) (i + 1);

                    if (cb_ret < 0)
                        FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADITER, FAIL,
                                        "Iterator callback returned error");
                    else if (cb_ret > 0) {
                        ret_value = cb_ret;
                        goto done;
                    }
                }
            } else if (o->obj_type == H5I_DATASET) {
                const inv_dataset_t *d = NULL;
                bufr_dataset_t *ds = &o->u.dataset;

                if (!ds || !ds->inv || !ds->name)
                    FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL,
                                    "Invalid dataset object or missing inventory");

                d = bufr_find_inventory_dataset(ds->inv, ds->name);
                if (!d)
                    FUNC_GOTO_ERROR(H5E_ATTR, H5E_NOTFOUND, FAIL,
                                    "Failed to find dataset in inventory for attribute iteration");

                for (size_t i = (size_t) start_idx; i < d->nattrs; i++) {
                    const inv_dset_attr_t *a = &d->attrs[i];

                    memset(&attr_info, 0, sizeof(attr_info));
                    attr_info.corder_valid = true;
                    attr_info.corder = (int64_t) i;
                    attr_info.cset = H5T_CSET_ASCII;
                    attr_info.data_size = bufr_attr_info_data_size_from_dset_attr(a, d);

                    cb_ret = iter_args->op(loc_id, a->name, &attr_info, iter_args->op_data);
                    *iter_args->idx = (hsize_t) (i + 1);

                    if (cb_ret < 0)
                        FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADITER, FAIL,
                                        "Iterator callback returned error");
                    else if (cb_ret > 0) {
                        ret_value = cb_ret;
                        goto done;
                    }
                }
            } else {
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Invalid object type");
            }

            break;
        }

        case H5VL_ATTR_DELETE:
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_UNSUPPORTED, FAIL,
                            "Attribute deletion is not supported in read-only BUFR VOL connector");
            break;

        default:
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_UNSUPPORTED, FAIL,
                            "Unsupported attribute specific operation");
    }

done:
    return ret_value;
}
herr_t bufr_attr_close(void *attr, hid_t dxpl_id, void **req)
{
    bufr_object_t *o = (bufr_object_t *) attr;
    bufr_attr_t *a = &o->u.attr;
    bufr_object_t *parent_obj = NULL;
    herr_t ret_value = SUCCEED;

    (void) dxpl_id;
    (void) req;

    assert(a);

    /* Decrement attribute's ref count */
    if (o->ref_count == 0)
        FUNC_DONE_ERROR(H5E_ATTR, H5E_CANTCLOSEOBJ, FAIL,
                        "Attribute already closed (ref_count is 0)");

    o->ref_count--;

    /* Only do the real close when ref_count reaches 0 */
    if (o->ref_count == 0) {

        /* Free cached data */
        bufr_free_attr_cached_data(a);

        if (a->name)
            free(a->name);

        if (a->space_id != H5I_INVALID_HID)
            if (H5Sclose(a->space_id) < 0)
                FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                "Failed to close attribute dataspace");

        /* Only close type_id if it is a created VL string type */
        if ((a->type_id != H5I_INVALID_HID) && a->is_vlen_string)
            if (H5Tclose(a->type_id) < 0)
                FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                "Failed to close attribute datatype");

        /* Release only the reference this attribute holds on its parent.
         * Do not recursively close the parent object here.
         */
        parent_obj = (bufr_object_t *) a->parent;
        a->parent = NULL;

        if (parent_obj) {
            if (parent_obj->ref_count == 0)
                FUNC_DONE_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL,
                                "attribute parent already has ref_count 0");
            else
                parent_obj->ref_count--;
        }

        free(o);
    }

    return ret_value;
}

herr_t bufr_attr_read(void *attr, hid_t mem_type_id, void *buf,
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

    /* ===== VL STRING SPECIAL HANDLING ===== */
    if (a->is_vlen_string) {
        /* Scalar VL string */
        if (a->nvals <= 1) {
            char *src = (char *) a->data;
            char **dst = (char **) buf;
            if (!src) {
                *dst = NULL;
                FUNC_GOTO_DONE(SUCCEED);
            }
            size_t len = strlen(src) + 1;
            *dst = (char *) H5allocate_memory(len, 0);
            if (!*dst)
                FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL, "VL attr: allocation failed");
            memcpy(*dst, src, len);
            FUNC_GOTO_DONE(SUCCEED);
        }
        /* Array of VL strings */
        else {
            char **src = (char **) a->data;
            char **dst = (char **) buf;
            for (size_t i = 0; i < a->nvals; i++) {
                if (src[i]) {
                    size_t len = strlen(src[i]) + 1;
                    dst[i] = (char *) H5allocate_memory(len, 0);
                    if (!dst[i]) {
                        /* rollback */
                        for (size_t j = 0; j < i; j++) {
                            if (dst[j]) {
                                H5free_memory(dst[j]);
                                dst[j] = NULL;
                            }
                        }
                        FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL,
                                        "VL attr array: allocation failed");
                    }
                    memcpy(dst[i], src[i], len);
                } else {
                    dst[i] = NULL;
                }
            }
            FUNC_GOTO_DONE(SUCCEED);
        }
    }
    /* ===== END VL STRING HANDLING ===== */
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

/*---------------------------------------------------------------------------
 * Function:    bufr_link_specific
 *
 * Purpose:     Handles link-specific operations for the BUFR VOL connector
 *
 * Return:      SUCCEED/FAIL
 *
 * Note:        The BUFR file has N groups (BUFR messages) with the names
 *              message_0 ... message_(N-1) at the root level.
 *---------------------------------------------------------------------------
 */
/* cppcheck-suppress constParameterCallback */
herr_t bufr_link_specific(void *obj, const H5VL_loc_params_t *loc_params,
                          H5VL_link_specific_args_t *args, hid_t __attribute__((unused)) dxpl_id,
                          void __attribute__((unused)) * *req)
{
    herr_t ret_value = SUCCEED;
    const char *link_name = NULL;
    hid_t loc_id = H5I_INVALID_HID;
    bufr_object_t *o = (bufr_object_t *) obj;

    if (!o || !loc_params || !args)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "Invalid arguments to link_specific");

    /* Only file and group objects expose links in this connector */
    if ((o->obj_type != H5I_FILE) && (o->obj_type != H5I_GROUP))
        FUNC_GOTO_ERROR(H5E_LINK, H5E_UNSUPPORTED, FAIL,
                        "Unsupported object type for link operation");

    switch (args->op_type) {
        case H5VL_LINK_EXISTS: {
            if (!args->args.exists.exists)
                FUNC_GOTO_ERROR(H5E_LINK, H5E_BADVALUE, FAIL, "NULL exists output pointer");

            *args->args.exists.exists = false;

            if (loc_params->type != H5VL_OBJECT_BY_NAME)
                FUNC_GOTO_ERROR(H5E_LINK, H5E_BADVALUE, FAIL,
                                "Link exists check requires name-based location");

            link_name = loc_params->loc_data.loc_by_name.name;
            if (!link_name)
                FUNC_GOTO_ERROR(H5E_LINK, H5E_BADVALUE, FAIL, "No link name provided");

            if (o->obj_type == H5I_FILE) {
                /* message_N at root */
                size_t n;
                if (is_group_name(link_name, o->u.file.nmsgs, &n))
                    *args->args.exists.exists = true;
            } else { /* Parent object is a BUFR group */
                bufr_group_t *g = &o->u.group;
                if (!g || !g->inv)
                    FUNC_GOTO_ERROR(H5E_LINK, H5E_BADVALUE, FAIL,
                                    "Invalid group object or missing inventory");

                for (size_t i = 0; i < g->inv->ndatasets; i++) {
                    if (strcmp(g->inv->datasets[i].name, link_name) == 0) {
                        *args->args.exists.exists = true;
                        break;
                    }
                }
            }

            break;
        }

        case H5VL_LINK_ITER: {
            H5VL_link_iterate_args_t *iter_args = &args->args.iterate;

            if (!iter_args || !iter_args->op || !iter_args->idx_p)
                FUNC_GOTO_ERROR(H5E_LINK, H5E_BADVALUE, FAIL, "Invalid iterate arguments");

            if (loc_params->type != H5VL_OBJECT_BY_SELF)
                FUNC_GOTO_ERROR(H5E_LINK, H5E_BADVALUE, FAIL,
                                "Link iteration requires object-by-self location");

            loc_id = H5VLwrap_register(obj, o->obj_type);
            if (loc_id == H5I_INVALID_HID)
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTREGISTER, FAIL,
                                "Could not register object for link iteration");

            if (o->obj_type == H5I_FILE) {
                size_t num_groups = o->u.file.nmsgs;

                for (hsize_t i = *iter_args->idx_p; i < num_groups; i++) {
                    H5L_info2_t link_info;
                    herr_t cb_ret;
                    char child_name[32];

                    snprintf(child_name, sizeof(child_name), "message_%u", (unsigned) i);

                    memset(&link_info, 0, sizeof(link_info));
                    link_info.type = H5L_TYPE_HARD;
                    link_info.corder_valid = true;
                    link_info.corder = (int64_t) i;
                    link_info.cset = H5T_CSET_ASCII;

                    cb_ret = iter_args->op(loc_id, child_name, &link_info, iter_args->op_data);
                    *iter_args->idx_p = i + 1;

                    if (cb_ret < 0)
                        FUNC_GOTO_ERROR(H5E_LINK, H5E_BADITER, FAIL,
                                        "Iterator callback returned error");
                    else if (cb_ret > 0) {
                        ret_value = cb_ret;
                        goto done;
                    }
                }
            } else { /* Object is a BUFR group */
                size_t num_datasets = 0;
                bufr_group_t *g = &o->u.group;

                if (!g || !g->inv)
                    FUNC_GOTO_ERROR(H5E_LINK, H5E_BADVALUE, FAIL,
                                    "Invalid group object or missing inventory");

                num_datasets = g->inv->ndatasets;

                for (hsize_t i = *iter_args->idx_p; i < num_datasets; i++) {
                    H5L_info2_t link_info;
                    herr_t cb_ret;
                    char child_name[512];

                    bufr_safe_strcpy(child_name, sizeof(child_name), g->inv->datasets[i].name);

                    memset(&link_info, 0, sizeof(link_info));
                    link_info.type = H5L_TYPE_HARD;
                    link_info.corder_valid = true;
                    link_info.corder = (int64_t) i;
                    link_info.cset = H5T_CSET_ASCII;

                    cb_ret = iter_args->op(loc_id, child_name, &link_info, iter_args->op_data);
                    *iter_args->idx_p = i + 1;

                    if (cb_ret < 0)
                        FUNC_GOTO_ERROR(H5E_LINK, H5E_BADITER, FAIL,
                                        "Iterator callback returned error");
                    else if (cb_ret > 0) {
                        ret_value = cb_ret;
                        goto done;
                    }
                }
            }

            break;
        }

        case H5VL_LINK_DELETE:
            FUNC_GOTO_ERROR(H5E_LINK, H5E_UNSUPPORTED, FAIL,
                            "Link deletion is not supported in read-only BUFR VOL connector");
            break;

        default:
            FUNC_GOTO_ERROR(H5E_LINK, H5E_UNSUPPORTED, FAIL, "Unsupported link specific operation");
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

        H5_bufr_err_class_g = H5I_INVALID_HID;
        H5_bufr_obj_err_maj_g = H5I_INVALID_HID;
    }

    return ret_value;
}
