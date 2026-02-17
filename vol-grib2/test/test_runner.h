#ifndef TEST_GRIB2_H
#define TEST_GRIB2_H

#include <hdf5.h>

/* GRIB2 functionality tests */
int OpenGRIB2BasicTest(const char *filename);
int OpenGRIB2Test(const char *filename, const char *dsetname);
int LinkExistsTest(const char *filename);
int MultiLinkExistsTest(const char *filename);
int LinkIterateTest(const char *filename);

#endif /* TEST_GRIB2_H */
