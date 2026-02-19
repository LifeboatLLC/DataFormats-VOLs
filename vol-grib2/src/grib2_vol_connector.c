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

/* Purpose:     HDF5 Virtual Object Layer (VOL) connector for GRIB2 files */

/* This connector's header */
#include "grib2_vol_connector.h"

#include <H5PLextern.h>
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <ctype.h>

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

static hbool_t H5_grib2_initialized_g = FALSE;

/* Identifiers for HDF5's error API */
hid_t H5_grib2_err_stack_g = H5I_INVALID_HID;
hid_t H5_grib2_err_class_g = H5I_INVALID_HID;
hid_t H5_grib2_obj_err_maj_g = H5I_INVALID_HID;

/* The VOL class struct */
static const H5VL_class_t grib2_class_g = {
    3,                           /* VOL class struct version */
    GRIB2_VOL_CONNECTOR_VALUE,    /* value                    */
    GRIB2_VOL_CONNECTOR_NAME,     /* name                     */
    1,                           /* version                  */
    0,                           /* capability flags         */
    grib2_init_connector,         /* initialize               */
    grib2_term_connector,         /* terminate                */
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
        NULL,                /* create       */
        grib2_attr_open,     /* open         */
        grib2_attr_read,     /* read         */
        NULL,                /* write        */
        grib2_attr_get,      /* get          */
        grib2_attr_specific, /* specific     */
        NULL,                /* optional     */
        grib2_attr_close     /* close        */
    },
    {
        /* dataset_cls */
        NULL,                  /* create       */
        grib2_dataset_open,    /* open         */
        grib2_dataset_read,    /* read         */
        NULL,                  /* write        */
        grib2_dataset_get,     /* get          */
        NULL,                  /* specific     */
        NULL,                  /* optional     */
        grib2_dataset_close    /* close        */
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
        grib2_file_open,     /* open         */
        grib2_file_get,      /* get          */
        NULL,                /* specific     */
        NULL,                /* optional     */
        grib2_file_close     /* close        */
    },
    {
        /* group_cls */
        NULL,               /* create       */
        grib2_group_open,   /* open         */
        grib2_group_get,    /* get          */
        NULL,               /* specific     */
        NULL,               /* optional     */
        grib2_group_close   /* close        */
    },
    {
        /* link_cls */
        NULL,                  /* create       */
        NULL,                  /* copy         */
        NULL,                  /* move         */
        NULL,                  /* get          */
        grib2_link_specific,   /* specific     */
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
        grib2_introspect_get_conn_cls, /* get_conn_cls  */
        NULL,                         /* get_cap_flags */
        grib2_introspect_opt_query     /* opt_query     */
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

/*
 * Helper function to check if key exists in a message
 * The safest and cheapets way is to check type of the key
 */

int key_exists(codes_handle *h, const char *key)
{
    int type = 0;
    int err = codes_get_native_type(h, key, &type);

    if (err == CODES_SUCCESS)
        return 1;   /* key exists */

    if (err == CODES_NOT_FOUND)
        return 0;   /* key does not exist */

    /* Some other error occurred */
    return -1;
}
/*
 * Helper function to check if the key should be skipped.
 */
static int should_skip_key(const char *key)
{
    if (!key) return 1;

    /* Field values / masks */
    if (strcmp(key, "values") == 0) return 1;
    if (strcmp(key, "codedValues") == 0) return 1;
    if (strcmp(key, "bitmap") == 0) return 1;

    /* Coordinate “views” (you expose as datasets) */
    if (strcmp(key, "lon") == 0) return 1;
    if (strcmp(key, "lat") == 0) return 1;
    if (strcmp(key, "longitude") == 0) return 1;
    if (strcmp(key, "latitude") == 0) return 1;

    /* Coordinate arrays (often computed) */
    if (strcmp(key, "longitudes") == 0) return 1;
    if (strcmp(key, "latitudes") == 0) return 1;
    if (strcmp(key, "distinctLongitudes") == 0) return 1;
    if (strcmp(key, "distinctLatitudes") == 0) return 1;

    /* Per-point iterator keys sometimes appear in some contexts */
    if (strcmp(key, "i") == 0) return 1;
    if (strcmp(key, "j") == 0) return 1;

    return 0;
}


/* 
 * Helper function to get HDF5 memory type from ecCodes type 
 */
herr_t grib2_get_hdf5_type(int codes_type, hid_t *type_id, size_t len)
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
//            H5Tset_size(predef_type, H5T_VARIABLE);
            H5Tset_size(predef_type, len);
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
static char *grib2_strdup(const char *s)
{
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = (char *)malloc(n);
    if (!p) return NULL;
    memcpy(p, s, n);
    return p;
}

/*
 * Helper function to parse the name of the form /message_<N>/key_path" to extract i
 * message number and key path 
 */
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
static int grib2_key_type_and_size(codes_handle *h, const char *key, int *out_type, size_t *out_size)
{
    int err;
    int t = 0;
    size_t n = 0;

    err = codes_get_native_type(h, key, &t);
    if (err != 0) return err;

    /* For string type the function returns the number of characters 
       in the string including the terminating NUL (\0).
     */ 
    err = codes_get_size(h, key, &n);
    if (err != 0) return err;

    *out_type = t;
    *out_size = n;
    return 0;
}

static int grib2_build_message_index(grib2_file_t *bf)
{
    assert(bf);
    assert(bf->grib2);

    bf->msg_offsets = NULL;
    bf->nmsgs = 0;

    int err = 0;
    size_t cap = 1024;
    off_t *offs = (off_t *)malloc(cap * sizeof(off_t));
    if (!offs) return -3;

    while (1) {
        off_t off = ftello(bf->grib2);
        if (off < 0) { err = -4; break; }

        int err = 0;
        /* Try to read next GRIB2 message */
        codes_handle *h = codes_handle_new_from_file(NULL, bf->grib2, PRODUCT_GRIB, &err);

        if (!h || err != 0) {
            /* Normal end-of-file: ecCodes returns NULL at EOF */
            if (h) codes_handle_delete(h);
            break;
        }

        /* Record offset for this message */
        if (bf->nmsgs == cap) {
            cap *= 2;
            off_t *tmp = (off_t *)realloc(offs, cap * sizeof(off_t));
            if (!tmp) {
                codes_handle_delete(h);
                err = -5;
                break;
            }
            offs = tmp;
        }

        offs[bf->nmsgs++] = off;

        /* We only index offsets; discard handle.  */
        codes_handle_delete(h);

        /* File pointer is now positioned at the next message automatically. */
    }

    if (err < 0) {
        free(offs);
        bf->msg_offsets = NULL;
        bf->nmsgs = 0;
        return err;
    }

    /* Shrink to fit */
    if (bf->nmsgs == 0) {
        free(offs);
        bf->msg_offsets = NULL;
    } else {
        off_t *tmp = (off_t *)realloc(offs, bf->nmsgs * sizeof(off_t));
        bf->msg_offsets = tmp ? tmp : offs;
    }
    return 0;
}

/* Read big-endian unsigned 64-bit from 8 bytes */
static uint64_t be64(const unsigned char b[8]) {
    return ((uint64_t)b[0] << 56) |
           ((uint64_t)b[1] << 48) |
           ((uint64_t)b[2] << 40) |
           ((uint64_t)b[3] << 32) |
           ((uint64_t)b[4] << 24) |
           ((uint64_t)b[5] << 16) |
           ((uint64_t)b[6] <<  8) |
           ((uint64_t)b[7] <<  0);
}

static void die(const char* msg) {
    fprintf(stderr, "%s\n", msg);
    exit(1);
}

static void die_ecc(int err, const char* where) {
    if (err) {
        fprintf(stderr, "ecCodes error in %s: %s\n", where, codes_get_error_message(err));
        exit(1);
    }
}

/* Helper function to open the N-the message using the offset index */
static codes_handle *grib2_open_message_by_index(grib2_file_t *bf, size_t msg_index_1based)
{
    if (!bf || !bf->grib2 || !bf->msg_offsets) return NULL;
    if (msg_index_1based == 0 || msg_index_1based > bf->nmsgs) return NULL;

    off_t off = bf->msg_offsets[msg_index_1based - 1];
    if (fseeko(bf->grib2, off, SEEK_SET) != 0) return NULL;

    /* Read GRIB2 Section 0 (Indicator Section): 16 bytes */
    unsigned char sec0[16];
    if (fread(sec0, 1, sizeof(sec0), bf->grib2) != sizeof(sec0)) {
        fprintf(stderr, "Failed to read GRIB2 section0 at offset %" PRIu64 "\n", off);
        return NULL;
    }

    /* Validate magic */
    if (memcmp(sec0, "GRIB", 4) != 0) {
        fprintf(stderr, "Not a GRIB message at offset %" PRIu64 " (missing 'GRIB')\n", off);
        return NULL;
    }

    /* Edition is octet 8 (index 7) */
    unsigned edition = sec0[7];
    if (edition != 2) {
        fprintf(stderr, "Offset %" PRIu64 " is GRIB edition %u, not GRIB2\n", off, edition);
        return NULL;
    }

    /* Total message length is octets 9-16 (index 8..15) */
    uint64_t msg_len = be64(&sec0[8]);
    if (msg_len < 16) {
        fprintf(stderr, "Invalid GRIB2 message length %" PRIu64 " at offset %" PRIu64 "\n",
                msg_len, off);
        return NULL;
    }

    /* Read full message */
    unsigned char* buf = (unsigned char*)malloc((size_t)msg_len);
    if (!buf) { die("OOM"); return NULL; }

    /* Copy section0 we already read */
    memcpy(buf, sec0, 16);

       /* Read remainder */
    uint64_t remaining = msg_len - 16;
    if (remaining > 0) {
        if (fread(buf + 16, 1, (size_t)remaining, bf->grib2) != (size_t)remaining) {
            fprintf(stderr, "Failed to read full GRIB2 message (len=%" PRIu64 ") at offset %" PRIu64 "\n",
                    msg_len, off);
            free(buf);
            return NULL;
        }
    }

    /* Create ecCodes handle (copy variant is safest) */
    int err = 0;
    codes_handle* h = codes_handle_new_from_message_copy(NULL, buf, (size_t)msg_len);
    free(buf);

    if (!h) {
        fprintf(stderr, "codes_handle_new_from_message_copy failed at offset %" PRIu64 "\n", off);
        return NULL;
    }

    return h; /* caller owns */
}

/* Helper functionto read GRIB2 data into dset->data and set dset->data_size */ 
static herr_t grib2_read_data(grib2_dataset_t *dset)
{
    herr_t ret_value = SUCCEED;
    int err;
    double *tmp = NULL;

    assert(dset);
    assert(dset->msg);
    assert(dset->msg->h);

    codes_handle *h = dset->msg->h;
    const char *key = dset->name;

    /* Determine of grid points (values) */
    size_t nvals = 0;
    err = codes_get_size(h, "values", &nvals);
    if (err != 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to get number of grid points"); 

    /* Empty dataset: nothing to read */
    if (nvals == 0) {
        dset->data = NULL;
        dset->data_size = 0;
        dset->nvals = 0;
        FUNC_GOTO_DONE(SUCCEED);
    }
    tmp = (double*)malloc(nvals * sizeof(double));
    if (!tmp) 
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL, "Failed to allocate buffer for double values buffer");

    /* We are using lazy approach for retrieving data */

    err = 0;
    codes_iterator* it = codes_grib_iterator_new(h, /*flags*/0, &err);
    if (!it || err != CODES_SUCCESS) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL, "Failed to create data iterator");    
    }

    size_t i = 0;
    double lat = 0.0, lon = 0.0, val = 0.0;
    while (i < nvals && codes_grib_iterator_next(it, &lat, &lon, &val)) {
        if (key[0] == 'l' && key[1] == 'o')      tmp[i] = lon;   /* "lon"   */
        else if (key[0] == 'l' && key[1] == 'a') tmp[i] = lat;   /* "lat"   */
        else                                     tmp[i] = val;   /* "values" */
        i++;
    }
    codes_grib_iterator_delete(it);

    /* Sanity check if itertor yielded fewer points than expected */

    if (i != nvals) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to get requested number of points"); 
    }

    dset->data = (double*)malloc(nvals * sizeof(double));
    if (!dset->data) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL, "Failed allocate data buffer");
    }

    /* Copy */
    memcpy(dset->data, tmp, nvals * sizeof(double));
    dset->data_size = nvals * sizeof(double);
    dset->nvals = nvals;
    FUNC_GOTO_DONE(SUCCEED);

done:
    if (!tmp) 
        free (tmp);
    return ret_value;
}

/* Helper function to free cached data */
static void grib2_free_cached_data(grib2_dataset_t *dset)
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
void *grib2_file_open(const char *name, unsigned flags, hid_t fapl_id,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    grib2_object_t *file_obj = NULL;
    grib2_object_t *ret_value = NULL;

    grib2_file_t *file = NULL; /* Convenience pointer */

    /* We only support read-only access for GRIB2 files */
    if (flags != H5F_ACC_RDONLY)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL,
                        "GRIB2 VOL connector only supports read-only access");

    if ((file_obj = (grib2_object_t *) calloc(1, sizeof(grib2_object_t))) == NULL)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for GRIB2 file struct");

    file_obj->obj_type = H5I_FILE;
    file = &file_obj->u.file;
    /* Parent file pointers points to itself */
    file_obj->parent_file = file_obj;
    file_obj->ref_count = 1;

    if ((file->grib2 = fopen(name, "rb")) == NULL)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTOPENFILE, NULL, "Failed to open GRIB2 file: %s", name);

    if (grib2_build_message_index(file) != 0) 
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL, "Failed to build message index");


    if ((file->filename = strdup(name)) == NULL)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL, "Failed to duplicate GRIB2 filename string");

    file->flags = flags;
    file->plist_id = fapl_id;

    ret_value = file_obj;

done:
    if (!ret_value) {
        if (file_obj) {
            H5E_BEGIN_TRY
            {
                grib2_file_close(file_obj, dxpl_id, req);
            }
            H5E_END_TRY;
        }
    }

    return ret_value;
}

/* VOL file get callback */

// cppcheck-suppress constParameterCallback
herr_t grib2_file_get(void *file, H5VL_file_get_args_t *args,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    const grib2_object_t *o = (const grib2_object_t *) file;
    const grib2_file_t *f = &o->u.file;
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
herr_t grib2_file_close(void *file, hid_t __attribute__((unused)) dxpl_id,
                          void __attribute__((unused)) * *req)
{
    int grib2_status;
    grib2_object_t *o = (grib2_object_t *) file;
    herr_t ret_value = SUCCEED;

    assert(o);

    if (o->ref_count == 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCLOSEFILE, FAIL,
                        "GRIB2 file already closed (ref_count is 0)");

    o->ref_count--;

    if (o->ref_count == 0) {
        if (o->u.file.grib2) {
	    grib2_status = fclose(o->u.file.grib2);
            if (grib2_status == EOF)
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCLOSEFILE, FAIL, "Failed to close GRIB2 file");
            else
	        o->u.file.grib2 = NULL;
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


/* Helper function to check valid group names 
 * Returns 1 if name is exactly "/message_<N>" where 1 <= N <= K
 * Returns 0 otherwise
 */
static size_t is_group_name(const char* s, size_t K)
{
    if (!s) return 0;

    /* Must start with "/message_" */
    if (strncmp(s, "/message_", 9) != 0)
        return 0;

    const char* p = s + 9;

    /* Must have at least one digit */
    if (!isdigit((unsigned char)*p))
        return 0;

    /* Parse integer safely */
    size_t n = 0;
    while (isdigit((unsigned char)*p)) {
        n = n * 10 + (*p - '0');
        if (n > K)      /* early stop if out of range */
            return 0;
        p++;
    }

    /* Must end exactly after the digits */
    if (*p != '\0')
        return 0;

    /* Check bounds */
    if (n < 1 || n > K) return 0;
 
    /* Return message number */
    return n;
}


/* Group operations */
void *grib2_group_open(void *obj, const H5VL_loc_params_t __attribute__((unused)) * loc_params,
                         const char *name, hid_t __attribute__((unused)) gapl_id,
                         hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    grib2_object_t *file = (grib2_object_t *) obj;
    grib2_object_t *grp_obj = NULL;
    grib2_object_t *ret_value = NULL;
    grib2_group_t *grp = NULL; /* Convenience pointer */

    size_t n = 0;
    codes_handle *h = NULL;
    codes_keys_iterator *it = NULL;
    grib2_message_t *msg = NULL;

    if (!file || !name)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "Invalid file or group name");

    /* Check for valid group name; it can be '/' or '/message_<N>', where N is between
       1 and number of messages in the GRIB2 file stored in file->nmsgs.
     */ 

    n = is_group_name(name, file->u.file.nmsgs);
/*    if ((strcmp(name, "/") != 0) || (n == 0))
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_UNSUPPORTED, NULL,
                        "GRIB2 VOL connector only supports root group '/' or group with the name '/message_<N>'");
*/
    if ((grp_obj = (grib2_object_t *) calloc(1, sizeof(grib2_object_t))) == NULL)
        FUNC_GOTO_ERROR(H5E_SYM, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for GRIB2 group struct");

    grp_obj->obj_type = H5I_GROUP;
    grp_obj->parent_file = file->parent_file;
    grp_obj->ref_count = 1; /* Initialize group's own ref count */
    /* Increment file reference count since this group holds a reference */
    grp_obj->parent_file->ref_count++;
    /* Store message number. TODO - do we really need it? */
    grp_obj->u.group.msg_num = n;

    /* Fast open using message offsets to get message handle */
    h = grib2_open_message_by_index(&file->u.file, n);
    if (!h)  {
       FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL,
                          "Failed to get GRIB2 message handle ");
    }
    msg = (grib2_message_t *)calloc(1, sizeof(*msg));
    if (!msg)  {
       FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL,
                          "Failed to initialize message handle for dataset object");
    }
    msg->h = h;
    grp_obj->u.group.msg = msg;

    grp = &grp_obj->u.group;
    if ((grp->name = strdup(name)) == NULL)
        FUNC_GOTO_ERROR(H5E_SYM, H5E_CANTALLOC, NULL, "Failed to duplicate group name string");
    
    
    hsize_t num_attrs = 0;
    hsize_t num_grids = 0;

    unsigned long flags = 0;
    flags |= CODES_KEYS_ITERATOR_SKIP_COMPUTED;
    flags |= CODES_KEYS_ITERATOR_SKIP_DUPLICATES;

    it = codes_keys_iterator_new(h, flags, NULL);
    if (!it)
         FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, NULL, "Cannot get CODES iterator");

    while (codes_keys_iterator_next(it)) {
        const char *key = codes_keys_iterator_get_name(it);
        if (!key) continue;
        if (should_skip_key(key)) { 
            num_grids++; 
        } else  {
           num_attrs++;
        }
    }
    grp_obj->u.group.num_attrs = num_attrs;
    grp_obj->u.group.num_grids = num_grids;

    ret_value = grp_obj;
done:
    if (!it) codes_keys_iterator_delete(it);
    if (!ret_value && grp) {
        H5E_BEGIN_TRY
        {
            grib2_group_close(grp_obj, dxpl_id, req);
        }
        H5E_END_TRY;
    }

    return ret_value;
}

herr_t grib2_group_get(void *obj, H5VL_group_get_args_t *args,
                         hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    grib2_object_t *o = (grib2_object_t *) obj;
    const grib2_group_t *grp = (const grib2_group_t *) &o->u.group; /* Convenience pointer */
    herr_t ret_value = SUCCEED;

    if (!args)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid arguments");

    switch (args->op_type) {
        case H5VL_GROUP_GET_INFO: {
            H5G_info_t *ginfo = args->args.get_info.ginfo;

            if (!grp || !ginfo)
                FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid group or info pointer");

            if (!o->parent_file || !o->parent_file->u.file.grib2)
                FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid file object");

            /* Fill in group info structure */
            ginfo->storage_type = H5G_STORAGE_TYPE_COMPACT;
            ginfo->nlinks = 3; /*TODO: if product is present then we have 3 datasets; need add code to deal with multiple products in the message */

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

herr_t grib2_group_close(void *grp, hid_t dxpl_id, void **req)
{
    grib2_object_t *o = (grib2_object_t *) grp;
    grib2_group_t *g = &o->u.group; /* Convenience pointer */
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

         /* Delete GRIB2 message handle and free message object */
        if (g->msg) {
            if (g->msg->h) {
                codes_handle_delete(g->msg->h);
                g->msg->h = NULL;
            }
            free(g->msg);
            g->msg = NULL;
        }

        /* Decrement parent file's reference count */
        if (grib2_file_close(o->parent_file, dxpl_id, req) < 0)
            FUNC_DONE_ERROR(H5E_SYM, H5E_CLOSEERROR, FAIL, "Failed to close group file object");

        free(o);
    }

    return ret_value;
}


/* Helper function to check validity of GRIB2 dataset name 
 * Returns:
 *   1  if key_view is exactly "lon", "lat", or "values"
 *   0  otherwise
 */
static int check_grib2_dataset_name(const char* key_view)
{
    if (!key_view)
        return 0;

    /* Fast path: lengths 3 or 5 only */
    size_t n = strlen(key_view);
    if (n == 3) {
        return (strcmp(key_view, "lon") == 0 ||
                strcmp(key_view, "lat") == 0);
    }
    if (n == 6) {
        return (strcmp(key_view, "values") == 0);
    }
    return 0;
}



/* VOL dataset open callback */
void *grib2_dataset_open(void *obj, const H5VL_loc_params_t __attribute__((unused)) * loc_params,
                           const char *name, hid_t __attribute__((unused)) dapl_id,
                           hid_t __attribute__((unused)) dxpl_id,
                           void __attribute__((unused)) * *req)
{
    grib2_object_t *file_obj = (grib2_object_t *) obj;
    grib2_object_t *dset_obj = NULL;
    grib2_object_t *ret_value = NULL;

    grib2_dataset_t *dset = NULL;           /* Convenience pointer */
    grib2_file_t *file = &file_obj->u.file; /* Convenience pointer */    

    int parse_return = -1;
    hsize_t dims[1] = {0};    /* GRIB2 datasets are always read as 1-dim datasets */

    long msg_index = 0;
    const char *key_view = NULL;
    codes_handle *h = NULL;
    grib2_message_t *msg = NULL;
    int message_type = 0; 
    size_t len = 0;
    size_t string_len = 0;


    /* Parse "/message_<N>/<key>" */
    parse_return = parse_message_key_path(name, &msg_index, &key_view); 

    if (!file_obj || (parse_return != 0)) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "Invalid GRIB2 file name or key path");
    }


    /* GRIB2 datasets can have only these names "lon", "lat" and "values". 
       TODO: GRIB2 allows multiple products, i.e., repeated sections 3 and 4,
       we will need to add code to handle this case. For now, the assumption is 
       that there is only one product in the message.
    */ 
    
    /* Check validity of the dataset name */
    if (check_grib2_dataset_name(key_view) == 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "Invalid dataset name, expect lon, lat or values strings");
    } 
       if ((dset_obj = (grib2_object_t *) malloc(sizeof(grib2_object_t))) == NULL) {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for GRIB2 dataset struct");
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
    dset->data_size = 0; /* The value is set to the size of the data buffer; see grib2_read_data function below */

    /* Fast open using message offsets */
    h = grib2_open_message_by_index(file, (size_t)msg_index);
    if (!h)  { 
       FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL,
                          "Failed to get GRIB2 message handle ");
    }

    msg = (grib2_message_t *)calloc(1, sizeof(*msg));
    if (!msg)  {
       FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL,
                          "Failed to initialize message handle for dataset object");
    }
    msg->h = h;
    dset->msg = msg;

    /* Copy key to set up dataset name */
    if ((dset->name = grib2_strdup(key_view)) == NULL) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL,
                        "Failed to duplicate dataset name string");
    }

    /* For GRIB2 dataset the type is always double */
    dset->codes_type = CODES_TYPE_DOUBLE;
    if (grib2_get_hdf5_type(dset->codes_type, &dset->type_id, len) < 0) {
                FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL,
                            "Failed to covert GRIB2 datatype to native HDF5 type");
    } 

    /* Cache the key data */
    if (grib2_read_data(dset) !=0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "Failed to read data for the key");

    /* GRIB2 datasets are one-dimensional; 
       the length of the array data was set in the previous call.
    */

    dims[0] = (hsize_t)dset->nvals;
    dset->space_id = H5Screate_simple(1, dims, NULL);
    if (dset->space_id == H5I_INVALID_HID) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL,
                        "Failed to create simple dataspace for dataset");
    }

    return dset_obj;

done:
    if (!ret_value) {
        if (dset)
            H5E_BEGIN_TRY
            {
                grib2_dataset_close(dset_obj, dxpl_id, req);
            }
        H5E_END_TRY;
    }

    return ret_value;
}

/* Helper functions below are resused from the GeoTIFF connector.                     
 * Support for selections in GRIB2 are overkill since size of raw data is not large,          
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
static herr_t prepare_converted_buffer(const grib2_dataset_t *dset, hid_t mem_type_id,
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

herr_t grib2_dataset_read(size_t __attribute__((unused)) count, void *dset[], hid_t mem_type_id[],
                            hid_t __attribute__((unused)) mem_space_id[], hid_t file_space_id[],
                            hid_t __attribute__((unused)) dxpl_id, void *buf[],
                            void __attribute__((unused)) * *req)
{
    const grib2_object_t *dset_obj = (const grib2_object_t *) dset[0];
    const grib2_dataset_t *d = NULL; /* Convenience pointer */

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
    d = (const grib2_dataset_t *) &dset_obj->u.dataset;

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
herr_t grib2_dataset_get(void *dset, H5VL_dataset_get_args_t *args,
                           hid_t __attribute__((unused)) dxpl_id,
                           void __attribute__((unused)) * *req)
{       
    const grib2_object_t *o = (const grib2_object_t *) dset;
    const grib2_dataset_t *d = &o->u.dataset;
        
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
            FUNC_GOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, FAIL,
                            "Unsupported dataset get operation");
            break;
    }
done:
    return ret_value;
}

/* VOL dataset close callback */
herr_t grib2_dataset_close(void *dset, hid_t dxpl_id, void **req)
{

    grib2_object_t *d = (grib2_object_t *) dset;
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
        grib2_free_cached_data(&d->u.dataset);

        if (d->u.dataset.name)
            free(d->u.dataset.name);
        if (d->u.dataset.space_id != H5I_INVALID_HID)
            if (H5Sclose(d->u.dataset.space_id) < 0)
                FUNC_DONE_ERROR(H5E_DATASET, H5E_CLOSEERROR, FAIL, "Failed to close dataspace");
        
        /* Delete GRIB2 message handle and free message object */
        if (d->u.dataset.msg) {
            if (d->u.dataset.msg->h) {
                codes_handle_delete(d->u.dataset.msg->h);
                d->u.dataset.msg->h = NULL;
            }
            free(d->u.dataset.msg);
            d->u.dataset.msg = NULL;
        }
       /* Decrement parent file's reference count */
        if (grib2_file_close(d->parent_file, dxpl_id, req) < 0)
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL,
                            "Failed to close dataset file object");

        free(d);
   }
    return ret_value;
}

/* Attribute operations */
/* Helper functions to check valid attribute name */


/* ---------- helpers ---------- */

static bool contains_arrow(const char *key) {
    return strstr(key, "->") != NULL;
}

static bool starts_with(const char *s, const char *p) {
    return s && p && strncmp(s, p, strlen(p)) == 0;
}

static bool equals_any(const char *s, const char *const *list) {
    for (size_t i = 0; list[i]; i++) {
        if (strcmp(s, list[i]) == 0)
            return true;
    }
    return false;
}

/* ---------- Per-subset keys checkers ---------- */

static bool varies_per_subset(const char *key) {

    /* Exact canonical per-observation keys */
    static const char *const exact_keys[] = {
        "latitude",
        "longitude",
        "height",
        "pressure",
        "depth",
        "elevation",
        "subsetNumber",
        NULL
    };

    if (equals_any(key, exact_keys))
        return true;

    /* Common per-subset prefixes */
    static const char *const prefixes[] = {
        "time",          /* time, timePeriod, timeIncrement */
        "year", "month", "day", "hour", "minute", "second",
        "station",
        "sensor",
        "quality",
        "confidence",
        "wind",
        "temperature",
        "humidity",
        NULL
    };

    for (size_t i = 0; prefixes[i]; i++) {
        if (starts_with(key, prefixes[i]))
            return true;
    }

    return false;
}

/* ---------- Decision function ---------- */

static bool reject_key(const char *key) {

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


/* Helper functionto read GRIB2 data into attr->data and set attr->data_size */ 
static herr_t grib2_read_attr_data(grib2_attr_t *attr)
{
    herr_t ret_value = SUCCEED;
    int err;
    char *buf_c = NULL;
    char *buf_l = NULL;
    char *buf_d = NULL;

    assert(attr);
    assert(attr->msg);
    assert(attr->msg->h);
    assert(attr->nvals);
    assert(attr->data_size);

    codes_handle *h = attr->msg->h;
    const char *key = attr->name;

    /* -----------------------------
     * STRING (VL)
     * ----------------------------- */
    if (attr->codes_type == CODES_TYPE_STRING) {

        size_t len = attr->data_size;

        char *buf_c = (char *)malloc(len*sizeof(char));
        if (!buf_c)
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL, "Failed to allocate buffer for a key string");
        if ((err = codes_get_string(h, key, buf_c, &len)) != 0)
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to read key string");
        
        attr->data = (char *)malloc(len*sizeof(char));
        memcpy(attr->data, buf_c, attr->data_size);
        FUNC_GOTO_DONE(SUCCEED);
   }

    /* -----------------------------
     * NUMERIC (LONG / DOUBLE)
     * ----------------------------- */

    if (attr->codes_type == CODES_TYPE_LONG) {
        long *buf_l = (long *)malloc((attr->nvals)*sizeof(long));
        if (!buf_l)
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL, 
                                     "Failed to allocate buffer for long attribute value");
    
        if (attr->nvals == 1) {
            if((err = codes_get_long(h, key, buf_l)) !=0)
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, 
                                         "Failed to get long attribute value");
        }
        else {
            if ((err = codes_get_long_array(h, key, buf_l, &attr->nvals)) !=0 ) 
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to get long attribute value");
            
        }
        attr->data = (long*)malloc((attr->nvals)*sizeof(long)); 
        memcpy(attr->data, buf_l, attr->data_size);
        FUNC_GOTO_DONE(SUCCEED);
    }

    if (attr->codes_type == CODES_TYPE_DOUBLE) {
        double *buf_d = (double *)malloc((attr->nvals)*sizeof(double));
        if (!buf_d)
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL, "Failed to allocate buffer for double attribute value");

        if (attr->nvals == 1) {
            if ((err = codes_get_double(h, key, buf_d) !=0))
                 FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to get double values");
        }
        else {
            if ((err = codes_get_double_array(h, key, buf_d, &attr->nvals)) !=0 ) 
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to get long attribute value");

        }
        attr->data = (double*)malloc((attr->nvals)*sizeof(double)); 
        memcpy(attr->data, buf_d, attr->data_size);
        FUNC_GOTO_DONE(SUCCEED);
    }

    /* Unsupported type */
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "Discovered unknown datatype");
done:
    if (buf_c)
        free(buf_c);
    if (buf_l)
        free(buf_l);
    if (buf_d)
        free(buf_d);
    return ret_value;
}

/* Helper function to free cached attribute data */
static void grib2_free_cached_attr_data(grib2_attr_t *attr)
{
    assert(attr);
    assert(attr->data);

    free(attr->data);

    attr->data = NULL;
    attr->data_size = 0;
}


/* Attribute operations */
void *grib2_attr_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                        hid_t __attribute__((unused)) aapl_id,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    grib2_object_t *parent_obj = NULL;
    grib2_object_t *attr_obj = NULL;
    grib2_object_t *ret_value = NULL;

    grib2_attr_t *attr = NULL; /* Convenience pointer */

    const char *key_view = NULL;
    codes_handle *h = NULL;
    grib2_message_t *msg = NULL;
    int message_type = 0;
    long msg_index = 0;
    size_t len = 0;
    size_t string_len = 0;
    int parse_return = -1;
    hsize_t dims[1] = {0};


    if (!obj || !name || !loc_params)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, NULL, "Invalid object or attribute name");
    key_view = name;

    /* 
     * Only group ID can be an object ID in the call to H5Aopen.
     * We may add dataset ID in the future.
     */
   
    parent_obj = (grib2_object_t *) obj;

    if (parent_obj->obj_type != H5I_GROUP)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_UNSUPPORTED, NULL,
                        "Unsupported location parameter type for attribute open");

    /* Determine the type of the parent object */
    if (loc_params->type != H5VL_OBJECT_BY_SELF)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_UNSUPPORTED, NULL,
                        "Unsupported location parameter type for attribute open");

    if ((attr_obj = (grib2_object_t *) calloc(1, sizeof(grib2_object_t))) == NULL)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for GRIB2 attribute struct");

    attr_obj->obj_type = H5I_ATTR;
    attr_obj->parent_file = parent_obj->parent_file;
    attr_obj->ref_count = 1; /* Initialize attribute's own ref count */
    /* Increment file reference count since this attribute holds a reference */
    attr_obj->parent_file->ref_count++;
    /* Increment parent object's reference count since this attribute holds a reference */
    parent_obj->ref_count++;
    attr = &attr_obj->u.attr;
    msg = (grib2_message_t *)calloc(1, sizeof(*msg));
    if (!msg)  {
       FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTALLOC, NULL,
                          "Failed to initialize message handle for dataset object");
    }
    msg->h = parent_obj->u.group.msg->h;
    attr->msg = msg;

    if ((attr->name = strdup(key_view)) == NULL)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTALLOC, NULL, "Failed to duplicate attribute name string");
    printf("Name %s\n", attr->name);
    attr->parent = obj;
    attr->space_id = H5I_INVALID_HID;
    attr->type_id = H5I_INVALID_HID;
    attr->data = NULL;
    attr->data_size = 0; /* The value is set to the size of the data buffer; see grib2_read_data function below */ 



    /* Find GRIB2 datatype and size (replication) for the key.
       Returned len value will be a number of elements for the numeric types 
       and length of the string including NUL character for the key of the string type.
     */
    if (grib2_key_type_and_size(parent_obj->u.group.msg->h, key_view, &message_type, &len) != 0) {
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, NULL,
                            "Failed to discover type or size for the key");
    }
    
    printf("message_type %d \n", message_type);
    if ((message_type != CODES_TYPE_STRING) && (message_type != CODES_TYPE_LONG) && (message_type != CODES_TYPE_DOUBLE))
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, NULL,
                            "Found unsupported datatype for the attribute");   
    if (message_type == CODES_TYPE_STRING) {
        attr->nvals = 1;
        attr->data_size = len;
    } else {
        attr->nvals = len;
        if (message_type == CODES_TYPE_LONG)  attr->data_size = len*sizeof(long);
        if (message_type == CODES_TYPE_DOUBLE) attr->data_size = len*sizeof(double);
    }     

    /* Find corresponding HDF5 native type; set a special flag if GRIB2 type is string */
    attr->codes_type = message_type;
    if (grib2_get_hdf5_type(attr->codes_type, &attr->type_id, len) < 0) {
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, NULL,
                            "Failed to covert GRIB2 datatype to native HDF5 type");
    }
    /* For simplicity we assume simple dataset; 
       TODO: we could use H5S_SCALAR for the attributes with only one value 
     */
    dims[0] = (hsize_t)attr->nvals;
    attr->space_id = H5Screate_simple(1, dims, NULL);
    if (attr->space_id == H5I_INVALID_HID) {
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTCREATE, NULL,
                            "Failed to create SCALAR dataspace for attribute");
    }
    /* Cache the key data */
    if (grib2_read_attr_data(attr) !=0)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_CANTGET, NULL, "Failed to read data for the key");    

   ret_value = attr_obj;

done:
    if (!ret_value && attr_obj) {
        H5E_BEGIN_TRY
        {
            grib2_attr_close(attr_obj, dxpl_id, req);
        }
        H5E_END_TRY;
    }
    return ret_value;
}


herr_t grib2_attr_close(void *attr, hid_t dxpl_id, void **req)
{
    grib2_object_t *o = (grib2_object_t *) attr;
    grib2_attr_t *a = &o->u.attr;
    grib2_object_t *parent_obj = NULL;
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
        grib2_free_cached_attr_data(a);         

        /* Use FUNC_DONE_ERROR to try to complete resource release after failure */
        if (a->name)
            free(a->name);
        if (a->space_id != H5I_INVALID_HID)
            if (H5Sclose(a->space_id) < 0)
                FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                "Failed to close attribute dataspace");
        /* Only close type_id if it's not a predefined type (like H5T_NATIVE_*) */
        if ((a->type_id != H5I_INVALID_HID) && (a->codes_type == CODES_TYPE_STRING))
            if (H5Tclose(a->type_id) < 0)
                FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                "Failed to close attribute datatype");

        /* Close parent object (dataset, group, or file) */
        parent_obj = (grib2_object_t *) a->parent;
        if (parent_obj) {
            switch (parent_obj->obj_type) {
                case H5I_FILE:
                    if (grib2_file_close(parent_obj, dxpl_id, req) < 0)
                        FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                        "Failed to close attribute's parent file");
                    break;
                case H5I_DATASET:
                    if (grib2_dataset_close(parent_obj, dxpl_id, req) < 0)
                        FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                        "Failed to close attribute's parent dataset");
                    break;
                case H5I_GROUP:
                    if (grib2_group_close(parent_obj, dxpl_id, req) < 0)
                        FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                        "Failed to close attribute's parent group");
                    break;
                default:
                    FUNC_DONE_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Invalid parent object type");
            }
        }

        /* Also decrement the file reference count */
        if (grib2_file_close(o->parent_file, dxpl_id, req) < 0)
            FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                            "Failed to close attribute file object");

        free(o);
    }

    return ret_value;
}

herr_t grib2_attr_read(void *attr, hid_t __attribute__((unused)) mem_type_id, void *buf,
                         hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    const grib2_object_t *o = (const grib2_object_t *) attr;
    const grib2_attr_t *a = NULL; /* Convenience pointer */
    htri_t types_equal = 0;
    herr_t ret_value = SUCCEED;

    assert(o);

    a = &o->u.attr;

    if (!buf)
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Invalid attribute or buffer");

    /* Check if memory datatype corresponds to GRIB2 datatype before transferring data */
    /* TODO: add type conversion */
    if ((types_equal = H5Tequal(mem_type_id, a->type_id)) <= 0)
        FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_BADVALUE, FAIL, "failed to compare datatypes or datatypes are different"); 
    memcpy(buf, a->data, a->data_size); 
    
done:
    return ret_value;
}

// cppcheck-suppress constParameterCallback
herr_t grib2_attr_get(void *obj, H5VL_attr_get_args_t *args,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    const grib2_object_t *o = (const grib2_object_t *) obj;
    const grib2_attr_t *a = &o->u.attr;

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
 * Function:    grib2_attr_specific
 *
 * Purpose:     Handles attr-specific operations for the GRIB2 VOL connector
 *
 * Return:      SUCCEED/FAIL
 *
 * Note:        In GRIB2 file only groups have attributes (these are all keys    
 *              except lon, lat, values (i.e., non-grid keys) stored in the 
 *              messages). The number of grid and non-grid keys is determined 
 *              at group open time.  
 *---------------------------------------------------------------------------
 */
/* cppcheck-suppress constParameterCallback */
herr_t grib2_attr_specific(void *obj, const H5VL_loc_params_t *loc_params,
                             H5VL_attr_specific_args_t *args, hid_t __attribute__((unused)) dxpl_id,
                             void __attribute__((unused)) * *req)
{
    herr_t ret_value = SUCCEED;
    const char *attr_name = NULL;
    grib2_object_t *parent_obj = (grib2_object_t *) obj;
    codes_handle *h = NULL;
    codes_keys_iterator *it = NULL;               
    int codes_err = 0;


    if (!obj || !loc_params || !args)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "Invalid arguments to link_specific");

    /* obj can be only group object*/

    if (parent_obj->obj_type != H5I_GROUP) 
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_UNSUPPORTED, FAIL,
                        "Unsupported location parameter type for attribute operation");

    grib2_group_t *group = &parent_obj->u.group;
    h = group->msg->h; 
    if (!h) 
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL,
                        "Do not have valid CODES handle to proceed with attribute operation");

    switch (args->op_type) {
        case H5VL_ATTR_EXISTS: {
            *args->args.exists.exists = false;
             
            /* Get the attribute name from loc_params */
            if (loc_params->type == H5VL_OBJECT_BY_NAME) {
                attr_name = loc_params->loc_data.loc_by_name.name;
            } else {
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL,
                                "Attribute exists check requires name-based location");
            }

            if (!attr_name)
                FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "No attribute name provided");
            codes_err = key_exists(h, attr_name);
            if (codes_err) { 
                *args->args.exists.exists = true;
            } else if (codes_err == -1) {
              FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL,
                                "CODES Function to check key existance failed ");
            }
            break;
        }


        case H5VL_ATTR_ITER: {

            H5VL_attr_iterate_args_t *iter_args = &args->args.iterate;
            size_t num_attrs = group->num_attrs;
            printf("Here, number of non-grid attributes is %d \n", (int)num_attrs);

            assert(iter_args);
            assert(iter_args->idx);

            /* Only iterate over all attributes starting from the current index*/

            if (iter_args->op) {
                unsigned long flags = 0;
                flags |= CODES_KEYS_ITERATOR_SKIP_COMPUTED;
                flags |= CODES_KEYS_ITERATOR_SKIP_DUPLICATES;

                codes_keys_iterator *it = codes_keys_iterator_new(h, flags, NULL);
                if (!it)
                    FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Cannot get CODES iterator"); 

                hsize_t iter_index = 0;
                while (codes_keys_iterator_next(it)) {

                    /* Skip the first keys to start from the required location */
                    if (iter_index < *iter_args->idx) {
                        iter_index++;
                        continue;
                    }
                    /* Sanity check that we are not going over the number of attributes */
                    if (iter_index == num_attrs) 
                        FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Iteration index is out of bounds");

                    const char *key = codes_keys_iterator_get_name(it);

                    if (!key) continue;
                    if (should_skip_key(key)) continue;

                    int t = 0;
                    size_t n = 0;
                    size_t key_size = 0;

                    /*TODO: have to decide if it is better to use grib2_key_type_and_size helper function
                     *  or call CODES functions that are easier to understand and report errors.
                     * if (grib2_key_type_and_size(h, key, &t, &n) !=0) 
                     *     FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Cannot get key typei or size");
                     */

                    if (codes_get_native_type(h, key, &t))
                        FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Cannot get key type");

                    if (codes_get_size(h, key, &n))
                        FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Cannot get key size");
         
                    /* Figure out the size of the atribute's raw data */
                
                    if (t == CODES_TYPE_LONG)   key_size = n*sizeof(long);
                    if (t == CODES_TYPE_DOUBLE) key_size = n*sizeof(double); 
                    if (t == CODES_TYPE_STRING) key_size = n; /* Size includes teminated NULL character */
                    if (t == CODES_TYPE_BYTES)  key_size = n;


                    H5A_info_t attr_info;
                    herr_t cb_ret;

                    /* Fill in attr info */
                    memset(&attr_info, 0, sizeof(H5A_info_t));
                    attr_name = strdup(key);
                    attr_info.corder_valid = true;
                    attr_info.corder = (int64_t) iter_index;
                    attr_info.cset = H5T_CSET_ASCII;
                    attr_info.data_size = key_size;

                    cb_ret = iter_args->op(0, attr_name, &attr_info, iter_args->op_data);

                    iter_index++;
                    /* Check callback return value */
                    if (cb_ret < 0) {
                        FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADITER, FAIL,
                                        "Iterator callback returned error");
                    } else if (cb_ret > 0) {
                        /* Callback requested early termination */
                        ret_value = cb_ret;
                        goto done;
                    }
                }
            }

            break;
        }

        case H5VL_ATTR_DELETE:
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_UNSUPPORTED, FAIL,
                            "Attribute deletion is not supported in read-only GRIB2 VOL connector");
            break;

        default:
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_UNSUPPORTED, FAIL, "Unsupported attribute specific operation");
    }

done:
    if (!it) codes_keys_iterator_delete(it); 
    return ret_value;
}

/*---------------------------------------------------------------------------
 * Function:    grib2_link_specific
 *
 * Purpose:     Handles link-specific operations for the GRIB2 VOL connector
 *
 * Return:      SUCCEED/FAIL
 *
 * Note:        The GRIB2 file has VOL has a flat structure with N groups 
 *              (GRIB2 messages) with the names <message_K>, where 1=< K =< N 
 *              at the root level, and possible three datasets "lon", "lat"
 *              and "values" under each group. 
 *---------------------------------------------------------------------------
 */
/* cppcheck-suppress constParameterCallback */
herr_t grib2_link_specific(void *obj, const H5VL_loc_params_t *loc_params,
                             H5VL_link_specific_args_t *args, hid_t __attribute__((unused)) dxpl_id,
                             void __attribute__((unused)) * *req)
{
    herr_t ret_value = SUCCEED;
    const char *link_name = NULL;
    grib2_object_t *parent_obj = NULL;

    if (!obj || !loc_params || !args)
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "Invalid arguments to link_specific");

    /* obj could be file, or group */

    parent_obj = (grib2_object_t *) obj;

    if ((parent_obj->obj_type != H5I_FILE) && (parent_obj->obj_type != H5I_GROUP)) 
        FUNC_GOTO_ERROR(H5E_LINK, H5E_UNSUPPORTED, FAIL,
                        "Unsupported location parameter type for link operation");

    switch (args->op_type) {
        case H5VL_LINK_EXISTS: {
            *args->args.exists.exists = true;
             
            /* Get the link name from loc_params */
            if (loc_params->type == H5VL_OBJECT_BY_NAME) {
                link_name = loc_params->loc_data.loc_by_name.name;
            } else {
                FUNC_GOTO_ERROR(H5E_LINK, H5E_BADVALUE, FAIL,
                                "Link exists check requires name-based location");
            }

            if (!link_name)
                FUNC_GOTO_ERROR(H5E_LINK, H5E_BADVALUE, FAIL, "No link name provided");

            if (parent_obj->obj_type == H5I_FILE) {
                grib2_object_t *file = (grib2_object_t *) obj;
                size_t n = 0;
                n = is_group_name(link_name, file->u.file.nmsgs);
                if (n == 0) {
                      *args->args.exists.exists = false;
                }
             } else {  /* Parent object is a group */
                 if (check_grib2_dataset_name(link_name) == 0) {
                       *args->args.exists.exists = false;
                    }
             }

            break;
        }


        case H5VL_LINK_ITER: {
            if (parent_obj->obj_type == H5I_GROUP)
                FUNC_GOTO_ERROR(H5E_LINK, H5E_UNSUPPORTED, FAIL,
                        "Iteration over datasets is not supported by GRIB2 VOL");

            grib2_object_t *file = (grib2_object_t *) obj;
            H5VL_link_iterate_args_t *iter_args = &args->args.iterate;
            size_t num_groups = file->u.file.nmsgs;

            assert(iter_args);
            assert(iter_args->idx_p);

            /* Only iterate over all groups links starting from the current index*/

            if (iter_args->op) {
                for (hsize_t i = *iter_args->idx_p; i < num_groups; i++) {
                    H5L_info2_t link_info;
                    herr_t cb_ret;
                    char link_name[32];

                    snprintf(link_name, sizeof(link_name), "message_%u", (unsigned) (i+1));

                    /* Fill in minimal link info */
                    memset(&link_info, 0, sizeof(H5L_info2_t));
                    link_info.type = H5L_TYPE_HARD;
                    link_info.corder_valid = true;
                    link_info.corder = (int64_t) i;
                    link_info.cset = H5T_CSET_ASCII;

                    cb_ret = iter_args->op(0, link_name, &link_info, iter_args->op_data);
                    *iter_args->idx_p = i + 1;

                    /* Check callback return value */
                    if (cb_ret < 0) {
                        FUNC_GOTO_ERROR(H5E_LINK, H5E_BADITER, FAIL,
                                        "Iterator callback returned error");
                    } else if (cb_ret > 0) {
                        /* Callback requested early termination */
                        ret_value = cb_ret;
                        goto done;
                    }
                }
            }

            break;
        }

        case H5VL_LINK_DELETE:
            FUNC_GOTO_ERROR(H5E_LINK, H5E_UNSUPPORTED, FAIL,
                            "Link deletion is not supported in read-only GRIB2 VOL connector");
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
    return &grib2_class_g;
}

/*---------------------------------------------------------------------------
 * Function:    grib2_introspect_opt_query
 *
 * Purpose:     Query if an optional operation is supported by this connector
 *
 * Returns:     SUCCEED (Can't fail)
 *
 *---------------------------------------------------------------------------
 */
herr_t grib2_introspect_opt_query(void __attribute__((unused)) * obj, H5VL_subclass_t subcls,
                                    int opt_type, uint64_t __attribute__((unused)) * flags)
{
    /* We don't support any optional operations */
    (void) subcls;
    (void) opt_type;
    *flags = 0;
    return SUCCEED;
}

herr_t grib2_introspect_get_conn_cls(void __attribute__((unused)) * obj,
                                       H5VL_get_conn_lvl_t __attribute__((unused)) lvl,
                                       const H5VL_class_t __attribute__((unused)) * *conn_cls)
{
    herr_t ret_value = SUCCEED;

    assert(conn_cls);

    /* Retrieve the VOL connector class */
    *conn_cls = &grib2_class_g;

    return ret_value;
}

herr_t grib2_init_connector(hid_t __attribute__((unused)) vipl_id)
{
    herr_t ret_value = SUCCEED;

    /* Register the connector with HDF5's error reporting API */
    if ((H5_grib2_err_class_g =
             H5Eregister_class(HDF5_VOL_GRIB2_ERR_CLS_NAME, HDF5_VOL_GRIB2_LIB_NAME,
                               HDF5_VOL_GRIB2_LIB_VER)) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "can't register with HDF5 error API");

    /* Create a separate error stack for the GRIB2 VOL to report errors with */
    if ((H5_grib2_err_stack_g = H5Ecreate_stack()) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "can't create error stack");

    /* Set up a few GRIB2 VOL-specific error API message classes */
    if ((H5_grib2_obj_err_maj_g =
             H5Ecreate_msg(H5_grib2_err_class_g, H5E_MAJOR, "Object interface")) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL,
                        "can't create error message for object interface");

    /* Initialized */
    H5_grib2_initialized_g = TRUE;

done:
    if (ret_value < 0)
        grib2_term_connector();

    return ret_value;
}

herr_t grib2_term_connector(void)
{
    herr_t ret_value = SUCCEED;

    /* Unregister from the HDF5 error API */
    if (H5_grib2_err_class_g >= 0) {
        if (H5_grib2_obj_err_maj_g >= 0 && H5Eclose_msg(H5_grib2_obj_err_maj_g) < 0)
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL,
                            "can't unregister error message for object interface");
        if (H5Eunregister_class(H5_grib2_err_class_g) < 0)
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "can't unregister from HDF5 error API");

        /* Print the current error stack before destroying it */
        PRINT_ERROR_STACK;

        /* Destroy the error stack */
        if (H5Eclose_stack(H5_grib2_err_stack_g) < 0) {
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "can't close error stack");
            PRINT_ERROR_STACK;
        }

        H5_grib2_err_stack_g = H5I_INVALID_HID;
        H5_grib2_err_class_g = H5I_INVALID_HID;
        H5_grib2_obj_err_maj_g = H5I_INVALID_HID;
    }

    return ret_value;
}
