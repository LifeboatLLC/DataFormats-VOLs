#include "bufr_vol_connector.h"
#include <hdf5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
#define BUFR_VOL_PLUGIN_PATH "<path_to_the_connector>"
*/

static void
print_header(const char *title)
{
    printf("\n=== %s ===\n", title);
}

static void
print_attr_table_header(void)
{
    printf("%-50.50s%s\n", "Attribute Name", "HDF5 Datatype");
    printf("%-50.50s%s\n", "==============", "=============");
}

static const char *
datatype_name(hid_t type_id)
{
    if (H5Tequal(type_id, H5T_NATIVE_LONG) > 0)
        return "H5T_NATIVE_LONG";
    else if (H5Tequal(type_id, H5T_NATIVE_DOUBLE) > 0)
        return "H5T_NATIVE_DOUBLE";
    else if (H5Tequal(type_id, H5T_NATIVE_UCHAR) > 0)
        return "H5T_NATIVE_UCHAR";
    else if (H5Tget_class(type_id) == H5T_STRING) {
        if (H5Tis_variable_str(type_id))
            return "HDF5 variable-length string";
        else
            return "HDF5 fixed-length string";
    }
    else
        return "Other";
}

/* Callback to find number of groups in the file */
static herr_t
file_info(hid_t loc_id, const char *name, const H5L_info2_t *finfo, void *opdata)
{   
    size_t *count    = (size_t *)opdata;
    (void)loc_id;
    (void)name;
    (void)finfo;

    printf (" Group name \"%s\" \n", name);   
    (*count)++;

    return 0;
}

/* Callback to find number of datasets in the group */
static herr_t
group_info(hid_t loc_id, const char *name, const H5L_info2_t *ginfo, void *opdata)
{   
    size_t *count    = (size_t *)opdata;
    (void)loc_id;
    (void)name;
    (void)ginfo;

    printf (" Dataset name \"%s\" \n", name);   
    (*count)++;

    return 0;
}

/* Callback executed for each attribute */
static herr_t
attr_info(hid_t loc_id, const char *name, const H5A_info_t *ainfo, void *opdata)
{
    hid_t   attr_id  = H5I_INVALID_HID;
    hid_t   type_id  = H5I_INVALID_HID;
    size_t *count    = (size_t *)opdata;

    (void)ainfo;

    attr_id = H5Aopen(loc_id, name, H5P_DEFAULT);
    if (attr_id < 0) {
        fprintf(stderr, "Failed to open attribute '%s'\n", name);
        return 0; /* continue iteration */
    }

    type_id = H5Aget_type(attr_id);
    if (type_id < 0) {
        fprintf(stderr, "Found unsupported datatype for attribute '%s'\n", name);
        H5Aclose(attr_id);
        return 0; /* continue iteration */
    }

    printf("%-50.50s%s\n", name, datatype_name(type_id));
    (*count)++;

    H5Tclose(type_id);
    H5Aclose(attr_id);

    return 0; /* continue iteration */
}

static int
read_long_attribute(hid_t obj_id, const char *attr_name, long *value)
{
    hid_t attr_id = H5I_INVALID_HID;
    int   ret     = -1;

    if (H5Aexists(obj_id, attr_name) <= 0)
        return -1;

    attr_id = H5Aopen(obj_id, attr_name, H5P_DEFAULT);
    if (attr_id < 0)
        return -1;

    if (H5Aread(attr_id, H5T_NATIVE_LONG, value) >= 0)
        ret = 0;

    H5Aclose(attr_id);
    return ret;
}

static int
read_scalar_vl_string_attribute(hid_t obj_id, const char *attr_name, char **value)
{
    hid_t attr_id  = H5I_INVALID_HID;
    hid_t type_id  = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    int   ret      = -1;

    *value = NULL;

    if (H5Aexists(obj_id, attr_name) <= 0)
        return -1;

    attr_id = H5Aopen(obj_id, attr_name, H5P_DEFAULT);
    if (attr_id < 0)
        goto done;

    type_id = H5Aget_type(attr_id);
    if (type_id < 0)
        goto done;

    space_id = H5Aget_space(attr_id);
    if (space_id < 0)
        goto done;

    if (H5Aread(attr_id, type_id, value) < 0)
        goto done;

    ret = 0;

done:
    if (ret < 0 && *value) {
        H5Treclaim(type_id, space_id, H5P_DEFAULT, value);
        *value = NULL;
    }

    if (space_id >= 0)
        H5Sclose(space_id);
    if (type_id >= 0)
        H5Tclose(type_id);
    if (attr_id >= 0)
        H5Aclose(attr_id);

    return ret;
}

static int
print_double_dataset(hid_t group_id, const char *dset_name, hid_t *dset_out)
{
    hid_t    dset_id  = H5I_INVALID_HID;
    hid_t    space_id = H5I_INVALID_HID;
    hid_t    type_id  = H5I_INVALID_HID;
    double  *data     = NULL;
    hsize_t  dims[1]  = {0};

    dset_id = H5Dopen(group_id, dset_name, H5P_DEFAULT);
    if (dset_id < 0)
        goto error;

    space_id = H5Dget_space(dset_id);
    if (space_id < 0)
        goto error;

    if (H5Sget_simple_extent_ndims(space_id) != 1) {
        fprintf(stderr, "Dataset '%s' is not 1-dimensional\n", dset_name);
        goto error;
    }

    if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0)
        goto error;

    type_id = H5Dget_type(dset_id);
    if (type_id < 0)
        goto error;

    data = (double *)malloc((size_t)dims[0] * sizeof(*data));
    if (!data) {
        fprintf(stderr, "Memory allocation failed for dataset '%s'\n", dset_name);
        goto error;
    }

    if (H5Dread(dset_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) < 0)
        goto error;

    printf("Dataset '%s' has %llu elements:\n", dset_name, (unsigned long long)dims[0]);
    for (hsize_t i = 0; i < dims[0]; i++)
        printf("  [%llu] %f\n", (unsigned long long)i, data[i]);

    free(data);
    H5Tclose(type_id);
    H5Sclose(space_id);

    *dset_out = dset_id;
    return 0;

error:
    free(data);
    if (type_id >= 0)
        H5Tclose(type_id);
    if (space_id >= 0)
        H5Sclose(space_id);
    if (dset_id >= 0)
        H5Dclose(dset_id);

    *dset_out = H5I_INVALID_HID;
    return -1;
}

static void
print_vl_string_dataset(hid_t group_id, const char *dset_name)
{
    hid_t   dset_id  = H5I_INVALID_HID;
    hid_t   space_id = H5I_INVALID_HID;
    hid_t   type_id  = H5I_INVALID_HID;
    char  **data     = NULL;
    hsize_t dims[1]  = {0};

    dset_id = H5Dopen(group_id, dset_name, H5P_DEFAULT);
    if (dset_id < 0) {
        printf("Dataset '%s' not found\n", dset_name);
        return;
    }

    space_id = H5Dget_space(dset_id);
    type_id  = H5Dget_type(dset_id);

    if (space_id < 0 || type_id < 0) {
        fprintf(stderr, "Failed to inspect dataset '%s'\n", dset_name);
        goto done;
    }

    if (H5Sget_simple_extent_ndims(space_id) != 1) {
        fprintf(stderr, "Dataset '%s' is not 1-dimensional\n", dset_name);
        goto done;
    }

    if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0) {
        fprintf(stderr, "Failed to get dimensions for '%s'\n", dset_name);
        goto done;
    }

    if (!(H5Tget_class(type_id) == H5T_STRING && H5Tis_variable_str(type_id))) {
        fprintf(stderr, "Dataset '%s' is not a VL string dataset\n", dset_name);
        goto done;
    }

    data = (char **)malloc((size_t)dims[0] * sizeof(*data));
    if (!data) {
        fprintf(stderr, "Memory allocation failed for '%s'\n", dset_name);
        goto done;
    }

    if (H5Dread(dset_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) < 0) {
        fprintf(stderr, "Failed to read dataset '%s'\n", dset_name);
        goto done;
    }

    printf("Dataset '%s' contains %llu VL strings:\n",
           dset_name, (unsigned long long)dims[0]);
    for (hsize_t i = 0; i < dims[0]; i++)
        printf("  [%llu] %s\n",
               (unsigned long long)i,
               data[i] ? data[i] : "(null)");

done:
    if (data) {
        H5Dvlen_reclaim(type_id, space_id, H5P_DEFAULT, data);
        free(data);
    }
    if (type_id >= 0)
        H5Tclose(type_id);
    if (space_id >= 0)
        H5Sclose(space_id);
    if (dset_id >= 0)
        H5Dclose(dset_id);
}

static void
print_attribute_count(hid_t obj_id, const char *title)
{
    hsize_t idx       = 0;
    size_t  num_attrs = 0;

    print_header(title);
    print_attr_table_header();

    H5Aiterate2(obj_id, H5_INDEX_NAME, H5_ITER_INC, &idx, attr_info, &num_attrs);
    printf("Number of attributes: %zu\n", num_attrs);
}

int
main(void)
{
    hid_t vol_id   = H5I_INVALID_HID;
    hid_t fapl_id  = H5I_INVALID_HID;
    hid_t file_id  = H5I_INVALID_HID;
    hid_t group_id = H5I_INVALID_HID;
    hid_t dset_id  = H5I_INVALID_HID;
    hsize_t idx       = 0;

    int   ret = EXIT_FAILURE;

    /* Tell the library where to find the BUFR VOL connector library */
    /* H5PLappend(BUFR_VOL_PLUGIN_PATH); */

    vol_id = H5VLregister_connector_by_name(BUFR_VOL_CONNECTOR_NAME, H5P_DEFAULT);
    if (vol_id < 0) {
        fprintf(stderr, "Failed to register BUFR VOL connector\n");
        goto done;
    }

    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    if (fapl_id < 0) {
        fprintf(stderr, "Failed to create FAPL\n");
        goto done;
    }

    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        fprintf(stderr, "Failed to set VOL connector on FAPL\n");
        goto done;
    }

    file_id = H5Fopen("temp.bufr", H5F_ACC_RDONLY, fapl_id);
    if (file_id < 0) {
        fprintf(stderr, "Failed to open BUFR file 'temp.bufr'\n");
        goto done;
    }

    /* ------------------------------------------------------------------ */
    /* Find number of groupsinthe file and print their names              */
    /* ------------------------------------------------------------------ */
    size_t  num_groups = 0;
    if (H5Literate2(file_id, H5_INDEX_NAME, H5_ITER_INC, &idx, file_info, &num_groups) < 0) {
        fprintf(stderr, "Failed to find number of groups in 'temp.bufr'\n");
        goto done;
    }
    printf ("Number of group in the file 'temp.bufr' is %zu \n", num_groups);
    
    group_id = H5Gopen2(file_id, "message_0", H5P_DEFAULT);
    if (group_id < 0) {
        fprintf(stderr, "Failed to open group 'message_0'\n");
        goto done;
    }
    /* ------------------------------------------------------------------ */
    /* Find number of datasets in the group and print their names         */
    /* ------------------------------------------------------------------ */

    idx = 0;
    size_t  num_dsets = 0;
    if (H5Literate2(group_id, H5_INDEX_NAME, H5_ITER_INC, &idx, group_info, &num_dsets) < 0) {
        fprintf(stderr, "Failed to find number of datasets in 'message_0'\n");
        goto done;
    }
    printf ("Number of datasets in the group 'message_0' is %zu \n", num_dsets);
    

    /* ------------------------------------------------------------------ */
    /* Read a numeric group attribute                                     */
    /* ------------------------------------------------------------------ */
    {
        long nsubsets = 0;

        print_header("Group attribute: numberOfSubsets");
        if (read_long_attribute(group_id, "numberOfSubsets", &nsubsets) == 0)
            printf("Value of 'numberOfSubsets' = %ld\n", nsubsets);
        else
            printf("Attribute 'numberOfSubsets' not found or could not be read\n");
    }

    /* ------------------------------------------------------------------ */
    /* Read a scalar VL string group attribute                            */
    /* ------------------------------------------------------------------ */
    {
        char *typical_date = NULL;

        print_header("Group attribute: typicalDate");
        if (read_scalar_vl_string_attribute(group_id, "typicalDate", &typical_date) == 0) {
            printf("Value of 'typicalDate' = \"%s\"\n",
                   typical_date ? typical_date : "(null)");

            /* Re-open type/space for reclaim in this simple helper-based example */
            hid_t attr_id  = H5Aopen(group_id, "typicalDate", H5P_DEFAULT);
            hid_t type_id  = H5Aget_type(attr_id);
            hid_t space_id = H5Aget_space(attr_id);

            H5Treclaim(type_id, space_id, H5P_DEFAULT, &typical_date);

            H5Sclose(space_id);
            H5Tclose(type_id);
            H5Aclose(attr_id);
        }
        else
            printf("Attribute 'typicalDate' not found or could not be read\n");
    }

    /* ------------------------------------------------------------------ */
    /* Read numeric dataset                                               */
    /* ------------------------------------------------------------------ */
    print_header("Dataset: pressure");
    if (print_double_dataset(group_id, "pressure", &dset_id) < 0) {
        fprintf(stderr, "Failed to read dataset 'pressure'\n");
        goto done;
    }

    /* ------------------------------------------------------------------ */
    /* Inspect dataset attribute 'units'                                  */
    /* ------------------------------------------------------------------ */
    print_header("Dataset attribute: pressure/units");
    if (H5Aexists(dset_id, "units") > 0) {
        hid_t   attr_id   = H5Aopen(dset_id, "units", H5P_DEFAULT);
        hid_t   aspace_id = H5Aget_space(attr_id);
        hsize_t adims[1]  = {0};

        H5Sget_simple_extent_dims(aspace_id, adims, NULL);
        printf("Attribute 'units' is an array of %llu elements\n",
               (unsigned long long)adims[0]);

        H5Sclose(aspace_id);
        H5Aclose(attr_id);
    }
    else
        printf("Attribute 'units' not found\n");

    /* ------------------------------------------------------------------ */
    /* Read shadow dataset pressure_units                                 */
    /* ------------------------------------------------------------------ */
    print_header("Shadow dataset: pressure_units");
    if (H5Lexists(group_id, "pressure_units", H5P_DEFAULT) > 0)
        print_vl_string_dataset(group_id, "pressure_units");
    else
        printf("Dataset 'pressure_units' not found\n");

    /* ------------------------------------------------------------------ */
    /* Iterate over attributes                                            */
    /* ------------------------------------------------------------------ */
    print_attribute_count(group_id, "Attributes on group message_0");
    print_attribute_count(dset_id,   "Attributes on dataset pressure");

    ret = EXIT_SUCCESS;

done:
    if (dset_id >= 0)
        H5Dclose(dset_id);
    if (group_id >= 0)
        H5Gclose(group_id);
    if (file_id >= 0)
        H5Fclose(file_id);
    if (fapl_id >= 0)
        H5Pclose(fapl_id);
    if (vol_id >= 0)
        H5VLunregister_connector(vol_id);

    return ret;
}
