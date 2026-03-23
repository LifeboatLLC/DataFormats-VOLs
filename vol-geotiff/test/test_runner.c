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
 * Test program for GeoTIFF VOL connector
 * Tests reading a GeoTIFF file through HDF5 interface
 */

#include "test_runner.h"
#include "geotiff_vol_connector.h"
#include "test_helpers.h"
#include <geotiffio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <xtiffio.h>

/* Test functions */
int main(int argc, char **argv)
{
    int num_failures = 0;
    const char *test_name = (argc > 1) ? argv[1] : "all";
    int run_all = (strcmp(test_name, "all") == 0);

    /* Handle real_file_test with filename parameter */
    if (strcmp(test_name, "real_file_test") == 0) {
        if (argc < 3) {
            printf("Error: real_file_test requires a filename argument\n");
            printf("Usage: %s real_file_test <filename>\n", argv[0]);
            return 1;
        }
        const char *filename = argv[2];
        printf("Running real file test on: %s\n\n", filename);
        return RealFileComprehensiveTest(filename) != 0 ? 1 : 0;
    }

    /* Generate test files first (always needed) */
    printf("Generating test GeoTIFF files...\n");
    if (CreateGrayscaleGeoTIFF(GRAYSCALE_FILENAME) != 0) {
        printf("Failed to generate grayscale test file\n");
        goto error;
    }
    if (CreateRGBGeoTIFF(RGB_FILENAME) != 0) {
        printf("Failed to generate RGB test file\n");
        goto error;
    }
    printf("Test files generated successfully\n\n");

    /* Run plugin-handling tests */
    if (run_all || strcmp(test_name, "test_getters") == 0)
        num_failures += (test_getters() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "test_multiple_registration") == 0)
        num_failures += (test_multiple_registration() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "test_registration_by_name") == 0)
        num_failures += (test_registration_by_name() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "test_registration_by_value") == 0)
        num_failures += (test_registration_by_value() != 0 ? 1 : 0);

    /* Run GeoTIFF functionality tests (each test manages its own connector registration) */
    if (run_all || strcmp(test_name, "open_grayscale") == 0)
        num_failures += (OpenGeoTIFFTest(GRAYSCALE_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "open_rgb") == 0)
        num_failures += (OpenGeoTIFFTest(RGB_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "read_grayscale") == 0)
        num_failures += (ReadGeoTIFFTest(GRAYSCALE_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "read_rgb") == 0)
        num_failures += (ReadGeoTIFFTest(RGB_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "band_read_rgb") == 0)
        num_failures += (BandReadGeoTIFFTest(RGB_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "point_read_grayscale") == 0)
        num_failures += (PointReadGeoTIFFTest(GRAYSCALE_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "point_read_rgb") == 0)
        num_failures += (PointReadGeoTIFFTest(RGB_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "multi_image_read") == 0)
        num_failures += (MultiImageReadGeoTIFFTest() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "dataset_error_handling") == 0)
        num_failures += (DatasetErrorHandlingTest(GRAYSCALE_FILENAME) != 0 ? 1 : 0);

    /* Datatype conversion tests */
    hid_t test_types[] = {H5T_NATIVE_UCHAR, H5T_NATIVE_USHORT, H5T_NATIVE_UINT, H5T_NATIVE_UINT64,
                          H5T_NATIVE_CHAR,  H5T_NATIVE_SHORT,  H5T_NATIVE_INT,  H5T_NATIVE_INT64,
                          H5T_NATIVE_FLOAT, H5T_NATIVE_DOUBLE};

    const char *type_names[] = {"UCHAR", "USHORT", "UINT",  "UINT64", "CHAR",
                                "SHORT", "INT",    "INT64", "FLOAT",  "DOUBLE"};

    int num_types = sizeof(test_types) / sizeof(test_types[0]);

    for (int i = 0; i < num_types; i++) {
        for (int j = 0; j < num_types; j++) {
            /* Skip same-type "conversions" */
            if (i == j)
                continue;

            /* Build test name: dtype_conv_MEMTYPE_FILETYPE */
            char dtype_test_name[128];
            snprintf(dtype_test_name, sizeof(dtype_test_name), "dtype_conv_%s_%s", type_names[i],
                     type_names[j]);

            if (run_all || strcmp(test_name, dtype_test_name) == 0) {
                num_failures += (DatatypeConversionTest(test_types[i], test_types[j], type_names[i],
                                                        type_names[j]) != 0
                                     ? 1
                                     : 0);
            }
        }
    }
    if (run_all || strcmp(test_name, "link_exists_rgb") == 0)
        num_failures += (LinkExistsTest(RGB_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "multi_image_link_exists") == 0)
        num_failures += (MultiImageLinkExistsTest() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "link_iterate_rgb") == 0)
        num_failures += (LinkIterateTest(RGB_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "unsupported_features") == 0)
        num_failures += (UnsupportedFeaturesTest() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "group_get_info") == 0)
        num_failures += (GroupGetInfoTest() != 0 ? 1 : 0);

    /* Tiled TIFF tests */
    if (run_all || strcmp(test_name, "tiled_read_grayscale") == 0)
        num_failures += (TiledTIFFReadTest("test_tiled_grayscale.tif", 0) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "tiled_read_rgb") == 0)
        num_failures += (TiledTIFFReadTest("test_tiled_rgb.tif", 1) != 0 ? 1 : 0);

    /* Coordinates attribute tests */
    if (run_all || strcmp(test_name, "coordinates_attr_geographic") == 0)
        num_failures += (CoordinatesAttributeGeographicTest(NULL) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "coordinates_attr_projected") == 0)
        num_failures += (CoordinatesAttributeProjectedTest(NULL) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "coordinates_attr_plain_tiff") == 0)
        num_failures += (CoordinatesAttributePlainTIFFTest() != 0 ? 1 : 0);

    /* Reference counting tests */
    if (run_all || strcmp(test_name, "refcount_close_file_before_dataset") == 0)
        num_failures += (RefCountCloseFileBeforeDatasetTest(GRAYSCALE_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "refcount_close_dataset_before_attribute") == 0)
        num_failures += (RefCountCloseDatasetBeforeAttributeTest() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "refcount_close_file_with_multiple_children") == 0)
        num_failures +=
            (RefCountCloseFileWithMultipleChildrenTest(GRAYSCALE_FILENAME) != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "num_images_attribute") == 0)
        num_failures += (NumImagesAttributeTest() != 0 ? 1 : 0);

    /* TIFF tag attribute tests */
    if (run_all || strcmp(test_name, "tiff_tag_attributes") == 0)
        num_failures += (TiffTagAttributeReadTest() != 0 ? 1 : 0);

    if (num_failures == 0) {
        printf("\n%s: All tests completed successfully\n", test_name);
    } else {
        printf("\n%s: %d test(s) failed\n", test_name, num_failures);
    }

    return (num_failures == 0 ? 0 : 1);
error:
    printf("Error occurred during the testing process, aborting\n");
    return 1;
}
