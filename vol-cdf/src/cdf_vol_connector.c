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

/*
 * Purpose:     HDF5 Virtual Object Layer (VOL) connector for CDF files
 */

/* This connector's header */
#include "cdf_vol_connector.h"

#include <H5PLextern.h>
#include <errno.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <float.h>

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

static hbool_t H5_cdf_initialized_g = FALSE;

/* Identifiers for HDF5's error API */
hid_t H5_cdf_err_stack_g = H5I_INVALID_HID;
hid_t H5_cdf_err_class_g = H5I_INVALID_HID;
hid_t H5_cdf_obj_err_maj_g = H5I_INVALID_HID;

/* The VOL class struct */
static const H5VL_class_t cdf_class_g = {
    3,                           /* VOL class struct version */
    CDF_VOL_CONNECTOR_VALUE,     /* value                    */
    CDF_VOL_CONNECTOR_NAME,      /* name                     */
    1,                           /* version                  */
    0,                           /* capability flags         */
    cdf_init_connector,          /* initialize               */
    cdf_term_connector,          /* terminate                */
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
        cdf_attr_open,     /* open         */
        cdf_attr_read,     /* read         */
        NULL,              /* write        */
        cdf_attr_get,      /* get          */
        NULL,              /* specific     */
        NULL,              /* optional     */
        cdf_attr_close     /* close        */
    },
    {
        /* dataset_cls */
        NULL,                 /* create       */
        cdf_dataset_open,     /* open         */
        cdf_dataset_read,     /* read         */
        NULL,                 /* write        */
        cdf_dataset_get,      /* get          */
        NULL,                 /* specific     */
        NULL,                 /* optional     */
        cdf_dataset_close     /* close        */
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
        cdf_file_open,       /* open         */
        NULL,                /* get          */
        NULL,                /* specific     */
        NULL,                /* optional     */
        cdf_file_close       /* close        */
    },
    {
        /* group_cls */
        NULL,               /* create       */
        cdf_group_open,     /* open         */
        cdf_group_get,      /* get          */
        NULL,               /* specific     */
        NULL,               /* optional     */
        cdf_group_close     /* close        */
    },
    {
        /* link_cls */
        NULL,                  /* create       */
        NULL,                  /* copy         */
        NULL,                  /* move         */
        NULL,                  /* get          */
        cdf_link_specific,     /* specific     */
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
        cdf_introspect_get_conn_cls, /* get_conn_cls  */
        NULL,                        /* get_cap_flags */
        cdf_introspect_opt_query     /* opt_query     */
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

/* Helper function: Print CDF specific error messages */
static void cdf_print_error(CDFstatus status)
{
    char error_text[CDF_STATUSTEXT_LEN + 1];
    CDFgetStatusText(status, error_text);
    fprintf(stderr, "CDF ERROR: %s (status code: %ld)\n", error_text, status);
}

/* Helper function: Map CDF datatype to HDF5 datatype for reading data */
static herr_t cdf_get_hdf5_type_from_cdf(long cdf_datatype, hid_t *type_id)
{
    herr_t ret_value = SUCCEED;
    hid_t new_type = H5I_INVALID_HID;
    hid_t predef_type;
    assert(type_id);
    switch (cdf_datatype) {
        /* 8-bit types */
        case CDF_BYTE: /* 1-byte, signed integer */
        case CDF_INT1: /* 1-byte, signed integer */
            predef_type = H5T_NATIVE_INT8;
            break;
        
        case CDF_UINT1: /* 1-byte, unsigned integer */
            predef_type = H5T_NATIVE_UINT8;
            break;
        
        /* 16-bit types */
        case CDF_INT2: /* 2-byte, signed integer */
            predef_type = H5T_NATIVE_INT16;
            break;

        case CDF_UINT2: /* 2-byte, unsigned integer */
            predef_type = H5T_NATIVE_UINT16;
            break;

        /* 32-bit types */
        case CDF_INT4: /* 4-byte, signed integer */
            predef_type = H5T_NATIVE_INT32;  /* Explicitly 32-bit signed */
            break;
            
        case CDF_UINT4: /* 4-byte, unsigned integer */
            predef_type = H5T_NATIVE_UINT32; /* Explicitly 32-bit unsigned */
            break;
            
        /* 64-bit types */
        case CDF_INT8: /* 8-byte, signed integer */
        case CDF_TIME_TT2000: /* 8-byte, signed integer representing nanoseconds since 2000 */
            predef_type = H5T_NATIVE_INT64;  /* Explicitly 64-bit signed */
            break;
            
        /* Floating point */
        case CDF_REAL4: /* 4-byte, floating point */
        case CDF_FLOAT: /* 4-byte, floating point */
            predef_type = H5T_NATIVE_FLOAT;  /* 32-bit float */
            break;
            
        case CDF_REAL8: /* 8-byte, floating point */
        case CDF_DOUBLE: /* 8-byte, floating point */
        case CDF_EPOCH: /* 8-byte, floating point representing milliseconds since 0000-01-01 */
            predef_type = H5T_NATIVE_DOUBLE; /* 64-bit double */
            break;
        
        /* High-precision timestamp */
        case CDF_EPOCH16: /* 16-byte, two 8-byte floating point values representing picoseconds since 0000-01-01 */
        {
            hsize_t dims[1] = {2};  /* 1D array of 2 doubles */
            predef_type = H5Tarray_create(H5T_NATIVE_DOUBLE, 1, dims);
            break;
        }

        /* Character types */
        case CDF_CHAR: /* 1-byte, signed character */
        case CDF_UCHAR: /* 1-byte, unsigned character */
            FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_UNSUPPORTED, FAIL,
                            "CDF char types must be handled specifically as string datatypes");

        default:
            FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_UNSUPPORTED, FAIL,
                            "Unsupported CDF datatype: %ld", cdf_datatype);
    }

    /* Copy the datatype value */
    if ((new_type = H5Tcopy(predef_type)) < 0){
        FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_CANTCOPY, FAIL, "Failed to copy predef_type datatype");
    }

    /* Close the temporary array datatype if created */
    if (cdf_datatype == CDF_EPOCH16) {
        H5Tclose(predef_type);
    }

    *type_id = new_type;

done:
    if (ret_value < 0) {
        if (new_type != H5I_INVALID_HID) {
            H5E_BEGIN_TRY
            {
                H5Tclose(new_type);
            }
            H5E_END_TRY;
        }
        /* Also close predef_type if it was an array datatype */
        if (cdf_datatype == CDF_EPOCH16 && predef_type != H5I_INVALID_HID) {
            H5E_BEGIN_TRY
            {
                H5Tclose(predef_type);
            }
            H5E_END_TRY;
        }
    }

    return ret_value;
} /* end cdf_get_hdf5_type_from_cdf() */

/* Helper function: Prepare a buffer with data in the requested memory type.
 * If conversion is needed, allocates a new buffer and performs conversion.
 * If no conversion needed, returns pointer to original data.
 */
static herr_t prepare_converted_buffer(hid_t type_id, hid_t mem_type_id, size_t num_elements,
                                       void *source_buf, size_t source_size,
                                       void **out_buffer, size_t *out_buffer_size,
                                       hbool_t *out_tconv_buf_allocated)
{
    herr_t ret_value = SUCCEED;
    htri_t types_equal = 0;
    size_t dataset_type_size = 0;
    size_t mem_type_size = 0;
    void *conversion_buf = NULL;

    if (!source_buf || !source_size || !out_buffer || !out_buffer_size || !out_tconv_buf_allocated) {
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid arguments");
    }

    /* Check if types are equal */
    if ((types_equal = H5Tequal(mem_type_id, type_id)) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCOMPARE, FAIL, "failed to compare datatypes");
    }
    
    if (types_equal) {
        /* No conversion needed - return borrowed pointer to cached data */
        *out_buffer = source_buf;
        *out_buffer_size = source_size;
        *out_tconv_buf_allocated = FALSE;
        FUNC_GOTO_DONE(SUCCEED);
    }

    /* Conversion needed - allocate buffer and convert */
    if ((dataset_type_size = H5Tget_size(type_id)) == 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "failed to get dataset type size");
    }
    if ((mem_type_size = H5Tget_size(mem_type_id)) == 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "failed to get memory type size");
    }

    /* Allocate buffer large enough for in-place conversion (max of src/dst) */
    size_t src_data_size = num_elements * dataset_type_size;
    size_t dst_data_size = num_elements * mem_type_size;
    size_t conversion_buf_size = (src_data_size > dst_data_size) ? src_data_size : dst_data_size;

    if ((conversion_buf = malloc(conversion_buf_size)) == NULL) {
        FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL,
                        "failed to allocate memory for datatype conversion");
    }

    /* Copy source data */
    memcpy(conversion_buf, source_buf, src_data_size);

    /* Perform in-place conversion */
    if (H5Tconvert(type_id, mem_type_id, num_elements, conversion_buf, NULL, H5P_DEFAULT) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCONVERT, FAIL,
                        "failed to convert data from dataset type to memory type");
    }

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

/* Helper function: Transfer data from source buffer to user buffer.
 * Handles both simple memcpy (when both selections are ALL) and scatter operations.
 */
static herr_t transfer_data_to_user(const void *source_buf, size_t source_size, hid_t mem_type_id,
                                    hid_t mem_space_id, void *user_buf)
{
    herr_t ret_value = SUCCEED;
    H5S_sel_type mem_sel_type = H5S_SEL_ERROR;

    if (!source_buf || !user_buf) {
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid arguments");
    }
    /* Determine if we can use simple memcpy or need scatter */
    if (mem_space_id == 0 || mem_space_id == H5S_ALL) {
        /* Simple case: copy entire buffer directly */
        memcpy(user_buf, source_buf, source_size);
        FUNC_GOTO_DONE(SUCCEED);
    }

    /* Check selection type */
    if ((mem_sel_type = H5Sget_select_type(mem_space_id)) < 0) {
        FUNC_GOTO_ERROR(H5E_DATASPACE, H5E_CANTGET, FAIL, "failed to get memory space selection type");
    }

    if (mem_sel_type == H5S_SEL_ALL) {
        /* Simple case: copy entire buffer directly */
        memcpy(user_buf, source_buf, source_size);
    } else {
        /* Use scatter for non-trivial selections */
        response_read_info resp_info;
        resp_info.read_size = &source_size;
        resp_info.buffer = (void *) source_buf;

        if (H5Dscatter(dataset_read_scatter_op, &resp_info, mem_type_id, mem_space_id, user_buf) < 0) {
            FUNC_GOTO_ERROR(H5E_DATASET, H5E_READERROR, FAIL, "can't scatter data to user buffer");
        }
    }

done:
    return ret_value;
} /* end transfer_data_to_user() */

/* Helper function: Parse attribute name to find optional [index] suffix
 * If no index is found, the name will just be copied as is */
static herr_t parse_attr_name(const char *name, cdf_attr_t *attr)
{
    herr_t ret_value = SUCCEED;
    const char *bracket_open = NULL;
    const char *bracket_close = NULL;

    if (!name || !attr) {
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid arguments");
    }

    /* Look for [index] suffix */
    bracket_open = strrchr(name, '[');
    bracket_close = strrchr(name, ']');

    /* Check if there is only one valid index */
    if (bracket_open != strchr(name, '[') || bracket_close != strrchr(name, ']')) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "invalid attribute index format");
    }

    if (bracket_open && bracket_close && (bracket_close > bracket_open)) {
        /* Ensure that the index is at the end of the name */
        if (bracket_close != name + strlen(name) - 1) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "invalid attribute index format");
        }

        /* Found brackets - attempt to parse index */
        char *endptr = NULL;
        long index = strtol(bracket_open + 1, &endptr, 10);
        /* Check for non numerical chars in index or conversion errors */
        if (endptr != bracket_close || errno == ERANGE) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "invalid attribute index format");
        }
        if (index < 0) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "attribute index cannot be negative");
        }
        attr->indexed = true;
        attr->index = index;
        
        /* Copy the base name of the attribute (without the [index] suffix) */
        size_t base_len = (size_t)(bracket_open - name);
        
        attr->name = (char *) malloc(base_len + 1);
        if (!attr->name) {
            FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL,
                            "failed to allocate memory for attribute name");
        }

        memcpy(attr->name, name, base_len);

        attr->name[base_len] = '\0'; /* Null-terminate the name */
    } else if (!bracket_open && !bracket_close) {
        /* No brackets - not indexed */
        attr->indexed = false;
        attr->index = -1;

        /* Copy the full name */
        attr->name = strdup(name);
        if (!attr->name) {
            FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL,
                            "failed to copy attribute name to attribute struct");
        }
    } else {
        /* Mismatched brackets */
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "invalid attribute index format");
    }

done:
    return ret_value;
} /* end parse_attr_name() */

/* Helper: resolve a root-only name.
 * Accepts "name" or "/name".
 * Rejects empty, "/" alone, and any other '/' in the string. */
static herr_t resolve_root_name(const char *in_name, const char **out_name)
{
    herr_t ret_value = SUCCEED;

    if (!in_name || !out_name) {
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid name");
    }

    /* Skip leading '/' */
    if (in_name[0] == '/') {
        in_name++;
    }
    /* Empty after stripping is invalid */
    if (*in_name == '\0') {
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "empty name");
    }

    /* Root-only namespace: no other '/' allowed */
    if (strchr(in_name, '/') != NULL) {
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "only root-level names are supported");
    }

    *out_name = in_name;

done:
    return ret_value;
}

void *cdf_file_open(const char *name, unsigned flags, hid_t fapl_id,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    CDFid id = NULL;
    CDFstatus status;    /* CDF completion status. */
    cdf_object_t *file_obj = NULL;
    cdf_object_t *ret_value = NULL;

    cdf_file_t *file = NULL; /* Convenience pointer */

    /* We only support read-only access for CDF files */
    if (flags != H5F_ACC_RDONLY) {
        FUNC_GOTO_ERROR(H5E_FILE, H5E_UNSUPPORTED, NULL,
                        "CDF VOL connector only supports read-only access");
    }

    if ((file_obj = (cdf_object_t *) calloc(1, sizeof(cdf_object_t))) == NULL) {
        FUNC_GOTO_ERROR(H5E_FILE, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for CDF file struct");
    }

    file_obj->obj_type = H5I_FILE;
    file = &file_obj->u.file;
    /* Parent file pointers points to itself */
    file_obj->parent_file = file_obj;
    file_obj->ref_count = 1;

    /* CDF open */
    status = CDFopenCDF (name, &id);
    if (status != CDF_OK) {
        cdf_print_error(status);
        FUNC_GOTO_ERROR(H5E_FILE, H5E_CANTOPENFILE, NULL, "Failed to open CDF file: %s", name);
    }
    /* Save CDF ID */
    file->id = id;

    if ((file->filename = strdup(name)) == NULL) {
        FUNC_GOTO_ERROR(H5E_FILE, H5E_CANTALLOC, NULL, "Failed to duplicate filename string");
    }

    /* Set file zMode/2 - Specifies that all rVariables are to be converted to zVariables,
     * and all false dimensions from dimension variances are removed */
    status = CDFsetzMode(file->id, zMODEon2);

    file->flags = flags;
    file->plist_id = fapl_id;

    ret_value = file_obj;

done:
    if (!ret_value) {
        if (file_obj) {
            H5E_BEGIN_TRY
            {
                cdf_file_close(file_obj, dxpl_id, req);
            }
            H5E_END_TRY;
        }
    }

    return ret_value;
} /* end cdf_file_open() */

herr_t cdf_file_close(void *file, hid_t __attribute__((unused)) dxpl_id,
                          void __attribute__((unused)) * *req)
{
    CDFstatus status;
    cdf_object_t *o = (cdf_object_t *) file;
    herr_t ret_value = SUCCEED;

    assert(o);

    if (o->ref_count == 0) {
        FUNC_GOTO_ERROR(H5E_FILE, H5E_CANTCLOSEFILE, FAIL,
                        "CDF file already closed (ref_count is 0)");
    }

    o->ref_count--;

    if (o->ref_count == 0) {
        if (o->u.file.id) {
            status = CDFcloseCDF(o->u.file.id);
            if (status != CDF_OK) {
                cdf_print_error(status);
                FUNC_GOTO_ERROR(H5E_FILE, H5E_CANTCLOSEFILE, FAIL, "Failed to close CDF file");
            } else {
                o->u.file.id = NULL;
            }
        }
        if (o->u.file.filename) {
            free(o->u.file.filename);
        }
        free(o);
    }

done:
    return ret_value;
} /* end cdf_file_close() */

/* Helper function to create a fixed-length string datatype.
 * Factored out to reduce code duplication */
static hid_t create_fixed_string_type(size_t len){
    hid_t type_id = H5I_INVALID_HID;
    if ((type_id = H5Tcopy(H5T_C_S1)) < 0) {
        return H5I_INVALID_HID;
    }

    if (H5Tset_size(type_id, len) < 0) {
        H5Tclose(type_id);
        return H5I_INVALID_HID;
    }

    if (H5Tset_strpad(type_id, H5T_STR_NULLTERM) < 0) {
        H5Tclose(type_id);
        return H5I_INVALID_HID;
    }

    return type_id;
} /* end create_fixed_string_type() */

/* Dataset operations */
void *cdf_dataset_open(void *obj, const H5VL_loc_params_t __attribute__((unused)) * loc_params,
                       const char *name, hid_t __attribute__((unused)) dapl_id,
                       hid_t __attribute__((unused)) dxpl_id,
                       void __attribute__((unused)) * *req)
{
    CDFstatus status;

    cdf_object_t *base_obj = (cdf_object_t *) obj;
    cdf_object_t *file_obj = NULL;
    cdf_object_t *dset_obj = NULL;
    cdf_object_t *ret_value = NULL;

    cdf_dataset_t *dset = NULL; /* Convenience pointer */
    cdf_file_t *file = NULL;    /* Convenience pointer */

    H5T_class_t dtype_class = H5T_NO_CLASS; 
    hsize_t dspace_dims[CDF_MAX_DIMS];    /* Dataspace dimensions */

    if (!base_obj || !name) {
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "Invalid file or dataset name");
    }

    /* Resolve file object from base object (file or group) */
    file_obj = base_obj->parent_file;
    if (!file_obj || file_obj->obj_type != H5I_FILE) {
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "Invalid parent file object");
    }
    file = &file_obj->u.file;

    if ((dset_obj = (cdf_object_t *) malloc(sizeof(cdf_object_t))) == NULL) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for CDF dataset struct");
    }

    dset_obj->obj_type = H5I_DATASET;
    dset_obj->parent_file = file_obj;
    dset_obj->ref_count = 1; /* Initialize dataset's own ref count */
    /* Increment file reference count since this dataset holds a reference */
    file_obj->ref_count++;

    dset = &dset_obj->u.dataset;

    /* Search for and resolve leading '/' and validate root-only name */
    const char *resolved_name = NULL;
    if (resolve_root_name(name, &resolved_name) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "Failed to resolve root name");
    }

    /* Duplicate dataset name */
    if ((dset->name = strdup(resolved_name)) == NULL) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL,
                        "Failed to duplicate dataset name string");
    }

    /* Initialize other fields */
    dset->var_num = -1;
    dset->num_records = 0;
    dset->num_dims = 0;
    memset(dset->dim_sizes, 0, sizeof(dset->dim_sizes));
    dset->type_id = H5I_INVALID_HID;
    dset->space_id = H5I_INVALID_HID;
    long data_type = -1;

    /* Parse dataset name to extract variable number */
    dset->var_num = CDFgetVarNum (file->id, dset->name);
    if (dset->var_num < CDF_OK) {
        cdf_print_error(dset->var_num);
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL,
                        "Failed to get variable number from name: %s", dset->name);
    }
    
    char tmp[CDF_VAR_NAME_LEN256 + 1]; /* Dummy buffer - we already know the name */
    /* Get variable info. */
    status = CDFinquirezVar(file->id, dset->var_num, tmp, &data_type,
                            &dset->num_elements, &dset->num_dims, dset->dim_sizes, 
                            &dset->rec_vary, dset->dim_varys);
    if (status != CDF_OK) {
        cdf_print_error(status);
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL,
                        "Failed to get variable info for var num %ld", dset->var_num);
    }

    if (data_type == CDF_CHAR || data_type == CDF_UCHAR) {
        /* CDF chars are exposed as null-terminated strings even if the underlying CDF attribute is not null-terminated */
        dset->type_id = create_fixed_string_type(dset->num_elements + 1); /* +1 for null terminator */
        if (dset->type_id == H5I_INVALID_HID) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL,
                            "Failed to create string datatype for CDF zVariable '%s'", dset->name);
        }
    } else {
        /* Map CDF data type to HDF5 datatype */
        if (cdf_get_hdf5_type_from_cdf(data_type, &dset->type_id) < 0) {
            FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_CANTGET, NULL,
                            "Failed to map CDF data type %ld to HDF5 datatype",
                            data_type);
        }
    }

    if ((dtype_class = H5Tget_class(dset->type_id)) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "Failed to get datatype class");
    }

    /* Get the maximum number of records written to this variable*/
    status = CDFgetzVarNumRecsWritten(file->id, dset->var_num, &dset->num_records);
    if (status != CDF_OK) {
        cdf_print_error(status);
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL,
                        "Failed to get number of records in CDF file");
    }

    /* First dimension is record dimension */
    dspace_dims[0] = (hsize_t)(dset->num_records); 
    
    /* Copy remaining dimensions. The 0th element will always be the number of records */
    for (int i = 0; i < dset->num_dims; i++) {
        dspace_dims[i+1] = (hsize_t)(dset->dim_sizes[i]);
    }

    /* Dataspace rank is number of variable dimensions + 1 for record dimension */
    int rank = dset->num_dims + 1;

    /* Create dataspace */
    if ((dset->space_id = H5Screate_simple(rank, dspace_dims, NULL)) < 0) {
        FUNC_GOTO_ERROR(H5E_DATASET, H5E_CANTCREATE, NULL, "Failed to create dataspace");
    }

    ret_value = dset_obj;

done:
    if (!ret_value && dset_obj) {
        H5E_BEGIN_TRY
        {
            cdf_dataset_close(dset_obj, dxpl_id, req);
        }
        H5E_END_TRY;
    }

    return ret_value;
} /* end cdf_dataset_open() */

herr_t cdf_dataset_read(size_t __attribute__((unused)) count, void *dset[], hid_t mem_type_id[],
                            hid_t __attribute__((unused)) mem_space_id[], hid_t file_space_id[],
                            hid_t __attribute__((unused)) dxpl_id, void *buf[],
                            void __attribute__((unused)) * *req)
{
    const cdf_object_t *dset_obj = (const cdf_object_t *) dset[0];
    const cdf_dataset_t *d = NULL; /* Convenience pointer */
    
    herr_t ret_value = SUCCEED;
    H5S_sel_type file_sel_type = H5S_SEL_ERROR;
    H5S_sel_type mem_sel_type = H5S_SEL_ERROR;
    hssize_t num_elements = 0;
    /* To follow H5S_ALL semantics, we set up local vars for effective values of mem/filespace */
    hid_t effective_file_space_id = file_space_id[0];
    hid_t effective_mem_space_id = mem_space_id[0];

    void *cdf_buf = NULL;
    void *selected_buf = NULL;
    void *gathered_buf = NULL;
    size_t selected_size = 0;
    size_t dataset_type_size = 0;
    size_t cdf_data_size = 0;

    assert(dset_obj);
    d = (const cdf_dataset_t *) &dset_obj->u.dataset;

    if (!buf[0]) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "invalid dataset buffer");
    }

    /* Get file dataspace selection type */
    if (file_space_id[0] == 0) {
        file_sel_type = H5S_SEL_ALL;
    } else if ((file_sel_type = H5Sget_select_type(file_space_id[0])) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL,
                        "failed to get file dataspace selection type");
    }

    /* Get memory dataspace selection type */
    if (mem_space_id[0] == 0) {
        mem_sel_type = H5S_SEL_ALL;
    } else if ((mem_sel_type = H5Sget_select_type(mem_space_id[0])) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL,
                        "failed to get memory dataspace selection type");
    }

    /* Determine number of elements to read based on selections */
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
        if (num_elements != H5Sget_select_npoints(mem_space_id[0])) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL,
                            "file and memory selections have different number of points");
        }
    }

    /* Prepare for gathering selected data from dataset */
    size_t prepare_num_elements;
    hssize_t temp_npoints;

    if (file_sel_type != H5S_SEL_ALL && effective_file_space_id != d->space_id) {
        /* Will need to gather - prepare full dataset */
        if ((temp_npoints = H5Sget_simple_extent_npoints(d->space_id)) < 0) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "failed to get dataset extent");
            }
        prepare_num_elements = (size_t) temp_npoints;
    } else {
        /* No gather needed - prepare only selected elements */
        if (num_elements < 0) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "invalid number of elements");
        }
        prepare_num_elements = (size_t) num_elements;
    }
    
    /* Calculate size of data to prepare */
    if ((dataset_type_size = H5Tget_size(d->type_id)) == 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "failed to get dataset type size");
    }

    cdf_data_size = prepare_num_elements * dataset_type_size;

    /* Allocate buffer for CDF data */
    if((cdf_buf = malloc(cdf_data_size)) == NULL) {
        FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL, "failed to allocate buffer for CDF data");
    }
    
    /* Call CDF library to read data */
    CDFstatus status;
    CDFid id = dset_obj->parent_file->u.file.id;
    long indices[CDF_MAX_DIMS];
    long counts[CDF_MAX_DIMS];
    long intervals[CDF_MAX_DIMS];

    /* Initialize dimension arrays */
    for (int i = 0; i < d->num_dims; i++) {
        indices[i] = 0L;
        intervals[i] = 1L;
        counts[i] = d->dim_sizes[i];
    }

    status = CDFhyperGetzVarData(id,
                                d->var_num,
                                0L, /* start reading from first record */
                                d->num_records, /* number of records to read (ALL) */
                                1L, /* stride */
                                indices,
                                counts,
                                intervals,
                                cdf_buf);
    if (status != CDF_OK) {
        cdf_print_error(status);
        FUNC_GOTO_ERROR(H5E_VOL, H5E_READERROR, FAIL, 
                        "failed to read data from CDF variable '%s'", d->name);
    }

    /* Prepare converted buffer if needed */
    hbool_t tconv_buf_allocated = FALSE;
    if (prepare_converted_buffer(d->type_id, mem_type_id[0], prepare_num_elements,
            cdf_buf, cdf_data_size, &selected_buf, &selected_size, 
            &tconv_buf_allocated) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "failed to prepare converted buffer");
    }

    /* Free temporary CDF buffer if new buffer was allocated in prepare_converted_buffer() */
    if (tconv_buf_allocated && cdf_buf) {
        free(cdf_buf);
        cdf_buf = NULL;
    }

    /* If file selection is non-trivial (hyperslab, points), gather selected data first */
    if (file_sel_type != H5S_SEL_ALL && effective_file_space_id != d->space_id) {
        /* Allocate buffer for gathered data */
        if (num_elements < 0) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "invalid number of elements for gather");
        }
        size_t gathered_size = (size_t) num_elements * H5Tget_size(mem_type_id[0]);
        
        if ((gathered_buf = malloc(gathered_size)) == NULL) {
            FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL, "failed to allocate buffer for gathered data");
        }

        /* Gather selected data from source buffer according to file space selection.
         * Note: We pass file_space_id[0] which has the selection, and selected_buf which
         * must be sized according to the full extent described by the selection's dataspace.
         */
        if (H5Dgather(file_space_id[0], selected_buf, mem_type_id[0], gathered_size, gathered_buf,
                      NULL, NULL) < 0) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_READERROR, FAIL, "failed to gather selected data");
        }

        /* Free original buffer if we allocated it for type conversion */
        if (tconv_buf_allocated && selected_buf) {
            free(selected_buf);
            selected_buf = NULL;
        }

        /* Use gathered buffer as new source */
        selected_buf = gathered_buf;
        selected_size = gathered_size;
        tconv_buf_allocated = TRUE;
    }

    /* Transfer data to user buffer (handles selections via scatter if needed) */
    if (transfer_data_to_user(selected_buf, selected_size, mem_type_id[0], effective_mem_space_id,
        buf[0]) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_READERROR, FAIL, "failed to transfer data to user buffer");
    }

done:
    /* Clean up allocated buffer if needed */
    if (tconv_buf_allocated && selected_buf) {
        free(selected_buf);
    }

    if (cdf_buf) {
        free(cdf_buf);
    }

    if (ret_value < 0 && gathered_buf) {
        free(gathered_buf);
    }

    return ret_value;
} /* end cdf_dataset_read() */

herr_t cdf_dataset_get(void *dset, H5VL_dataset_get_args_t *args,
                           hid_t __attribute__((unused)) dxpl_id,
                           void __attribute__((unused)) * *req)
{
    const cdf_object_t *o = (const cdf_object_t *) dset;
    const cdf_dataset_t *d = &o->u.dataset;

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
} /* end cdf_dataset_get() */

herr_t cdf_dataset_close(void *dset, hid_t dxpl_id, void **req)
{
    cdf_object_t *d = (cdf_object_t *) dset;
    herr_t ret_value = SUCCEED;

    assert(d);

    /* Use FUNC_DONE_ERROR to try to complete resource release after failure */
    if (!d->parent_file){
        FUNC_DONE_ERROR(H5E_VOL, H5E_BADVALUE, FAIL,
                        "Dataset has no valid parent file reference");
    }

    /* Decrement dataset's ref count */
    if (d->ref_count == 0) {
        FUNC_DONE_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL,
                        "Dataset already closed (ref_count is 0)");
    }
    d->ref_count--;

    /* Only do the real close when ref_count reaches 0 */
    if (d->ref_count == 0) {
        if (d->u.dataset.name) {
            free(d->u.dataset.name);
        }
        if (d->u.dataset.space_id != H5I_INVALID_HID) {
            if (H5Sclose(d->u.dataset.space_id) < 0) {
                FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "Failed to close dataspace");
            }
        }
        if (d->u.dataset.type_id != H5I_INVALID_HID) {
            if (H5Tclose(d->u.dataset.type_id) < 0) {
                FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "Failed to close datatype");
            }
        }
        /* Decrement parent file's reference count */
        if (cdf_file_close(d->parent_file, dxpl_id, req) < 0) {
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "Failed to close dataset file object");
        }

        free(d);
    }

    return ret_value;
} /* end cdf_dataset_close() */

/* Group operations */
void *cdf_group_open(void *obj, const H5VL_loc_params_t __attribute__((unused)) * loc_params,
                         const char *name, hid_t __attribute__((unused)) gapl_id,
                         hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    cdf_object_t *file = (cdf_object_t *) obj;
    cdf_object_t *grp_obj = NULL;
    cdf_object_t *ret_value = NULL;

    cdf_group_t *grp = NULL; /* Convenience pointer */

    if (!file || !name) {
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "Invalid file or group name");
    }

    if (strcmp(name, "/") != 0) {
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_UNSUPPORTED, NULL,
                        "CDF VOL connector currently only supports root group '/'");
    }

    if ((grp_obj = (cdf_object_t *) calloc(1, sizeof(cdf_object_t))) == NULL) {
        FUNC_GOTO_ERROR(H5E_SYM, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for CDF group struct");
    }

    grp_obj->obj_type = H5I_GROUP;
    grp_obj->parent_file = file->parent_file;
    grp_obj->ref_count = 1; /* Initialize group's own ref count */
    /* Increment file reference count since this group holds a reference */
    grp_obj->parent_file->ref_count++;

    grp = &grp_obj->u.group;
    if ((grp->name = strdup(name)) == NULL) {
        FUNC_GOTO_ERROR(H5E_SYM, H5E_CANTALLOC, NULL, "Failed to duplicate group name string");
    }

    ret_value = grp_obj;
done:
    if (!ret_value && grp) {
        H5E_BEGIN_TRY
        {
            cdf_group_close(grp_obj, dxpl_id, req);
        }
        H5E_END_TRY;
    }

    return ret_value;
}

herr_t cdf_group_get(void *obj, H5VL_group_get_args_t *args,
                         hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    cdf_object_t *o = (cdf_object_t *) obj;
    const cdf_group_t *grp = (const cdf_group_t *) &o->u.group; /* Convenience pointer */
    herr_t ret_value = SUCCEED;

    if (!args) {
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid arguments");
    }
    
    switch (args->op_type) {
        case H5VL_GROUP_GET_INFO: {
            H5G_info_t *ginfo = args->args.get_info.ginfo;
            long num_zvars = 0;

            if (!grp || !ginfo)
                FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid group or info pointer");

            if (!o->parent_file)
                FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "invalid file object");

            /* Get number of zVariables in the CDF file (datasets) */
            if (CDFgetNumzVars(o->parent_file->u.file.id, &num_zvars) != CDF_OK)
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "failed to get number of zVariables");

            /* Fill in group info structure */
            ginfo->storage_type = H5G_STORAGE_TYPE_COMPACT;
            ginfo->nlinks = (hsize_t) num_zvars; /* Number of dataset links */
            ginfo->max_corder = -1;   /* No creation order tracking */
            ginfo->mounted = false;   /* No files mounted on this group */

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

herr_t cdf_group_close(void *grp, hid_t dxpl_id, void **req)
{
    cdf_object_t *o = (cdf_object_t *) grp;
    cdf_group_t *g = &o->u.group; /* Convenience pointer */
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

        /* Decrement parent file's reference count */
        if (cdf_file_close(o->parent_file, dxpl_id, req) < 0)
            FUNC_DONE_ERROR(H5E_SYM, H5E_CLOSEERROR, FAIL, "Failed to close group file object");

        free(o);
    }

    return ret_value;
}

/* Helper function: Calculate the size of the string needed to represent
 * a gEntry value given the datatype and number of elements */
static size_t calculate_gEntry_str_len(long cdf_datatype, long num_elements)
{
    
    if (num_elements <= 0){
        return 0; /* No elements */
    }

    /* Determine max string length needed for a single value based on CDF datatype */
    size_t value_size = 0;
    switch (cdf_datatype) {
        case CDF_CHAR:
        case CDF_UCHAR:
            value_size = 1; /* Single character */
            break;

        case CDF_UINT1:
            value_size = 3; /* "255" */
            break;

        case CDF_BYTE:
        case CDF_INT1:
            value_size = 4; /* "-128" */
            break;

        case CDF_UINT2:
            value_size = 5; /* "65535" */
            break;
            
        case CDF_INT2:
            value_size = 6; /* "-32768" */
            break;

        case CDF_UINT4:
            value_size = 10; /* "4294967295" */
            break;

        case CDF_INT4:
            value_size = 11; /* "-2147483648" */
            break;

        case CDF_INT8:
        case CDF_TIME_TT2000:
            value_size = 20; /* "-9223372036854775808" */
            break;

        case CDF_REAL4:
        case CDF_FLOAT:
            value_size = FLT_DECIMAL_DIG; /* Maximum precision length needed for conversion of float to string and back */
            break;

        case CDF_REAL8:
        case CDF_DOUBLE:
        case CDF_EPOCH:
            value_size = DBL_DECIMAL_DIG; /* Maximum precision length needed for conversion of double to string and back */
            break;
            
        case CDF_EPOCH16:
            value_size = 2 * DBL_DECIMAL_DIG + 2 + 2;  /* 2 doubles + a comma and a space + 2 brackets */
            break;
            
        default:
            return 0; /* Unsupported datatype */
    }

    /* Format overhead for (index)(CDF_TYPE/num_elements): prefix.
     * Fixed 57 bytes covers max index (19) + datatype name (15) + 
     * num_elements (19) + fixed chars "(/): " (4) */
    size_t format_overhead = 57; 

    size_t n = (size_t)num_elements; /* Convert to size_t for calculations */
    size_t total_size = value_size * n; /* Size for elements only */

    if (n > 1 && cdf_datatype != CDF_CHAR && cdf_datatype != CDF_UCHAR) {
        total_size += (n - 1) * 2 + 2; /* Add size for ", " separators and enclosing brackets */
    }

    return format_overhead + total_size;
} /* end calculate_gEntry_str_len() */


void *cdf_attr_open(void *obj, const H5VL_loc_params_t *loc_params, const char *name,
                        hid_t __attribute__((unused)) aapl_id,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    cdf_object_t *parent_obj = NULL;
    cdf_object_t *attr_obj = NULL;
    cdf_object_t *ret_value = NULL;
    hsize_t dims[1];

    cdf_attr_t *attr = NULL; /* Convenience pointer */

    if (!obj || !name || !loc_params) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "Invalid object or attribute name");
    }

    parent_obj = (cdf_object_t *) obj;

    /* Determine the type of the parent object */
    if (loc_params->type != H5VL_OBJECT_BY_SELF) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL,
                        "Unsupported location parameter type for attribute open");
    }

    if ((attr_obj = (cdf_object_t *) calloc(1, sizeof(cdf_object_t))) == NULL) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL,
                        "Failed to allocate memory for CDF attribute struct");
    }

    attr_obj->obj_type = H5I_ATTR;
    attr_obj->parent_file = parent_obj->parent_file;
    attr_obj->ref_count = 1; /* Initialize attribute's own ref count */
    /* Increment file reference count since this attribute holds a reference */
    attr_obj->parent_file->ref_count++;
    /* Increment parent object's reference count since this attribute holds a reference */
    parent_obj->ref_count++;
    attr = &attr_obj->u.attr;

    /* Parse attribute name to potentially extract index
     *  Otherwise the name will just be used as is */
    if (parse_attr_name(name, attr) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL, "Failed to parse attribute name '%s'", name);
    }

    if (!attr->name || strlen(attr->name) == 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL, "Attribute name is NULL after parsing");
    }

    attr->parent = obj;
    attr->space_id = H5I_INVALID_HID;
    attr->type_id = H5I_INVALID_HID;

    /* Check if attribute exists in CDF file at all (Either as gAttribute or vAttribute) */
    CDFid id = parent_obj->parent_file->u.file.id;
    attr->attr_num = CDFgetAttrNum(id, attr->name);

    if (attr->attr_num < CDF_OK) {
        cdf_print_error(attr->attr_num);
        FUNC_GOTO_ERROR(H5E_VOL, H5E_NOTFOUND, NULL, "Attribute '%s' not found in CDF file.", attr->name);
    }

    /* Inquire attribute information from CDF */
    CDFstatus status;
    long max_gEntry, max_rEntry, max_zEntry;
    char attrName[CDF_ATTR_NAME_LEN256+1];

    status = CDFinquireAttr(id,
                            attr->attr_num, 
                            attrName, 
                            &attr->scope,
                            &max_gEntry,
                            &max_rEntry,  /* Only applicable for vAttribute */
                            &max_zEntry); /* Only applicable for vAttribute */
    if (status != CDF_OK) {
        cdf_print_error(status);
        FUNC_GOTO_ERROR(H5E_VOL, H5E_READERROR, NULL, 
                        "failed to read data from CDF attribute '%s'", 
                        attr->name);
    }
    
    /* Determine if parent object is a file or group */
    bool is_file_or_group = (parent_obj->obj_type == H5I_FILE || parent_obj->obj_type == H5I_GROUP);

    /* Only allow gAttributes to be opened when using file or group obj_type */
    if (is_file_or_group && attr->scope == GLOBAL_SCOPE) {
        
        if (max_gEntry < 0) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL,
                            "gAttribute '%s' has no gEntries", attr->name);
        }

        cdf_file_t *parent_file = &parent_obj->u.file;
        
        /* Set dataspace either for array of gEntries or single gEntry based on indexing */
        if (attr->indexed) {
            if (attr->index > max_gEntry) {
                FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, NULL,
                                "gAttribute '%s' index %ld exceeds maximum gEntry index %ld",
                                attr->name, attr->index, max_gEntry);
            }

            /* Query specific gEntry using parsed index */
            status = CDFinquireAttrgEntry(id, attr->attr_num, attr->index, &attr->datatype,
                                    &attr->num_elements);
            if (status != CDF_OK) {
                cdf_print_error(status);
                FUNC_GOTO_ERROR(H5E_VOL, H5E_READERROR, NULL,
                                "failed to read data from CDF gEntry #%ld from CDF gAttribute '%s'", 
                                attr->index, attr->name);
            }

            /* Ensure valid number of elements */
            if (attr->num_elements <= 0) {
                    FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL,
                                    "gAttribute '%s' has invalid number of elements: %ld",
                                    attr->name, attr->num_elements);
            }

            /* Create dataspace based on type and num_elements */
            if (attr->datatype == CDF_CHAR || attr->datatype == CDF_UCHAR) { /* String attribute */
                /* Create scalar dataspace for string attribute */
                if ((attr->space_id = H5Screate(H5S_SCALAR)) < 0) {
                    FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "Failed to create scalar dataspace for attribute.");
                }

                /* CDF chars are exposed as null-terminated strings even if the underlying CDF attribute is not null-terminated */
                attr->type_id = create_fixed_string_type(attr->num_elements + 1); /* +1 for null terminator */
                if (attr->type_id == H5I_INVALID_HID) {
                    FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL,
                                    "Failed to create string datatype for gAttribute '%s'", attr->name);
                }
            }
            else { /* Non-string indexed attribute */
                
                if (attr->num_elements == 1) {
                    /* Create scalar dataspace for single gEntry */
                    if ((attr->space_id = H5Screate(H5S_SCALAR)) < 0) {
                        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "Failed to create scalar dataspace for gAttribute '%s'", attr->name);
                    }
                }
                else {
                    /* Create 1D dataspace for array gEntry */
                    dims[0] = (hsize_t)(attr->num_elements);
                    if ((attr->space_id = H5Screate_simple(1, dims, NULL)) < 0) {
                        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "Failed to create dataspace for gAttribute '%s'", attr->name);
                    }
                }

                /* Map CDF data type to HDF5 datatype */
                if (cdf_get_hdf5_type_from_cdf(attr->datatype, &attr->type_id) < 0) {
                    FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL,
                                    "Failed to map CDF data type %ld to HDF5 datatype for gAttribute '%s'",
                                    attr->datatype, attr->name);    
                }
            }
        } else { /* Not indexed - create dataspace for all gEntries */
            long datatype;         /* Reported datatype from CDF */
            long num_elements;     /* Reported number of elements from CDF */
            long num_gEntries;     /* Total number of gEntries for this attribute */
            long count = 0;        /* Count of valid gEntries found (for indexing the gEntry_indices array) */
            size_t gEntry_len = 0; /* Length of current gEntry string */
            size_t max_len = 0;    /* Max length of single gEntry string */
            
            /* Get total number of gEntries */
            status = CDFgetNumAttrgEntries(id, attr->attr_num, &num_gEntries);
            if (status != CDF_OK) {
                cdf_print_error(status);
                FUNC_GOTO_ERROR(H5E_VOL, H5E_READERROR, NULL,
                                "failed to get number of gEntries for gAttribute '%s'", attr->name);
            }

            /* Allocate array to store indices of existing gEntries (those that can be read successfully) */
            attr->gEntry_indices = (long *) malloc(num_gEntries * sizeof(long));
            if (!attr->gEntry_indices) {
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL,
                                "Failed to allocate memory for gEntry indices for gAttribute '%s'", attr->name);
            }

            /* Determine maximum string length among all gEntries 
             * Notes:
             * - NO_SUCH_ENTRY and existing entries with num_elements == 0
             *   are both represented as empty strings.
             * - max_len may legitimately remain 0 if all gEntries are empty;
             *   the resulting HDF5 string type will still have size 1
             *   to accommodate the null terminator. 
             */
            for (int i=0; i <= max_gEntry; i++) {
                status = CDFinquireAttrgEntry (id, attr->attr_num, (long) i, &datatype, 
                                        &num_elements);
                if (status == NO_SUCH_ENTRY || num_elements == 0) {
                    gEntry_len = 0; /* Empty string for non-existent gEntry */
                }
                else if (status != CDF_OK) {
                    cdf_print_error(status);
                    FUNC_GOTO_ERROR(H5E_VOL, H5E_READERROR, NULL, 
                                    "failed to read data from CDF gEntry #%d from CDF gAttribute '%s'", 
                                    i, attr->name);
                } else {
                    gEntry_len = calculate_gEntry_str_len(datatype, num_elements);
                    if (gEntry_len <= 0) {
                        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL,
                                        "failed to calculate string size for gEntry #%d from gAttribute '%s'", i, attr->name);
                    }
                    attr->gEntry_indices[count] = (long) i; /* Store index of existing gEntry */
                    count++;
                }
                if (gEntry_len > max_len) {
                    max_len = gEntry_len;
                }
            }

            /* Unindexed gAttributes are exposed as an array of strings, one per gEntry, 
             * regardless of underlying CDF datatype */
            dims[0] = count; /* Number of valid gEntries */
            if ((attr->space_id = H5Screate_simple(1, dims, NULL)) < 0) {
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, 
                    "Failed to create dataspace for gAttribute '%s'", attr->name);
            }

            /* Create string datatype for gAttribute, adding 1 for null terminator */
            attr->type_id = create_fixed_string_type(max_len + 1);
            if (attr->type_id == H5I_INVALID_HID) {
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL,
                                "Failed to create string datatype for gAttribute '%s'", attr->name);
            }
        }
    }
    /* Only allow vAttributes to be opened when using dataset obj_type */  
    else if (parent_obj->obj_type == H5I_DATASET && attr->scope == VARIABLE_SCOPE) {
        if (attr->indexed) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_UNSUPPORTED, NULL,
                            "Indexed vAttributes are not supported");
        }
        
        if (max_zEntry < 0) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL,
                            "vAttribute '%s' has no zEntries", attr->name);
        }
        cdf_dataset_t *parent_dset = &parent_obj->u.dataset;

        /* vAttribute entries are indexed by varNum; values above max entry num cannot exist 
         * (only supports zVariables and zEntries at the moment) */
        if (parent_dset->var_num > max_zEntry) {
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_NOTFOUND, NULL, "vAttribute '%s' does not exist for variable number %ld", attr->name, parent_dset->var_num);
        }

        /* Inquire vAttribute zEntry info */
        status = CDFinquireAttrzEntry (id, attr->attr_num, parent_dset->var_num,
                               &attr->datatype, &attr->num_elements);
        if (status != CDF_OK) {
            cdf_print_error(status);
            FUNC_GOTO_ERROR(H5E_VOL, H5E_READERROR, NULL, 
                            "failed to read data from CDF vAttribute '%s'", 
                            attr->name); 
        }

        if (attr->num_elements <= 0) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL,
                            "vAttribute '%s' has invalid number of elements: %ld",
                            attr->name, attr->num_elements);
        }

        /* Create dataspace dynamically based on num_elements and whether or not the attribute is a string */
        if (attr->datatype == CDF_CHAR || attr->datatype == CDF_UCHAR) {  /* String attribute */
            /* Create scalar dataspace for string attribute */
            if ((attr->space_id = H5Screate(H5S_SCALAR)) < 0) {
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "Failed to create scalar dataspace for attribute.");
            }

            /* CDF chars are exposed as null-terminated strings even if the underlying CDF attribute is not null-terminated */
            attr->type_id = create_fixed_string_type(attr->num_elements + 1); /* +1 for null terminator */
            if (attr->type_id == H5I_INVALID_HID) {
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL,
                                "Failed to create string datatype for vAttribute '%s'", attr->name);
            }
        } 
        else { /* Non-string attribute */
            if (attr->num_elements == 1) {
                /* Scalar dataspace for single element attributes */
                if ((attr->space_id = H5Screate(H5S_SCALAR)) < 0) {
                    FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "Failed to create scalar dataspace for vAttribute '%s'", attr->name);
                }
            }
            else {
                /* Simple dataspace for multi-element attributes */
                dims[0] = (hsize_t)(attr->num_elements);
                if ((attr->space_id = H5Screate_simple(1, dims, NULL)) < 0) {
                    FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCREATE, NULL, "Failed to create simple dataspace for vAttribute '%s'", attr->name);
                }
            }

            /* Map CDF datatype to HDF5 datatype */
            if (cdf_get_hdf5_type_from_cdf(attr->datatype, &attr->type_id) < 0) {
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL,
                                "Failed to map CDF data type %ld to HDF5 datatype",
                                attr->datatype);
            }
        }
    } else {
        /* Unknown attribute - report error */
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_NOTFOUND, NULL, "Unknown attribute '%s' on object type %d",
                        attr->name, parent_obj->obj_type);
    }

    ret_value = attr_obj;
done:
    if (!ret_value && attr_obj) {
        H5E_BEGIN_TRY
        {
            cdf_attr_close(attr_obj, dxpl_id, req);
        }
        H5E_END_TRY;
    }
    return ret_value;
} /* end cdf_attr_open() */

/* Helper function: stringify a single gEntry value based on CDF datatype */
static herr_t stringify_single_gEntry_value(long cdf_datatype, unsigned char *buffer, 
                                             char *out, size_t size)
{
    herr_t ret_value = SUCCEED;

    /* Ensure there's no error with the buffer or out */
    if (!out || !buffer) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "Invalid buffer or output pointer");
    }

    memset(out, 0, size);

    switch (cdf_datatype) {
        case CDF_BYTE:
        case CDF_INT1: {
            int8_t v = 0;
            memcpy(&v, buffer, sizeof(v));
            snprintf(out, size, "%" PRId8, v);
            break;
        }
        case CDF_UINT1: {
            uint8_t v = 0;
            memcpy(&v, buffer, sizeof(v));
            snprintf(out, size, "%" PRIu8, v);
            break;
        }

        case CDF_INT2: {
            int16_t v = 0;
            memcpy(&v, buffer, sizeof(v));
            snprintf(out, size, "%" PRId16, v);
            break;
        }
        case CDF_UINT2: {
            uint16_t v = 0;
            memcpy(&v, buffer, sizeof(v));
            snprintf(out, size, "%" PRIu16, v);
            break;
        }

        case CDF_INT4: {
            int32_t v = 0;
            memcpy(&v, buffer, sizeof(v));
            snprintf(out, size, "%" PRId32, v);
            break;
        }
        case CDF_UINT4: {
            uint32_t v = 0;
            memcpy(&v, buffer, sizeof(v));
            snprintf(out, size, "%" PRIu32, v);
            break;
        }

        case CDF_INT8:
        case CDF_TIME_TT2000: {
            int64_t v = 0;
            memcpy(&v, buffer, sizeof(v));
            snprintf(out, size, "%" PRId64, v);
            break;
        }

        case CDF_REAL4:
        case CDF_FLOAT: {
            float v = 0.0f;
            memcpy(&v, buffer, sizeof(v));
            snprintf(out, size, "%.8g", (double)v);
            break;
        }

        case CDF_REAL8:
        case CDF_DOUBLE:
        case CDF_EPOCH: {
            double v = 0.0;
            memcpy(&v, buffer, sizeof(v));
            snprintf(out, size, "%.16g", v);
            break;
        }

        case CDF_EPOCH16: {
            double vals[2] = {0.0, 0.0};
            memcpy(vals, buffer, sizeof(vals));
            snprintf(out, size, "{%.8g, %.8g}", vals[0], vals[1]);
            break;
        }

        default:
            /* If we get here, datatype isn't recognized */
            FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "Unsupported CDF datatype for stringification: %ld", cdf_datatype);
            break;
    }

done:
    return ret_value;
} /* end stringify_single_gEntry_value() */

/* Helper function: Get the name of a CDF datatype as a string. 
 * Note: the maximum buffer size should be 16 characters (which includes null terminator)*/
static herr_t get_cdf_datatype_name(long cdf_datatype, char *name_buf, size_t buf_size) {
    herr_t ret_value = SUCCEED;
    const char *type_name = NULL;
    switch (cdf_datatype) {
        case CDF_CHAR: type_name = "CDF_CHAR"; break;
        case CDF_UCHAR: type_name = "CDF_UCHAR"; break;
        case CDF_BYTE: type_name = "CDF_BYTE"; break;
        case CDF_INT1: type_name = "CDF_INT1"; break;
        case CDF_UINT1: type_name = "CDF_UINT1"; break;
        case CDF_INT2: type_name = "CDF_INT2"; break;
        case CDF_UINT2: type_name = "CDF_UINT2"; break;
        case CDF_INT4: type_name = "CDF_INT4"; break;
        case CDF_UINT4: type_name = "CDF_UINT4"; break;
        case CDF_INT8: type_name = "CDF_INT8"; break;
        case CDF_TIME_TT2000: type_name = "CDF_TIME_TT2000"; break;
        case CDF_REAL4: type_name = "CDF_REAL4"; break;
        case CDF_FLOAT: type_name = "CDF_FLOAT"; break;
        case CDF_REAL8: type_name = "CDF_REAL8"; break;
        case CDF_DOUBLE: type_name = "CDF_DOUBLE"; break;
        case CDF_EPOCH: type_name = "CDF_EPOCH"; break;
        case CDF_EPOCH16: type_name = "CDF_EPOCH16"; break;
        default:
            FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "Unsupported CDF datatype: %ld", cdf_datatype);
            break;
    }

    if (strlen(type_name) + 1 > buf_size) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "Buffer too small for CDF datatype name");
    }

    strncpy(name_buf, type_name, buf_size);
    name_buf[buf_size - 1] = '\0'; /* Ensure null termination */

done:
    return ret_value;
} /* end get_cdf_datatype_name() */

/* Helper function: stringify a given gEntry */
static herr_t stringify_gEntry(CDFid id, long attr_num, long gEntry_index, long cdf_datatype, 
                                long num_elements, size_t str_size, char *out )
{
    herr_t ret_value = SUCCEED;
    CDFstatus status;
    long dtype_size;
    unsigned char *buffer = NULL;
    size_t offset = 0;

    /* Handle empty output buffer or zero str_size */
    if (!out || str_size == 0){
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "Invalid output buffer or str_size for gEntry stringification");
    }

    /* Should already be handled in cdf_attr_read, but return empty string if num_elements == 0 */
    if (num_elements == 0) {
        out[0] = '\0';
        return ret_value;
    }
    char cdf_type_name[16];
    if (get_cdf_datatype_name(cdf_datatype, cdf_type_name, sizeof(cdf_type_name)) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, "Failed to get CDF datatype name for gEntry stringification");
    }

    /* Format the prefix: "<index> (CDF_DATATYPE/num_elements): "*/
    int prefix_len = snprintf(out, str_size, "%ld (%s/%ld): ", gEntry_index, cdf_type_name, num_elements);
    if (prefix_len < 0 || (size_t)prefix_len >= str_size) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL,
                        "Buffer overflow: prefix for gEntry stringification exceeds allocated space");
    }

    offset = (size_t)prefix_len; /* Start writing after the prefix */

    /* Get size of the CDF datatype to calculate buffer size needed to read the gEntry data */
    status = CDFgetDataTypeSize(cdf_datatype, &dtype_size);
    if (status != CDF_OK) {
        cdf_print_error(status);
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to get CDF data type size");
    }
    /* Ensure valid datatype size */
    if (dtype_size <= 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Invalid CDF data type size");
    }

    /* Calculate buffer size needed to store the data */
    size_t elem_size = (size_t) dtype_size;
    size_t total_size = elem_size * (size_t)num_elements;

    /* String case */
    if (cdf_datatype == CDF_CHAR || cdf_datatype == CDF_UCHAR) {
        /* Read string data directly into output buffer */
        status = CDFgetAttrgEntry(id, attr_num, gEntry_index, out + offset);
        if (status != CDF_OK){
            cdf_print_error(status);
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to get CDF attribute gEntry data");
        }

        /* Ensure null-termination */
        out[offset + total_size] = '\0';
        FUNC_GOTO_DONE(SUCCEED);
    }

    /* NON-STRING TYPES */
    buffer = (unsigned char *) malloc(total_size);
    if (!buffer) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, FAIL, "Failed to allocate buffer for gEntry data");
    }

    /* Read data into buffer */
    status = CDFgetAttrgEntry(id, attr_num, gEntry_index, buffer);
    if (status != CDF_OK) {
        cdf_print_error(status);
        free(buffer);
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to get CDF attribute gEntry data");
    }

    /* Deal with single element gEntry */
    if (num_elements == 1) {
        char tmp[64]; /* Temporary buffer for single value */

        /* Stringify single value */
        ret_value = stringify_single_gEntry_value(cdf_datatype, buffer, tmp, sizeof(tmp));
        free(buffer);
        if (ret_value == FAIL) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "Failed to stringify single gEntry value");
        }
        
        /* Check that the stringified value fits in the remaining buffer space */
        size_t tmp_len = strnlen(tmp, sizeof(tmp));
        if (offset + tmp_len >= str_size) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL,
                            "Buffer overflow: stringified gEntry exceeds allocated space");
        }

        /* Copy to output */
        memcpy(out + offset, tmp, tmp_len);
        out[offset + tmp_len] = '\0';
        FUNC_GOTO_DONE(ret_value);
    }

    /* MULTI-ELEMENT gEntry */
    char tmp[64]; /* Temporary buffer for each element (max 52 chars for EPOCH16) */
    out[offset++] = '[';  /* Start the array with a bracket */

    for (size_t i = 0; i < (size_t)num_elements; i++) {
        /* Add separator for all but the first element */
        if (i > 0) {
            if (offset + 2 >= str_size) { /* Check for space for ", " */
                FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL,
                                "Buffer overflow: stringified gEntry exceeds allocated space");
            }

            out[offset++] = ',';  /* Comma separator */
            out[offset++] = ' ';  /* Space after comma */
        }

        /* Format element into temporary buffer */
        ret_value = stringify_single_gEntry_value(
            cdf_datatype,
            buffer + (i * elem_size), /* Pointer to current element */
            tmp,
            sizeof(tmp)
        );
        if (ret_value != SUCCEED) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL,
                            "Failed to stringify element %zu", i);
        }

        size_t len = strlen(tmp);

        /* Check for space for element string */
        if (offset + len >= str_size) { 
            FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL,
                            "Buffer overflow: stringified gEntry exceeds allocated space");
        }

        /* Copy the stringified element to the output buffer */
        memcpy(out + offset, tmp, len);  /* Copy formatted string */
        offset += len;  /* Move offset forward */
    }

    out[offset++] = ']';  /* End the array with a bracket */
    out[offset] = '\0';    /* Null-terminate the string */

    /* Check that the stringified gEntry does not overflow */
    if (strlen(out) >= str_size) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_BADVALUE, FAIL, 
                    "Buffer overflow: stringified gEntry exceeds allocated space");
    }

done:
    if (buffer) {
        free(buffer);
        buffer = NULL;
    }

    return ret_value;
} /* end stringify_gEntry() */


herr_t cdf_attr_read(void *attr, hid_t mem_type_id, void *buf, hid_t dxpl_id, void **req)
{
    const cdf_object_t *o = (const cdf_object_t *) attr;
    const cdf_attr_t *a = NULL;
    const cdf_object_t *parent_obj = NULL;
    const cdf_dataset_t *parent_dset = NULL;
    herr_t ret_value = SUCCEED;

    /* Datatype conversion variables */
    void *cdf_buf = NULL;
    void *converted_buf = NULL;
    size_t converted_buf_size = 0;
    size_t dataset_type_size = 0;
    size_t cdf_data_size = 0;
    hbool_t tconv_buf_allocated = FALSE;

    /* Temporary buffer for string attribute read */
    char *str_buf = NULL; 

    CDFstatus status;
    CDFid id = o->parent_file->u.file.id;
    
    assert(o);

    a = &o->u.attr;

    if (!buf) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "invalid attribute buffer");
    }

    /* gAttribute case: stringified gEntries */
    if (a->scope == GLOBAL_SCOPE) {
        if (a->indexed) {

            /* Check if string attribute */
            if (a->datatype == CDF_CHAR || a->datatype == CDF_UCHAR) {

                if (!H5Tequal(mem_type_id, a->type_id)) {
                    FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_BADTYPE, FAIL,
                                    "Datatype conversion not supported for CDF string attributes");
                }

                /* Verify HDF5 datatype size matches CDF string length + null terminator */
                size_t hdf5_size = H5Tget_size(a->type_id);
                if (hdf5_size != a->num_elements + 1) {
                    FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_BADTYPE, FAIL,
                                    "CDF string attribute size mismatch for '%s'", a->name);
                }

                /* Read string attribute */
                str_buf = (char *)malloc(a->num_elements); /* Temporary buffer for CDF string (can be big so avoid stack allocation) */
                if (!str_buf) {
                    FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL,
                                    "Failed to allocate temporary buffer for gAttribute string read");
                }

                memset(str_buf, 0, a->num_elements);

                status = CDFgetAttrgEntry(id, a->attr_num, a->index, str_buf);
                if (status != CDF_OK) {
                    cdf_print_error(status);
                    FUNC_GOTO_ERROR(H5E_VOL, H5E_READERROR, FAIL, 
                                    "failed to read string data from CDF gEntry #%ld from gAttribute '%s'", 
                                    a->index, a->name); 
                }

                /* Copy to user buffer, ensuring null-termination */
                memcpy(buf, str_buf, a->num_elements);
                ((char *)buf)[a->num_elements] = '\0'; /* Null-terminate */

            } else { /* Non-string indexed gAttribute */
                /* Get CDF datatype size */
                if ((dataset_type_size = H5Tget_size(a->type_id)) == 0) {
                    FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "failed to get dataset type size");
                }
                cdf_data_size = a->num_elements * dataset_type_size;

                /* Allocate buffer for CDF data */
                if((cdf_buf = malloc(cdf_data_size)) == NULL) {
                    FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL, "failed to allocate buffer for CDF data");
                }

                /* Read non-string global attribute */
                status = CDFgetAttrgEntry(id, a->attr_num, a->index, cdf_buf);
                if (status != CDF_OK) {
                    cdf_print_error(status);
                    FUNC_GOTO_ERROR(H5E_VOL, H5E_READERROR, FAIL, 
                                    "failed to read data from CDF gEntry #%ld from gAttribute '%s'", 
                                    a->index, a->name);
                }

                /* Prepare converted buffer if needed */
                if (prepare_converted_buffer(a->type_id, mem_type_id, (size_t)a->num_elements,
                        cdf_buf, cdf_data_size, &converted_buf, &converted_buf_size, 
                        &tconv_buf_allocated) < 0) {
                    FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "failed to prepare converted buffer");
                }

                /* Free temporary CDF buffer if new buffer was allocated in prepare_converted_buffer() */
                if (tconv_buf_allocated && cdf_buf) {
                    free(cdf_buf);
                    cdf_buf = NULL;
                }

                /* Copy data to user buffer */
                memcpy(buf, converted_buf, converted_buf_size);
            }
        } else { 
            /* Non-indexed global attribute - read all entries as array of fixed length strings */

            /* Cannot support type conversion for string array*/
            if (!H5Tequal(mem_type_id, a->type_id)) {
                    FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_BADTYPE, FAIL,
                                    "Datatype conversion not supported for non-indexed gAttributes");
            }

            if (!a->gEntry_indices) {
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL,
                                "gEntry indices array not set for gAttribute '%s'", a->name);
            }

            /* Determine HDF5 string length and validate */
            size_t str_size = H5Tget_size(a->type_id);
            if (str_size == 0) {
                FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_CANTGET, FAIL,
                                "Reported string size for gAttribute array is zero '%s'", a->name);
            }

            /* Determine number of entries from the dataspace */
            hsize_t dims[1];
            if (H5Sget_simple_extent_dims(a->space_id, dims, NULL) < 0) {
                FUNC_GOTO_ERROR(H5E_DATASPACE, H5E_CANTGET, FAIL,
                                "Failed to get dataspace extent for gAttribute '%s'", a->name);
            }

            size_t num_entries = (size_t)dims[0];

            /* Set the buffer to all zeros */
            memset(buf, 0, num_entries * str_size);

            /* Read each gEntry string into the user buffer */
            for (size_t i = 0; i < num_entries; i++) {
                /* Find location of current string in out buffer */
                char *entry_buf = (char *)buf + i * str_size;

                /* Get the actual CDF entry index from the gEntry indices array */
                long cdf_entry_index = a->gEntry_indices[i];

                /* Inquire gEntry to ensure it exists and get its datatype/num_elements */
                long datatype, num_elements;
                status = CDFinquireAttrgEntry (id, a->attr_num, cdf_entry_index,
                                       &datatype, &num_elements);
                if (status == NO_SUCH_ENTRY || num_elements == 0) {
                    /* Entry no longer exists -> write empty string */
                    memset(entry_buf, 0, str_size);
                    continue;
                }
                else if (status != CDF_OK) {
                    cdf_print_error(status);
                    FUNC_GOTO_ERROR(H5E_VOL, H5E_READERROR, FAIL, 
                                    "failed to read data from CDF gEntry #%zu from gAttribute '%s'", 
                                    i, a->name); 
                }
                /* Stringify gEntry into the appropriate location in user buffer */
                if (stringify_gEntry(id, a->attr_num, cdf_entry_index, datatype, 
                                    num_elements, str_size, entry_buf) < 0) {
                    FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_READERROR, FAIL,
                                    "failed to stringify gEntry #%zu from gAttribute '%s'", 
                                    i, a->name);
                }
            }
        }
    } else if (a->scope == VARIABLE_SCOPE) { /* vAttribute case */
        long varNum;

        /* For vAttributes, the parent must be a dataset; use its var_num */
        parent_obj = (const cdf_object_t *)a->parent;
        if (!parent_obj || parent_obj->obj_type != H5I_DATASET) {
            FUNC_GOTO_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL,
                            "Invalid parent object for vAttribute '%s'", a->name);
        }
        parent_dset = &parent_obj->u.dataset;
        varNum = parent_dset->var_num;

        /* Check if string attribute */
        if (a->datatype == CDF_CHAR || a->datatype == CDF_UCHAR) {
            if (!H5Tequal(mem_type_id, a->type_id)) {
                FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_BADTYPE, FAIL,
                                "Datatype conversion not supported for CDF string attributes");
            }

            /* Verify HDF5 datatype size matches CDF string length + null terminator */
            size_t hdf5_size = H5Tget_size(a->type_id);
            if (hdf5_size != a->num_elements + 1) {
                FUNC_GOTO_ERROR(H5E_DATATYPE, H5E_BADTYPE, FAIL,
                                "CDF string attribute size mismatch for '%s'", a->name);
            }

            str_buf = (char *)malloc(a->num_elements); /* Temporary buffer for CDF string (can be big so avoid stack allocation) */
            if (!str_buf) {
                FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL,
                                "Failed to allocate temporary buffer for vAttribute string read");
            }
            memset(str_buf, 0, a->num_elements);
            
            /* Read string attribute */
            status = CDFgetAttrzEntry(id, a->attr_num, varNum, str_buf);
            if (status != CDF_OK) {
                cdf_print_error(status);
                FUNC_GOTO_ERROR(H5E_VOL, H5E_READERROR, FAIL, 
                                "failed to read string data from CDF vAttribute '%s'", 
                                a->name);
            }

            /* Copy to user buffer, ensuring null-termination */
            memcpy(buf, str_buf, a->num_elements);
            ((char *)buf)[a->num_elements] = '\0'; /* Null-terminate */
        } else { /* Non-string vAttribute */
            /* Get CDF datatype size for possible datatype conversion */
            if ((dataset_type_size = H5Tget_size(a->type_id)) == 0) {
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "failed to get dataset type size");
            }
            cdf_data_size = a->num_elements * dataset_type_size;

            /* Allocate buffer for CDF data */
            if((cdf_buf = malloc(cdf_data_size)) == NULL) {
                FUNC_GOTO_ERROR(H5E_RESOURCE, H5E_CANTALLOC, FAIL, "failed to allocate buffer for CDF data");
            }

            /* Read non-string attribute */
            status = CDFgetAttrzEntry(id, a->attr_num, varNum, cdf_buf);
            if (status != CDF_OK) {
                cdf_print_error(status);
                FUNC_GOTO_ERROR(H5E_VOL, H5E_READERROR, FAIL, 
                                "failed to read data from CDF vAttribute '%s'", 
                                a->name);
            }

            /* Prepare converted buffer if needed */
            if (prepare_converted_buffer(a->type_id, mem_type_id, (size_t)a->num_elements,
                    cdf_buf, cdf_data_size, &converted_buf, &converted_buf_size, 
                    &tconv_buf_allocated) < 0) {
                FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "failed to prepare converted buffer");
            }

            /* Free temporary CDF buffer if new buffer was allocated in prepare_converted_buffer() */
            if (tconv_buf_allocated && cdf_buf) {
                free(cdf_buf);
                cdf_buf = NULL;
            }

            /* Copy data to user buffer */
            memcpy(buf, converted_buf, converted_buf_size);
        }
    } else {
        FUNC_GOTO_ERROR(H5E_ATTR, H5E_NOTFOUND, FAIL, "Unknown attribute scope for attribute '%s'",
                        a->name);
    }

done:
    /* Free allocated buffers */
    if (cdf_buf) {
        free(cdf_buf);
        cdf_buf = NULL;
    }
    if (tconv_buf_allocated && converted_buf) {
        free(converted_buf);
        converted_buf = NULL;
    }
    if (str_buf) {
        free(str_buf);
        str_buf = NULL;
    }
    return ret_value;
} /* end cdf_attr_read() */

// cppcheck-suppress constParameterCallback
herr_t cdf_attr_get(void *obj, H5VL_attr_get_args_t *args,
                        hid_t __attribute__((unused)) dxpl_id, void __attribute__((unused)) * *req)
{
    const cdf_object_t *o = (const cdf_object_t *) obj;
    const cdf_attr_t *a = &o->u.attr;
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
} /* end cdf_attr_get() */

herr_t cdf_attr_close(void *attr, hid_t dxpl_id, void **req)
{
    cdf_object_t *o = (cdf_object_t *) attr;
    cdf_attr_t *a = &o->u.attr;
    cdf_object_t *parent_obj = NULL;
    herr_t ret_value = SUCCEED;

    assert(a);

    /* Decrement attribute's ref count */
    if (o->ref_count == 0) { 
        FUNC_DONE_ERROR(H5E_ATTR, H5E_CANTCLOSEOBJ, FAIL,
                        "Attribute already closed (ref_count is 0)");
    }

    o->ref_count--;

    /* Only do the real close when ref_count reaches 0 */
    if (o->ref_count == 0) {
        /* Free attribute name if it exists */
        if (a->name) {
            free(a->name);
            a->name = NULL;
        }
        /* Free gEntry indices array if it exists */
        if (a->gEntry_indices) {
            free(a->gEntry_indices);
            a->gEntry_indices = NULL;
        }

        /* Close dataspace and datatype if they exist. */
        if (a->space_id != H5I_INVALID_HID && H5Sclose(a->space_id) < 0) {
                FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                "Failed to close attribute dataspace");

        }
        /* Attempt to close type_id. */
        if (a->type_id != H5I_INVALID_HID && H5Tclose(a->type_id) < 0) {
            FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                            "Failed to close attribute datatype");
        }

        /* Close parent object (dataset, group, or file) */
        parent_obj = (cdf_object_t *) a->parent;
        if (parent_obj) {
            switch (parent_obj->obj_type) {
                case H5I_FILE:
                    if (cdf_file_close(parent_obj, dxpl_id, req) < 0) {
                        FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                        "Failed to close attribute's parent file");
                    }
                    break;
                case H5I_DATASET:
                    if (cdf_dataset_close(parent_obj, dxpl_id, req) < 0) {
                        FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                                        "Failed to close attribute's parent dataset");
                    }
                    break;
                default:
                    FUNC_DONE_ERROR(H5E_ATTR, H5E_BADVALUE, FAIL, "Invalid parent object type");
            }
        }

        /* Also decrement the file reference count */
        if (cdf_file_close(o->parent_file, dxpl_id, req) < 0) {
            FUNC_DONE_ERROR(H5E_ATTR, H5E_CLOSEERROR, FAIL,
                            "Failed to close attribute file object");
        }

        free(o);
    }

    return ret_value;
} /* end cdf_attr_close() */



/* These two functions are necessary to load this plugin using
 * the HDF5 library. */
H5PL_type_t H5PLget_plugin_type(void)
{
    return H5PL_TYPE_VOL;
}
const void *H5PLget_plugin_info(void)
{
    return &cdf_class_g;
}

/*---------------------------------------------------------------------------
 * Function:    cdf_introspect_opt_query
 *
 * Purpose:     Query if an optional operation is supported by this connector
 *
 * Returns:     SUCCEED (Can't fail)
 *
 *---------------------------------------------------------------------------
 */
herr_t cdf_introspect_opt_query(void __attribute__((unused)) * obj, H5VL_subclass_t subcls,
                                    int opt_type, uint64_t __attribute__((unused)) * flags)
{
    /* We don't support any optional operations */
    (void) subcls;
    (void) opt_type;
    *flags = 0;
    return SUCCEED;
} /* end cdf_introspect_opt_query() */

herr_t cdf_introspect_get_conn_cls(void __attribute__((unused)) * obj,
                                       H5VL_get_conn_lvl_t __attribute__((unused)) lvl,
                                       const H5VL_class_t __attribute__((unused)) * *conn_cls)
{
    herr_t ret_value = SUCCEED;

    assert(conn_cls);

    /* Retrieve the VOL connector class */
    *conn_cls = &cdf_class_g;

    return ret_value;
} /* end cdf_introspect_get_conn_cls() */

herr_t cdf_init_connector(hid_t __attribute__((unused)) vipl_id)
{
    herr_t ret_value = SUCCEED;

    /* Register the connector with HDF5's error reporting API */
    if ((H5_cdf_err_class_g =
             H5Eregister_class(HDF5_VOL_CDF_ERR_CLS_NAME, HDF5_VOL_CDF_LIB_NAME,
                               HDF5_VOL_CDF_LIB_VER)) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "can't register with HDF5 error API");

    /* Create a separate error stack for the CDF VOL to report errors with */
    if ((H5_cdf_err_stack_g = H5Ecreate_stack()) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL, "can't create error stack");

    /* Set up a few CDF VOL-specific error API message classes */
    if ((H5_cdf_obj_err_maj_g =
             H5Ecreate_msg(H5_cdf_err_class_g, H5E_MAJOR, "Object interface")) < 0)
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTINIT, FAIL,
                        "can't create error message for object interface");

    /* Initialized */
    H5_cdf_initialized_g = TRUE;

done:
    if (ret_value < 0)
        cdf_term_connector();

    return ret_value;
} /* end cdf_init_connector() */

herr_t cdf_term_connector(void)
{
    herr_t ret_value = SUCCEED;

    /* Unregister from the HDF5 error API */
    if (H5_cdf_err_class_g >= 0) {
        if (H5_cdf_obj_err_maj_g >= 0 && H5Eclose_msg(H5_cdf_obj_err_maj_g) < 0)
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL,
                            "can't unregister error message for object interface");
        if (H5Eunregister_class(H5_cdf_err_class_g) < 0)
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "can't unregister from HDF5 error API");

        /* Print the current error stack before destroying it */
        PRINT_ERROR_STACK;

        /* Destroy the error stack */
        if (H5Eclose_stack(H5_cdf_err_stack_g) < 0) {
            FUNC_DONE_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "can't close error stack");
            PRINT_ERROR_STACK;
        }

        H5_cdf_err_stack_g = H5I_INVALID_HID;
        H5_cdf_err_class_g = H5I_INVALID_HID;
        H5_cdf_obj_err_maj_g = H5I_INVALID_HID;
    }

    return ret_value;
} /* end cdf_term_connector() */


/* cppcheck-suppress constParameterCallback */
herr_t cdf_link_specific(void *obj, const H5VL_loc_params_t *loc_params,
                         H5VL_link_specific_args_t *args, hid_t __attribute__((unused)) dxpl_id,
                         void __attribute__((unused)) * *req)
{
    herr_t ret_value = SUCCEED;
    cdf_object_t *o = (cdf_object_t *)obj;
    cdf_object_t *file_obj = NULL;
    CDFstatus status;
    long num_zVars = 0;
    char var_name[CDF_VAR_NAME_LEN256 + 1];

    /* object could be file, group, or dataset - we need the file */
    /* For simplicity, try to extract file pointer based on common structure pattern */
    if (!o || !loc_params || !args) {
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "Invalid arguments to link_specific");
    }

    file_obj = o->parent_file;
    if (!file_obj || file_obj->obj_type != H5I_FILE) {
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, FAIL, "Invalid file object in link_specific");
    }

    /* Get number of zVars in the CDF file */
    status = CDFgetNumzVars(file_obj->u.file.id, &num_zVars);
    if (status != CDF_OK) {
        cdf_print_error(status);
        FUNC_GOTO_ERROR(H5E_LINK, H5E_CANTGET, FAIL,
                        "Failed to get number of zVariables in CDF file");
    }

    switch (args->op_type) {
        case H5VL_LINK_EXISTS: {
            const char *link_name = NULL;
            /* Get the link name from loc_params */
            if (loc_params->type == H5VL_OBJECT_BY_NAME) {
                link_name = loc_params->loc_data.loc_by_name.name;
            } else {
                FUNC_GOTO_ERROR(H5E_LINK, H5E_BADVALUE, FAIL,
                                "Link exists check requires name-based location");
            }

            if (!link_name) {
                FUNC_GOTO_ERROR(H5E_LINK, H5E_BADVALUE, FAIL, "No link name provided");
            }

            /* Resolve name with leading root group '/' */
            const char *resolved_name = NULL;
            if (resolve_root_name(link_name, &resolved_name) < 0) {
                *args->args.exists.exists = 0; /* Default to not found */
                break;
            }

            /* Loop through all zVariables to see if link_name matches any zVariable name */
            *args->args.exists.exists = 0; /* Default to not found */
            for (long i = 0; i < num_zVars; i++) {
                /* Get the name of the zVariable */
                status = CDFgetzVarName(file_obj->u.file.id, i, var_name);
                if (status != CDF_OK) {
                    cdf_print_error(status);
                    FUNC_GOTO_ERROR(H5E_LINK, H5E_CANTGET, FAIL,
                                    "Failed to get zVariable name for variable index %ld", i);
                }

                if (strcmp(var_name, resolved_name) == 0) {
                    *args->args.exists.exists = 1;
                    break;
                }
            }
            break;
        }

        case H5VL_LINK_ITER: {
            H5VL_link_iterate_args_t *iter_args = &args->args.iterate;

            assert(iter_args);
            assert(iter_args->idx_p);

            if (*iter_args->idx_p >= (hsize_t) num_zVars) {
                /* Iteration index is beyond number of zVariables - mark iteration complete */
                break;
            }

            if (!iter_args->op) {
                /* If no callback is provided, set iteration index to number of zVars to mark iteration complete */
                *iter_args->idx_p = (hsize_t) num_zVars;
                break;
            }

            /* Loop through zVariables starting from the provided index */
            for (long i = (long)(*iter_args->idx_p); i < num_zVars; i++) {
                H5L_info2_t link_info;
                herr_t cb_ret;

                status = CDFgetzVarName(file_obj->u.file.id, i, var_name);
                if (status != CDF_OK) {
                    cdf_print_error(status);
                    FUNC_GOTO_ERROR(H5E_LINK, H5E_CANTGET, FAIL,
                                    "Failed to get zVariable name for variable index %ld", i);
                }

                /* Fill in minimal link info */
                memset(&link_info, 0, sizeof(H5L_info2_t));
                /* Consider all CDF "links" to be hard links */
                link_info.type = H5L_TYPE_HARD;
                link_info.corder_valid = true;
                link_info.corder = i; /* Use zVariable index as creation order */
                link_info.cset = H5T_CSET_ASCII;

                /* Call user's callback. Use 0 as group hid_t since we don't have a proper group ID */
                cb_ret = iter_args->op(0, var_name, &link_info, iter_args->op_data);

                if (iter_args->idx_p) {
                    *iter_args->idx_p = (hsize_t)(i + 1);
                }
                
                if (cb_ret < 0) {
                    FUNC_GOTO_ERROR(H5E_LINK, H5E_BADITER, FAIL,
                                    "Iterator callback returned error");
                }
                else if (cb_ret > 0) {
                    ret_value = cb_ret;
                    goto done;
                }
            }

            break;
        }

        case H5VL_LINK_DELETE:
            FUNC_GOTO_ERROR(H5E_LINK, H5E_UNSUPPORTED, FAIL,
                            "Link deletion is not supported in read-only GeoTIFF VOL connector");
            break;

        default:
            FUNC_GOTO_ERROR(H5E_LINK, H5E_UNSUPPORTED, FAIL, "Unsupported link specific operation");
    }

done:
    return ret_value;
} /* end cdf_link_specific() */