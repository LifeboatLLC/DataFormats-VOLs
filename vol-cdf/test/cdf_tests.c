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
 * Purpose:     This file defines tests which evluate basic CDF file
 *              functionality through the CDF VOL connector.
 */

#include "cdf_vol_connector.h"
#include "test_runner.h"
#include <H5PLpublic.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Verify that CDF file open/close operations work properly */
int OpenCDFTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;

    printf("Testing CDF VOL connector open/close with file: %s\n", filename);

    /* Add the plugin path so HDF5 can find the connector */
#ifdef CDF_VOL_PLUGIN_PATH
    if (H5PLappend(CDF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the CDF VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(CDF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
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

    /* Open the CDF file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open CDF file\n");
        goto error;
    }

    /* Close the CDF file */
    if (H5Fclose(file_id) < 0) {
        printf("Failed to close CDF file\n");
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


int ReadCDFTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;
    hsize_t dims[3];
    int ndims;

    printf("Testing CDF VOL connector by reading 'Image' variable from file: %s\n", filename);

    /* Add the plugin path so HDF5 can find the connector */
#ifdef CDF_VOL_PLUGIN_PATH
    if (H5PLappend(CDF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the CDF VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(CDF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
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

    /* Open the CDF file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open CDF file\n");
        goto error;
    }

    /* Open a dataset */
    if ((dset_id = H5Dopen2(file_id, "Image", H5P_DEFAULT)) < 0) {
        printf("Failed to open dataset\n");
        goto error;
    }
    
    /* Get the dataspace */
    if ((space_id = H5Dget_space(dset_id)) < 0) {
        printf("Failed to get dataspace\n");
        goto error;
    }

    /* Get the number of dimensions and their sizes */
    if ((ndims = H5Sget_simple_extent_dims(space_id, dims, NULL)) < 0) {
        printf("Failed to get dataspace dimensions\n");
        goto error;
    }

    /* Verify dimensions - This information was found by running `cdfdump example1.cdf` */
    if (ndims != 3 || dims[0] != 3 || dims[1] != 10 || dims[2] != 20) {
        printf("Dataspace dimensions mismatch\n");
        printf("  Expected: 3:[3, 10, 20], Got: %d[%llu, %llu, %llu]\n", 
               ndims, (unsigned long long)dims[0], (unsigned long long)dims[1], (unsigned long long)dims[2]);
        goto error;
    }

    /* Get datatype */
    if ((type_id = H5Dget_type(dset_id)) < 0) {
        printf("Failed to get datatype\n");
        goto error;
    }

    /* Verify Datatype */
    if (H5Tequal(type_id, H5T_NATIVE_INT) < 1) {
        printf("Datatype mismatch - Expected H5T_NATIVE_INT\n");
        goto error;
    }

    /* Calculate buffer size */
    size_t buffer_size = dims[0] * dims[1] * dims[2] * H5Tget_size(type_id);
    printf("Allocating read buffer of size: %zu bytes\n", buffer_size);

    /* Allocate buffer */
    int *data_buffer = (int *)malloc(buffer_size);
    if (!data_buffer) {
        printf("Failed to allocate read buffer\n");
        goto error;
    }

    /* Read data */
    if (H5Dread(dset_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, data_buffer) < 0) {
        printf("Failed to read dataset\n");
        goto error;
    }

    /* Verify data - This information was found by running `cdfdump example1.cdf` */
    int data_valid = 1;
    for(int record = 0; record < dims[0]; record++) {
        for(int i = 0; i < dims[1]; i++) {
            for(int j = 0; j < dims[2]; j++) {
                int expected_value = record * dims[1] * dims[2] + i * dims[2] + j;
                int actual_value = data_buffer[record * dims[1] * dims[2] + i * dims[2] + j];
                if (actual_value != expected_value) {
                    printf("VERIFICATION FAILED: Data mismatch at [%d, %d, %d]: expected %d, got %d\n",
                           record, i, j, expected_value, actual_value);
                    goto error;
                }
            }
        }
            
    }

    printf("PASSED\n");

    /* Clean up*/
    free(data_buffer);
    if (H5Tclose(type_id) < 0) {
        printf("Failed to close datatype\n");
        goto error;
    }
    if (H5Sclose(space_id) < 0) {
        printf("Failed to close dataspace\n");
        goto error;
    }
    if (H5Dclose(dset_id) < 0) {
        printf("Failed to close dataset\n");
        goto error;
    }
    if (H5Fclose(file_id) < 0) {
        printf("Failed to close file\n");
        goto error;
    }
    if( H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }

    type_id = H5I_INVALID_HID;
    space_id = H5I_INVALID_HID;
    dset_id = H5I_INVALID_HID;
    file_id = H5I_INVALID_HID;
    fapl_id = H5I_INVALID_HID;

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
        H5Tclose(type_id);
        H5Sclose(space_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID) {
            H5VLunregister_connector(vol_id);
        }
    }
    H5E_END_TRY;

    printf("FAILED\n");
    return 1;
}