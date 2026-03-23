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
} /* end OpenCDFTest() */

/* Verify that CDF open/close operations work properly with HDF5 file, group, dataset, and attribute
 * id's */
int CheckExistenceAndOpenTest(void)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t group_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t attr_id = H5I_INVALID_HID;
    H5G_info_t ginfo;
    hsize_t dims[1];
    int ndims;
    double *data = NULL;
    long shape = -1;
    htri_t exists;

    const char *filename = "example1.cdf";
    printf("Testing CDF VOL connector group, dataset, and attribute open/close with file: %s\n",
           filename);

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

    if (H5Gget_info(file_id, &ginfo) < 0) {
        printf("Failed to get group info\n");
        goto error;
    }

    if (ginfo.storage_type != H5G_STORAGE_TYPE_COMPACT || ginfo.nlinks != 3 ||
        ginfo.max_corder != 2 || ginfo.mounted) {
        printf("Group info returned from H5Gget_info() is wrong\n");
        goto error;
    }

    /* Check image dataset link with '/' for root group */
    if ((exists = H5Lexists(file_id, "/Image", H5P_DEFAULT)) < 0) {
        printf("Failed to check existence of dataset\n");
        goto error;
    }
    if (!exists) {
        printf("Dataset '/Image' doesn't exist but it should\n");
        goto error;
    }

    /* Open a group */
    if ((group_id = H5Gopen2(file_id, "/", H5P_DEFAULT)) < 0) {
        printf("Failed to open root group\n");
        goto error;
    }

    /* Test if link exists from root group to image dataset */
    if ((exists = H5Lexists(group_id, "/Image", H5P_DEFAULT)) < 0) {
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

    /* Test if "UNITS" attribute exists for "/Time" dataset */
    if ((exists = H5Aexists(dset_id, "UNITS")) < 0) {
        printf("Failed to check existence of dataset attribute\n");
        goto error;
    }
    if (!exists) {
        printf("Attribute 'UNITS' doesn't exist for dataset '/Time', but it should\n");
        goto error;
    }

    /* Open the existing attribute */
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
} /* end CheckExistenceAndOpenTest() */

/* Verify that H5Lexists works for all variables in CDF file */
int MultiLinksExistTest(void)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    htri_t exists;

    const char *filename = "example2.cdf";
    const char *var_names[] = {"rVar_char", "rVar_float2x2", "zVar_char", "zVar_epoch16",
                               "zVar_tt2000"};

    size_t num_vars = 5;

    printf("Testing H5Lexists for %zu variables in example2.cdf... \n", num_vars);

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

    /* Check that all 5 variables exist */
    for (size_t i = 0; i < num_vars; i++) {

        if ((exists = H5Lexists(file_id, var_names[i], H5P_DEFAULT)) < 0) {
            printf("Failed to check link existence for '%s'\n", var_names[i]);
            goto error;
        }

        if (!exists) {
            printf("VERIFICATION FAILED: Link '%s' should exist but doesn't\n", var_names[i]);
            goto error;
        }
    }

    /* Verify that nonexistant variables don't exist */
    if ((exists = H5Lexists(file_id, "nonexistent", H5P_DEFAULT)) < 0) {
        printf("Failed to check link existence for '/message_7'\n");
        goto error;
    }

    if (exists) {
        printf("VERIFICATION FAILED: Link 'nonexistant' should not exist but does\n");
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
} /* end MultiLinksExistTest() */

/* Helper function used in link iteration callback functions to verify link information from
 * example2.cdf */
static herr_t verify_link(const char *name, const H5L_info2_t *info)
{
    const char *expected_names[] = {"rVar_char", "rVar_float2x2", "zVar_char", "zVar_epoch16",
                                    "zVar_tt2000"};
    const int expected_count = 5;

    int found = 0;
    for (int i = 0; i < expected_count; i++) {
        if (strcmp(name, expected_names[i]) == 0) {
            printf("Found expected link: %s\n", name);
            found = 1;
            break;
        }
    }

    if (!found) {
        printf("VERIFICATION FAILED: link name '%s' shouldn't exist\n", name);
        return -1;
    }

    if (info->type != H5L_TYPE_HARD) {
        printf("VERIFICATION FAILED: Expected hard link, got type %d\n", info->type);
        return -1;
    }

    return 0;
} /* end verify_link() */

/* Helper function to use as callback function for link iteration with early stop capability */
static herr_t early_stop_link_iterate_cb(hid_t loc_id, const char *name, const H5L_info2_t *info,
                                         void *op_data)
{
    int *iteration_count = (int *) op_data;

    (void) loc_id;

    if (verify_link(name, info) < 0) {
        return -1;
    }

    (*iteration_count)++;

    if (*iteration_count == 3) {
        return 1; /* stop iteration */
    }

    return 0;
} /* end early_stop_link_iterate_cb */

/* Helper function to use as callback function for global attribute iteration */
static herr_t gAttr_iteration_cb(hid_t loc_id, const char *name, const H5A_info_t *info,
                                 void *op_data)
{
    int *iteration_count = (int *) op_data;

    (void) loc_id; /* Unused */

    int n = 0;
    if (sscanf(name, "gAttr%d", &n) == 1 && n >= 1 && n <= 3) {
        (*iteration_count)++;
        printf("Found global attribute: gAttr%d\n", n);
    } else {
        fprintf(stderr, "VERIFICATION FAILED: link name '%s' shouldn't exist \n", name);
        return -1;
    }

    return 0;
} /* end gAttr_iteration_cb() */

static const char *type_class_name(H5T_class_t c)
{
    switch (c) {
        case H5T_INTEGER:
            return "INTEGER";
        case H5T_FLOAT:
            return "FLOAT";
        case H5T_STRING:
            return "STRING";
        default:
            return "OTHER";
    }
} /* end type_class_name() */

/* Attribute iteration callback helper function that does simple verification specifically
 * for example2.cdf vAttributes */
static herr_t vAttr_iteration_cb(hid_t loc_id, const char *name, const H5A_info_t *ainfo,
                                 void *op_data)
{
    (void) ainfo;
    int *iteration_count = (int *) op_data;

    hid_t attr_id = H5Aopen(loc_id, name, H5P_DEFAULT);
    if (attr_id < 0) {
        fprintf(stderr, "  [attr] Failed to open attribute '%s'\n", name);
        return -1;
    }

    (*iteration_count)++;
    hid_t type_id = H5Aget_type(attr_id);
    if (type_id < 0) {
        fprintf(stderr, "  [attr] Failed to get type for '%s'\n", name);
        H5Aclose(attr_id);
        return -1;
    }

    H5T_class_t tclass = H5Tget_class(type_id);
    printf("    @%s  (class=%s)\n", name, type_class_name(tclass));

    H5Tclose(type_id);
    H5Aclose(attr_id);

    return 0; /* continue iteration */
} /* end vAttr_iteration_cb() */

/* Link iteration callback helper function that iterates all of the attributes associated with that
 * link */
static herr_t link_iterate_attrs_cb(hid_t loc_id, const char *name, const H5L_info2_t *info,
                                    void *op_data)
{
    hid_t dset_id = H5I_INVALID_HID;
    int *iteration_count = (int *) op_data;
    hsize_t idx = 0;
    int attr_iteration_count = 0;

    (void) loc_id;

    if (verify_link(name, info) < 0) {
        return -1;
    }

    (*iteration_count)++;

    /* Open the link (which is a dataset)*/
    if ((dset_id = H5Dopen2(loc_id, name, H5P_DEFAULT)) < 0) {
        fprintf(stderr, "Failed to open dataset for attribute iteration: %s\n", name);
        return -1;
    }

    /* Iterate over all attributes attached to this dataset */
    if (H5Aiterate2(dset_id, H5_INDEX_NAME, H5_ITER_INC, &idx, vAttr_iteration_cb,
                    &attr_iteration_count) < 0) {
        fprintf(stderr, "Failed attribute iteration on dataset link");
        return -1;
    }

    if (H5Dclose(dset_id) < 0) {
        fprintf(stderr, "Failed to close dataset\n");
        return -1;
    }

    /* Make sure that the index and our count variable are equal */
    if (attr_iteration_count != idx) {
        fprintf(stderr, "Expected iteration index and iteration count to be equal\n");
        fprintf(stderr, "   idx = %llu   count = %d\n", idx, attr_iteration_count);
        return -1;
    }

    /* Verify that number of attributes iterated matches expected number */
    if (strcmp(name, "rVar_char") == 0 || strcmp(name, "rVar_float2x2") == 0) {
        if (idx != 1) {
            fprintf(stderr, "Expected to iterate over 1 attribute on link %s\n", name);
            return -1;
        }
    } else if (strcmp(name, "zVar_char") == 0 || strcmp(name, "zVar_epoch16") == 0) {
        if (idx != 2) {
            fprintf(stderr, "Expected to iterate over 1 attribute on link %s\n", name);
            return -1;
        }
    } else if (strcmp(name, "zVar_tt2000") == 0) {
        if (idx != 0) {
            fprintf(stderr, "Expected link %s to have 0 attributes\n", name);
            return -1;
        }
    } else {
        fprintf(stderr, "Link name not expected: %s\n", name);
        return -1;
    }

    return 0;
} /* end link_iterate_attrs_cb() */

/* Verify that link iteration works properly */
int LinkAttrIterateTest(void)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t group_id = H5I_INVALID_HID;
    int iteration_count = 0;
    hsize_t idx = 0;

    const char *filename = "example2.cdf";
    printf("Testing CDF VOL connector link iteration with file: %s\n", filename);

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

    /* Iterate over file id links using basic link iteration callback */
    if (H5Literate2(file_id, H5_INDEX_NAME, H5_ITER_INC, &idx, early_stop_link_iterate_cb,
                    &iteration_count) < 0) {
        printf("Failed to iterate over links\n");
        goto error;
    }

    /* Verify we stopped iterating after 3 links */
    if (iteration_count != 3) {
        printf("VERIFICATION FAILED: Expected 3 links, found %d\n", iteration_count);
        goto error;
    }

    /* Verify index was updated */
    if (idx != 3) {
        printf("VERIFICATION FAILED: Expected index 3 after iteration, got %llu\n",
               (unsigned long long) idx);
        goto error;
    }

    /* Resume iteration after early stop */
    if (H5Literate2(file_id, H5_INDEX_NAME, H5_ITER_INC, &idx, early_stop_link_iterate_cb,
                    &iteration_count) < 0) {
        printf("Failed to iterate over links\n");
        goto error;
    }

    /* Verify the other 2 links were iterated */
    if (iteration_count != 5) {
        printf("VERIFICATION FAILED: Expected 5 links, found %d\n", iteration_count);
        goto error;
    }

    /* Verify index was updated */
    if (idx != 5) {
        printf("VERIFICATION FAILED: Expected index 5 after link iteration, got %llu\n",
               (unsigned long long) idx);
        goto error;
    }

    /* Open root group */
    if ((group_id = H5Gopen2(file_id, "/", H5P_DEFAULT)) < 0) {
        printf("Failed to open root group\n");
        goto error;
    }

    /* Test gAttribute iteration with root group */
    idx = 0;
    iteration_count = 0;
    if (H5Aiterate2(group_id, H5_INDEX_NAME, H5_ITER_INC, &idx, gAttr_iteration_cb,
                    &iteration_count) < 0) {
        printf("Failed to iterate over links\n");
        goto error;
    }

    /* Verify we stopped iterating after 3 attributes */
    if (iteration_count != 3) {
        printf("VERIFICATION FAILED: Expected 3 attributes, found %d\n", iteration_count);
        goto error;
    }

    /* Verify index was updated */
    if (idx != 3) {
        printf("VERIFICATION FAILED: Expected index 3 after gAttr iteration, got %llu\n",
               (unsigned long long) idx);
        goto error;
    }

    idx = 0;
    iteration_count = 0;
    /* Test vAttribute iteration by iterating all attributes of all links */
    if (H5Literate2(group_id, H5_INDEX_NAME, H5_ITER_INC, &idx, link_iterate_attrs_cb,
                    &iteration_count)) {
        printf("Failed link iteration with internal attribute iteration\n");
        goto error;
    }

    /* Clean up */
    if (H5Gclose(group_id) < 0) {
        printf("Failed to close root group\n");
        goto error;
    }

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
} /* end LinkAttrIterateTest*/

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
    /* We dont bother checking the number of dimensions before this because we know it has 3
     * dimensions already */
    if ((ndims = H5Sget_simple_extent_dims(space_id, dims, NULL)) < 0) {
        printf("Failed to get dataspace dimensions\n");
        goto error;
    }

    /* Verify dimensions - This information was found by running `cdfdump example1.cdf` */
    if (ndims != 3 || dims[0] != 3 || dims[1] != 10 || dims[2] != 20) {
        printf("Dataspace dimensions mismatch\n");
        printf("  Expected: 3:[3, 10, 20], Got: %d[%llu, %llu, %llu]\n", ndims,
               (unsigned long long) dims[0], (unsigned long long) dims[1],
               (unsigned long long) dims[2]);
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
    int *data_buffer = (int *) malloc(buffer_size);
    if (!data_buffer) {
        printf("Failed to allocate read buffer\n");
        goto error;
    }

    /* Read data */
    if (H5Dread(dset_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, data_buffer) < 0) {
        printf("Failed to read dataset\n");
        goto error;
    }

    /* Verify data - Each element should be it's own index because of the way the data is structured
     */
    int data_valid = 1;
    for (int record = 0; record < dims[0]; record++) {
        for (int i = 0; i < dims[1]; i++) {
            for (int j = 0; j < dims[2]; j++) {
                int expected_value = record * dims[1] * dims[2] + i * dims[2] + j;
                int actual_value = data_buffer[record * dims[1] * dims[2] + i * dims[2] + j];
                if (actual_value != expected_value) {
                    printf(
                        "VERIFICATION FAILED: Data mismatch at [%d, %d, %d]: expected %d, got %d\n",
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
    if (H5Pclose(fapl_id) < 0) {
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

/* Test simple dataset datatype conversion by specifically reading "Latitude" zVariable from file
 * "example1.cdf" and verifying expected conversion behavior */
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
    hid_t conversion_types[] = {H5T_NATIVE_SCHAR,  H5T_NATIVE_UCHAR,  H5T_NATIVE_SHORT,
                                H5T_NATIVE_USHORT, H5T_NATIVE_UINT,   H5T_NATIVE_LONG,
                                H5T_NATIVE_LLONG,  H5T_NATIVE_ULLONG, H5T_NATIVE_FLOAT,
                                H5T_NATIVE_DOUBLE};

    const char *type_names[] = {"SCHAR", "UCHAR", "SHORT",  "USHORT", "UINT",
                                "LONG",  "LLONG", "ULLONG", "FLOAT",  "DOUBLE"};

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
        bool verification_failed = false;
        for (size_t idx = 0; idx < num_elements; ++idx) {
            int expected_source = -90 + (int) idx; /* Latitude values from -90 to 90 */

            /* Get actual converted value based on memory type */
            double actual_converted = 0.0;
            double expected_converted = (double) expected_source;

            if (H5Tequal(mem_type_id, H5T_NATIVE_SCHAR)) {
                actual_converted = (double) ((signed char *) data)[idx];
                /* Clamp to signed char range [-128, 127] */
                if (expected_source > 127) {
                    expected_converted = 127.0;
                } else if (expected_source < -128) {
                    expected_converted = -128.0;
                }
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_UCHAR)) {
                actual_converted = (double) ((unsigned char *) data)[idx];
                /* Clamp to unsigned char range [0, 255] */
                if (expected_source > 255) {
                    expected_converted = 255.0;
                } else if (expected_source < 0) {
                    expected_converted = 0.0;
                }
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_SHORT)) {
                actual_converted = (double) ((short *) data)[idx];
                /* Clamp to short range */
                if (expected_source > 32767) {
                    expected_converted = 32767.0;
                } else if (expected_source < -32768) {
                    expected_converted = -32768.0;
                }
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_USHORT)) {
                actual_converted = (double) ((unsigned short *) data)[idx];
                if (expected_source > 65535) {
                    expected_converted = 65535.0;
                } else if (expected_source < 0) {
                    expected_converted = 0.0;
                }
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_UINT)) {
                actual_converted = (double) ((unsigned int *) data)[idx];
                if (expected_source < 0) {
                    expected_converted = 0.0;
                }
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_INT)) {
                actual_converted = (double) ((int64_t *) data)[idx];
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_LONG)) {
                actual_converted = (double) ((long *) data)[idx];
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_LLONG)) {
                actual_converted = (double) ((long long *) data)[idx];
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_ULLONG)) {
                actual_converted = (double) ((unsigned long long *) data)[idx];
                if (expected_source < 0) {
                    expected_converted = 0.0;
                }
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_FLOAT)) {
                actual_converted = (double) ((float *) data)[idx];
            } else if (H5Tequal(mem_type_id, H5T_NATIVE_DOUBLE)) {
                actual_converted = ((double *) data)[idx];
            }

            /* Check if values match expected values */
            if (actual_converted != expected_converted) {
                printf(
                    "\n    VERIFICATION FAILED at idx %zu: expected %.0f, got %.0f (source was %d)",
                    idx, expected_converted, actual_converted, expected_source);
                verification_failed = true;
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
    if (H5Pclose(fapl_id) < 0) {
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

/* Test reading a global array attribute from the CDF file by not passing an index in the attribute
 * name */
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
    printf("Testing CDF VOL connector by reading '%s' vAttribute from dataset '%s' in file: %s\n",
           attr_name, dataset_name, filename);

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

    if ((ndims = H5Sget_simple_extent_ndims(space_id)) < 0) {
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
    attr_data = (char *) malloc(type_size * num_entries);
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
        printf("  Got:      '%.*s'\n", (int) type_size, s);
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
    if (H5Pclose(fapl_id) < 0) {
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

/* Test reading a global array attribute from "example1.cdf" CDF file by not passing an index in the
 * attribute name */
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

    const char *filename = "example2.cdf";
    const char *attr_name = "gAttr1";
    printf("Testing CDF VOL connector by reading '%s' gAttribute from file: %s\n", attr_name,
           filename);

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

    /* We expect this gAttribute to be a 1D array with 5 entries (based on `cdfdump example2.cdf`)
     */
    if (ndims != 1) {
        printf("Attribute dataspace rank mismatch - expected 1, got %d\n", ndims);
        goto error;
    }

    if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0) {
        printf("Failed to get attribute dataspace dimensions\n");
        goto error;
    }

    num_entries = dims[0];
    if (num_entries != 5) {
        printf("Attribute dataspace dimension mismatch - expected 2 entries, got %llu\n",
               (unsigned long long) num_entries);
        goto error;
    }

    if ((type_size = H5Tget_size(type_id)) == 0) {
        printf("Failed to get attribute datatype size\n");
        goto error;
    }

    /* Allocate buffer for attribute data */
    attr_data = (char *) calloc(num_entries, type_size);
    if (!attr_data) {
        printf("Failed to allocate attribute data buffer\n");
        goto error;
    }

    /* Read attribute data */
    if (H5Aread(attr_id, type_id, attr_data) < 0) {
        printf("Failed to read attribute data\n");
        goto error;
    }

    const char *expected_titles[] = {
        "0 (CDF_CHAR/18): Second Example CDF", "1 (CDF_UCHAR/21): Author: Lifeboat, LLC",
        "2 (CDF_DOUBLE/1): 3.141592653589793",
        "3 (CDF_EPOCH16/2): [{6.2798418e+10, 1.23321e+11}, {6.3933885e+10, 1.2345679e+11}]",
        "20 (CDF_TIME_TT2000/1): 1625097600000000000"};
    for (size_t i = 0; i < 5; i++) {
        const char *s = attr_data + (i * type_size);
        size_t expected_len = strlen(expected_titles[i]);

        if (expected_len > type_size || strncmp(s, expected_titles[i], expected_len) != 0) {
            printf("VERIFICATION FAILED: Attribute data mismatch at gEntry[%zu]\n", i);
            printf("  Expected: '%s'\n", expected_titles[i]);
            printf("  Got:      '%.*s'\n", (int) type_size, s);
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
    if (H5Pclose(fapl_id) < 0) {
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

/* Test reading a global attribute entry from the CDF file by passing the gEntry index in the
 * attribute name */
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
    const char *attr_names[] = {"TITLE_0", "TITLE_1"};
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

    /* Test invalid gEntry index */
    if ((attr_id = H5Aopen(file_id, "TITLE_5", H5P_DEFAULT)) >= 0) {
        printf("Opened attribute that shouldnt exist!\n");
        goto error;
    }

    /* Now test valid indices */
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
            printf("Expected Scalar string attribute for %s attribute in file %s\n", attr_names[i],
                   filename);
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
        attr_data = (char *) calloc(type_size * num_entries, sizeof(char));
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
            printf("  Got:      '%.*s'\n", (int) type_size, s);
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
    if (H5Pclose(fapl_id) < 0) {
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
    printf("Testing CDF VOL connector by reading rVariable dataset 'rVar_char' and its rEntry "
           "attributes from file: %s\n",
           filename);

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
    char *s = (char *) buffer;
    size_t expected_len = strlen(expected_var);

    if (expected_len > type_size || strncmp(s, expected_var, expected_len) != 0) {
        printf("VERIFICATION FAILED: Attribute data mismatch\n");
        printf("  Expected: '%s'\n", expected_var);
        printf("  Got:      '%.*s'\n", (int) type_size, s);
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
    if ((attr_id = H5Aopen(dset_id, "vAttribute1", H5P_DEFAULT)) < 0) {
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

    /* We expect ndims to be 0 for a scalar attribute, but we'll paste the pattern anyway (for
     * future reference) */
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
    s = (char *) buffer;
    expected_len = strlen(expected_attr);
    if (expected_len > type_size || strncmp(s, expected_attr, expected_len) != 0) {
        printf("VERIFICATION FAILED: Attribute data mismatch\n");
        printf("  Expected: '%s'\n", expected_attr);
        printf("  Got:      '%.*s'\n", (int) type_size, s);
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
    if (H5Pclose(fapl_id) < 0) {
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
