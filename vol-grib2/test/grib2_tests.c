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

typedef struct {
    hid_t id;
    int   count;
} iter_info_t;

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
    hid_t vol_id    = H5I_INVALID_HID;
    hid_t fapl_id   = H5I_INVALID_HID;
    hid_t file_id   = H5I_INVALID_HID;
    hid_t group_id  = H5I_INVALID_HID;
    hid_t dset_id   = H5I_INVALID_HID;
    hid_t dapl_id   = H5I_INVALID_HID;
    hid_t space_id  = H5I_INVALID_HID;
    hid_t aspace_id = H5I_INVALID_HID;
    hid_t type_id   = H5I_INVALID_HID;
    hid_t atype_id  = H5I_INVALID_HID;
    hid_t attr_id   = H5I_INVALID_HID;
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

    if (H5Tequal(type_id, H5T_NATIVE_DOUBLE) <= 0) {
        printf("Failed to check datatype of the shapeOfTheEarth attribute\n");
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
    /* Get attribute dataspace */
    
    if ((aspace_id = H5Aget_space(attr_id)) < 0) {
        printf("Failed to get dataspce of the shapeOfTheEarth attribute\n");
        goto error;
    }
    H5S_class_t aspace_class = H5Sget_simple_extent_type(aspace_id);
    if (aspace_class != H5S_SCALAR) {
        printf("Wrong class of dataspace of the shapeOfTheEarth attribute detected\n");
        goto error;
    }

    /* Get datatype */
    if ((atype_id = H5Aget_type(attr_id)) < 0) { 
        printf("Failed to get attribute datatype\n");
        goto error;
    }

    if (H5Tequal(atype_id, H5T_NATIVE_LONG) <= 0) {
        printf("Failed to check datatype of the shapeOfTheEarth attribute\n");
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
    
    /* Close the GRIB2 attribute, dataset and group*/
    free(data);
    if (H5Sclose(aspace_id) < 0) {
        printf("Failed to close attr dataspace\n");
        goto error;
    }
    aspace_id = H5I_INVALID_HID;
    if (H5Tclose(atype_id) < 0) {
        printf("Failed to close attribute datatype\n");
        goto error;
    }
    atype_id = H5I_INVALID_HID;
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
        H5Pclose(aspace_id);
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

static const char* type_class_name(H5T_class_t c)
{
    switch (c) {
        case H5T_INTEGER:   return "INTEGER";
        case H5T_FLOAT:     return "FLOAT";
        case H5T_STRING:    return "STRING";
        default:            return "OTHER";
    }
}

static herr_t attr_iter_cb(hid_t loc_id, const char *attr_name,
                           const H5A_info_t *ainfo, void *op_data)
{
    (void)ainfo;
    iter_info_t *data = (iter_info_t *)op_data;

    hid_t attr_id = H5Aopen(data->id, attr_name, H5P_DEFAULT);
    if (attr_id < 0) {
        fprintf(stderr, "  [attr] Failed to open attribute '%s'\n", attr_name);
        return 0; /* continue */
    }
    
    data->count++;
    hid_t type_id = H5Aget_type(attr_id);
    if (type_id < 0) {
       fprintf(stderr, "  [attr] Failed to get type for '%s'\n", attr_name);
        H5Aclose(attr_id);
        return 0;
    }

    H5T_class_t tclass = H5Tget_class(type_id);
    printf("    @%s  (class=%s)\n",
           attr_name,
           type_class_name(tclass));

    H5Tclose(type_id);
    H5Aclose(attr_id);
    return 0; /* continue iteration */
}

/* Helper: print all attributes for a group id */
static void print_group_attrs(hid_t gid)
{
    hsize_t idx = 0;
    
    iter_info_t data;
    data.id = gid;
    data.count = 0;
    herr_t st = H5Aiterate2(gid, H5_INDEX_NAME, H5_ITER_INC, &idx, attr_iter_cb, &data);
    if (st < 0) {
        fprintf(stderr, "    [attr] H5Aiterate2 failed\n");
    }
    printf("Number of attributes is %d \n", data.count);
}

/* Callback for link iteration */
static herr_t link_iterate_callback(hid_t loc_id, const char *name, const H5L_info2_t *info,
                                    void *op_data)
{
    iter_info_t *data = (iter_info_t *)op_data;

    hid_t grp_id = H5I_INVALID_HID;
    (void) info;  /* Unused */

    int n = 0;
    if (sscanf(name, "message_%d", &n) == 1 &&
        n >= 1 && n <= 6)
    {
        data->count++;
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

    /* TODO: name is expected to start with / ; fix*/
     /* Build "/name" */
    size_t len = strlen(name);
    char *tmp_name = (char *)malloc(len + 2);  /* "/" + name + '\0' */
    if (!tmp_name) {
        fprintf(stderr, "Memory allocation failed\n");
        return 0;
    }

    tmp_name[0] = '/';
    memcpy(tmp_name + 1, name, len + 1);  /* include '\0' */
    
    if ((grp_id = H5Gopen2(data->id, tmp_name, H5P_DEFAULT)) < 0) {
            fprintf(stderr, "[root] Failed to open group '%s'\n", tmp_name);
            free(tmp_name);
            return 0; /* continue */
    }
    printf("[group] %s\n", tmp_name);
    /* Iterate over group's attributes */
    print_group_attrs(grp_id);

    H5Gclose(grp_id);
    
    free(tmp_name);
    return 0;
}


/* Verify that link iteration works properly */
int LinkAttrIterateTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    iter_info_t info;
    info.id = H5I_INVALID_HID;
    info.count = 0;
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
    info.id = file_id;
    if (H5Literate2(file_id, H5_INDEX_NAME, H5_ITER_INC, &idx, link_iterate_callback, &info) <
        0) {
        printf("Failed to iterate over links\n");
        goto error;
    }

    /* Verify we found exactly 6 links */
    if (info.count != 6) {
        printf("VERIFICATION FAILED: Expected 6 links, found %d\n", info.count);
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

typedef enum {
    ATTRK_DOUBLE_SCALAR,
    ATTRK_LONG_SCALAR,
    ATTRK_FIXED_STRING
} attr_kind_t;

typedef struct attr_spec_t {
    const char  *name;
    attr_kind_t  kind;

    /* for numeric */
    double expected_double;
    long   expected_long;
    double epsilon; /* used for double */

    /* for fixed string */
    const char *expected_str;
    size_t      expected_len; /* fixed string length in file */
} attr_spec_t;

static int
read_verify_attr(hid_t obj_id, const attr_spec_t *spec)
{
    hid_t  attr_id = H5I_INVALID_HID;
    hid_t  file_type = H5I_INVALID_HID;
    hid_t  mem_type  = H5I_INVALID_HID;
    hid_t  exp_type  = H5I_INVALID_HID;
    hsize_t storage  = 0;

    void  *buf = NULL;
    int    ret = -1;

    if (!spec || !spec->name)
        goto done;

    if ((attr_id = H5Aopen(obj_id, spec->name, H5P_DEFAULT)) < 0) {
        printf("Failed to open attribute '%s'\n", spec->name);
        goto done;
    }

    if ((file_type = H5Aget_type(attr_id)) < 0) {
        printf("Failed to get datatype for attribute '%s'\n", spec->name);
        goto done;
    }

    storage = (hsize_t)H5Aget_storage_size(attr_id);
    if (storage == 0) {
        printf("Attribute '%s' has zero storage size\n", spec->name);
        goto done;
    }

    switch (spec->kind) {
        case ATTRK_DOUBLE_SCALAR: {
            if (H5Tequal(file_type, H5T_NATIVE_DOUBLE) <= 0) {
                printf("Attribute '%s': expected H5T_NATIVE_DOUBLE\n", spec->name);
                goto done;
            }
            if (storage != sizeof(double)) {
                printf("Attribute '%s': expected storage %zu, got %llu\n",
                       spec->name, sizeof(double), (unsigned long long)storage);
                goto done;
            }

            mem_type = H5T_NATIVE_DOUBLE;

            buf = malloc((size_t)storage);
            if (!buf) {
                printf("Out of memory reading attribute '%s'\n", spec->name);
                goto done;
            }

            if (H5Aread(attr_id, mem_type, buf) < 0) {
                printf("Failed to read attribute '%s'\n", spec->name);
                goto done;
            }

            {
                double got = ((double *)buf)[0];
                double eps = (spec->epsilon > 0.0) ? spec->epsilon : 1e-12;
                if (fabs(got - spec->expected_double) > eps) {
                    printf("VERIFICATION FAILED '%s': expected %.17g, got %.17g\n",
                           spec->name, spec->expected_double, got);
                    goto done;
                }
            }
        } break;

        case ATTRK_LONG_SCALAR: {
            if (H5Tequal(file_type, H5T_NATIVE_LONG) <= 0) {
                printf("Attribute '%s': expected H5T_NATIVE_LONG\n", spec->name);
                goto done;
            }
            if (storage != sizeof(long)) {
                printf("Attribute '%s': expected storage %zu, got %llu\n",
                       spec->name, sizeof(long), (unsigned long long)storage);
                goto done;
            }

            mem_type = H5T_NATIVE_LONG;

            buf = malloc((size_t)storage);
            if (!buf) {
                printf("Out of memory reading attribute '%s'\n", spec->name);
                goto done;
            }

            if (H5Aread(attr_id, mem_type, buf) < 0) {
                printf("Failed to read attribute '%s'\n", spec->name);
                goto done;
            }

            {
                long got = ((long *)buf)[0];
                if (got != spec->expected_long) {
                    printf("VERIFICATION FAILED '%s': expected %ld, got %ld\n",
                           spec->name, spec->expected_long, got);
                    goto done;
                }
            }
        } break;

        case ATTRK_FIXED_STRING: {
            /* Build the expected fixed-length string type */
            exp_type = H5Tcopy(H5T_C_S1);
            if (exp_type < 0)
                goto done;

            if (H5Tset_size(exp_type, spec->expected_len) < 0)
                goto done;

            if (H5Tset_strpad(exp_type, H5T_STR_NULLTERM) < 0)
                goto done;

            if (H5Tequal(file_type, exp_type) <= 0) {
                printf("Attribute '%s': expected fixed string len=%zu (nullterm)\n",
                       spec->name, spec->expected_len);
                goto done;
            }

            if (storage != spec->expected_len) {
                printf("Attribute '%s': expected storage %zu, got %llu\n",
                       spec->name, spec->expected_len, (unsigned long long)storage);
                goto done;
            }

            /* Read into a buffer with room for a forced terminator */
            buf = calloc((size_t)storage, 1);
            if (!buf) {
                printf("Out of memory reading attribute '%s'\n", spec->name);
                goto done;
            }

            /* For fixed strings, reading with file_type is fine */
            if (H5Aread(attr_id, file_type, buf) < 0) {
                printf("Failed to read attribute '%s'\n", spec->name);
                goto done;
            }
            ((char *)buf)[storage] = '\0';

            if (strcmp((const char *)buf, spec->expected_str) != 0) {
                printf("VERIFICATION FAILED '%s': expected '%s', got '%s'\n",
                       spec->name, spec->expected_str, (const char *)buf);
                goto done;
            }
        } break;

        default:
            printf("Attribute '%s': unknown spec kind\n", spec->name);
            goto done;
    }

    ret = 0;

done:
    if (buf)
        free(buf);

    if (exp_type != H5I_INVALID_HID)
        H5Tclose(exp_type);

    if (file_type != H5I_INVALID_HID)
        H5Tclose(file_type);

    if (attr_id != H5I_INVALID_HID)
        H5Aclose(attr_id);

    return ret;
}

/* Verify that GRIB2 attribute operations work properly */
int
AttrGRIB2Test(const char *filename)
{
    hid_t vol_id   = H5I_INVALID_HID;
    hid_t fapl_id  = H5I_INVALID_HID;
    hid_t file_id  = H5I_INVALID_HID;
    hid_t group_id = H5I_INVALID_HID;

    int rc = -1;

    printf("Testing GRIB2 VOL attributes of different datatypes...\n");

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

    if ((group_id = H5Gopen2(file_id, "/message_6", H5P_DEFAULT)) < 0) {
        printf("Failed to open group '/message_6'\n");
        goto error;
    }

    /* Attribute checks live in a single table */
    {
        const attr_spec_t specs[] = {
            {
                .name = "maximum",
                .kind = ATTRK_DOUBLE_SCALAR,
                .expected_double = 19.7,
                .epsilon = 1e-5
            },
            {
                .name = "forecastTime",
                .kind = ATTRK_LONG_SCALAR,
                .expected_long = 3
            },
            {
                .name = "parameterName",
                .kind = ATTRK_FIXED_STRING,
                .expected_str = "v-component of wind",
                .expected_len = 20 
            }
        };

        size_t i;
        int err;
        for (i = 0; i < (sizeof(specs) / sizeof(specs[0])); i++) {
            err = read_verify_attr(group_id, &specs[i]); 
            if (err < 0)
                goto error;
        }
    }

    printf("PASSED\n");
    rc = 0;

error:
    H5E_BEGIN_TRY
    {
        if (group_id != H5I_INVALID_HID)
            H5Gclose(group_id);
        if (file_id != H5I_INVALID_HID)
            H5Fclose(file_id);
        if (fapl_id != H5I_INVALID_HID)
            H5Pclose(fapl_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;

    return rc;
}
