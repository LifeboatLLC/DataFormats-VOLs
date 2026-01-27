#ifndef TEST_GRIB2_H
#define TEST_GRIB2_H

#include <hdf5.h>

/* GRIB2 functionality tests */
int OpenGRIB2Test(const char *filename);
int OpenGRIB2DatasetTest(const char *filename, const char *dsetname);

#endif /* TEST_GRIB2_H */
