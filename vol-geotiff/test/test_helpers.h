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

#ifndef TEST_GEOTIFF_HELPERS_H
#define TEST_GEOTIFF_HELPERS_H

#include <geo_normalize.h>
#include <geotiffio.h>
#include <xtiffio.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void SetUpGeoKeys(GTIF *gtif);
int CreateGrayscaleGeoTIFF(const char *filename);
int CreateTypedGeoTIFF(const char *filename, uint16_t sample_format, uint16_t bits_per_sample);
int CreateMultiImageGeoTIFF(const char *filename, uint32_t num_images);
int CreateGeographicGeoTIFF(const char *filename);
int CreateProjectedGeoTIFF(const char *filename);
int CreateComprehensiveTiffTagFile(const char *filename);
int CreateRGBGeoTIFF(const char *filename);
void SetUpGrayscaleTIFF(TIFF *tif);
int generate_tiled_tiff(const char *filename, int is_rgb, uint32_t width, uint32_t height,
                        uint32_t tile_width, uint32_t tile_height);

#endif // TEST_GEOTIFF_HELPERS_H