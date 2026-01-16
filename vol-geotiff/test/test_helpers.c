/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Copyright by The HDF Group.                                               *
 * All rights reserved.                                                      *
 *                                                                           *
 * This file is part of HDF5.  The full HDF5 copyright notice, including     *
 * terms governing use, modification, and redistribution, is contained in    *
 * the COPYING file, which can be found at the root of the source code       *
 * distribution tree, or in https://support.hdfgroup.org/ftp/HDF5/releases.  *
 * If you do not have access to either file, you may request a copy from     *
 * help@hdfgroup.org.                                                        *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/*
 * Purpose:     This file defines helper routines to be used in
                tests for the GeoTIFF VOL connector.
 */

#include "geotiff_vol_connector.h"
#include "test_runner.h"
#include <H5PLpublic.h>
#include <geo_normalize.h>
#include <geotiffio.h>
#include <xtiffio.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Helper function to set up GeoTIFF keys */
void SetUpGeoKeys(GTIF *gtif)
{
    GTIFKeySet(gtif, GTModelTypeGeoKey, TYPE_SHORT, 1, ModelGeographic);
    GTIFKeySet(gtif, GTRasterTypeGeoKey, TYPE_SHORT, 1, RasterPixelIsArea);
    GTIFKeySet(gtif, GTCitationGeoKey, TYPE_ASCII, 0, "Test GeoTIFF");
    GTIFKeySet(gtif, GeographicTypeGeoKey, TYPE_SHORT, 1, GCS_WGS_84);
}

/* Helper function to create a grayscale GeoTIFF file */
int CreateGrayscaleGeoTIFF(const char *filename)
{
    TIFF *tif = NULL;
    GTIF *gtif = NULL;
    unsigned char buffer[WIDTH];

    if ((tif = XTIFFOpen(filename, "w")) == NULL) {
        printf("Failed to create %s\n", filename);
        return -1;
    }

    if ((gtif = GTIFNew(tif)) == NULL) {
        printf("Failed to create GeoTIFF handle for %s\n", filename);
        TIFFClose(tif);
        return -1;
    }

    /* Set up TIFF tags */
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, WIDTH);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, HEIGHT);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, HEIGHT);

    /* Tie points and pixel scale */
    const double tiepoints[6] = {0, 0, 0, 100.0, 50.0, 0.0};
    const double pixscale[3] = {1.0, 1.0, 0.0};
    TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
    TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);

    SetUpGeoKeys(gtif);

    /* Create a gradient pattern: value = (row * 8 + col / 4) % 256 */
    for (uint32_t row = 0; row < HEIGHT; row++) {
        for (uint32_t col = 0; col < WIDTH; col++) {
            buffer[col] = (unsigned char) ((row * 8 + col / 4) % 256);
        }
        if (!TIFFWriteScanline(tif, buffer, row, 0)) {
            printf("Failed to write scanline %u\n", row);
            GTIFFree(gtif);
            TIFFClose(tif);
            return -1;
        }
    }

    GTIFWriteKeys(gtif);
    GTIFFree(gtif);
    XTIFFClose(tif);

    return 0;
}

/* Helper function to create a GeoTIFF file with a specific datatype */
int CreateTypedGeoTIFF(const char *filename, uint16_t sample_format, uint16_t bits_per_sample)
{
    TIFF *tif = NULL;
    GTIF *gtif = NULL;
    void *buffer = NULL;
    size_t element_size = bits_per_sample / 8;
    size_t row_size = WIDTH * element_size;

    if ((tif = XTIFFOpen(filename, "w")) == NULL) {
        printf("Failed to create %s\n", filename);
        return -1;
    }

    if ((gtif = GTIFNew(tif)) == NULL) {
        printf("Failed to create GeoTIFF handle for %s\n", filename);
        TIFFClose(tif);
        return -1;
    }

    /* Set up TIFF tags */
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, WIDTH);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, HEIGHT);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, bits_per_sample);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, sample_format);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, HEIGHT);

    /* Tie points and pixel scale */
    const double tiepoints[6] = {0, 0, 0, 100.0, 50.0, 0.0};
    const double pixscale[3] = {1.0, 1.0, 0.0};
    TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
    TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);

    /* Set up geo keys */
    GTIFKeySet(gtif, GTModelTypeGeoKey, TYPE_SHORT, 1, ModelGeographic);
    GTIFKeySet(gtif, GTRasterTypeGeoKey, TYPE_SHORT, 1, RasterPixelIsArea);
    GTIFKeySet(gtif, GTCitationGeoKey, TYPE_ASCII, 0, "Datatype Test GeoTIFF");
    GTIFKeySet(gtif, GeographicTypeGeoKey, TYPE_SHORT, 1, GCS_WGS_84);

    /* Allocate buffer for one row */
    buffer = malloc(row_size);
    if (!buffer) {
        printf("Failed to allocate buffer\n");
        GTIFFree(gtif);
        TIFFClose(tif);
        return -1;
    }

    /* Write test pattern data */
    for (int row = 0; row < HEIGHT; row++) {
        /* Fill buffer with simple pattern based on datatype */
        switch (sample_format) {
            case SAMPLEFORMAT_UINT:
                if (bits_per_sample == 8) {
                    for (int col = 0; col < WIDTH; col++)
                        ((uint8_t *) buffer)[col] = (uint8_t) ((row + col) % 256);
                } else if (bits_per_sample == 16) {
                    for (int col = 0; col < WIDTH; col++)
                        ((uint16_t *) buffer)[col] = (uint16_t) ((row * 256 + col) % 65536);
                } else if (bits_per_sample == 32) {
                    for (int col = 0; col < WIDTH; col++)
                        ((uint32_t *) buffer)[col] = (uint32_t) (row * 1000 + col);
                } else if (bits_per_sample == 64) {
                    for (int col = 0; col < WIDTH; col++)
                        ((uint64_t *) buffer)[col] = (uint64_t) (row * 1000 + col);
                }
                break;

            case SAMPLEFORMAT_INT:
                if (bits_per_sample == 8) {
                    for (int col = 0; col < WIDTH; col++)
                        ((int8_t *) buffer)[col] = (int8_t) ((row + col - 64) % 128);
                } else if (bits_per_sample == 16) {
                    for (int col = 0; col < WIDTH; col++)
                        ((int16_t *) buffer)[col] = (int16_t) ((row * 100 + col - 1000));
                } else if (bits_per_sample == 32) {
                    for (int col = 0; col < WIDTH; col++)
                        ((int32_t *) buffer)[col] = (int32_t) (row * 1000 + col - 16000);
                } else if (bits_per_sample == 64) {
                    for (int col = 0; col < WIDTH; col++)
                        ((int64_t *) buffer)[col] = (int64_t) (row * 1000 + col - 16000);
                }
                break;

            case SAMPLEFORMAT_IEEEFP:
                if (bits_per_sample == 32) {
                    for (int col = 0; col < WIDTH; col++)
                        ((float *) buffer)[col] = (float) (row + col * 0.5);
                } else if (bits_per_sample == 64) {
                    for (int col = 0; col < WIDTH; col++)
                        ((double *) buffer)[col] = (double) (row + col * 0.5);
                }
                break;

            default:
                /* For unsupported formats, write zeros */
                memset(buffer, 0, row_size);
                break;
        }

        if (!TIFFWriteScanline(tif, buffer, (uint32_t) row, 0)) {
            printf("Failed to write scanline %d\n", row);
            free(buffer);
            GTIFFree(gtif);
            TIFFClose(tif);
            return -1;
        }
    }

    free(buffer);
    GTIFWriteKeys(gtif);
    GTIFFree(gtif);
    XTIFFClose(tif);

    return 0;
}

/* Helper function to create a multi-image GeoTIFF file with distinct RGB data */
int CreateMultiImageGeoTIFF(const char *filename, uint32_t num_images)
{
    TIFF *tif = NULL;

    if ((tif = XTIFFOpen(filename, "w")) == NULL) {
        printf("Failed to create multi-image GeoTIFF %s\n", filename);
        return -1;
    }

    for (uint32_t img_idx = 0; img_idx < num_images; img_idx++) {
        GTIF *gtif = NULL;

        if ((gtif = GTIFNew(tif)) == NULL) {
            printf("Failed to create GeoTIFF handle for image %u\n", img_idx);
            TIFFClose(tif);
            return -1;
        }

        /* Set up TIFF tags for RGB image */
        TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, WIDTH);
        TIFFSetField(tif, TIFFTAG_IMAGELENGTH, HEIGHT);
        TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
        TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
        TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
        TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
        TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
        TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, HEIGHT);

        /* Tie points and pixel scale */
        const double tiepoints[6] = {0, 0, 0, 100.0 + img_idx * 10, 50.0 + img_idx * 10, 0.0};
        const double pixscale[3] = {1.0, 1.0, 0.0};
        TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
        TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);

        /* Set up geo keys */
        GTIFKeySet(gtif, GTModelTypeGeoKey, TYPE_SHORT, 1, ModelGeographic);
        GTIFKeySet(gtif, GTRasterTypeGeoKey, TYPE_SHORT, 1, RasterPixelIsArea);
        GTIFKeySet(gtif, GTCitationGeoKey, TYPE_ASCII, 0, "Multi-image Test GeoTIFF");
        GTIFKeySet(gtif, GeographicTypeGeoKey, TYPE_SHORT, 1, GCS_WGS_84);

        /* Allocate buffer for one scanline (row) */
        size_t scanline_size = WIDTH * 3;
        unsigned char *scanline = (unsigned char *) malloc(scanline_size);
        if (!scanline) {
            printf("Failed to allocate scanline buffer for image %u\n", img_idx);
            GTIFFree(gtif);
            TIFFClose(tif);
            return -1;
        }

        /* Write RGB data with pattern unique to this image
         * Image 0: R=(row*8)%256,        G=(col*8)%256,        B=((row+col)*4)%256
         * Image 1: R=(row*8+50)%256,     G=(col*8+50)%256,     B=((row+col)*4+50)%256
         * Image 2: R=(row*8+100)%256,    G=(col*8+100)%256,    B=((row+col)*4+100)%256
         * etc.
         */
        uint32_t offset = img_idx * 50;

        for (uint32_t row = 0; row < HEIGHT; row++) {
            for (uint32_t col = 0; col < WIDTH; col++) {
                uint32_t idx = col * 3;
                scanline[idx + 0] = (unsigned char) ((row * 8 + offset) % 256);
                scanline[idx + 1] = (unsigned char) ((col * 8 + offset) % 256);
                scanline[idx + 2] = (unsigned char) (((row + col) * 4 + offset) % 256);
            }

            if (!TIFFWriteScanline(tif, scanline, row, 0)) {
                printf("Failed to write scanline %u for image %u\n", row, img_idx);
                free(scanline);
                GTIFFree(gtif);
                TIFFClose(tif);
                return -1;
            }
        }

        free(scanline);
        GTIFWriteKeys(gtif);
        GTIFFree(gtif);

        /* Write directory for this image (except for the last one) */
        if (img_idx < num_images - 1) {
            if (!TIFFWriteDirectory(tif)) {
                printf("Failed to write directory for image %u\n", img_idx);
                TIFFClose(tif);
                return -1;
            }
        }
    }

    XTIFFClose(tif);
    return 0;
}

/* Helper to create a small GeoTIFF with geographic (lat/lon) coordinates */
int CreateGeographicGeoTIFF(const char *filename)
{
    TIFF *tif = NULL;
    GTIF *gtif = NULL;
    unsigned char buffer[10]; /* 10x10 image */
    const uint32_t width = 10;
    const uint32_t height = 10;

    if ((tif = XTIFFOpen(filename, "w")) == NULL) {
        printf("Failed to create %s\\n", filename);
        return -1;
    }

    if ((gtif = GTIFNew(tif)) == NULL) {
        printf("Failed to create GeoTIFF handle for %s\\n", filename);
        TIFFClose(tif);
        return -1;
    }

    /* Set up TIFF tags */
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, height);

    /* Geographic coordinates: top-left at (-120°, 40°), 0.1° per pixel */
    const double tiepoints[6] = {0, 0, 0, -120.0, 40.0, 0.0};
    const double pixscale[3] = {0.1, 0.1, 0.0};
    TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
    TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);

    /* Set GeoKeys for geographic coordinate system (WGS84) */
    GTIFKeySet(gtif, GTModelTypeGeoKey, TYPE_SHORT, 1, ModelGeographic);
    GTIFKeySet(gtif, GTRasterTypeGeoKey, TYPE_SHORT, 1, RasterPixelIsArea);
    GTIFKeySet(gtif, GeographicTypeGeoKey, TYPE_SHORT, 1, GCS_WGS_84);

    /* Write simple gradient data */
    for (uint32_t row = 0; row < height; row++) {
        for (uint32_t col = 0; col < width; col++) {
            buffer[col] = (unsigned char) ((row + col) * 10);
        }
        if (!TIFFWriteScanline(tif, buffer, row, 0)) {
            printf("Failed to write scanline %u\\n", row);
            GTIFFree(gtif);
            TIFFClose(tif);
            return -1;
        }
    }

    GTIFWriteKeys(gtif);
    GTIFFree(gtif);
    XTIFFClose(tif);

    return 0;
}

/* Helper to create a small GeoTIFF with projected (UTM) coordinates */
int CreateProjectedGeoTIFF(const char *filename)
{
    TIFF *tif = NULL;
    GTIF *gtif = NULL;
    unsigned char buffer[10]; /* 10x10 image */
    const uint32_t width = 10;
    const uint32_t height = 10;

    if ((tif = XTIFFOpen(filename, "w")) == NULL) {
        printf("Failed to create %s\n", filename);
        return -1;
    }

    if ((gtif = GTIFNew(tif)) == NULL) {
        printf("Failed to create GeoTIFF handle for %s\n", filename);
        TIFFClose(tif);
        return -1;
    }

    /* Set up TIFF tags */
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, height);

    /* UTM Zone 11N coordinates: top-left at (500000E, 4500000N), 100m per pixel */
    const double tiepoints[6] = {0, 0, 0, 500000.0, 4500000.0, 0.0};
    const double pixscale[3] = {100.0, 100.0, 0.0};
    TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
    TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);

    /* Set GeoKeys for UTM Zone 11N, WGS84 */
    GTIFKeySet(gtif, GTModelTypeGeoKey, TYPE_SHORT, 1, ModelProjected);
    GTIFKeySet(gtif, GTRasterTypeGeoKey, TYPE_SHORT, 1, RasterPixelIsArea);
    GTIFKeySet(gtif, ProjectedCSTypeGeoKey, TYPE_SHORT, 1, 32611); /* WGS84 UTM Zone 11N */

    /* Write simple gradient data */
    for (uint32_t row = 0; row < height; row++) {
        for (uint32_t col = 0; col < width; col++) {
            buffer[col] = (unsigned char) ((row + col) * 10);
        }
        if (!TIFFWriteScanline(tif, buffer, row, 0)) {
            printf("Failed to write scanline %u\n", row);
            GTIFFree(gtif);
            TIFFClose(tif);
            return -1;
        }
    }

    GTIFWriteKeys(gtif);
    GTIFFree(gtif);
    XTIFFClose(tif);

    return 0;
}

/* Helper function to create a TIFF file with comprehensive tag coverage */
int CreateComprehensiveTiffTagFile(const char *filename)
{
    TIFF *tif = NULL;
    GTIF *gtif = NULL;
    unsigned char buffer[32]; /* WIDTH from test_runner.h */

    if ((tif = XTIFFOpen(filename, "w")) == NULL) {
        printf("Failed to create %s\n", filename);
        return -1;
    }

    if ((gtif = GTIFNew(tif)) == NULL) {
        printf("Failed to create GeoTIFF handle for %s\n", filename);
        TIFFClose(tif);
        return -1;
    }

    /* Set all basic TIFF tags */
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, (uint32_t) 32);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, (uint32_t) 24);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, (uint16_t) COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, (uint16_t) PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, (uint16_t) PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, (uint16_t) 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, (uint16_t) 1);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, (uint32_t) 24);
    TIFFSetField(tif, TIFFTAG_SAMPLEFORMAT, (uint16_t) SAMPLEFORMAT_UINT);

    /* Set resolution tags (RATIONAL type) */
    float xres = 300.0f;
    float yres = 300.0f;
    TIFFSetField(tif, TIFFTAG_XRESOLUTION, xres);
    TIFFSetField(tif, TIFFTAG_YRESOLUTION, yres);
    TIFFSetField(tif, TIFFTAG_RESOLUTIONUNIT, (uint16_t) RESUNIT_INCH);

    /* Set position tags (RATIONAL type) */
    float xpos = 12.34f;
    float ypos = 56.78f;
    TIFFSetField(tif, TIFFTAG_XPOSITION, xpos);
    TIFFSetField(tif, TIFFTAG_YPOSITION, ypos);

    /* Set string tags */
    TIFFSetField(tif, TIFFTAG_SOFTWARE, "GeoTIFF VOL Test Suite v1.0");
    TIFFSetField(tif, TIFFTAG_DATETIME, "2025:01:15 12:34:56");
    TIFFSetField(tif, TIFFTAG_ARTIST, "Test Artist");
    TIFFSetField(tif, TIFFTAG_COPYRIGHT, "Copyright 2025 Test");

    /* Set orientation */
    TIFFSetField(tif, TIFFTAG_ORIENTATION, (uint16_t) ORIENTATION_TOPLEFT);

    /* Set min/max sample values - these are per-sample arrays for multi-sample images */
    /* For grayscale (1 sample), libtiff may not write these if they match defaults */
    /* We'll skip these in the test file creation and remove from test expectations */

    /* Additional string tags */
    TIFFSetField(tif, TIFFTAG_DOCUMENTNAME, "Test Document");
    TIFFSetField(tif, TIFFTAG_IMAGEDESCRIPTION, "Test image for comprehensive tag coverage");
    TIFFSetField(tif, TIFFTAG_MAKE, "Test Scanner Make");
    TIFFSetField(tif, TIFFTAG_MODEL, "Test Scanner Model");
    TIFFSetField(tif, TIFFTAG_HOSTCOMPUTER, "Test Computer");
    TIFFSetField(tif, TIFFTAG_PAGENAME, "Page 1");
    TIFFSetField(tif, TIFFTAG_TARGETPRINTER, "Test Printer");

    /* Numeric tags */
    TIFFSetField(tif, TIFFTAG_SUBFILETYPE, (uint32_t) 0);
    TIFFSetField(tif, TIFFTAG_FILLORDER, (uint16_t) FILLORDER_MSB2LSB);
    TIFFSetField(tif, TIFFTAG_THRESHHOLDING, (uint16_t) THRESHHOLD_BILEVEL);
    // TIFFSetField(tif, TIFFTAG_PREDICTOR, (uint16_t) 1);
    TIFFSetField(tif, TIFFTAG_IMAGEDEPTH, (uint32_t) 1);
    TIFFSetField(tif, TIFFTAG_TILEDEPTH, (uint32_t) 1);
    TIFFSetField(tif, TIFFTAG_MATTEING, (uint16_t) 0);
    // TIFFTAG_DATATYPE is obsolete, replaced by TIFFTAG_SAMPLEFORMAT
    // TIFFSetField(tif, TIFFTAG_DATATYPE, (uint16_t) SAMPLEFORMAT_UINT);

    /* FAX-related tags */
    // TIFFSetField(tif, TIFFTAG_BADFAXLINES, (uint32_t) 0);
    // TIFFSetField(tif, TIFFTAG_CLEANFAXDATA, (uint16_t) CLEANFAXDATA_CLEAN);
    // TIFFSetField(tif, TIFFTAG_CONSECUTIVEBADFAXLINES, (uint32_t) 0);
    // TIFFSetField(tif, TIFFTAG_FAXMODE, (int) FAXMODE_CLASSIC);
    // TIFFSetField(tif, TIFFTAG_GROUP3OPTIONS, (uint32_t) 0);
    // TIFFSetField(tif, TIFFTAG_GROUP4OPTIONS, (uint32_t) 0);

    /* JPEG-related tags */
    // TIFFSetField(tif, TIFFTAG_JPEGQUALITY, (int) 75);
    // TIFFSetField(tif, TIFFTAG_JPEGCOLORMODE, (int) JPEGCOLORMODE_RGB);
    // TIFFSetField(tif, TIFFTAG_JPEGTABLESMODE, (int) JPEGTABLESMODE_QUANT | JPEGTABLESMODE_HUFF);

    /* Array tags - page number */
    uint16_t pagenumber[2] = {1, 3}; /* Page 1 of 3 */
    TIFFSetField(tif, TIFFTAG_PAGENUMBER, pagenumber[0], pagenumber[1]);

    /* Array tags - halftone hints */
    uint16_t halftonehints[2] = {0, 255};
    TIFFSetField(tif, TIFFTAG_HALFTONEHINTS, halftonehints[0], halftonehints[1]);

    /* Array tags - dot range */
    uint16_t dotrange[2] = {0, 255};
    TIFFSetField(tif, TIFFTAG_DOTRANGE, dotrange[0], dotrange[1]);

    /* Reference black/white (2 values for grayscale: black, white) */
    float refblackwhite[2] = {0.0f, 255.0f};
    TIFFSetField(tif, TIFFTAG_REFERENCEBLACKWHITE, refblackwhite);

    /* Min/max sample values (floating point versions) */
    double sminsamplevalue = 0.0;
    double smaxsamplevalue = 255.0;
    TIFFSetField(tif, TIFFTAG_SMINSAMPLEVALUE, sminsamplevalue);
    TIFFSetField(tif, TIFFTAG_SMAXSAMPLEVALUE, smaxsamplevalue);

    /* Stone units */
    double stonits = 1.0;
    TIFFSetField(tif, TIFFTAG_STONITS, stonits);

    /* Binary/opaque data tags */
    const char *xmlpacket = "<?xml version=\"1.0\"?><test>XML data</test>";
    TIFFSetField(tif, TIFFTAG_XMLPACKET, (uint32_t) strlen(xmlpacket), xmlpacket);

    const unsigned char iccprofile[10] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    TIFFSetField(tif, TIFFTAG_ICCPROFILE, (uint32_t) 10, iccprofile);

    const unsigned char photoshop[8] = {0x38, 0x42, 0x49, 0x4D, 0, 0, 0, 0}; /* "8BIM" header */
    TIFFSetField(tif, TIFFTAG_PHOTOSHOP, (uint32_t) 8, photoshop);

    const unsigned char richtiffiptc[12] = {0x1C, 0x02, 0x00, 0x00, 0x08, 0, 0, 0, 0, 0, 0, 0};
    TIFFSetField(tif, TIFFTAG_RICHTIFFIPTC, (uint32_t) 12, richtiffiptc);

    /* Transfer function - gamma correction table (grayscale needs 1 array of 256 values) */
    uint16_t *transferfunction = (uint16_t *) malloc(256 * sizeof(uint16_t));
    if (transferfunction) {
        for (int i = 0; i < 256; i++) {
            transferfunction[i] = (uint16_t) (i * 257); /* Linear 8-bit to 16-bit */
        }
        TIFFSetField(tif, TIFFTAG_TRANSFERFUNCTION, transferfunction);
        free(transferfunction);
    }

    /* Set GeoTIFF keys */
    const double tiepoints[6] = {0, 0, 0, 100.0, 50.0, 0.0};
    const double pixscale[3] = {1.0, 1.0, 0.0};
    TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
    TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);

    SetUpGeoKeys(gtif);

    /* Write image data */
    for (uint32_t row = 0; row < 24; row++) {
        for (uint32_t col = 0; col < 32; col++) {
            buffer[col] = (unsigned char) ((row * 8 + col / 4) % 256);
        }
        if (!TIFFWriteScanline(tif, buffer, row, 0)) {
            printf("Failed to write scanline %u\n", row);
            GTIFFree(gtif);
            TIFFClose(tif);
            return -1;
        }
    }

    GTIFWriteKeys(gtif);
    GTIFFree(gtif);
    XTIFFClose(tif);

    return 0;
}

int CreateRGBGeoTIFF(const char *filename)
{
    unsigned char buffer[WIDTH * 3]; /* RGB: 3 bytes per pixel */
    TIFF *tif = NULL;
    GTIF *gtif = NULL;

    if ((tif = XTIFFOpen(filename, "w")) == NULL) {
        printf("Failed to create %s\n", filename);
        return -1;
    }

    if ((gtif = GTIFNew(tif)) == NULL) {
        printf("Failed to create GeoTIFF handle for %s\n", filename);
        TIFFClose(tif);
        return -1;
    }

    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, WIDTH);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, HEIGHT);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 3);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, HEIGHT);

    /* Tie points: pixel (0,0,0) -> geographic (200.0, 60.0, 0.0) */
    const double tiepoints[6] = {0, 0, 0, 200.0, 60.0, 0.0};
    /* Pixel scale: 2.0 units per pixel in X and Y */
    const double pixscale[3] = {2.0, 2.0, 0.0};

    TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
    TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);

    SetUpGeoKeys(gtif);

    /* Write to RGB image.
     * Create a color pattern:
     * R = row * 8
     * G = col * 8
     * B = (row + col) * 4
     */
    for (uint32_t row = 0; row < HEIGHT; row++) {
        for (uint32_t col = 0; col < WIDTH; col++) {
            uint32_t idx = col * 3;
            buffer[idx + 0] = (unsigned char) ((row * 8) % 256);         /* Red */
            buffer[idx + 1] = (unsigned char) ((col * 8) % 256);         /* Green */
            buffer[idx + 2] = (unsigned char) (((row + col) * 4) % 256); /* Blue */
        }
        if (!TIFFWriteScanline(tif, buffer, row, 0)) {
            TIFFError("WriteRGBImage", "Failed to write scanline %d\n", row);
        }
    }

    GTIFWriteKeys(gtif);
    GTIFFree(gtif);
    XTIFFClose(tif);

    return 0;
}

void SetUpGrayscaleTIFF(TIFF *tif)
{
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, WIDTH);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, HEIGHT);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, 1);
    TIFFSetField(tif, TIFFTAG_ROWSPERSTRIP, HEIGHT);

    /* Tie points: pixel (0,0,0) -> geographic (100.0, 50.0, 0.0) */
    const double tiepoints[6] = {0, 0, 0, 100.0, 50.0, 0.0};
    /* Pixel scale: 1.0 units per pixel in X and Y */
    const double pixscale[3] = {1.0, 1.0, 0.0};

    TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
    TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);
}

/* Generate a tiled TIFF with a predictable pattern for testing */
int generate_tiled_tiff(const char *filename, int is_rgb, uint32_t width, uint32_t height,
                        uint32_t tile_width, uint32_t tile_height)
{
    TIFF *tif = NULL;
    GTIF *gtif = NULL;
    unsigned char *tile_buf = NULL;
    uint32_t samples_per_pixel = is_rgb ? 3 : 1;
    uint32_t tile_size = tile_width * tile_height * samples_per_pixel;
    int ret = 0;

    /* Open TIFF for writing */
    if ((tif = XTIFFOpen(filename, "w")) == NULL) {
        printf("Failed to create tiled TIFF %s\n", filename);
        return -1;
    }

    /* Create GeoTIFF handle */
    if ((gtif = GTIFNew(tif)) == NULL) {
        printf("Failed to create GeoTIFF handle for %s\n", filename);
        XTIFFClose(tif);
        return -1;
    }

    /* Set TIFF tags */
    TIFFSetField(tif, TIFFTAG_IMAGEWIDTH, width);
    TIFFSetField(tif, TIFFTAG_IMAGELENGTH, height);
    TIFFSetField(tif, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tif, TIFFTAG_PHOTOMETRIC, is_rgb ? PHOTOMETRIC_RGB : PHOTOMETRIC_MINISBLACK);
    TIFFSetField(tif, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tif, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tif, TIFFTAG_SAMPLESPERPIXEL, samples_per_pixel);
    TIFFSetField(tif, TIFFTAG_TILEWIDTH, tile_width);
    TIFFSetField(tif, TIFFTAG_TILELENGTH, tile_height);

    /* Set GeoTIFF metadata */
    const double tiepoints[6] = {0, 0, 0, 100.0, 50.0, 0.0};
    const double pixscale[3] = {1.0, 1.0, 0.0};
    TIFFSetField(tif, TIFFTAG_GEOTIEPOINTS, 6, tiepoints);
    TIFFSetField(tif, TIFFTAG_GEOPIXELSCALE, 3, pixscale);
    SetUpGeoKeys(gtif);

    /* Allocate tile buffer */
    tile_buf = (unsigned char *) malloc(tile_size);
    if (!tile_buf) {
        printf("Failed to allocate tile buffer\n");
        ret = -1;
        goto cleanup;
    }

    /* Write tiles with predictable pattern */
    for (uint32_t row = 0; row < height; row += tile_height) {
        for (uint32_t col = 0; col < width; col += tile_width) {
            /* Fill tile with pattern data */
            uint32_t tile_h = (row + tile_height > height) ? height - row : tile_height;
            uint32_t tile_w = (col + tile_width > width) ? width - col : tile_width;

            for (uint32_t ty = 0; ty < tile_h; ty++) {
                for (uint32_t tx = 0; tx < tile_w; tx++) {
                    uint32_t global_row = row + ty;
                    uint32_t global_col = col + tx;
                    uint32_t idx = (ty * tile_width + tx) * samples_per_pixel;

                    if (is_rgb) {
                        /* RGB pattern: R=row*8, G=col*8, B=(row+col)*4 */
                        tile_buf[idx + 0] = (unsigned char) ((global_row * 8) % 256);
                        tile_buf[idx + 1] = (unsigned char) ((global_col * 8) % 256);
                        tile_buf[idx + 2] = (unsigned char) (((global_row + global_col) * 4) % 256);
                    } else {
                        /* Grayscale pattern: value = (row * 8 + col / 4) % 256 */
                        tile_buf[idx] = (unsigned char) ((global_row * 8 + global_col / 4) % 256);
                    }
                }
            }

            /* Write the tile */
            if (TIFFWriteTile(tif, tile_buf, col, row, 0, 0) < 0) {
                printf("Failed to write tile at row=%u, col=%u\n", row, col);
                ret = -1;
                goto cleanup;
            }
        }
    }

cleanup:
    if (tile_buf)
        free(tile_buf);
    if (gtif) {
        GTIFWriteKeys(gtif);
        GTIFFree(gtif);
    }
    if (tif)
        XTIFFClose(tif);

    return ret;
}