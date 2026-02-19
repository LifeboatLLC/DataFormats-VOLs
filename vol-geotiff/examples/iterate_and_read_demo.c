/*
 * Iterate and Read Demo
 *
 * Demonstrates iterating through all images in a real multi-image
 * GeoTIFF file and reading the raster data from each one.
 *
 * Uses byte_with_ovr.tif from the test directory, which contains
 * a base image plus two overview (pyramid) levels.
 *
 * Features:
 * - Discovering number of images via num_images attribute
 * - Iterating through all images
 * - Reading full raster data from each image
 * - Printing summary statistics (min, max, mean)
 */

#include <hdf5.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define GEOTIFF_VOL_CONNECTOR_NAME "geotiff_vol_connector"
#define TEST_FILENAME              "../test/byte_with_ovr.tif"

int
main(void)
{
    hid_t       vol_id  = H5I_INVALID_HID;
    hid_t       fapl_id = H5I_INVALID_HID;
    hid_t       file_id = H5I_INVALID_HID;
    hid_t       attr_id = H5I_INVALID_HID;
    char        plugin_path[PATH_MAX];
    char        cwd[PATH_MAX];
    struct stat st;
    uint64_t    num_images = 0;

    printf("=========================================\n");
    printf("Iterate and Read Demo\n");
    printf("=========================================\n\n");

    /* Check if being run from build directory */
    if (stat("./src", &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "ERROR: This program must be run from the build directory\n");
        fprintf(stderr, "Usage: cd build && ./examples/iterate_and_read_demo\n");
        return 1;
    }

    /* Verify the test file exists */
    if (stat(TEST_FILENAME, &st) != 0) {
        fprintf(stderr, "ERROR: Test file not found: %s\n", TEST_FILENAME);
        fprintf(stderr, "Make sure you are running from the build directory\n");
        return 1;
    }

    /* Set HDF5_PLUGIN_PATH */
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        fprintf(stderr, "ERROR: Failed to get current working directory\n");
        return 1;
    }

    int ret = snprintf(plugin_path, sizeof(plugin_path), "%s/src", cwd);
    if (ret < 0 || (size_t)ret >= sizeof(plugin_path)) {
        fprintf(stderr, "ERROR: Path too long\n");
        return 1;
    }

    if (setenv("HDF5_PLUGIN_PATH", plugin_path, 1) != 0) {
        fprintf(stderr, "ERROR: Failed to set HDF5_PLUGIN_PATH\n");
        return 1;
    }

    /* Register VOL connector */
    vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT);
    if (vol_id < 0) {
        fprintf(stderr, "ERROR: Failed to register GeoTIFF VOL connector\n");
        return 1;
    }
    printf("Registered GeoTIFF VOL connector\n");

    /* Create FAPL and set VOL connector */
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    if (fapl_id < 0) {
        fprintf(stderr, "ERROR: Failed to create FAPL\n");
        H5VLunregister_connector(vol_id);
        return 1;
    }

    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        fprintf(stderr, "ERROR: Failed to set VOL connector\n");
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    /* Open the test file */
    file_id = H5Fopen(TEST_FILENAME, H5F_ACC_RDONLY, fapl_id);
    if (file_id < 0) {
        fprintf(stderr, "ERROR: Failed to open test file\n");
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }
    printf("Opened file: %s\n\n", TEST_FILENAME);

    /* Discover number of images */
    attr_id = H5Aopen(file_id, "num_images", H5P_DEFAULT);
    if (attr_id < 0) {
        fprintf(stderr, "ERROR: Failed to open num_images attribute\n");
        goto cleanup;
    }

    if (H5Aread(attr_id, H5T_NATIVE_UINT64, &num_images) < 0) {
        fprintf(stderr, "ERROR: Failed to read num_images attribute\n");
        H5Aclose(attr_id);
        goto cleanup;
    }
    H5Aclose(attr_id);

    printf("Found %lu image(s) in file\n\n", (unsigned long)num_images);

    /* Iterate through each image and read its data */
    for (uint64_t i = 0; i < num_images; i++) {
        hid_t          dset_id  = H5I_INVALID_HID;
        hid_t          space_id = H5I_INVALID_HID;
        hid_t          type_id  = H5I_INVALID_HID;
        char           dset_name[32];
        hsize_t        dims[2];
        int            ndims;
        unsigned char *data = NULL;
        size_t         npixels;
        unsigned char  min_val, max_val;
        double         sum;

        snprintf(dset_name, sizeof(dset_name), "/image%lu", (unsigned long)i);

        printf("--- Image %lu: %s ---\n", (unsigned long)i, dset_name);

        /* Open dataset */
        dset_id = H5Dopen2(file_id, dset_name, H5P_DEFAULT);
        if (dset_id < 0) {
            fprintf(stderr, "ERROR: Failed to open dataset %s\n", dset_name);
            continue;
        }

        /* Get dataspace and dimensions */
        space_id = H5Dget_space(dset_id);
        if (space_id < 0) {
            fprintf(stderr, "ERROR: Failed to get dataspace\n");
            H5Dclose(dset_id);
            continue;
        }

        ndims = H5Sget_simple_extent_ndims(space_id);
        H5Sget_simple_extent_dims(space_id, dims, NULL);
        printf("  Dimensions: %lu x %lu (%dD)\n", (unsigned long)dims[0], (unsigned long)dims[1],
               ndims);

        /* Get datatype info */
        type_id = H5Dget_type(dset_id);
        if (type_id >= 0) {
            printf("  Datatype size: %lu byte(s)\n", (unsigned long)H5Tget_size(type_id));
            H5Tclose(type_id);
        }

        /* Read the full raster data */
        npixels = (size_t)(dims[0] * dims[1]);
        data    = (unsigned char *)malloc(npixels);
        if (!data) {
            fprintf(stderr, "ERROR: Failed to allocate %zu bytes\n", npixels);
            H5Sclose(space_id);
            H5Dclose(dset_id);
            continue;
        }

        if (H5Dread(dset_id, H5T_NATIVE_UCHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, data) < 0) {
            fprintf(stderr, "ERROR: Failed to read dataset %s\n", dset_name);
            free(data);
            H5Sclose(space_id);
            H5Dclose(dset_id);
            continue;
        }

        /* Compute summary statistics */
        min_val = data[0];
        max_val = data[0];
        sum     = 0.0;

        for (size_t p = 0; p < npixels; p++) {
            if (data[p] < min_val)
                min_val = data[p];
            if (data[p] > max_val)
                max_val = data[p];
            sum += data[p];
        }

        printf("  Pixels read: %zu\n", npixels);
        printf("  Min: %u  Max: %u  Mean: %.1f\n", min_val, max_val, sum / (double)npixels);

        /* Print first few pixel values as a preview */
        printf("  First 10 values:");
        for (size_t p = 0; p < 10 && p < npixels; p++) {
            printf(" %u", data[p]);
        }
        printf("\n\n");

        free(data);
        H5Sclose(space_id);
        H5Dclose(dset_id);
    }

    printf("=========================================\n");
    printf("Done - successfully read all %lu image(s)\n", (unsigned long)num_images);
    printf("=========================================\n");

cleanup:
    if (file_id >= 0)
        H5Fclose(file_id);
    if (fapl_id >= 0)
        H5Pclose(fapl_id);
    if (vol_id >= 0)
        H5VLunregister_connector(vol_id);

    return 0;
}
