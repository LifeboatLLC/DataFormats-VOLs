#include "cdf_vol_connector.h"
#include <hdf5.h>
#include <stdio.h>
#include <stdlib.h>

static const char *datatype_name(hid_t type_id)
{
    if (H5Tequal(type_id, H5T_NATIVE_LONG) > 0)
        return "H5T_NATIVE_LONG";
    else if (H5Tequal(type_id, H5T_NATIVE_DOUBLE) > 0)
        return "H5T_NATIVE_DOUBLE";
    else if (H5Tequal(type_id, H5T_NATIVE_CHAR) > 0)
        return "H5T_NATIVE_CHAR";
    else if (H5Tequal(type_id, H5T_NATIVE_UCHAR) > 0)
        return "H5T_NATIVE_UCHAR";
    else if (H5Tget_class(type_id) == H5T_STRING) {
        if (H5Tis_variable_str(type_id))
            return "HDF5 variable-length string";
        else
            return "HDF5 fixed-length string";
    } else
        return "Other";
}

/* Callback executed for each attribute */
static herr_t attr_info(hid_t loc_id, const char *name, const H5A_info_t *ainfo, void *opdata)
{
    hid_t attr_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;
    size_t *count = (size_t *) opdata;

    (void) ainfo;

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

/* Callback to find number of groups in the file */
static herr_t file_info(hid_t loc_id, const char *name, const H5L_info2_t *finfo, void *opdata)
{
    size_t *count = (size_t *) opdata;
    hid_t dset_id = H5I_INVALID_HID;

    (void) name;
    (void) finfo;

    printf("Dataset name \"%s\" \n", name);
    (*count)++;

    if ((dset_id = H5Dopen2(loc_id, name, H5P_DEFAULT)) < 0) {
        fprintf(stderr, "Failed to open dataset for attribute iteration: %s\n", name);
        return -1;
    }

    /* Find number of global attributes in the file and print their names */
    size_t num_attrs = 0;
    hsize_t idx = 0;
    if (H5Aiterate2(dset_id, H5_INDEX_NAME, H5_ITER_INC, &idx, attr_info, &num_attrs) < 0) {
        fprintf(stderr, "Failed to find number of attributes attached to dataset '%s'\n", name);
        return 0; /* continue iteration */
    }
    printf("Number of attributes attached to dataset '%s' is %zu \n", name, num_attrs);

    H5Dclose(dset_id);

    return 0;
}

/* Helper function to open, read, and print a gAttribute gEntry list */
static int print_gAttribute_gentry_list(hid_t obj_id, const char *attr_name)
{
    hid_t attr_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;
    char *data = NULL;
    hsize_t dims[1] = {0};
    size_t type_size;

    attr_id = H5Aopen(obj_id, attr_name, H5P_DEFAULT);
    if (attr_id < 0)
        goto error;

    space_id = H5Aget_space(attr_id);
    if (space_id < 0)
        goto error;

    if (H5Sget_simple_extent_ndims(space_id) != 1) {
        fprintf(stderr, "Attribute '%s' is not 1-dimensional\n", attr_name);
        goto error;
    }

    if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0)
        goto error;

    type_id = H5Aget_type(attr_id);
    if (type_id < 0)
        goto error;

    type_size = H5Tget_size(type_id);
    if (type_size <= 0)
        goto error;

    data = (char *) calloc(dims[0], type_size);
    if (!data) {
        fprintf(stderr, "Memory allocation failed for attribute '%s'\n", attr_name);
        goto error;
    }

    if (H5Aread(attr_id, type_id, data) < 0)
        goto error;

    printf("Global attribute '%s' contains %llu gEntries\n", attr_name, dims[0]);

    /* Print all gEntries*/
    for (hsize_t i = 0; i < dims[0]; i++) {
        char *s = data + (i * type_size);
        printf("%s\n", s);
    }

    free(data);

    H5Tclose(type_id);
    H5Sclose(space_id);
    H5Aclose(attr_id);

    return 0;
error:
    if (data) {
        free(data);
        data = NULL;
    }

    if (type_id >= 0)
        H5Tclose(type_id);
    if (space_id >= 0)
        H5Sclose(space_id);
    if (attr_id >= 0)
        H5Dclose(attr_id);

    return -1;
}

/* Helper function to open, read, and print the raw value of an individual string type gEntry value */
static int print_string_gentry(hid_t obj_id, const char *attr_name)
{
    hid_t attr_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;
    char *data = NULL;
    size_t type_size;

    attr_id = H5Aopen(obj_id, attr_name, H5P_DEFAULT);
    if (attr_id < 0)
        return -1;

    space_id = H5Aget_space(attr_id);
    if (space_id < 0)
        goto error;

    if (H5Sget_simple_extent_ndims(space_id) != 0) {
        fprintf(stderr, "Attribute '%s' is not scalar\n", attr_name);
        goto error;
    }

    type_id = H5Aget_type(attr_id);
    if (type_id < 0)
        goto error;

    type_size = H5Tget_size(type_id);
    if (type_size <= 0)
        goto error;

    /* Only allocate for a single element of this type since attr is scalar */
    data = (char *) malloc(type_size);
    if (!data) {
        fprintf(stderr, "Memory allocation failed for attribute '%s'\n", attr_name);
        goto error;
    }

    if (H5Aread(attr_id, type_id, data) < 0)
        goto error;

    printf("Value of attribute '%s':  %s\n", attr_name, data);

    free(data);

    H5Tclose(type_id);
    H5Sclose(space_id);
    H5Aclose(attr_id);

    return 0;
error:
    if (data) {
        free(data);
        data = NULL;
    }

    if (type_id >= 0)
        H5Tclose(type_id);
    if (space_id >= 0)
        H5Sclose(space_id);
    if (attr_id >= 0)
        H5Aclose(attr_id);

    return -1;
}

/* Helper function to open, read, and print the raw value of an individual EPOCH16 type gEntry value */
static int print_epoch16_gentry(hid_t obj_id, const char *attr_name)
{
    hid_t attr_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;
    double *data = NULL;
    hsize_t dims[1] = {0};
    size_t type_size;

    attr_id = H5Aopen(obj_id, attr_name, H5P_DEFAULT);
    if (attr_id < 0)
        return -1;

    space_id = H5Aget_space(attr_id);
    if (space_id < 0)
        goto error;

    if (H5Sget_simple_extent_ndims(space_id) != 1) {
        fprintf(stderr, "Attribute '%s' is not 1-dimensional\n", attr_name);
        goto error;
    }

    if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0)
        goto error;

    type_id = H5Aget_type(attr_id);
    if (type_id < 0)
        goto error;

    type_size = H5Tget_size(type_id);
    if (type_size != 2 * sizeof(double))
        goto error;

    /* Allocate buffer for all epoch16 values in attribute */
    data = (double *) malloc(type_size * dims[0]);
    if (!data) {
        fprintf(stderr, "Memory allocation failed for attribute '%s'\n", attr_name);
        goto error;
    }

    if (H5Aread(attr_id, type_id, data) < 0)
        goto error;

    /* Every epoch16 contains two double values */
    const size_t DOUBLES_IN_EPOCH16 = 2;

    /* print both double values for each epoch16 in attribute */
    for (hsize_t i = 0; i < dims[0]; i++) {
        double first = data[DOUBLES_IN_EPOCH16 * i];
        double second = data[DOUBLES_IN_EPOCH16 * i + 1];
        printf("Attribute '%s', element %llu: [%f, %f]\n", attr_name, (unsigned long long) i, first,
               second);
    }

    free(data);

    H5Tclose(type_id);
    H5Sclose(space_id);
    H5Aclose(attr_id);

    return 0;
error:
    if (data) {
        free(data);
        data = NULL;
    }

    if (type_id >= 0)
        H5Tclose(type_id);
    if (space_id >= 0)
        H5Sclose(space_id);
    if (attr_id >= 0)
        H5Aclose(attr_id);

    return -1;
}

/* Helper function to open, read, and print the raw value of an EPOCH16 type dataset */
static int print_epoch16_dataset(hid_t obj_id, const char *dset_name)
{
    hid_t dset_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;
    double *data = NULL;
    hsize_t dims[2] = {0};
    size_t type_size;

    dset_id = H5Dopen2(obj_id, dset_name, H5P_DEFAULT);
    if (dset_id < 0)
        return -1;

    space_id = H5Dget_space(dset_id);
    if (space_id < 0)
        goto error;

    if (H5Sget_simple_extent_ndims(space_id) != 2) {
        fprintf(stderr, "Attribute '%s' is not 2-dimensional\n", dset_name);
        goto error;
    }

    if (H5Sget_simple_extent_dims(space_id, dims, NULL) < 0)
        goto error;

    type_id = H5Dget_type(dset_id);
    if (type_id < 0)
        goto error;

    type_size = H5Tget_size(type_id);
    if (type_size != 2 * sizeof(double))
        goto error;

    /* Allocate buffer for all epoch16 values in dataset */
    data = (double *) malloc(type_size * dims[0] * dims[1]);
    if (!data) {
        fprintf(stderr, "Memory allocation failed for dataset '%s'\n", dset_name);
        goto error;
    }

    if (H5Dread(dset_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) < 0)
        goto error;

    /* Every epoch16 contains two double values */
    const size_t DOUBLES_IN_EPOCH16 = 2;

    /* print both double values for each epoch16 in each record */
    for (hsize_t i = 0; i < dims[0]; i++) {
        for (hsize_t j = 0; j < dims[0]; j++) {
            size_t idx = (i * 2 + j) * 2;

            double first = data[idx];
            double second = data[idx + 1];

            printf("Dataset '%s', record %llu, epoch16 %llu: [%f, %f]\n", dset_name,
                   (unsigned long long) i, (unsigned long long) j, first, second);
        }
    }

    free(data);

    H5Tclose(type_id);
    H5Sclose(space_id);
    H5Dclose(dset_id);

    return 0;
error:
    if (data) {
        free(data);
        data = NULL;
    }

    if (type_id >= 0)
        H5Tclose(type_id);
    if (space_id >= 0)
        H5Sclose(space_id);
    if (dset_id >= 0)
        H5Dclose(dset_id);

    return -1;
}


int main(void)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t group_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hsize_t idx = 0;
    htri_t exists = 0;

    int ret = EXIT_FAILURE;

#ifdef CDF_VOL_PLUGIN_PATH
    if (H5PLappend(CDF_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto done;
    }
#endif

    /* Register CDF VOL connector */
    vol_id = H5VLregister_connector_by_name(CDF_VOL_CONNECTOR_NAME, H5P_DEFAULT);
    if (vol_id < 0) {
        fprintf(stderr, "Failed to register CDF VOL connector\n");
        goto done;
    }

    /* Create FAPL */
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    if (fapl_id < 0) {
        fprintf(stderr, "Failed to create FAPL\n");
        goto done;
    }

    /* Set FAPL to use registered VOL connector */
    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        fprintf(stderr, "Failed to set VOL connector on FAPL\n");
        goto done;
    }

    /* Open a file */
    file_id = H5Fopen("example2.cdf", H5F_ACC_RDONLY, fapl_id);
    if (file_id < 0) {
        fprintf(stderr, "Failed to open CDF file 'example2.cdf'\n");
        goto done;
    }

    /* Open root group */
    group_id = H5Gopen2(file_id, "/", H5P_DEFAULT);
    if (group_id < 0) {
        fprintf(stderr, "Failed to open CDF file 'example2.cdf'\n");
    }

    /* Find number of global attributes in the file and print their names */
    size_t num_gAttrs = 0;
    if (H5Aiterate2(group_id, H5_INDEX_NAME, H5_ITER_INC, &idx, attr_info, &num_gAttrs) < 0) {
        fprintf(stderr, "Failed to find number of datasets in 'example2.cdf'\n");
        goto done;
    }
    printf("Number of global attributes in the file 'example2.cdf' is %zu \n", num_gAttrs);

    /* Check existence of and print global attribute */
    exists = H5Aexists(group_id, "gAttr1");
    if (exists) {
        if (print_gAttribute_gentry_list(group_id, "gAttr1") < 0) {
            fprintf(stderr, "Failed to read attribute 'gAttr1' from file 'example2.cdf'\n");
            goto done;
        }
    } else {
        fprintf(stderr, "Failed existence check for attribute 'gAttr1' from file 'example2.cdf'\n");
        goto done;
    }

    /* Check existence of and print indexed global attribute (string gentry) */
    exists = H5Aexists(group_id, "gAttr1_1");
    if (exists) {
        if (print_string_gentry(group_id, "gAttr1_1") < 0) {
            fprintf(stderr, "Failed to read attribute 'gAttr1_1' from file 'example2.cdf'\n");
            goto done;
        }
    } else {
        fprintf(stderr,
                "Failed existence check for attribute 'gAttr1_1' from file 'example2.cdf'\n");
        goto done;
    }

    /* Check existence of and print indexed global attribute (EPOCH16 gentry)*/
    exists = H5Aexists(group_id, "gAttr1_3");
    if (exists) {
        if (print_epoch16_gentry(group_id, "gAttr1_3") < 0) {
            fprintf(stderr, "Failed to read attribute 'gAttr1_3' from file 'example2.cdf'\n");
            goto done;
        }
    } else {
        fprintf(stderr,
                "Failed existence check for attribute 'gAttr1_3' from file 'example2.cdf'\n");
        goto done;
    }

    /* Find number of datasets in the file and print their names.
     * 'file_info' will also iterate over all dataset attributes and print their names */
    size_t num_dsets = 0;
    idx = 0;
    if (H5Literate2(group_id, H5_INDEX_NAME, H5_ITER_INC, &idx, file_info, &num_dsets) < 0) {
        fprintf(stderr, "Failed to find number of datasets in 'example2.cdf'\n");
        goto done;
    }
    printf("Number of datasets in the file 'example2.cdf' is %zu\n", num_dsets);

    /* Check existence of zVar_epoch16 dataset */
    exists = H5Lexists(group_id, "zVar_epoch16", H5P_DEFAULT);
    if (exists) {
        if (print_epoch16_dataset(group_id, "zVar_epoch16") < 0) {
            fprintf(stderr, "Failed to read dataset 'zVar_epoch16' from file 'example2.cdf'\n");
            goto done;
        }
    } else {
        fprintf(stderr, "'zVar_epoch16' does not exist in file 'example2.cdf' but it should!\n");
        goto done;
    }

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