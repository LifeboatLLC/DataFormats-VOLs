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
 */

/* This connector's header */
#include "cdf_vol_connector.h"

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

static hbool_t H5_cdf_initialized_g = FALSE;

/* Identifiers for HDF5's error API */
hid_t H5_cdf_err_stack_g = H5I_INVALID_HID;
hid_t H5_cdf_err_class_g = H5I_INVALID_HID;
hid_t H5_cdf_obj_err_maj_g = H5I_INVALID_HID;

/* Helper functions */
static herr_t cdf_get_hdf5_type_from_cdf(long cdf_datatype, hid_t *type_id);

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
        cdf_dataset_open,     /* open         */
        cdf_dataset_read,     /* read         */
        NULL,                 /* write        */
        NULL,                 /* get          */
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

herr_t cdf_get_hdf5_type_from_cdf(long cdf_datatype, hid_t *type_id)
{
    herr_t ret_value = SUCCEED;
    hid_t new_type = H5I_INVALID_HID;
    hid_t predef_type;
    assert(type_id);
    switch (cdf_datatype) {
        /* Character types */
        case CDF_CHAR: /* 1-byte, signed character */
            predef_type = H5T_NATIVE_SCHAR;   /* 8-bit character */
            break;

        case CDF_UCHAR: /* 1-byte, unsigned character */
            predef_type = H5T_NATIVE_UCHAR;  /* 8-bit unsigned character */
            break;
        
        /* 8-bit types */
        case CDF_BYTE: /* 1-byte, signed integer */
        case CDF_INT1: /* 1-byte, signed integer */
            predef_type = H5T_NATIVE_INT8;
            break;
        
        case CDF_UNIT1: /* 1-byte, unsigned integer */
            predef_type = H5T_NATIVE_UINT8;
            break;
        
        /* 16-bit types */
        case CDF_INT2: /* 2-byte, signed integer */
            predef_type = H5T_NATIVE_INT16;
            break;

        case CDF_UNIT2: /* 2-byte, unsigned integer */
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
            predef_type = H5T_NATIVE_INT64;  /* Explicitly 64-bit signed */
            break;
            
        /* Floating point */
        case CDF_REAL4: /* 4-byte, floating point */
        case CDF_FLOAT: /* 4-byte, floating point */
            predef_type = H5T_NATIVE_FLOAT;  /* 32-bit float */
            break;
            
        case CDF_REAL8: /* 8-byte, floating point */
        case CDF_DOUBLE: /* 8-byte, floating point */
        case CDF_EPOCH: /* 8-byte, floating point */
            predef_type = H5T_NATIVE_DOUBLE; /* 64-bit double */
            break;
        
        /* High-precision timestamp */
        case CDF_EPOCH16:
        {
            hsize_t dims[1] = {2};  /* 1D array of 2 doubles */
            predef_type = H5Tarray_create(H5T_NATIVE_DOUBLE, 1, dims);
            break;
        }

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
}

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
        FUNC_GOTO_ERROR(H5E_FILE, H5E_CANTOPENFILE, NULL, "Failed to open CDF file: %s", name);
    }
    /* Save CDF ID */
    file->id = id;

    if ((file->filename = strdup(name)) == NULL) {
        FUNC_GOTO_ERROR(H5E_FILE, H5E_CANTALLOC, NULL, "Failed to duplicate filename string");
    }

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

/* Dataset operations */
void *cdf_dataset_open(void *obj, const H5VL_loc_params_t __attribute__((unused)) * loc_params,
                       const char *name, hid_t __attribute__((unused)) dapl_id,
                       hid_t __attribute__((unused)) dxpl_id,
                       void __attribute__((unused)) * *req)
{
    CDFstatus status;
    CDFdataType data_type;

    cdf_object_t *file_obj = (cdf_object_t *) obj;
    cdf_object_t *dset_obj = NULL;
    cdf_object_t *ret_value = NULL;

    cdf_dataset_t *dset = NULL;           /* Convenience pointer */
    cdf_file_t *file = &file_obj->u.file; /* Convenience pointer */

    H5T_class_t dtype_class = H5T_NO_CLASS; 

    if (!file_obj || !name) {
        FUNC_GOTO_ERROR(H5E_ARGS, H5E_BADVALUE, NULL, "Invalid file or dataset name");
    }

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

    if ((dset->name = strdup(name)) == NULL) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTALLOC, NULL, "Failed to duplicate dataset name string");
    }

    /* Initialize other fields */
    dset->var_num = -1;
    dset->num_records = 0;
    dset->num_dims = 0;
    memset(dset->dim_sizes, 0, sizeof(dset->dim_sizes));
    dset->type_id = H5I_INVALID_HID;
    dset->space_id = H5I_INVALID_HID;


    /* Parse dataset name to extract variable number */
    dset->var_num = CDFgetVarNum (file->id, name);
    if (dset->var_num < CDF_OK) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL,
                        "Failed to get variable number from name: %s", name);
    }

    /* Get variable info. Passing NULL to 'name' since we already have it. */
    status = CDFinquirezVar(file->id, dset->var_num, NULL, &data_type,
                            &dset->num_elements, &dset->num_dims, dset->dim_sizes, 
                            &dset->rec_vary, dset->dim_varys);
    if (status != CDF_OK) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL,
                        "Failed to get variable info for var num %ld", dset->var_num);
    }

    /* Map CDF data type to HDF5 datatype */
    if (cdf_get_hdf5_type_from_cdf(data_type, &dset->type_id) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL,
                        "Failed to map CDF data type %ld to HDF5 datatype",
                        data_type);
    }

    if ((dtype_class = H5Tget_class(dset->type_id)) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL, "Failed to get datatype class");
    }

    /* Get the maximum number of records written to this variable*/
    status = CDFgetNumRecords(file->id, dset->var_num, &dset->num_records);
    if (status < CDF_OK) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, NULL,
                        "Failed to get number of records in CDF file");
    }

    /* Set up dataspace dimensions */
    hsize_t dims[dset->num_dims];
    
    /* First dimension is record dimension */
    dims[0] = (hsize_t)(dset->num_records); 
    
    /* Set remaining dimensions */
    for (int i = 1; i < dset->num_dims; i++) {
        dims[i] = (hsize_t)(dset->dim_sizes[i]);
    }

    int rank = dset->num_dims;

    /* Create dataspace */
    if ((dset->space_id = H5Screate_simple(rank, dims, NULL)) < 0) {
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
    long indices[CDF_MAX_DIMS] = {0}; /* Start at beginning of each dimension */
    long counts[CDF_MAX_DIMS] = {1}; /* Read all records along record dimension */
    long intervals[CDF_MAX_DIMS] = {1}; /* No striding */

    status = CDFhyperGetVarData(id,
                                d->var_num,
                                0L, /* start reading from first record */
                                d->num_records, /* number of records to read (ALL) */
                                1L, /* stride */
                                indices,
                                counts,
                                intervals,
                                cdf_buf);
    if (status != CDF_OK) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_READERROR, FAIL, "failed to read data from CDF variable");
    }

    /* Prepare converted buffer if needed */
    hbool_t tconv_buf_allocated = FALSE;
    if (prepare_converted_buffer(d->type_id, mem_type_id[0], prepare_num_elements,
            cdf_buf, cdf_data_size, &selected_buf, &selected_size, 
            &tconv_buf_allocated) < 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTGET, FAIL, "failed to prepare converted buffer");
    }

    /* Free CDF buffer */
    free(cdf_buf);
    cdf_buf = NULL;

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

herr_t cdf_dataset_close(void *dset, hid_t dxpl_id, void **req)
{
    cdf_object_t *o = (cdf_object_t *) dset;
    herr_t ret_value = SUCCEED;

    assert(o);

    /* Use FUNC_DONE_ERROR to try to complete resource release after failure */
    if (!d->parent_file){
        FUNC_DONE_ERROR(H5E_VOL, H5E_BADVALUE, FAIL,
                        "Dataset has no valid parent file reference");
    }

    /* Decrement dataset's ref count */
    if (o->ref_count == 0) {
        FUNC_GOTO_ERROR(H5E_VOL, H5E_CANTCLOSEOBJ, FAIL,
                        "Dataset already closed (ref_count is 0)");
    }
    o->ref_count--;

    /* Only do the real close when ref_count reaches 0 */
    if (o->ref_count == 0) {
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
        if (cdf_file_close(o->parent_file, dxpl_id, req) < 0) {
            FUNC_GOTO_ERROR(H5E_VOL, H5E_CLOSEERROR, FAIL, "Failed to close dataset file object");
        }

        free(o);
    }

    return ret_value;
} /* end cdf_dataset_close() */

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
