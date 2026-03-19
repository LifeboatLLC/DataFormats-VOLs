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
 * Purpose:     This file defines tests which evluate basic BUFR file
 *              functionality through the BUFR VOL connector.
 */

#include "bufr_vol_connector.h"
#include "test_runner.h"
#include <H5PLpublic.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Verify that BUFR file open/close operations work properly */
int OpenBUFRTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;

    printf("Testing BUFR VOL connector open/close with file");

    /* Add the plugin path so HDF5 can find the connector */
#ifdef BUFR_VOL_PLUGIN_PATH
    if (H5PLappend(BUFR_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the BUFR VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(BUFR_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    /* Create file access property list */
    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    /* Set the VOL connector */
    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    /* Open the BUFR file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open BUFR file\n");
        goto error;
    }

    /* Close the BUFR file */
    if (H5Fclose(file_id) < 0) {
        printf("Failed to close BUFR file\n");
        goto error;
    }

    /* Clean up*/
    if (H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }

    /* Unregister VOL connector */
    if (H5VLunregister_connector(vol_id) < 0) {
        printf("Failed to unregister VOL connector\n");
        goto error;
    }

    printf("PASSED\n");
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Pclose(fapl_id);
        H5Fclose(file_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;
    return -1;
}
/* Verify that BUFR dataset open/close operations work properly */
int OpenBUFRDatasetTest(const char *filename, const char *dsetname)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t group_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t dapl_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;
    hid_t attr_id = H5I_INVALID_HID;
    hsize_t dims[1];
    int ndims;
    long *data = NULL;
    long table_num = 0;

    printf("Testing BUFR VOL connector open/close with dataset");

    /* Add the plugin path so HDF5 can find the connector */
#ifdef BUFR_VOL_PLUGIN_PATH
    if (H5PLappend(BUFR_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the BUFR VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(BUFR_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    /* Create file access property list */
    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    /* Create dataset access property list */
    if ((dapl_id = H5Pcreate(H5P_DATASET_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    /* Set the VOL connector */
    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    /* Open the BUFR file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open BUFR file\n");
        goto error;
    }
    /* Open first group; index is 0-based */
    if ((group_id = H5Gopen2(file_id, "/message_0", H5P_DEFAULT)) < 0) {
        printf("Failed to open a group \n");
        goto error;
    }
    /* Open the BUFR dataset */
    if ((dset_id = H5Dopen(group_id, dsetname, dapl_id)) < 0) {
        printf("Failed to open BUFR dataset\n");
        goto error;
    }
    /* Get dataspace */
    if ((space_id = H5Dget_space(dset_id)) < 0) {
        printf("Failed to get dataspace\n");
        goto error;
    }

    if ((ndims = H5Sget_simple_extent_ndims(space_id)) < 0) {
        printf("Failed to get number of dimensions\n");
        goto error;
    }

    if (ndims >= 0) {
        if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0) {
            printf("Failed to get dimensions\n");
            goto error;
        }
    }

    /* Get datatype */
    if ((type_id = H5Dget_type(dset_id)) < 0) {
        printf("Failed to get datatype\n");
        goto error;
    }

    data = (long *) malloc(dims[0] * sizeof(long));
    if (!data) {
        printf("Failed to allocate read buffer\n");
        goto error;
    }

    if (H5Dread(dset_id, H5T_NATIVE_LONG, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) < 0) {
        printf("Failed to read dataset\n");
        goto error;
    } else {
        if (dims[0] != 24) {
            printf("VERIFICATION FAILED: Expected dimensions 24, got %llu \n", dims[0]);
            printf("\n");
            goto error;
        }
        printf("\n");
        for (int i = 0; i < dims[0]; i++) {
            printf("%ld \n", data[i]);
        }
    }

    /* Get attribute */
    if ((attr_id = H5Aopen(group_id, "masterTablesVersionNumber", H5P_DEFAULT)) < 0) {
        printf("Failed to open  masterTablesVersionNumber attribute\n");
        goto error;
    }

    if (H5Aread(attr_id, H5T_NATIVE_LONG, &table_num) < 0) {
        printf("Failed to read attribute\n");
        goto error;
    } else {
        if (table_num != 13) {
            printf("VERIFICATION FAILED: Expected masterTablesVersionNumber attribute value is 13 "
                   "but got %ld \n",
                   table_num);
            goto error;
        }
    }

    /* Close the BUFR dataset */
    free(data);

    if (H5Aclose(attr_id) < 0) {
        printf("Failed to close attr\n");
        goto error;
    }
    attr_id = H5I_INVALID_HID;

    if (H5Tclose(type_id) < 0) {
        printf("Failed to close datatype\n");
        goto error;
    }
    type_id = H5I_INVALID_HID;

    if (H5Sclose(space_id) < 0) {
        printf("Failed to close dataspace\n");
        goto error;
    }
    space_id = H5I_INVALID_HID;

    if (H5Dclose(dset_id) < 0) {
        printf("Failed to close dataset\n");
        goto error;
    }
    dset_id = H5I_INVALID_HID;
    if (H5Gclose(group_id) < 0) {
        printf("Failed to close group\n");
        goto error;
    }
    group_id = H5I_INVALID_HID;

    if (H5Fclose(file_id) < 0) {
        printf("Failed to close file\n");
        goto error;
    }
    file_id = H5I_INVALID_HID;
    if (H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }
    fapl_id = H5I_INVALID_HID;

    /* Unregister VOL connector */
    if (H5VLunregister_connector(vol_id) < 0) {
        printf("Failed to unregister VOL connector\n");
        goto error;
    }

    printf("PASSED\n");
    return 0;

error:
    if (data)
        free(data);
    H5E_BEGIN_TRY
    {
        H5Pclose(fapl_id);
        H5Pclose(dapl_id);
        H5Aclose(attr_id);
        H5Dclose(dset_id);
        H5Gclose(group_id);
        H5Fclose(file_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;
    return -1;
}
