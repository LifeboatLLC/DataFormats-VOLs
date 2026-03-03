/*
 * Benchmark
 *
 * Compares open and read times between raw libtiff and the GeoTIFF VOL connector
 * for a selection of real TIFF test files, and verifies data integrity by comparing
 * the bytes read through each path.
 */

#include <hdf5.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include <tiffio.h>

#define GEOTIFF_VOL_CONNECTOR_NAME "geotiff_vol_connector"

/* Test files from the test directory (run from build/) */
static const char *test_files[] = {
    "../test/byte.tif",          "../test/utmsmall.tif",
    "../test/byte_with_ovr.tif", "../test/stefan_full_rgba.tif",
    "../test/PIA26616.tif",
};
static const int num_files = sizeof(test_files) / sizeof(test_files[0]);

/* Suppress libtiff warnings (GeoTIFF tags are unknown to plain libtiff) */
static void tiff_warning_handler(const char *module, const char *fmt, va_list ap)
{
    (void) module;
    (void) fmt;
    (void) ap;
}

static double elapsed_us(struct timespec *start, struct timespec *end)
{
    double s = (double) (end->tv_sec - start->tv_sec);
    double ns = (double) (end->tv_nsec - start->tv_nsec);
    return (s * 1e6) + (ns / 1e3);
}

/*
 * Read all image data via libtiff into a caller-freed buffer.
 * Returns the buffer in *out_data and its size in *out_size.
 */
static int bench_libtiff(const char *filename, double *open_us, double *read_us, uint8_t **out_data,
                         size_t *out_size)
{
    struct timespec t0, t1, t2;

    *out_data = NULL;
    *out_size = 0;

    /* Time open */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    TIFF *tif = TIFFOpen(filename, "r");
    clock_gettime(CLOCK_MONOTONIC, &t1);
    *open_us = elapsed_us(&t0, &t1);

    if (!tif) {
        fprintf(stderr, "  libtiff: failed to open %s\n", filename);
        return 1;
    }

    uint32_t width = 0, height = 0;
    uint16_t spp = 1;
    TIFFGetField(tif, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &height);
    TIFFGetField(tif, TIFFTAG_SAMPLESPERPIXEL, &spp);

    size_t total = (size_t) width * height * spp;
    uint8_t *data = (uint8_t *) malloc(total);
    if (!data) {
        TIFFClose(tif);
        return 1;
    }

    /* Time read - accumulate into contiguous buffer */
    tsize_t scanline_size = TIFFScanlineSize(tif);

    clock_gettime(CLOCK_MONOTONIC, &t1);
    for (uint32_t row = 0; row < height; row++) {
        TIFFReadScanline(tif, data + (size_t) row * (size_t) scanline_size, row, 0);
    }
    clock_gettime(CLOCK_MONOTONIC, &t2);

    TIFFClose(tif);
    *read_us = elapsed_us(&t1, &t2);
    *out_data = data;
    *out_size = total;

    return 0;
}

/*
 * Read /image0 via the VOL connector into a caller-freed buffer.
 * Returns the buffer in *out_data and its size in *out_size.
 */
static int bench_vol(const char *filename, hid_t fapl_id, double *open_us, double *read_us,
                     uint8_t **out_data, size_t *out_size)
{
    struct timespec t0, t1, t2;

    *out_data = NULL;
    *out_size = 0;

    /* Time open: H5Fopen + H5Dopen2 (the VOL connector loads image data during Dopen) */
    clock_gettime(CLOCK_MONOTONIC, &t0);
    hid_t file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id);
    if (file_id < 0) {
        fprintf(stderr, "  VOL: failed to open %s\n", filename);
        return 1;
    }
    hid_t dset_id = H5Dopen2(file_id, "/image0", H5P_DEFAULT);
    clock_gettime(CLOCK_MONOTONIC, &t1);
    *open_us = elapsed_us(&t0, &t1);

    if (dset_id < 0) {
        fprintf(stderr, "  VOL: failed to open /image0 in %s\n", filename);
        H5Fclose(file_id);
        return 1;
    }

    hid_t space_id = H5Dget_space(dset_id);
    int ndims = H5Sget_simple_extent_ndims(space_id);
    hsize_t dims[3] = {0};
    H5Sget_simple_extent_dims(space_id, dims, NULL);

    size_t total = 1;
    for (int i = 0; i < ndims; i++)
        total *= (size_t) dims[i];

    uint8_t *data = (uint8_t *) malloc(total);
    if (!data) {
        H5Sclose(space_id);
        H5Dclose(dset_id);
        H5Fclose(file_id);
        return 1;
    }

    /* Time read (data is already cached from Dopen, so this is a memcpy) */
    clock_gettime(CLOCK_MONOTONIC, &t1);
    H5Dread(dset_id, H5T_NATIVE_UCHAR, H5S_ALL, H5S_ALL, H5P_DEFAULT, data);
    clock_gettime(CLOCK_MONOTONIC, &t2);

    H5Sclose(space_id);
    H5Dclose(dset_id);
    H5Fclose(file_id);

    *read_us = elapsed_us(&t1, &t2);
    *out_data = data;
    *out_size = total;

    return 0;
}

int main(void)
{
    char plugin_path[PATH_MAX];
    char cwd[PATH_MAX];
    struct stat st;
    int verify_failures = 0;

    /* Suppress libtiff warnings about unknown GeoTIFF tags */
    TIFFSetWarningHandler(tiff_warning_handler);

    printf("=========================================\n");
    printf("Benchmark: libtiff vs GeoTIFF VOL\n");
    printf("=========================================\n\n");

    /* Check if being run from build directory */
    if (stat("./src", &st) != 0 || !S_ISDIR(st.st_mode)) {
        fprintf(stderr, "ERROR: This program must be run from the build directory\n");
        fprintf(stderr, "Usage: cd build && ./examples/benchmark\n");
        return 1;
    }

    /* Set HDF5_PLUGIN_PATH */
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        fprintf(stderr, "ERROR: Failed to get current working directory\n");
        return 1;
    }
    int ret = snprintf(plugin_path, sizeof(plugin_path), "%s/src", cwd);
    if (ret < 0 || (size_t) ret >= sizeof(plugin_path)) {
        fprintf(stderr, "ERROR: Path too long\n");
        return 1;
    }
    if (setenv("HDF5_PLUGIN_PATH", plugin_path, 1) != 0) {
        fprintf(stderr, "ERROR: Failed to set HDF5_PLUGIN_PATH\n");
        return 1;
    }

    /* Register VOL connector */
    hid_t vol_id = H5VLregister_connector_by_name(GEOTIFF_VOL_CONNECTOR_NAME, H5P_DEFAULT);
    if (vol_id < 0) {
        fprintf(stderr, "ERROR: Failed to register GeoTIFF VOL connector\n");
        return 1;
    }

    hid_t fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    if (fapl_id < 0 || H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        fprintf(stderr, "ERROR: Failed to set up FAPL\n");
        return 1;
    }

    /* Print header */
    printf("%-25s %12s %12s %12s %12s %8s\n", "File", "tiff open", "tiff read", "VOL open",
           "VOL read", "verify");
    printf("%-25s %12s %12s %12s %12s %8s\n", "----", "---------", "---------", "--------",
           "--------", "------");

    for (int i = 0; i < num_files; i++) {
        const char *fname = test_files[i];

        /* Check file exists */
        if (stat(fname, &st) != 0) {
            const char *bname = strrchr(fname, '/');
            bname = bname ? bname + 1 : fname;
            printf("%-25s (not found, skipping)\n", bname);
            continue;
        }

        double tiff_open = 0, tiff_read = 0;
        double vol_open = 0, vol_read = 0;
        uint8_t *tiff_data = NULL, *vol_data = NULL;
        size_t tiff_size = 0, vol_size = 0;

        int tiff_ok = bench_libtiff(fname, &tiff_open, &tiff_read, &tiff_data, &tiff_size);
        int vol_ok = bench_vol(fname, fapl_id, &vol_open, &vol_read, &vol_data, &vol_size);

        /* Verify data matches */
        const char *verify = "---";
        if (tiff_ok == 0 && vol_ok == 0) {
            if (tiff_size == vol_size && memcmp(tiff_data, vol_data, tiff_size) == 0) {
                verify = "OK";
            } else {
                verify = "MISMATCH";
                verify_failures++;
            }
        }

        free(tiff_data);
        free(vol_data);

        /* Extract just the filename for display */
        const char *basename = strrchr(fname, '/');
        basename = basename ? basename + 1 : fname;

        if (tiff_ok == 0 && vol_ok == 0) {
            printf("%-25s %9.0f us %9.0f us %9.0f us %9.0f us %8s\n", basename, tiff_open,
                   tiff_read, vol_open, vol_read, verify);
        } else if (tiff_ok == 0) {
            printf("%-25s %9.0f us %9.0f us %12s %12s %8s\n", basename, tiff_open, tiff_read,
                   "FAIL", "FAIL", "---");
        } else {
            printf("%-25s %12s %12s %12s %12s %8s\n", basename, "FAIL", "FAIL", "FAIL", "FAIL",
                   "---");
        }
    }

    printf("\nAll times in microseconds (us)\n");
    if (verify_failures > 0)
        printf("WARNING: %d file(s) had data mismatches!\n", verify_failures);

    /* Cleanup */
    H5Pclose(fapl_id);
    H5VLunregister_connector(vol_id);

    return verify_failures > 0 ? 1 : 0;
}
