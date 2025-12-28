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
    3,                           /* VOL class struct version */
    BUFR_VOL_CONNECTOR_VALUE,    /* value                    */
    BUFR_VOL_CONNECTOR_NAME,     /* name                     */
    1,                           /* version                  */
    0,                           /* capability flags         */
    bufr_init_connector,         /* initialize               */
    bufr_term_connector,         /* terminate                */
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
        NULL,              /* create       */
        NULL,              /* open         */
        NULL,              /* read         */
        NULL,              /* write        */
        NULL,              /* get          */
        NULL,              /* specific     */
        NULL,              /* optional     */
        NULL               /* close        */
    },
    {
        /* dataset_cls */
        NULL,                 /* create       */
        bufr_dataset_open,    /* open         */
        NULL,                 /* read         */
        NULL,                 /* write        */
        NULL,                 /* get          */
        NULL,                 /* specific     */
        NULL,                 /* optional     */
        bufr_dataset_close    /* close        */
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
        NULL,                /* create       */
        bufr_file_open,      /* open         */
        NULL,                /* get          */
        NULL,                /* specific     */
        NULL,                /* optional     */
        bufr_file_close      /* close        */
    },
    {
        /* group_cls */
        NULL,               /* create       */
        NULL,               /* open         */
        NULL,               /* get          */
        NULL,               /* specific     */
        NULL,               /* optional     */
        NULL                /* close        */
    },
    {
        /* link_cls */
        NULL,                  /* create       */
        NULL,                  /* copy         */
        NULL,                  /* move         */
        NULL,                  /* get          */
        NULL,                  /* specific     */
        NULL                   /* optional     */
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
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

/* Helper function to parse the name of the form /message_<N>/key_path" to extract message number and key path */
static int parse_message_key_path(const char *name, long *msg_index, const char **key_view)
{
//    if (!name || !msg_index || !key_view) return -1;
    assert(name);
    assert(msg_index);
    assert(key_view);

    const char *p = name;

    /* Skip leading slashes */
    while (*p == '/') p++;

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
    *key_view  = end;
    return 0;
}

/* Helper function to find type and size for a key */
static int bufr_key_type_and_size(codes_handle *h, const char *key, int *out_type, size_t *out_size)
{
    int err;
    int t = 0;
    size_t n = 0;

    err = codes_get_native_type(h, key, &t);
    if (err != 0) return err;

    err = codes_get_size(h, key, &n);
    if (err != 0) return err;

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
    if (saved < 0) saved = 0;

    if (fseek(bf->bufr, 0, SEEK_SET) != 0) return -2;

    free(bf->msg_offsets);
    bf->msg_offsets = NULL;
    bf->nmsgs = 0;

    size_t cap = 128;
    long *offs = (long *)malloc(cap * sizeof(long));
    if (!offs) return -3;

    int err = 0;
    while (1) {
        long off = ftell(bf->bufr);
        if (off < 0) { err = -4; break; }

        /* Try to read next BUFR message */
        codes_handle *h = codes_handle_new_from_file(NULL, bf->bufr, PRODUCT_BUFR, &err);

        if (!h || err != 0) {
            /* Normal end-of-file: ecCodes returns NULL at EOF */
            if (h) codes_handle_delete(h);
            break;
        }

        /* Record offset for this message */
        if (bf->nmsgs == cap) {
            cap *= 2;
            long *tmp = (long *)realloc(offs, cap * sizeof(long));
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
        (void)fseek(bf->bufr, saved, SEEK_SET);
        return err;
    }

    /* Shrink to fit */
    if (bf->nmsgs == 0) {
        free(offs);
        bf->msg_offsets = NULL;
    } else {
        long *tmp = (long *)realloc(offs, bf->nmsgs * sizeof(long));
        bf->msg_offsets = tmp ? tmp : offs;
    }

    (void)fseek(bf->bufr, saved, SEEK_SET);
    return 0;
}

/* Helper function to open the N-the message using the offset index */
static codes_handle *bufr_open_message_by_index(bufr_file_t *bf, size_t msg_index_1based)
{
    if (!bf || !bf->bufr || !bf->msg_offsets) return NULL;
    if (msg_index_1based == 0 || msg_index_1based > bf->nmsgs) return NULL;

    long saved = ftell(bf->bufr);
    if (saved < 0) saved = 0;

    long off = bf->msg_offsets[msg_index_1based - 1];
    if (fseek(bf->bufr, off, SEEK_SET) != 0) return NULL;

    int err = 0;
    codes_handle *h = codes_handle_new_from_file(NULL, bf->bufr, PRODUCT_BUFR, &err);

    (void)fseek(bf->bufr, saved, SEEK_SET);

    if (!h || err != 0) {
        if (h) codes_handle_delete(h);
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

            char *s = (char *)H5allocate_memory(len, 0);
            if (!s)
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL, "Failed to allocate buffer for a key string");

            err = codes_get_string(h, key, s, &len);
            if (err != 0) {
                H5free_memory(s);
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to read key string");
            }

            dset->data = s;
            dset->data_size = sizeof(char *);
            FUNC_GOTO_DONE(SUCCEED);
        }

        /* Array of strings (replicated) */
        char **arr = (char **)calloc(nvals, sizeof(char *));
        if (!arr)
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL, "Failed to allocate array of strings");

        for (size_t i = 0; i < nvals; i++) {
            char ikey[512];
            snprintf(ikey, sizeof(ikey), "%s#%zu", key, i + 1);

            size_t len = 0;
            err = codes_get_string(h, ikey, NULL, &len);
            if (err != 0)
                goto string_array_fail;

            arr[i] = (char *)H5allocate_memory(len, 0);
            if (!arr[i]) {
                //err = -2;
                goto string_array_fail;
            }

            err = codes_get_string(h, ikey, arr[i], &len);
            if (err != 0)
                goto string_array_fail;
        }

        dset->data = arr;
        dset->data_size = nvals * sizeof(char *);
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
        long *buf = (long *)malloc(nvals * sizeof(long));
        if (!buf)
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL, "Failed to allocate buffer for long values");

        err = codes_get_long_array(h, key, buf, &nvals);
        if (err != 0) {
            free(buf);
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to get long values");
        }

        dset->data = buf;
        dset->data_size = nvals * sizeof(long);
        FUNC_GOTO_DONE(SUCCEED);
    }

    if (dset->codes_type == CODES_TYPE_DOUBLE) {
        double *buf = (double *)malloc(nvals * sizeof(double));
        if (!buf)
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL, "Failed to allocate buffer for double values");

        err = codes_get_double_array(h, key, buf, &nvals);
        if (err != 0) {
            free(buf);
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to get double values");
        }

        dset->data = buf;
        dset->data_size = nvals * sizeof(double);
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

            char **arr = (char **)dset->data;
            for (hsize_t i = 0; i < dims[0]; i++) {
                if (arr[i]) H5free_memory(arr[i]);
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
                           hid_t __attribute__((unused)) dxpl_id,
                           void __attribute__((unused)) * *req)
{
    bufr_object_t *file_obj = (bufr_object_t *) obj;
    bufr_object_t *dset_obj = NULL;
    bufr_object_t *ret_value = NULL;

    bufr_dataset_t *dset = NULL;           /* Convenience pointer */
    bufr_file_t *file = &file_obj->u.file; /* Convenience pointer */    

    int parse_return = -1;
    hsize_t dims[1] = {0};    /* BUFR key values are always read as 1-dim datasets */

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
    dset->data_size = 0;

    /* Fast open using message offsets */
    h = bufr_open_message_by_index(file, (size_t)msg_index);
    if (!h)  {
       FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL,
                          "Failed to get BUFR message handle ");
    }
    /* Unpack the  message data */
    if (codes_set_long(h, "unpack", 1) < 0) {
       FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL,
                          "Failed to unpack BUFR message handle");
    }

    msg = (bufr_message_t *)calloc(1, sizeof(*msg));
    if (!msg)  {
       FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL,
                          "Failed to initialize message handle for dataset object");
    }
    msg->h = h;
    dset->msg = msg;

    /* Copy key to set up dataset name */
    if ((dset->name = bufr_strdup(key_view)) == NULL) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL,
                        "Failed to duplicate dataset name string");
    }

    /* Copy key to set up dataset name */
    /* Find BUFR datatype and size (replication) for the key */
    //EP if (bufr_key_type_and_size(msg->h, dset->name, &message_type, &len) != 0) {
    if (bufr_key_type_and_size(h, key_view, &message_type, &len) != 0) {
                FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL,
                            "Failed to discover datatype and size for the key");        
    }

    /* Find corresponding HDF5 native type; set a special flag if BUFR type is string */
    dset->codes_type = message_type;
    dset->codes_type = message_type;
    if (dset->codes_type == CODES_TYPE_STRING) dset->is_vlen_string = 1; 
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
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTCREATE, NULL,
                        "Failed to create NULL dataspace for dataset");
        }
    }
    else if (len == 1) {
        dset->space_id = H5Screate(H5S_SCALAR);
        if (dset->space_id == H5I_INVALID_HID) {
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTCREATE, NULL,
                            "Failed to create SCALAR dataspace for dataset");
        }
    }
    else {
        hsize_t dims[1];
        dims[0] = (hsize_t)len;
        dset->space_id = H5Screate_simple(1, dims, NULL);
        if (dset->space_id == H5I_INVALID_HID) {
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTCREATE, NULL,
                        "Failed to create simple dataspace for dataset");
        }
    }
    /* Cache the key data */
    if (bufr_read_data(dset) !=0)
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
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL,
                            "Failed to close dataset file object");

        free(d);
   }
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
    if ((H5_bufr_err_class_g =
             H5Eregister_class(HDF5_VOL_BUFR_ERR_CLS_NAME, HDF5_VOL_BUFR_LIB_NAME,
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
