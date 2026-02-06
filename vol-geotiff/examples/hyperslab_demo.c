/*
 * Hyperslab Demo
 *
 * Demonstrates efficient region-of-interest extraction using hyperslab selections.
 *
 * Features:
 * - Creating test images with known patterns
 * - Reading specific rectangular regions
 * - Using stride for downsampling
 * - Extracting scanlines from images
 * - Working with both grayscale and RGB data
 * - Validating extracted regions
 */

#include <hdf5.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <geotiffio.h>
#include <xtiffio.h>

#define GEOTIFF_VOL_CONNECTOR_NAME "geotiff_vol_connector"
#define TEST_FILENAME_GRAY         "hyperslab_test_gray.tif"
#define TEST_FILENAME_RGB          "hyperslab_test_rgb.tif"

/* Image dimensions */
#define IMAGE_SIZE 512

/* Quadrant values for validation */
#define QUAD_TL 50   /* Top-left */
#define QUAD_TR 100  /* Top-right */
#define QUAD_BL 150  /* Bottom-left */
#define QUAD_BR 200  /* Bottom-right */

/*
 * Create a grayscale test image with 4 quadrants of different values
 */
int
create_quadrant_test_image(const char *filename)
{
    TIFF         *tif       = NULL;
    unsigned char *scanline = NULL;
    int           ret       = 1;
    const int     half      = IMAGE_SIZE / 2;

    printf("Creating quadrant test image: %s\n", filename);
    printf("  Size: %dx%d grayscale\n", IMAGE_SIZE, IMAGE_SIZE);
    printf("  Pattern: 4 quadrants with values %d, %d, %d, %d\n",
           QUAD_TL, QUAD_TR, QUAD_BL, QUAD_BR);

    tif = XTIFFOpen(filename, "w");
    if (!tif) {
        fprintf(stderr, "ERROR: Failed to create %s\n", filename);
        return 1;
    }

    /* Set TIFF tags */
    if (TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, IMAGE_SIZE) != 1 ||
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, IMAGE_SIZE) != 1 ||
        TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE) != 1 ||
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK) != 1 ||
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG) != 1 ||
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8) != 1 ||
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1) != 1 ||
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, IMAGE_SIZE) != 1) {
        fprintf(stderr, "ERROR: Failed to set TIFF tags\n");
        goto cleanup;
    }

    scanline = (unsigned char *)malloc(IMAGE_SIZE);
    if (!scanline) {
        fprintf(stderr, "ERROR: Failed to allocate memory\n");
        goto cleanup;
    }

    /* Write quadrant pattern */
    for (int row = 0; row < IMAGE_SIZE; row++) {
        for (int col = 0; col < IMAGE_SIZE; col++) {
            unsigned char value;

            if (row < half && col < half)
                value = QUAD_TL;
            else if (row < half && col >= half)
                value = QUAD_TR;
            else if (row >= half && col < half)
                value = QUAD_BL;
            else
                value = QUAD_BR;

            scanline[col] = value;
        }

        if (TIFFWriteScanline(tif, scanline, (uint32_t)row, 0) != 1) {
            fprintf(stderr, "ERROR: Failed to write scanline %d\n", row);
            goto cleanup;
        }
    }

    ret = 0;
    printf("Successfully created test image\n\n");

cleanup:
    if (scanline)
        free(scanline);
    if (tif)
        XTIFFClose(tif);
    return ret;
}

/*
 * Create an RGB test image with color gradient
 */
int
create_rgb_test_image(const char *filename)
{
    TIFF         *tif       = NULL;
    unsigned char *scanline = NULL;
    int           ret       = 1;

    printf("Creating RGB test image: %s\n", filename);
    printf("  Size: %dx%d RGB\n", IMAGE_SIZE, IMAGE_SIZE);
    printf("  Pattern: Color gradient\n");

    tif = XTIFFOpen(filename, "w");
    if (!tif) {
        fprintf(stderr, "ERROR: Failed to create %s\n", filename);
        return 1;
    }

    /* Set TIFF tags for RGB */
    if (TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, IMAGE_SIZE) != 1 ||
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, IMAGE_SIZE) != 1 ||
        TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE) != 1 ||
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB) != 1 ||
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG) != 1 ||
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8) != 1 ||
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3) != 1 ||
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, IMAGE_SIZE) != 1) {
        fprintf(stderr, "ERROR: Failed to set TIFF tags\n");
        goto cleanup;
    }

    scanline = (unsigned char *)malloc(IMAGE_SIZE * 3);
    if (!scanline) {
        fprintf(stderr, "ERROR: Failed to allocate memory\n");
        goto cleanup;
    }

    /* Write color gradient: R increases with col, G increases with row, B constant */
    for (int row = 0; row < IMAGE_SIZE; row++) {
        for (int col = 0; col < IMAGE_SIZE; col++) {
            int idx = col * 3;
            scanline[idx + 0] = (unsigned char)((col * 255) / IMAGE_SIZE);     /* R */
            scanline[idx + 1] = (unsigned char)((row * 255) / IMAGE_SIZE);     /* G */
            scanline[idx + 2] = 128;                                            /* B */
        }

        if (TIFFWriteScanline(tif, scanline, (uint32_t)row, 0) != 1) {
            fprintf(stderr, "ERROR: Failed to write scanline %d\n", row);
            goto cleanup;
        }
    }

    ret = 0;
    printf("Successfully created RGB test image\n\n");

cleanup:
    if (scanline)
        free(scanline);
    if (tif)
        XTIFFClose(tif);
    return ret;
}

int
main(void)
{
    hid_t       vol_id  = H5I_INVALID_HID;
    hid_t       fapl_id = H5I_INVALID_HID;
    char        plugin_path[PATH_MAX];
    char        cwd[PATH_MAX];
    struct stat st;
    int         validation_errors = 0;

    printf("=========================================\n");
    printf("Hyperslab Selection Demo\n");
    printf("=========================================\n\n");

    /* Check if being run from build directory */
    if (stat("./src", &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "ERROR: This program must be run from the build directory\n");
        fprintf(stderr, "Usage: cd build && ./examples/hyperslab_demo\n");
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

    /* Create test files */
    if (create_quadrant_test_image(TEST_FILENAME_GRAY) != 0) {
        return 1;
    }

    if (create_rgb_test_image(TEST_FILENAME_RGB) != 0) {
        return 1;
    }

    /* Register VOL connector */
    vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT);
    if (vol_id < 0) {
        fprintf(stderr, "ERROR: Failed to register VOL connector\n");
        return 1;
    }

    /* Create FAPL and set VOL */
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    if (fapl_id < 0) {
        fprintf(stderr, "ERROR: Failed to create FAPL\n");
        H5VLunregister_connector(vol_id);
        return 1;
    }

    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        fprintf(stderr, "ERROR: Failed to set VOL\n");
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    printf("Registered GeoTIFF VOL connector\n\n");

    /* ===== Example 1: Read center region from grayscale image ===== */
    printf("Example 1: Center Region Extraction\n");
    printf("====================================\n");
    {
        hid_t   file_id = H5I_INVALID_HID;
        hid_t   dset_id = H5I_INVALID_HID;
        hid_t   file_space = H5I_INVALID_HID;
        hid_t   mem_space = H5I_INVALID_HID;
        hsize_t start[2]  = {206, 206};  /* Start at pixel (206, 206) */
        hsize_t count[2]  = {100, 100};  /* Read 100x100 region */
        unsigned char *buffer = NULL;

        printf("Reading 100x100 region from center of image\n");
        printf("Start: [%lu, %lu], Count: [%lu, %lu]\n",
               (unsigned long)start[0], (unsigned long)start[1],
               (unsigned long)count[0], (unsigned long)count[1]);

        file_id = H5Fopen(TEST_FILENAME_GRAY, H5F_ACC_RDONLY, fapl_id);
        if (file_id < 0) {
            fprintf(stderr, "ERROR: Failed to open file\n");
            validation_errors++;
            goto ex1_cleanup;
        }

        dset_id = H5Dopen2(file_id, "/image0", H5P_DEFAULT);
        if (dset_id < 0) {
            fprintf(stderr, "ERROR: Failed to open dataset\n");
            validation_errors++;
            goto ex1_cleanup;
        }

        /* Get file dataspace and select hyperslab */
        file_space = H5Dget_space(dset_id);
        if (file_space < 0) {
            fprintf(stderr, "ERROR: Failed to get dataspace\n");
            validation_errors++;
            goto ex1_cleanup;
        }

        if (H5Sselect_hyperslab(file_space, H5S_SELECT_SET, start, NULL, count, NULL) < 0) {
            fprintf(stderr, "ERROR: Failed to select hyperslab\n");
            validation_errors++;
            goto ex1_cleanup;
        }

        /* Create memory space */
        mem_space = H5Screate_simple(2, count, NULL);
        if (mem_space < 0) {
            fprintf(stderr, "ERROR: Failed to create memory space\n");
            validation_errors++;
            goto ex1_cleanup;
        }

        /* Allocate buffer */
        buffer = (unsigned char *)malloc(count[0] * count[1]);
        if (!buffer) {
            fprintf(stderr, "ERROR: Failed to allocate buffer\n");
            validation_errors++;
            goto ex1_cleanup;
        }

        /* Read the hyperslab */
        if (H5Dread(dset_id, H5T_NATIVE_UCHAR, mem_space, file_space, H5P_DEFAULT, buffer) < 0) {
            fprintf(stderr, "ERROR: Failed to read hyperslab\n");
            validation_errors++;
            goto ex1_cleanup;
        }

        /* Validate: center region should span all 4 quadrants */
        printf("\nValidating extracted region (checking corners):\n");

        /* Check all four corners of extracted region */
        struct {
            int row;
            int col;
            unsigned char expected;
            const char *quad_name;
        } corners[] = {
            {0, 0, QUAD_TL, "top-left"},
            {0, 99, QUAD_TR, "top-right"},
            {99, 0, QUAD_BL, "bottom-left"},
            {99, 99, QUAD_BR, "bottom-right"}
        };

        for (int i = 0; i < 4; i++) {
            int idx = corners[i].row * count[1] + corners[i].col;
            printf("  Corner %s [%d,%d]: value=%u, expected=%u ",
                   corners[i].quad_name, corners[i].row, corners[i].col,
                   buffer[idx], corners[i].expected);

            if (buffer[idx] == corners[i].expected) {
                printf("✓\n");
            } else {
                printf("✗ FAILED\n");
                validation_errors++;
            }
        }

    ex1_cleanup:
        if (buffer) free(buffer);
        if (mem_space >= 0) H5Sclose(mem_space);
        if (file_space >= 0) H5Sclose(file_space);
        if (dset_id >= 0) H5Dclose(dset_id);
        if (file_id >= 0) H5Fclose(file_id);
    }

    printf("\n");

    /* ===== Example 2: Downsampling with stride ===== */
    printf("Example 2: Downsampling with Stride\n");
    printf("====================================\n");
    {
        hid_t   file_id = H5I_INVALID_HID;
        hid_t   dset_id = H5I_INVALID_HID;
        hid_t   file_space = H5I_INVALID_HID;
        hid_t   mem_space = H5I_INVALID_HID;
        hsize_t start[2]  = {0, 0};
        hsize_t stride[2] = {4, 4};       /* Read every 4th pixel */
        hsize_t count[2]  = {128, 128};   /* Results in 128x128 image */
        unsigned char *buffer = NULL;

        printf("Reading every 4th pixel (4x downsampling)\n");
        printf("Start: [%lu, %lu], Stride: [%lu, %lu], Count: [%lu, %lu]\n",
               (unsigned long)start[0], (unsigned long)start[1],
               (unsigned long)stride[0], (unsigned long)stride[1],
               (unsigned long)count[0], (unsigned long)count[1]);

        file_id = H5Fopen(TEST_FILENAME_GRAY, H5F_ACC_RDONLY, fapl_id);
        if (file_id < 0) {
            fprintf(stderr, "ERROR: Failed to open file\n");
            validation_errors++;
            goto ex2_cleanup;
        }

        dset_id = H5Dopen2(file_id, "/image0", H5P_DEFAULT);
        if (dset_id < 0) {
            fprintf(stderr, "ERROR: Failed to open dataset\n");
            validation_errors++;
            goto ex2_cleanup;
        }

        file_space = H5Dget_space(dset_id);
        if (file_space < 0) {
            fprintf(stderr, "ERROR: Failed to get dataspace\n");
            validation_errors++;
            goto ex2_cleanup;
        }

        /* Select with stride */
        if (H5Sselect_hyperslab(file_space, H5S_SELECT_SET, start, stride, count, NULL) < 0) {
            fprintf(stderr, "ERROR: Failed to select hyperslab with stride\n");
            validation_errors++;
            goto ex2_cleanup;
        }

        mem_space = H5Screate_simple(2, count, NULL);
        if (mem_space < 0) {
            fprintf(stderr, "ERROR: Failed to create memory space\n");
            validation_errors++;
            goto ex2_cleanup;
        }

        buffer = (unsigned char *)malloc(count[0] * count[1]);
        if (!buffer) {
            fprintf(stderr, "ERROR: Failed to allocate buffer\n");
            validation_errors++;
            goto ex2_cleanup;
        }

        if (H5Dread(dset_id, H5T_NATIVE_UCHAR, mem_space, file_space, H5P_DEFAULT, buffer) < 0) {
            fprintf(stderr, "ERROR: Failed to read hyperslab\n");
            validation_errors++;
            goto ex2_cleanup;
        }

        printf("\nValidating downsampled image:\n");
        printf("  Sample at [0,0] (from source [0,0]): %u (expected %u) ", buffer[0], QUAD_TL);
        if (buffer[0] == QUAD_TL) {
            printf("✓\n");
        } else {
            printf("✗\n");
            validation_errors++;
        }

        int idx_br = (count[0] - 1) * count[1] + (count[1] - 1);
        printf("  Sample at [127,127] (from source [508,508]): %u (expected %u) ",
               buffer[idx_br], QUAD_BR);
        if (buffer[idx_br] == QUAD_BR) {
            printf("✓\n");
        } else {
            printf("✗\n");
            validation_errors++;
        }

    ex2_cleanup:
        if (buffer) free(buffer);
        if (mem_space >= 0) H5Sclose(mem_space);
        if (file_space >= 0) H5Sclose(file_space);
        if (dset_id >= 0) H5Dclose(dset_id);
        if (file_id >= 0) H5Fclose(file_id);
    }

    printf("\n");

    /* ===== Example 3: Extract scanlines from RGB image ===== */
    printf("Example 3: RGB Scanline Extraction\n");
    printf("===================================\n");
    {
        hid_t   file_id = H5I_INVALID_HID;
        hid_t   dset_id = H5I_INVALID_HID;
        hid_t   file_space = H5I_INVALID_HID;
        hid_t   mem_space = H5I_INVALID_HID;
        hsize_t start[3]  = {100, 0, 0};      /* Start at row 100, column 0, channel 0 */
        hsize_t count[3]  = {50, IMAGE_SIZE, 3}; /* Read 50 rows, all columns, all channels */
        unsigned char *buffer = NULL;

        printf("Reading 50 scanlines (rows 100-149) from RGB image\n");
        printf("Start: [%lu, %lu, %lu], Count: [%lu, %lu, %lu]\n",
               (unsigned long)start[0], (unsigned long)start[1], (unsigned long)start[2],
               (unsigned long)count[0], (unsigned long)count[1], (unsigned long)count[2]);

        file_id = H5Fopen(TEST_FILENAME_RGB, H5F_ACC_RDONLY, fapl_id);
        if (file_id < 0) {
            fprintf(stderr, "ERROR: Failed to open file\n");
            validation_errors++;
            goto ex3_cleanup;
        }

        dset_id = H5Dopen2(file_id, "/image0", H5P_DEFAULT);
        if (dset_id < 0) {
            fprintf(stderr, "ERROR: Failed to open dataset\n");
            validation_errors++;
            goto ex3_cleanup;
        }

        file_space = H5Dget_space(dset_id);
        if (file_space < 0) {
            fprintf(stderr, "ERROR: Failed to get dataspace\n");
            validation_errors++;
            goto ex3_cleanup;
        }

        if (H5Sselect_hyperslab(file_space, H5S_SELECT_SET, start, NULL, count, NULL) < 0) {
            fprintf(stderr, "ERROR: Failed to select hyperslab\n");
            validation_errors++;
            goto ex3_cleanup;
        }

        mem_space = H5Screate_simple(3, count, NULL);
        if (mem_space < 0) {
            fprintf(stderr, "ERROR: Failed to create memory space\n");
            validation_errors++;
            goto ex3_cleanup;
        }

        buffer = (unsigned char *)malloc(count[0] * count[1] * count[2]);
        if (!buffer) {
            fprintf(stderr, "ERROR: Failed to allocate buffer\n");
            validation_errors++;
            goto ex3_cleanup;
        }

        if (H5Dread(dset_id, H5T_NATIVE_UCHAR, mem_space, file_space, H5P_DEFAULT, buffer) < 0) {
            fprintf(stderr, "ERROR: Failed to read hyperslab\n");
            validation_errors++;
            goto ex3_cleanup;
        }

        printf("\nValidating RGB values:\n");

        /* First pixel of first row (row 100, col 0) */
        unsigned char r0 = buffer[0];
        unsigned char g0 = buffer[1];
        unsigned char b0 = buffer[2];
        unsigned char expected_r0 = (unsigned char)((0 * 255) / IMAGE_SIZE);
        unsigned char expected_g0 = (unsigned char)((100 * 255) / IMAGE_SIZE);

        printf("  Row 100, Col 0: RGB=(%u, %u, %u), expected=(%u, %u, 128) ",
               r0, g0, b0, expected_r0, expected_g0);
        if (r0 == expected_r0 && g0 == expected_g0 && b0 == 128) {
            printf("✓\n");
        } else {
            printf("✗\n");
            validation_errors++;
        }

        /* Last pixel of last row (row 149, col 511) */
        int last_idx = (count[0] - 1) * count[1] * 3 + (count[1] - 1) * 3;
        unsigned char r_last = buffer[last_idx];
        unsigned char g_last = buffer[last_idx + 1];
        unsigned char b_last = buffer[last_idx + 2];
        unsigned char expected_r_last = (unsigned char)(((IMAGE_SIZE - 1) * 255) / IMAGE_SIZE);
        unsigned char expected_g_last = (unsigned char)((149 * 255) / IMAGE_SIZE);

        printf("  Row 149, Col 511: RGB=(%u, %u, %u), expected=(%u, %u, 128) ",
               r_last, g_last, b_last, expected_r_last, expected_g_last);
        if (r_last == expected_r_last && g_last == expected_g_last && b_last == 128) {
            printf("✓\n");
        } else {
            printf("✗\n");
            validation_errors++;
        }

    ex3_cleanup:
        if (buffer) free(buffer);
        if (mem_space >= 0) H5Sclose(mem_space);
        if (file_space >= 0) H5Sclose(file_space);
        if (dset_id >= 0) H5Dclose(dset_id);
        if (file_id >= 0) H5Fclose(file_id);
    }

    printf("\n");

    /* Summary */
    printf("========================================\n");
    printf("Demo Complete\n");
    printf("========================================\n");

    if (validation_errors == 0) {
        printf("All validations passed!\n");
    }
    else {
        fprintf(stderr, "\n%d validation error(s) occurred\n", validation_errors);
    }

    /* Cleanup */
    if (fapl_id >= 0)
        H5Pclose(fapl_id);
    if (vol_id >= 0)
        H5VLunregister_connector(vol_id);

    return (validation_errors == 0) ? 0 : 1;
}
