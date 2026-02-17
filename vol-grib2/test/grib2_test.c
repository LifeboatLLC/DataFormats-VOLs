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
 * Purpose:     This file defines tests which evluate basic GRIB2 file
 *              functionality through the GRIB2 VOL connector.
 */

#include "grib2_vol_connector.h"
#include "test_runner.h"
#include <H5PLpublic.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Verify that GRIB2 file open/close operations work properly */
int OpenGRIB2BasicTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;

    printf("Testing GRIB2 VOL connector open/close with file");

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GRIB2_VOL_PLUGIN_PATH
    if (H5PLappend(GRIB2_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the GRIB2 VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GRIB2_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
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

    /* Open the GRIB2 file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open GRIB2 file\n");
        goto error;
    }

    /* Close the GRIB2 file */
    if (H5Fclose(file_id) < 0) {
        printf("Failed to close GRIB2 file\n");
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
/* Verify that GRIB2 file, group, dataset, attribute open/read/close operations work properly */
int OpenGRIB2Test(const char *filename, const char *dsetname)
{
    hid_t vol_id   = H5I_INVALID_HID;
    hid_t fapl_id  = H5I_INVALID_HID;
    hid_t file_id  = H5I_INVALID_HID;
    hid_t group_id = H5I_INVALID_HID;
    hid_t dset_id  = H5I_INVALID_HID;
    hid_t dapl_id  = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    hid_t type_id  = H5I_INVALID_HID;
    hid_t attr_id  = H5I_INVALID_HID;
    hsize_t dims[1];
    int ndims;
    double *data = NULL;
    long shape = -1;
    htri_t exists;

    printf("Testing GRIB2 VOL connector open/read/close with grouop, dataset and attribute");

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GRIB2_VOL_PLUGIN_PATH
    if (H5PLappend(GRIB2_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the GRIB2 VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GRIB2_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
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

    /* Open the GRIB2 file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open GRIB2 file\n");
        goto error;
    }

    /* Open the GRIB2 dataset */
    if ((dset_id = H5Dopen(file_id, dsetname, dapl_id)) < 0) {
        printf("Failed to open GRIB2 dataset\n");
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

    data = (double *) malloc(dims[0]);
    if (!data) {
        printf("Failed to allocate read buffer\n");
        goto error;
    }

    if (H5Dread(dset_id, H5T_NATIVE_DOUBLE, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) < 0) {
        printf("Failed to read dataset\n");
        goto error;
    } else {
        if (dims[0] != 12) {
            printf("VERIFICATION FAILED: Expected dimensions 12, got %llu \n", dims[0]);
            printf("\n");
            goto error;
        }
        printf("\n");
        for (int i = 0; i < dims[0]; i++) {
             printf("%f \n", data[i]);
        }
    }
#ifdef TODO /* we should test attribute open with file_id and root_group_id when implemented */   
    /* Get attribute */
    if ((attr_id = H5Aopen(file_id, "/message_1/masterTablesVersionNumber", H5P_DEFAULT)) < 0) {
        printf("Failed to open  masterTablesVersionNumber attribute\n");
        goto error; 
    }
#endif

    /* Check is the first group exists */
    if ((exists = H5Lexists(file_id, "/message_1", H5P_DEFAULT)) < 0) {
        printf("Failed to check link existence for '/message_1'\n");
        goto error;
    }
  
     /* Open first group */
    if ((group_id = H5Gopen2(file_id, "/message_1", H5P_DEFAULT)) < 0) {
        printf("Failed to open a group \n");
        goto error;
    } 
    /* Check is the dataset 'values' exists */
    if ((exists = H5Lexists(group_id, "values", H5P_DEFAULT)) < 0) {
        printf("Failed to check link existence for '/message_1'\n");
        goto error;
    }
  
    /* Get attribute */
    if ((attr_id = H5Aopen(group_id, "shapeOfTheEarth", H5P_DEFAULT)) < 0) {
        printf("Failed to open  shapeOfTheEarth attribute\n");
        goto error; 
    }

    if (H5Aread(attr_id, H5T_NATIVE_LONG, &shape) < 0) {
        printf("Failed to read attribute\n");
        goto error;
    } else {
        if (shape != 6) {
            printf("VERIFICATION FAILED: Expected  shapeOfTheEarth attribute value is 13 but got %ld \n", shape);
            goto error;
        }
    }
    
    /* Close the GRIB2 attribute and dataset */
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

/* Verify that link existence check works properly */
int LinkExistsTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    htri_t exists;

    printf("Testing GRIB2 VOL connector link exists with file: %s  ", filename);

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GRIB2_VOL_PLUGIN_PATH
    if (H5PLappend(GRIB2_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the GRIB2 VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GRIB2_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
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

    /* Open the GRIB2 file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open GRIB2 file\n");
        goto error;
    }

    /* Check that "/message_1" exists */
    if ((exists = H5Lexists(file_id, "/message_1", H5P_DEFAULT)) < 0) {
        printf("Failed to check link existence for '/message_1'\n");
        goto error;
    }

    if (!exists) {
        printf("VERIFICATION FAILED: Link '/message_5' should exist but doesn't\n");
        goto error;
    }

    /* Check that a non-existent link doesn't exist */
    if ((exists = H5Lexists(file_id, "nonexistent", H5P_DEFAULT)) < 0) {
        printf("Failed to check link existence for 'nonexistent'\n");
        goto error;
    }
        printf(" exists value for nonexistent is %d\n", exists);

    if (exists) {
        printf("VERIFICATION FAILED: Link 'nonexistent' should not exist but does\n");
        goto error;
    }

    /* Clean up */
    if (H5Fclose(file_id) < 0) {
        printf("Failed to close file\n");
        goto error;
    }

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
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;

    printf("FAILED\n");
    return 1;
}

/* Verify that H5Lexists works for all messages in GRIB2 file */
int MultiLinkExistsTest(const char *filename)
{
    const uint32_t NUM_MESSAGES = 6;
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    htri_t exists;

    printf("Testing H5Lexists for /message_1 ... /message_6  ");


    /* Register the GRIB2 VOL connector */
#ifdef GRIB2_VOL_PLUGIN_PATH
    if (H5PLappend(GRIB2_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    if ((vol_id = H5VLregister_connector_by_name(GRIB2_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open GRIB2 file\n");
        goto error;
    }

    /* Check that all 6 messages exist */
    /* TODO: We should also allow to omit leading / */
    for (uint32_t i = 0; i < NUM_MESSAGES; i++) {
        char link_name[32];
        snprintf(link_name, sizeof(link_name), "/message_%u", i+1);

        if ((exists = H5Lexists(file_id, link_name, H5P_DEFAULT)) < 0) {
            printf("Failed to check link existence for '%s'\n", link_name);
            goto error;
        }

        if (!exists) {
            printf("VERIFICATION FAILED: Link '%s' should exist but doesn't\n", link_name);
            goto error;
        }
    }

    /* Verify /message_7 (one past the last) does not exist */
    if ((exists = H5Lexists(file_id, "/message_7", H5P_DEFAULT)) < 0) {
        printf("Failed to check link existence for '/message_7'\n");
        goto error;
    }

    if (exists) {
        printf("VERIFICATION FAILED: Link '/message_7' should not exist but does\n");
        goto error;
    }

    /* Verify a link name does not exist */
    /* TODO: add test for "foobar" when convention is implemented */
    if ((exists = H5Lexists(file_id, "/foobar", H5P_DEFAULT)) < 0) {
       printf("Failed to check link existence for 'foobar'\n");
      goto error;
    }

    if (exists) {
        printf("VERIFICATION FAILED: Link '/foobar' should not exist but does\n");
        goto error;
    }

    /* Clean up */
    if (H5Fclose(file_id) < 0) {
        printf("Failed to close file\n");
        goto error;
    }

    if (H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }

    if (H5VLunregister_connector(vol_id) < 0) {
        printf("Failed to unregister VOL connector\n");
        goto error;
    }

    printf("PASSED\n");
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;

    printf("FAILED\n");
    return 1;
}

/* Callback for link iteration */
static herr_t link_iterate_callback(hid_t group, const char *name, const H5L_info2_t *info,
                                    void *op_data)
{
    int *count = (int *) op_data;

    (void) group; /* Unused */
    (void) info;  /* Unused */

    int n = 0;
    if (sscanf(name, "message_%d", &n) == 1 &&
        n >= 1 && n <= 6)
    {
        printf("Found group (message_%d)\n", n);
    } else {
        printf("VERIFICATION FAILED: link name '%s' shouldn't exit \n", name); 
        return -1;
    }

    /* Verify link info */
    if (info->type != H5L_TYPE_HARD) {
        printf("VERIFICATION FAILED: Expected hard link, got type %d\n", info->type);
        return -1;
    }

    (*count)++;
    return 0;
}

/* Verify that link iteration works properly */
int LinkIterateTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    int link_count = 0;
    hsize_t idx = 0;

    printf("Testing GRIB2 VOL connector link iteration with file: %s  ", filename);

    /* Add the plugin path so HDF5 can find the connector */
#ifdef GRIB2_VOL_PLUGIN_PATH
    if (H5PLappend(GRIB2_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the GRIB2 VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(GRIB2_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
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

    /* Open the GRIB2 file */
    if ((file_id = H5Fopen("example-1.grib2", H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open GRIB2 file\n");
        goto error;
    }

    /* Iterate over links in root group */
    if (H5Literate2(file_id, H5_INDEX_NAME, H5_ITER_INC, &idx, link_iterate_callback, &link_count) <
        0) {
        printf("Failed to iterate over links\n");
        goto error;
    }

    /* Verify we found exactly 6 links */
    if (link_count != 6) {
        printf("VERIFICATION FAILED: Expected 6 links, found %d\n", link_count);
        goto error;
    }

    /* Verify index was updated */
    if (idx != 6) {
        printf("VERIFICATION FAILED: Expected index 6 after iteration, got %llu\n",
               (unsigned long long) idx);
        goto error;
    }

    /* Clean up */
    if (H5Fclose(file_id) < 0) {
        printf("Failed to close file\n");
        goto error;
    }

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
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;

    printf("FAILED\n");
    return 1;
}

