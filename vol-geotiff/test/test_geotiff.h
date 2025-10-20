
#ifndef TEST_GEOTIFF_H
#define TEST_GEOTIFF_H

#include <hdf5.h>

#define GRAYSCALE_FILENAME "test_image_gray.tif"
#define RGB_FILENAME "test_image_rgb.tif"
#define WIDTH 32
#define HEIGHT 32

/* Plugin-handling tests */
herr_t test_getters(void);
herr_t test_multiple_registration(void);
herr_t test_registration_by_name(void);
herr_t test_registration_by_value(void);

/* GeoTIFF functionality tests */
int OpenGeoTIFFTest(const char *filename);
int ReadGeoTIFFTest(const char *filename);
int BandReadGeoTIFFTest(const char *filename);
int DatatypeConversionTest(hid_t mem_type_id, hid_t file_type_id);

#endif // TEST_GEOTIFF_H