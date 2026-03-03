/*
 * TIFF Metadata Demo
 *
 * Demonstrates reading TIFF tags as HDF5 attributes through the VOL connector.
 * Creates a small test TIFF with rich metadata, then reads it back via HDF5.
 *
 * Features:
 * - Reading string attributes (IMAGEDESCRIPTION, etc.)
 * - Reading numeric attributes (dimensions, compression, format, etc.)
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
#define TEST_FILENAME              "metadata_test.tif"

/* Forward declaration - test file creation is at end of file */
int create_metadata_rich_tiff(const char *filename);

/*
 * Read and print a string attribute. Returns 0 on success.
 * The VOL connector returns variable-length strings as char* pointers.
 */
static int
print_string_attr(hid_t obj_id, const char *attr_name)
{
    hid_t  attr_id = H5Aopen(obj_id, attr_name, H5P_DEFAULT);
    if (attr_id < 0) {
        printf("  %-20s (not found)\n", attr_name);
        return 1;
    }

    char *value = NULL;
    H5Aread(attr_id, H5T_C_S1, &value);
    printf("  %-20s \"%s\"\n", attr_name, value ? value : "(null)");

    H5Aclose(attr_id);
    return 0;
}

/*
 * Read and print a uint16 attribute.
 */
static int
print_uint16_attr(hid_t obj_id, const char *attr_name)
{
    hid_t attr_id = H5Aopen(obj_id, attr_name, H5P_DEFAULT);
    if (attr_id < 0) {
        printf("  %-20s (not found)\n", attr_name);
        return 1;
    }

    uint16_t value;
    H5Aread(attr_id, H5T_NATIVE_UINT16, &value);
    printf("  %-20s %u\n", attr_name, (unsigned)value);

    H5Aclose(attr_id);
    return 0;
}

/*
 * Read and print a uint32 attribute.
 */
static int
print_uint32_attr(hid_t obj_id, const char *attr_name)
{
    hid_t attr_id = H5Aopen(obj_id, attr_name, H5P_DEFAULT);
    if (attr_id < 0) {
        printf("  %-20s (not found)\n", attr_name);
        return 1;
    }

    uint32_t value;
    H5Aread(attr_id, H5T_NATIVE_UINT32, &value);
    printf("  %-20s %u\n", attr_name, (unsigned)value);

    H5Aclose(attr_id);
    return 0;
}


int
main(void)
{
    hid_t       vol_id  = H5I_INVALID_HID;
    hid_t       fapl_id = H5I_INVALID_HID;
    hid_t       file_id = H5I_INVALID_HID;
    char        plugin_path[PATH_MAX];
    char        cwd[PATH_MAX];
    struct stat st;

    printf("=========================================\n");
    printf("TIFF Metadata Demo\n");
    printf("=========================================\n\n");

    /* Check if being run from build directory */
    if (stat("./src", &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "ERROR: This program must be run from the build directory\n");
        fprintf(stderr, "Usage: cd build && ./examples/metadata_demo\n");
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

    /* Create test file */
    if (create_metadata_rich_tiff(TEST_FILENAME) != 0)
        return 1;

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

    /* Open the test file and the image dataset */
    file_id = H5Fopen(TEST_FILENAME, H5F_ACC_RDONLY, fapl_id);
    if (file_id < 0) {
        fprintf(stderr, "ERROR: Failed to open test file\n");
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    /* Read string attributes (on file object) */
    printf("--- String Attributes ---\n");
    print_string_attr(file_id, "IMAGEDESCRIPTION");

    print_string_attr(file_id, "DATETIME");
    printf("\n");

    /* Read dimension attributes */
    printf("--- Dimension Attributes ---\n");
    print_uint32_attr(file_id, "IMAGEWIDTH");
    print_uint32_attr(file_id, "IMAGELENGTH");
    printf("\n");

    /* Read format/layout attributes */
    printf("--- Format Attributes ---\n");
    print_uint16_attr(file_id, "COMPRESSION");
    print_uint16_attr(file_id, "PHOTOMETRIC");
    print_uint16_attr(file_id, "SAMPLESPERPIXEL");
    print_uint16_attr(file_id, "BITSPERSAMPLE");
    print_uint16_attr(file_id, "PLANARCONFIG");
    print_uint16_attr(file_id, "ORIENTATION");
    print_uint32_attr(file_id, "ROWSPERSTRIP");
    printf("\n");

    printf("=========================================\n");
    printf("Done\n");
    printf("=========================================\n");

    /* Cleanup */
    H5Fclose(file_id);
    H5Pclose(fapl_id);
    H5VLunregister_connector(vol_id);

    return 0;
}

/* ================================================================
 * Test file creation helper (not the focus of this demo)
 * Creates a 128x128 RGB checkerboard with rich metadata tags.
 * ================================================================ */
int
create_metadata_rich_tiff(const char *filename)
{
    TIFF         *tif     = NULL;
    unsigned char *scanline = NULL;
    const int     width   = 128;
    const int     height  = 128;

    printf("Creating test file: %s (%dx%d RGB)\n", filename, width, height);

    tif = XTIFFOpen(filename, "w");
    if (!tif) {
        fprintf(stderr, "ERROR: Failed to create %s\n", filename);
        return 1;
    }

    /* Basic image tags */
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, height);

    TIFFSetField(tif, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);

    /* Descriptive string tags */
    TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION, "Test image for metadata demonstration");
    TIFFSetField(tif, TIFFTAG_DATETIME, "2026:02:05 12:00:00");

    /* Write checkerboard pattern */
    scanline = (unsigned char *)malloc((size_t)(width * 3));
    if (!scanline) {
        fprintf(stderr, "ERROR: Failed to allocate scanline buffer\n");
        XTIFFClose(tif);
        return 1;
    }

    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            int is_white = ((row / 16) + (col / 16)) % 2;
            int idx      = col * 3;
            unsigned char val = is_white ? 255 : 64;
            scanline[idx + 0] = val;
            scanline[idx + 1] = val;
            scanline[idx + 2] = val;
        }
        TIFFWriteScanline(tif, scanline, (uint32_t)row, 0);
    }

    free(scanline);
    XTIFFClose(tif);
    printf("Created successfully\n\n");
    return 0;
}
