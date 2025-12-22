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
        NULL,                 /* open         */
        NULL,                 /* read         */
        NULL,                 /* write        */
        NULL,                 /* get          */
        NULL,                 /* specific     */
        NULL,                 /* optional     */
        NULL                  /* close        */
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
        free(o);
    }

done:
    return ret_value;
}

/* These two functions are necessary to load this plugin using
 * the HDF5 library. */
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
