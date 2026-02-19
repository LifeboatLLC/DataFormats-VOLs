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
} /* end OpenBasicCDFTest() */


/* Verify that CDF file open/close operations work properly */
int OpenLinksandGroupsTest(void)
{
    hid_t vol_id   = H5I_INVALID_HID;
    hid_t fapl_id  = H5I_INVALID_HID;
    hid_t file_id  = H5I_INVALID_HID;
    hid_t group_id = H5I_INVALID_HID;
    hid_t dset_id  = H5I_INVALID_HID;
    hid_t attr_id  = H5I_INVALID_HID;
    hsize_t dims[1];
    int ndims;
    double *data = NULL;
    long shape = -1;
    htri_t exists;

    const char *filename = "example1.cdf";
    printf("Testing CDF VOL connector group, dataset, and attribute open/close with file: %s\n", filename);

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

    /* Check image dataset link with '/' for root group */
    if((exists = H5Lexists(file_id, "/Image", H5P_DEFAULT)) < 0) {
        printf("Failed to check existence of dataset\n");
        goto error;
    }
    
    if (!exists) {
        printf("Dataset '/Image' doesn't exist but it should\n");
        goto error;
    }

    /* Open a group */
    if ((group_id = H5Gopen2(file_id, "/", H5P_DEFAULT)) < 0) {
        printf("Failed to open group\n");
        goto error;
    }

    /* Test if link exists from root group to image dataset */
    if((exists = H5Lexists(group_id, "/Image", H5P_DEFAULT)) < 0) {
        printf("Failed to check existence of dataset\n");
        goto error;
    }
    if (!exists) {
        printf("Dataset '/Image' doesn't exist but it should\n");
        goto error;
    }

    /* Open a dataset with root group_id */
    if ((dset_id = H5Dopen2(group_id, "/Time", H5P_DEFAULT)) < 0) {
        printf("Failed to open dataset\n");
        goto error;
    }

    /* Open an attribute */
    if ((attr_id = H5Aopen(dset_id, "UNITS", H5P_DEFAULT)) < 0) {
        printf("Failed to open attribute\n");
        goto error;
    }

    /* Clean up */
    if (H5Aclose(attr_id) < 0) {
        printf("Failed to close attribute\n");
        goto error;
    }

    if (H5Dclose(dset_id) < 0) {
        printf("Failed to close dataset\n");
        goto error;
    }

    if (H5Gclose(group_id) < 0) {
        printf("Failed to close group\n");
        goto error;
    }

    if (H5Fclose(file_id) < 0) {
        printf("Failed to close CDF file\n");
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
        H5Aclose(attr_id);
        H5Dclose(dset_id);
        H5Gclose(group_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        H5Fclose(file_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;
    printf("FAILED\n");
    return -1;
} /* end OpenLinksandGroupsTest() */

/* Test ability to read a simple CDF variable by specifically opening the 
 * 'Image' zVariable from file "example1.cdf" and verify its contents are
 * as expected. */
int ReadCDFVariableTest(void)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;
    hsize_t dims[3];
    int ndims;

    const char *filename = "example1.cdf";
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
    /* We dont bother checking the number of dimensions before this because we know it has 3 dimensions already */
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

    /* Verify data - Each element should be it's own index because of the way the data is structured */
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
    if (vol_id != H5I_INVALID_HID && H5VLunregister_connector(vol_id) < 0) {
        printf("Failed to unregister VOL connector\n");
        goto error;
    }
    vol_id = H5I_INVALID_HID;

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
} /* end ReadCDFVariableTest() */

/* Test simple dataset datatype conversion by specifically reading "Latitude" zVariable from file "example1.cdf"
 * and verifying expected conversion behavior */
int DatasetDatatypeConversionTest(void)
{
    const char *filename = "example1.cdf";
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    hid_t file_type_id = H5I_INVALID_HID;
    hsize_t dims[2];
    int ndims;
    size_t num_elements;

    /* Datatypes to test conversion to */
    hid_t conversion_types[] = {
        H5T_NATIVE_SCHAR,
        H5T_NATIVE_UCHAR,
        H5T_NATIVE_SHORT,
        H5T_NATIVE_USHORT,
        H5T_NATIVE_UINT,
        H5T_NATIVE_LONG,
        H5T_NATIVE_LLONG,
        H5T_NATIVE_ULLONG,
        H5T_NATIVE_FLOAT,
        H5T_NATIVE_DOUBLE
    };
    
    const char *type_names[] = {
        "SCHAR",
        "UCHAR",
        "SHORT",
        "USHORT",
        "UINT",
        "LONG",
        "LLONG",
        "ULLONG",
        "FLOAT",
        "DOUBLE"
    };
    
    int num_types = sizeof(conversion_types) / sizeof(conversion_types[0]);
    int num_failed = 0;

    printf("Testing CDF VOL datatype conversion on 'Latitude' variable from file: %s\n", filename);

    /* Setup: Register VOL connector and open file/dataset (same as before) */
#ifdef CDF_VOL_PLUGIN_PATH
    if (H5PLappend(CDF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    if ((vol_id = H5VLregister_connector_by_name(CDF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
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
        printf("Failed to open CDF file\n");
        goto error;
    }

    if ((dset_id = H5Dopen2(file_id, "Latitude", H5P_DEFAULT)) < 0) {
        printf("Failed to open dataset\n");
        goto error;
    }
    
    if ((space_id = H5Dget_space(dset_id)) < 0) {
        printf("Failed to get dataspace\n");
        goto error;
    }

    if ((ndims = H5Sget_simple_extent_dims(space_id, dims, NULL)) < 0) {
        printf("Failed to get dataspace dimensions\n");
        goto error;
    }

    /* Verify dimensions */
    if (ndims != 2 || dims[0] != 1 || dims[1] != 181) {
        printf("Expected 2 dimensions, got %d\n", ndims);
        goto error;
    }

    /* Get dataset's normal datatype */
    if ((file_type_id = H5Dget_type(dset_id)) < 0) {
        printf("Failed to get dataset datatype\n");
        goto error;
    }

    /* Verify it's a SHORT (what we expect from Latitude variable) */
    if (!H5Tequal(file_type_id, H5T_NATIVE_SHORT)) {
        printf("Expected H5T_NATIVE_SHORT datatype in file\n");
        goto error;
    }

    num_elements = dims[0] * dims[1]; /* Should be 1 * 181 = 181 */

    /* Loop through each conversion type and test */
    for (int i = 0; i < num_types; ++i) {
        hid_t mem_type_id = conversion_types[i];
        void *data = NULL;
        size_t mem_type_size;
        
        printf("Testing INT to %s conversion... ", type_names[i]);
        
        /* Allocate buffer for this type */
        if ((mem_type_size = H5Tget_size(mem_type_id)) == 0) {
            printf("FAILED (couldn't get type size)\n");
            num_failed++;
            continue;
        }
        
        if ((data = malloc(num_elements * mem_type_size)) == NULL) {
            printf("FAILED (couldn't allocate buffer)\n");
            num_failed++;
            continue;
        }
        
        /* Attempt to read with datatype conversion */
        if (H5Dread(dset_id, mem_type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) < 0) {
            printf("FAILED (read error)\n");
            free(data);
            num_failed++;
            continue;
        }
        
        /* Verify converted values */
        int verification_failed = 0;
        for (size_t idx = 0; idx < num_elements; ++idx) {
            int expected_source = -90 + (int)idx; /* Latitude values from -90 to 90 */
        
            /* Get actual converted value based on memory type */
            double actual_converted = 0.0;
            double expected_converted = (double)expected_source;
            
            if (H5Tequal(mem_type_id, H5T_NATIVE_SCHAR)) {
                actual_converted = (double)((signed char *)data)[idx];
                /* Clamp to signed char range [-128, 127] */
                if (expected_source > 127){
                    expected_converted = 127.0;
                } else if (expected_source < -128) {
                    expected_converted = -128.0;
                }
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_UCHAR)) {
                actual_converted = (double)((unsigned char *)data)[idx];
                /* Clamp to unsigned char range [0, 255] */
                if (expected_source > 255) {
                    expected_converted = 255.0;
                } else if (expected_source < 0) {
                    expected_converted = 0.0;
                }
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_SHORT)) {
                actual_converted = (double)((short *)data)[idx];
                /* Clamp to short range */
                if (expected_source > 32767) {
                    expected_converted = 32767.0;
                } else if (expected_source < -32768) {
                    expected_converted = -32768.0;
                }
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_USHORT)) {
                actual_converted = (double)((unsigned short *)data)[idx];
                if (expected_source > 65535) {
                    expected_converted = 65535.0;
                } else if (expected_source < 0) {
                    expected_converted = 0.0;
                }
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_UINT)) {
                actual_converted = (double)((unsigned int *)data)[idx];
                if (expected_source < 0) {
                    expected_converted = 0.0;
                }
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_INT64)) {
                actual_converted = (double)((int64_t *)data)[idx];
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_UINT64)) {
                actual_converted = (double)((uint64_t *)data)[idx];
                if (expected_source < 0) {
                    expected_converted = 0.0;
                }
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_LONG)) {
                actual_converted = (double)((long *)data)[idx];
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_LLONG)) {
                actual_converted = (double)((long long *)data)[idx];
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_ULLONG)) {
                actual_converted = (double)((unsigned long long *)data)[idx];
                if (expected_source < 0) {
                    expected_converted = 0.0;
                }
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_FLOAT)) {
                actual_converted = (double)((float *)data)[idx];
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_DOUBLE)) {
                actual_converted = ((double *)data)[idx];
            }
            
            /* Check if values match expected values */
            if (actual_converted != expected_converted) {
                printf("\n    VERIFICATION FAILED at idx %zu: expected %.0f, got %.0f (source was %d)",
                        idx, expected_converted, actual_converted, expected_source);
                verification_failed = 1;
                break;
            }
        }
        
        free(data);
        
        if (verification_failed) {
            printf("FAILED\n");
            num_failed++;
        } else {
            printf("PASSED\n");
        }
    }

    /* Clean up */
    if (H5Tclose(file_type_id) < 0) {
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

    file_type_id = H5I_INVALID_HID;
    space_id = H5I_INVALID_HID;
    dset_id = H5I_INVALID_HID;
    file_id = H5I_INVALID_HID;
    fapl_id = H5I_INVALID_HID;

    /* Unregister VOL connector */
    if (vol_id != H5I_INVALID_HID && H5VLunregister_connector(vol_id) < 0) {
        printf("Failed to unregister VOL connector\n");
        goto error;
    }
    vol_id = H5I_INVALID_HID;

    if (num_failed > 0) {
        printf("Datatype conversion test completed with %d failures\n", num_failed);
        return 1;
    }

    printf("PASSED: All datatype conversions successful\n");
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Tclose(file_type_id);
        H5Sclose(space_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID) {
            H5VLunregister_connector(vol_id);
        }
    }
    H5E_END_TRY;

    printf("FAILED DATATYPE CONVERSION TEST\n");
    return 1;
} /* end DatasetDatatypeConversionTest() */

/* Test reading a global array attribute from the CDF file by not passing an index in the attribute name */
int ReadVariableAttributeTest(void)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t attr_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    char *attr_data = NULL;
    int ndims;
    size_t num_entries;
    size_t type_size;
    
    const char *filename = "example1.cdf";
    const char *attr_name = "FIELDNAM";
    const char *dataset_name = "Time";
    printf("Testing CDF VOL connector by reading '%s' vAttribute from dataset '%s' in file: %s\n", attr_name, dataset_name, filename);

    /* Setup: Register VOL connector and open file/dataset (same as before) */
#ifdef CDF_VOL_PLUGIN_PATH
    if (H5PLappend(CDF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    if ((vol_id = H5VLregister_connector_by_name(CDF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
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
        printf("Failed to open CDF file\n");
        goto error;
    }

    if ((dset_id = H5Dopen2(file_id, dataset_name, H5P_DEFAULT)) < 0) {
        printf("Failed to open dataset\n");
        goto error;
    }

    if ((attr_id = H5Aopen(dset_id, attr_name, H5P_DEFAULT)) < 0) {
        printf("Failed to open attribute\n");
        goto error;
    }
    
    if ((type_id = H5Aget_type(attr_id)) < 0) {
        printf("Failed to get attribute datatype\n");
        goto error;
    }

    if ((space_id = H5Aget_space(attr_id)) < 0) {
        printf("Failed to get attribute dataspace\n");
        goto error;
    }
    
    if((ndims = H5Sget_simple_extent_ndims(space_id)) < 0) {
        printf("Failed to get attribute dataspace rank\n");
        goto error;
    }

    /* We expect this vAttribute to be SCALAR */
    if (ndims != 0) {
        printf("Attribute dataspace rank mismatch - expected SCALAR (0), got %d\n", ndims);
        goto error;
    }

    num_entries = 1; /* Scalar attribute should have 1 entry */

    if ((type_size = H5Tget_size(type_id)) == 0) {
        printf("Failed to get attribute datatype size\n");
        goto error;
    }

    if (type_size == 0) {
        printf("Attribute has zero size\n");
        goto error;
    }

    /* Allocate buffer for attribute data */
    attr_data = (char *)malloc(type_size * num_entries);
    if (!attr_data) {
        printf("Failed to allocate attribute data buffer\n");
        goto error;
    }

    /* Read attribute data */
    if (H5Aread(attr_id, type_id, attr_data) < 0) {
        printf("Failed to read attribute data\n");
        goto error;
    }

    const char *expected_attr = "Time of observation";
    const char *s = attr_data;
    size_t expected_len = strlen(expected_attr);

    if (expected_len > type_size || strncmp(s, expected_attr, expected_len) != 0) {
        printf("VERIFICATION FAILED: Attribute data mismatch\n");
        printf("  Expected: '%s'\n", expected_attr);
        printf("  Got:      '%.*s'\n", (int)type_size, s);
        goto error;
    }

    /* Clean up */
    if (H5Tclose(type_id) < 0) {
        printf("Failed to close attribute datatype\n");
        goto error;
    }
    if (H5Sclose(space_id) < 0) {
        printf("Failed to close attribute dataspace\n");
        goto error;
    }
    if (H5Aclose(attr_id) < 0) {
        printf("Failed to close attribute\n");
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

    if (vol_id != H5I_INVALID_HID) {
        H5VLunregister_connector(vol_id);
    }
    printf("PASSED\n");
    return 0;
error:
    H5E_BEGIN_TRY
    {
        H5Tclose(type_id);
        H5Sclose(space_id);
        H5Aclose(attr_id);
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
} /* end ReadVariableAttributeTest() */

/* Test reading a global array attribute from "example1.cdf" CDF file by not passing an index in the attribute name */
int ReadUnindexedGlobalArrayAttributeTest(void)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t attr_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    char *attr_data = NULL;
    int ndims;
    hsize_t dims[1];
    size_t num_entries;
    size_t type_size;
    
    const char *filename = "example1.cdf";
    const char *attr_name = "TITLE";
    printf("Testing CDF VOL connector by reading '%s' gAttribute from file: %s\n", attr_name, filename);

    /* Setup: Register VOL connector and open file/dataset (same as before) */
#ifdef CDF_VOL_PLUGIN_PATH
    if (H5PLappend(CDF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    if ((vol_id = H5VLregister_connector_by_name(CDF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
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
        printf("Failed to open CDF file\n");
        goto error;
    }

    if ((attr_id = H5Aopen(file_id, attr_name, H5P_DEFAULT)) < 0) {
        printf("Failed to open attribute\n");
        goto error;
    }
    
    if ((type_id = H5Aget_type(attr_id)) < 0) {
        printf("Failed to get attribute datatype\n");
        goto error;
    }

    if ((space_id = H5Aget_space(attr_id)) < 0) {
        printf("Failed to get attribute dataspace\n");
        goto error;
    }

    if ((ndims = H5Sget_simple_extent_ndims(space_id)) < 0) {
        printf("Failed to get attribute dataspace rank\n");
        goto error;
    }

    /* We expect this gAttribute to be a 1D array with 2 entries (based on `cdfdump example1.cdf`) */
    if (ndims != 1) {
        printf("Attribute dataspace rank mismatch - expected 1, got %d\n", ndims);
        goto error;
    }

    if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0) {
        printf("Failed to get attribute dataspace dimensions\n");
        goto error;
    }

    num_entries = dims[0];
    if (num_entries != 2) {
        printf("Attribute dataspace dimension mismatch - expected 2 entries, got %llu\n", (unsigned long long)num_entries);
        goto error;
    }

    if ((type_size = H5Tget_size(type_id)) == 0) {
        printf("Failed to get attribute datatype size\n");
        goto error;
    }

    if (type_size == 0) {
        printf("Attribute has zero size\n");
        goto error;
    }

    /* Allocate buffer for attribute data */
    attr_data = (char *)calloc(type_size * num_entries, sizeof(char));
    if (!attr_data) {
        printf("Failed to allocate attribute data buffer\n");
        goto error;
    }

    /* Read attribute data */
    if (H5Aread(attr_id, type_id, attr_data) < 0) {
        printf("Failed to read attribute data\n");
        goto error;
    }

    const char *expected_titles[] = {"0 (CDF_CHAR/9): CDF title", "1 (CDF_CHAR/11): Author: CDF"};
    for (size_t i = 0; i < 2; i++) {
        const char *s = attr_data + (i * type_size);
        size_t expected_len = strlen(expected_titles[i]);

        if (expected_len > type_size || strncmp(s, expected_titles[i], expected_len) != 0) {
            printf("VERIFICATION FAILED: Attribute data mismatch at gEntry[%zu]\n", i);
            printf("  Expected: '%s'\n", expected_titles[i]);
            printf("  Got:      '%.*s'\n", (int)type_size, s);
            goto error;
        }
    }

    /* Clean up */
    if (H5Tclose(type_id) < 0) {
        printf("Failed to close attribute datatype\n");
        goto error;
    }
    if (H5Sclose(space_id) < 0) {
        printf("Failed to close attribute dataspace\n");
        goto error;
    }
    if (H5Aclose(attr_id) < 0) {
        printf("Failed to close attribute\n");
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

    if (vol_id != H5I_INVALID_HID) {
        H5VLunregister_connector(vol_id);
    }
    printf("PASSED\n");
    return 0;
error:
    H5E_BEGIN_TRY
    {
        H5Tclose(type_id);
        H5Sclose(space_id);
        H5Aclose(attr_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID) {
            H5VLunregister_connector(vol_id);
        }
    }
    H5E_END_TRY;
    printf("FAILED\n");
    return 1;
} /* end ReadUnindexedGlobalArrayAttributeTest() */

/* Test reading a global attribute entry from the CDF file by passing the gEntry index in the attribute name */
int ReadIndexedGlobalAttributeTest(void)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t attr_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    char *attr_data = NULL;
    int ndims;
    size_t num_entries;
    size_t type_size;

    const char *filename = "example1.cdf";
    const char *attr_names[] = {"TITLE[0]", "TITLE[1]"};
    const char *expected_titles[] = {"CDF title", "Author: CDF"};
    printf("Testing CDF VOL connector by reading indexed gAttributes '%s' and '%s' from file: %s\n",
        attr_names[0], attr_names[1], filename);

    /* Setup: Register VOL connector and open file/dataset (same as before) */
#ifdef CDF_VOL_PLUGIN_PATH
    if (H5PLappend(CDF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    if ((vol_id = H5VLregister_connector_by_name(CDF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
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
        printf("Failed to open CDF file\n");
        goto error;
    }

    for (size_t i = 0; i < 2; i++) {
        if ((attr_id = H5Aopen(file_id, attr_names[i], H5P_DEFAULT)) < 0) {
            printf("Failed to open attribute\n");
            goto error;
        }

        if ((type_id = H5Aget_type(attr_id)) < 0) {
            printf("Failed to get attribute datatype\n");
            goto error;
        }

        if ((space_id = H5Aget_space(attr_id)) < 0) {
            printf("Failed to get attribute dataspace\n");
            goto error;
        }

        if ((ndims = H5Sget_simple_extent_ndims(space_id)) < 0) {
            printf("Failed to get attribute dataspace rank\n");
            goto error;
        }

        if (ndims != 0) {
            printf("Expected Scalar string attribute for %s attribute in file %s\n", attr_names[i], filename);
            goto error;
        }

        /* Scalar types should have one entry */
        num_entries = 1;

        if ((type_size = H5Tget_size(type_id)) == 0) {
            printf("Failed to get attribute datatype size\n");
            goto error;
        }

        if (type_size == 0) {
            printf("Attribute has zero size\n");
            goto error;
        }

        /* Allocate buffer for attribute data */
        attr_data = (char *)calloc(type_size * num_entries, sizeof(char));
        if (!attr_data) {
            printf("Failed to allocate attribute data buffer\n");
            goto error;
        }

        /* Read attribute data */
        if (H5Aread(attr_id, type_id, attr_data) < 0) {
            printf("Failed to read attribute data\n");
            goto error;
        }

        const char *s = attr_data;
        size_t expected_len = strlen(expected_titles[i]);
        if (expected_len > type_size || strncmp(s, expected_titles[i], expected_len) != 0) {
            printf("VERIFICATION FAILED: Attribute data mismatch at gEntry[%zu]\n", i);
            printf("  Expected: '%s'\n", expected_titles[i]);
            printf("  Got:      '%.*s'\n", (int)type_size, s);
            goto error;
        }

        /* Clean up for this attribute */
        free(attr_data);
        attr_data = NULL;

        if (H5Sclose(space_id) < 0) {
            printf("Failed to close attribute dataspace\n");
            goto error;
        }
        space_id = H5I_INVALID_HID;
        if (H5Tclose(type_id) < 0) {
            printf("Failed to close attribute datatype\n");
            goto error;
        }
        type_id = H5I_INVALID_HID;
        if (H5Aclose(attr_id) < 0) {
            printf("Failed to close attribute\n");
            goto error;
        }
        attr_id = H5I_INVALID_HID;
    }

    /* Clean up */
    if (H5Fclose(file_id) < 0) {
        printf("Failed to close file\n");
        goto error;
    }
    if( H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }

    if (vol_id != H5I_INVALID_HID) {
        H5VLunregister_connector(vol_id);
    }
    printf("PASSED\n");
    return 0;
error:
    H5E_BEGIN_TRY
    {
        if (attr_data) {
            free(attr_data);
        }
        H5Tclose(type_id);
        H5Sclose(space_id);
        H5Aclose(attr_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID) {
            H5VLunregister_connector(vol_id);
        }
    }
    H5E_END_TRY;
    printf("FAILED\n");
    return 1;
} /* end ReadIndexedGlobalAttributeTest() */

/* Test reading rVariable datasets and rEntry attributes from "example2.cdf" file */
int ReadBasicRVariableAndREntryTest(void)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t attr_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    
    size_t num_entries;
    size_t type_size;
    int ndims;
    hsize_t dims[2];

    const char *filename = "example2.cdf";
    printf("Testing CDF VOL connector by reading rVariable dataset 'rVar_char' and its rEntry attributes from file: %s\n", filename);

    /* Setup: Register VOL connector and open file/dataset (same as before) */
#ifdef CDF_VOL_PLUGIN_PATH
    if (H5PLappend(CDF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    if ((vol_id = H5VLregister_connector_by_name(CDF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
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
        printf("Failed to open CDF file\n");
        goto error;
    }

    /* Attempt to open "rVar_char" rVariable */
    if ((dset_id = H5Dopen2(file_id, "rVar_char", H5P_DEFAULT)) < 0) {
        printf("Failed to open rVariable dataset\n");
        goto error;
    }

    if ((space_id = H5Dget_space(dset_id)) < 0) {
        printf("Failed to get rVariable dataspace\n");
        goto error;
    }

    if ((type_id = H5Dget_type(dset_id)) < 0) {
        printf("Failed to get rVariable datatype\n");
        goto error;
    }

    if ((ndims = H5Sget_simple_extent_ndims(space_id)) < 0) {
        printf("Failed to get rVariable dataspace dimensions\n");
        goto error;
    }

    num_entries = 1; /* Default to 1 for scalar, will adjust if it's an array */
    if (ndims > 0) {
        if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0) {
            printf("Failed to get rVariable dataspace dimensions\n");
            goto error;
        }
        
        /* Calculate total number of entries */
        for (int i = 0; i < ndims; i++) {
            num_entries *= dims[i];
        }
    }

    /* We expect this specific rVariable to be a singular string scalar in a 1-D array */
    if (num_entries != 1) {
        printf("Expected rVariable to be scalar, but it has %zu entries\n", num_entries);
        goto error;
    }

    /* Get the size of the rVariable datatype */
    type_size = H5Tget_size(type_id);
    if (type_size == 0) {
        printf("Failed to get rVariable datatype size\n");
        goto error;
    }

    /* Allocate void buffer to read data */
    void *buffer = malloc(num_entries * type_size);
    if (!buffer) {
        printf("Failed to allocate buffer for rVariable data\n");
        goto error;
    }

    /* Read the rVariable data */
    if (H5Dread(dset_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, buffer) < 0) {
        printf("Failed to read rVariable data\n");
        free(buffer);
        goto error;
    }

    /* Test expected values of the rVariable data */
    const char *expected_var = "rVariable character record.";
    char *s = (char *)buffer;
    size_t expected_len = strlen(expected_var);

    if (expected_len > type_size || strncmp(s, expected_var, expected_len) != 0) {
        printf("VERIFICATION FAILED: Attribute data mismatch\n");
        printf("  Expected: '%s'\n", expected_var);
        printf("  Got:      '%.*s'\n", (int)type_size, s);
        goto error;
    }
    free(buffer);

    /* Clean up space and type */
    if (H5Sclose(space_id) < 0) {
        printf("Failed to close attribute dataspace\n");
        goto error;
    }
    space_id = H5I_INVALID_HID;
    if (H5Tclose(type_id) < 0) {
        printf("Failed to close attribute datatype\n");
        goto error;
    }
    type_id = H5I_INVALID_HID;

    /* Now read the vAttribute data for this rVariable */
    if ((attr_id = H5Aopen(dset_id, "AttributeForVariables", H5P_DEFAULT)) < 0) {
        printf("Failed to open attribute\n");
        goto error;
    }

    if ((type_id = H5Aget_type(attr_id)) < 0) {
        printf("Failed to get attribute datatype\n");
        goto error;
    }

    if ((space_id = H5Aget_space(attr_id)) < 0) {
        printf("Failed to get attribute dataspace\n");
        goto error;
    }

    if ((ndims = H5Sget_simple_extent_ndims(space_id)) < 0) {
        printf("Failed to get attribute dataspace rank\n");
        goto error;
    }

    /* We expect ndims to be 0 for a scalar attribute, but we'll paste the pattern anyway (for future reference) */
    num_entries = 1; /* Scalar types should have one entry */
    if (ndims > 0) {
        if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0) {
            printf("Failed to get rVariable dataspace dimensions\n");
            goto error;
        }
        
        /* Calculate total number of entries */
        for (int i = 0; i < ndims; i++) {
            num_entries *= dims[i];
        }
    }

    /* BUT, again, we expect ndims to be 0 in this specific test, so we verify */
    if (ndims != 0 || num_entries != 1) {
        printf("Expected attribute to be scalar, but it has %zu entries\n", num_entries);
        goto error;
    }    

    /* Get the size of the attribute datatype */
    type_size = H5Tget_size(type_id);
    if (type_size == 0) {
        printf("Failed to get attribute datatype size\n");
        goto error;
    }

    /* Allocate void buffer to read data */
    buffer = malloc(num_entries * type_size);
    if (!buffer) {
        printf("Failed to allocate buffer for attribute data\n");
        goto error;
    }

    /* Read the attribute data */
    if (H5Aread(attr_id, type_id, buffer) < 0) {
        printf("Failed to read attribute data\n");
        free(buffer);
        goto error;
    }

    /* Test expected values of the attribute data */
    const char *expected_attr = "String rEntry for rVar_char";
    s = (char *)buffer;
    expected_len = strlen(expected_attr);
    if (expected_len > type_size || strncmp(s, expected_attr, expected_len) != 0) {
        printf("VERIFICATION FAILED: Attribute data mismatch\n");
        printf("  Expected: '%s'\n", expected_attr);
        printf("  Got:      '%.*s'\n", (int)type_size, s);
        free(buffer);
        goto error;
    }
    free(buffer);

    /* Clean up */
    if (H5Sclose(space_id) < 0) {
        printf("Failed to close attribute dataspace\n");
        goto error;
    }
    if (H5Tclose(type_id) < 0) {
        printf("Failed to close attribute datatype\n");
        goto error;
    }
    if (H5Aclose(attr_id) < 0) {
        printf("Failed to close attribute\n");
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

    if (vol_id != H5I_INVALID_HID) {
        H5VLunregister_connector(vol_id);
    }
    printf("PASSED\n");
    return 0;
error:
    H5E_BEGIN_TRY
    {
        H5Tclose(type_id);
        H5Sclose(space_id);
        H5Aclose(attr_id);
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
} /* end ReadBasicRVariableAndREntryTest() */

#ifdef TODO
int IndexedGAttributeDtypeConversionTest(void)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t attr_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    int ndims;

    size_t num_entries;
    size_t type_size;

    /* Datatypes to test conversion to */
    hid_t conversion_types[] = {
        H5T_NATIVE_SCHAR,
        H5T_NATIVE_UCHAR,
        H5T_NATIVE_SHORT,
        H5T_NATIVE_USHORT,
        H5T_NATIVE_UINT,
        H5T_NATIVE_LONG,
        H5T_NATIVE_LLONG,
        H5T_NATIVE_ULLONG,
        H5T_NATIVE_FLOAT,
        H5T_NATIVE_DOUBLE
    };
    
    const char *type_names[] = {
        "SCHAR",
        "UCHAR",
        "SHORT",
        "USHORT",
        "UINT",
        "LONG",
        "LLONG",
        "ULLONG",
        "FLOAT",
        "DOUBLE"
    };
    
    int num_types = sizeof(conversion_types) / sizeof(conversion_types[0]);
    int num_failed = 0;

    const char *filename = "example2.cdf";
    const char *attr_name = "GlobalAttribute[2]"; /* A CDF_REAL8 global attribute in example2.cdf */
    printf("Testing CDF VOL indexed gAttribute datatype conversion on '%s' from file: %s\n", attr_name, filename);

    /* Setup: Register VOL connector and open file/dataset (same as before) */
#ifdef CDF_VOL_PLUGIN_PATH
    if (H5PLappend(CDF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    if ((vol_id = H5VLregister_connector_by_name(CDF_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
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
        printf("Failed to open CDF file\n");
        goto error;
    }

    if ((attr_id = H5Aopen(file_id, attr_name, H5P_DEFAULT)) < 0) {
        printf("Failed to open attribute %s\n", attr_name);
        goto error;
    }

    if ((type_id = H5Aget_type(attr_id)) < 0) {
        printf("Failed to get attribute datatype\n");
        goto error;
    }

    /* Verify it's a DOUBLE (what we expect from this attribute) */
    if (!H5Tequal(type_id, H5T_NATIVE_DOUBLE)) {
        printf("Expected H5T_NATIVE_SHORT datatype in file\n");
        goto error;
    }

    if ((space_id = H5Aget_space(attr_id)) < 0) {
        printf("Failed to get attribute dataspace\n");
        goto error;
    }

    if((ndims = H5Sget_simple_extent_ndims(space_id)) < 0) {
        printf("Failed to get attribute dataspace rank\n");
        goto error;
    }
    if (ndims != 0) {
        printf("Expected attribute to be scalar, but it has rank %d\n", ndims);
        goto error;
    }

    /* Since we know the attribute is a scalar*/
    num_entries = 1;

    /* Loop through each conversion type and test */
    for (int i = 0; i < num_types; ++i) {
        hid_t mem_type_id = conversion_types[i];
        void *data = NULL;
        size_t mem_type_size;
        
        printf("Testing INT to %s conversion... ", type_names[i]);
        
        /* Allocate buffer for this type */
        if ((mem_type_size = H5Tget_size(mem_type_id)) == 0) {
            printf("FAILED (couldn't get type size)\n");
            num_failed++;
            continue;
        }
        
        if ((data = malloc(num_entries * mem_type_size)) == NULL) {
            printf("FAILED (couldn't allocate buffer)\n");
            num_failed++;
            continue;
        }

        /* Attempt to read the data with converted type */
        
        /*TODO: finish*/
    }

    
    /* Cleanup */
    if (H5Sclose(space_id) < 0) {
        printf("Failed to close attribute dataspace\n");
        goto error;
    }
    space_id = H5I_INVALID_HID;
    if (H5Tclose(type_id) < 0) {
        printf("Failed to close attribute datatype\n");
        goto error;
    }
    type_id = H5I_INVALID_HID;
    if (H5Aclose(attr_id) < 0) {
        printf("Failed to close attribute\n");
        goto error;
    }
    attr_id = H5I_INVALID_HID;
    if (H5Fclose(file_id) < 0) {
        printf("Failed to close file\n");
        goto error;
    }
    if( H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }
    if (vol_id != H5I_INVALID_HID) {
        H5VLunregister_connector(vol_id);
    }

    printf("PASSED\n");
    return 0;
error:
    H5E_BEGIN_TRY
    {
        H5Tclose(type_id);
        H5Sclose(space_id);
        H5Aclose(attr_id);
        H5Fclose(file_id);
        H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID) {
            H5VLunregister_connector(vol_id);
        }
    }
    H5E_END_TRY;
    printf("FAILED\n");
    return 1;
} /* end IndexedGAttributeDtypeConversionTest */
#endif