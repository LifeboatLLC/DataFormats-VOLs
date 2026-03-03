#include "grib2_vol_connector.h"
#include <hdf5.h>
/*
#define GRIB2_VOL_PLUGIN_PATH "<path_to_the connector>"
*/

typedef struct {
    hid_t id;
    int count;
} iter_info_t;

/* Callback function to be executed for each attribute */
herr_t attr_info(hid_t loc_id, const char *name, const H5A_info_t *ainfo, void *opdata)
{
    hid_t attr_id;
    hid_t type_id;
    char type_name[100];
    iter_info_t *op_data = (iter_info_t *) opdata;

    /* Open the attribute by name */
    attr_id = H5Aopen(op_data->id, name, H5P_DEFAULT);
    if (attr_id < 0) {
        fprintf(stderr, "  [attr] Failed to open attribute '%s'\n", name);
        return 0; /* continue */
    }

    /* Get the datatype */
    type_id = H5Aget_type(attr_id);
    // printf ("type_id %lld", type_id);
    if (type_id < 0) {
        printf(" Found unsupported datatype for an attribute %s \n", name);
        return 0;
    }
    /* Identify the datatype class (simplified example) */
    H5T_class_t t_class = H5Tget_class(type_id);
    if (t_class == H5T_INTEGER)
        sprintf(type_name, "Integer");
    else if (t_class == H5T_FLOAT)
        sprintf(type_name, "Float");
    else if (t_class == H5T_STRING)
        sprintf(type_name, "String");
    else
        sprintf(type_name, "Other");

    printf("Attribute Name: %s, Datatype: %s\n", name, type_name);
    op_data->count++;

    /* Close resources */
    H5Tclose(type_id);
    H5Aclose(attr_id);
    return 0; /* Return 0 to continue iterationa */
}

int main()
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;
    hid_t group_id = H5I_INVALID_HID;
    hid_t dset_id = H5I_INVALID_HID;
    hid_t space_id = H5I_INVALID_HID;
    hid_t type_id = H5I_INVALID_HID;
    double *data;
    int ndims = 0;
    hsize_t dims[1] = {-1};
    iter_info_t op_data;
    op_data.id = H5I_INVALID_HID;
    op_data.count = 0;

    /*     Tell the library where to find the GRIB2 VOL connector library */
    /*      (May be skipped if HDF5_VOL_CONNECTOR/HDF5_PLUGIN_PATH are defined in env) */
    /*       H5PLappend(GRIB2_VOL_PLUGIN_PATH); */

    /* Register the GRIB2 VOL connector */
    vol_id = H5VLregister_connector_by_name(GRIB2_VOL_CONNECTOR_NAME, H5P_DEFAULT);

    /* Create file access property list and set VOL connector */
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    H5Pset_vol(fapl_id, vol_id, NULL);

    /* Open GRIB2 file */
    file_id = H5Fopen("example-1.grib2", H5F_ACC_RDONLY, fapl_id);

    group_id = H5Gopen2(file_id, "message_1", H5P_DEFAULT);

    /* Open dataset 'values' and read it.
       Dataset name is always 'lon', or 'lat', or'values' and
       it is one-dimensionaal.
    */
    dset_id = H5Dopen(group_id, "lon", H5P_DEFAULT);

    space_id = H5Dget_space(dset_id);
    ndims = H5Sget_simple_extent_ndims(space_id);
    H5Sget_simple_extent_dims(space_id, dims, NULL);

    type_id = H5Dget_type(dset_id);

    /* It is safe to use file datatype to read data back since
       the connector always returns memory type. It is H5T_NATIVE_DOUBLE
       for datasets. Currently datatype conversion and space subsetting
       are not available.
    */
    data = (double *) malloc((size_t) dims[0] * sizeof(*data));
    H5Dread(dset_id, type_id, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);

    for (int i = 0; i < dims[0]; i++) {
        printf(" %f \n", data[i]);
    }
    printf("Iterating over attributes:\n");
    /* Iterate over attributes */
    hsize_t idx = 0;
    op_data.id = group_id;
    H5Aiterate2(group_id, H5_INDEX_NAME, H5_ITER_INC, &idx, attr_info, &op_data);

    free(data);
    H5Tclose(type_id);
    H5Sclose(space_id);
    H5Dclose(dset_id);
    H5Gclose(group_id);
    H5Fclose(file_id);
    H5Pclose(fapl_id);

    H5VLunregister_connector(vol_id);

    return 0;
}
