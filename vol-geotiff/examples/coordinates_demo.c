/*
 * Coordinates Demo
 *
 * Demonstrates reading geographic coordinates from a georeferenced GeoTIFF
 * using the "coordinates" attribute on the dataset.
 *
 * Features:
 * - Reading the coordinates compound attribute (lon, lat)
 * - Inspecting the compound datatype structure
 * - Printing coordinate values for sample pixels
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
#define TEST_FILENAME              "coordinates_test.tif"

/* Forward declaration - test file creation is at end of file */
int create_coordinate_test_tiff(const char *filename);

/* Coordinate structure matching the HDF5 compound type */
typedef struct {
    double lon;
    double lat;
} coord_t;

int
main(void)
{
    hid_t       vol_id        = H5I_INVALID_HID;
    hid_t       fapl_id       = H5I_INVALID_HID;
    hid_t       file_id       = H5I_INVALID_HID;
    hid_t       dset_id       = H5I_INVALID_HID;
    hid_t       attr_id       = H5I_INVALID_HID;
    hid_t       coord_type_id = H5I_INVALID_HID;
    hid_t       attr_space     = H5I_INVALID_HID;
    char        plugin_path[PATH_MAX];
    char        cwd[PATH_MAX];
    struct stat st;
    coord_t    *coords = NULL;

    printf("=========================================\n");
    printf("Coordinates Demo\n");
    printf("=========================================\n\n");

    /* Check if being run from build directory */
    if (stat("./src", &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "ERROR: This program must be run from the build directory\n");
        fprintf(stderr, "Usage: cd build && ./examples/coordinates_demo\n");
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
    if (create_coordinate_test_tiff(TEST_FILENAME) != 0)
        return 1;

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

    /* Open file and dataset */
    file_id = H5Fopen(TEST_FILENAME, H5F_ACC_RDONLY, fapl_id);
    if (file_id < 0) {
        fprintf(stderr, "ERROR: Failed to open file\n");
        H5Pclose(fapl_id);
        H5VLunregister_connector(vol_id);
        return 1;
    }

    dset_id = H5Dopen2(file_id, "/image0", H5P_DEFAULT);
    if (dset_id < 0) {
        fprintf(stderr, "ERROR: Failed to open dataset\n");
        goto cleanup;
    }
    printf("Opened: %s -> /image0\n\n", TEST_FILENAME);

    /* Open the coordinates attribute on the dataset */
    attr_id = H5Aopen(dset_id, "coordinates", H5P_DEFAULT);
    if (attr_id < 0) {
        fprintf(stderr, "ERROR: Failed to open coordinates attribute\n");
        goto cleanup;
    }

    /* Inspect the compound datatype */
    coord_type_id = H5Aget_type(attr_id);
    if (coord_type_id < 0 || H5Tget_class(coord_type_id) != H5T_COMPOUND) {
        fprintf(stderr, "ERROR: coordinates attribute is not a compound type\n");
        goto cleanup;
    }

    printf("--- Compound Datatype ---\n");
    printf("  Size: %lu bytes\n", (unsigned long)H5Tget_size(coord_type_id));
    printf("  Fields: %d\n", H5Tget_nmembers(coord_type_id));
    for (int i = 0; i < H5Tget_nmembers(coord_type_id); i++) {
        char *name = H5Tget_member_name(coord_type_id, (unsigned)i);
        printf("    [%d] \"%s\" (offset %lu)\n", i, name,
               (unsigned long)H5Tget_member_offset(coord_type_id, (unsigned)i));
        H5free_memory(name);
    }
    printf("\n");

    /* Get dataspace dimensions */
    attr_space = H5Aget_space(attr_id);
    hsize_t dims[2];
    H5Sget_simple_extent_dims(attr_space, dims, NULL);
    printf("--- Dataspace ---\n");
    printf("  Shape: [%lu, %lu] (%lu coordinate pairs)\n", (unsigned long)dims[0],
           (unsigned long)dims[1], (unsigned long)(dims[0] * dims[1]));
    printf("\n");

    /* Read all coordinates */
    size_t npixels = (size_t)(dims[0] * dims[1]);
    coords = (coord_t *)malloc(npixels * sizeof(coord_t));
    if (!coords) {
        fprintf(stderr, "ERROR: Failed to allocate coordinate buffer\n");
        goto cleanup;
    }

    if (H5Aread(attr_id, coord_type_id, coords) < 0) {
        fprintf(stderr, "ERROR: Failed to read coordinates\n");
        goto cleanup;
    }

    /* Print coordinates at the four corners and center */
    int height = (int)dims[0];
    int width  = (int)dims[1];

    struct {
        const char *label;
        int         row;
        int         col;
    } sample_points[] = {
        {"Top-left",     0,          0},
        {"Top-right",    0,          width - 1},
        {"Center",       height / 2, width / 2},
        {"Bottom-left",  height - 1, 0},
        {"Bottom-right", height - 1, width - 1},
    };
    int num_points = sizeof(sample_points) / sizeof(sample_points[0]);

    printf("--- Sample Coordinates ---\n");
    for (int i = 0; i < num_points; i++) {
        int idx = sample_points[i].row * width + sample_points[i].col;
        printf("  %-14s [%3d, %3d]  lon=%.6f  lat=%.6f\n", sample_points[i].label,
               sample_points[i].row, sample_points[i].col, coords[idx].lon, coords[idx].lat);
    }
    printf("\n");

    printf("=========================================\n");
    printf("Done\n");
    printf("=========================================\n");

cleanup:
    free(coords);
    if (attr_space >= 0)
        H5Sclose(attr_space);
    if (coord_type_id >= 0)
        H5Tclose(coord_type_id);
    if (attr_id >= 0)
        H5Aclose(attr_id);
    if (dset_id >= 0)
        H5Dclose(dset_id);
    if (file_id >= 0)
        H5Fclose(file_id);
    if (fapl_id >= 0)
        H5Pclose(fapl_id);
    if (vol_id >= 0)
        H5VLunregister_connector(vol_id);

    return 0;
}

/* ================================================================
 * Test file creation helper (not the focus of this demo)
 * Creates a small 100x100 georeferenced GeoTIFF over SF Bay Area.
 * ================================================================ */
#define IMAGE_WIDTH  100
#define IMAGE_HEIGHT 100
#define ORIGIN_LON   -122.5
#define ORIGIN_LAT   37.8
#define PIXEL_SIZE   0.0001 /* ~11 meters at this latitude */

int
create_coordinate_test_tiff(const char *filename)
{
    TIFF         *tif  = NULL;
    GTIF         *gtif = NULL;
    unsigned char *scanline = NULL;

    printf("Creating test file: %s (%dx%d, SF Bay Area)\n", filename, IMAGE_WIDTH, IMAGE_HEIGHT);

    tif = XTIFFOpen(filename, "w");
    if (!tif) {
        fprintf(stderr, "ERROR: Failed to create %s\n", filename);
        return 1;
    }

    gtif = GTIFNew(tif);
    if (!gtif) {
        fprintf(stderr, "ERROR: Failed to create GeoTIFF handle\n");
        XTIFFClose(tif);
        return 1;
    }

    /* Basic TIFF tags */
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, IMAGE_WIDTH);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, IMAGE_HEIGHT);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, IMAGE_HEIGHT);

    /* GeoTIFF georeferencing */
    const double tiepoints[6] = {0, 0, 0, ORIGIN_LON, ORIGIN_LAT, 0.0};
    const double pixscale[3]  = {PIXEL_SIZE, PIXEL_SIZE, 0.0};
    TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
    TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);

    /* GeoTIFF keys for WGS84 */
    GTIFKeySet(gtif, GTModelTypeGeoKey, TYPE_SHORT, 1, ModelGeographic);
    GTIFKeySet(gtif, GTRasterTypeGeoKey, TYPE_SHORT, 1, RasterPixelIsArea);
    GTIFKeySet(gtif, GeographicTypeGeoKey, TYPE_SHORT, 1, GCS_WGS_84);

    /* Write gradient pattern */
    scanline = (unsigned char *)malloc(IMAGE_WIDTH);
    if (!scanline) {
        fprintf(stderr, "ERROR: Failed to allocate memory\n");
        GTIFWriteKeys(gtif);
        GTIFFree(gtif);
        XTIFFClose(tif);
        return 1;
    }

    for (int row = 0; row < IMAGE_HEIGHT; row++) {
        for (int col = 0; col < IMAGE_WIDTH; col++)
            scanline[col] = (unsigned char)((row + col) % 256);
        TIFFWriteScanline(tif, scanline, (uint32_t)row, 0);
    }

    free(scanline);
    GTIFWriteKeys(gtif);
    GTIFFree(gtif);
    XTIFFClose(tif);
    printf("Created successfully\n\n");
    return 0;
}
