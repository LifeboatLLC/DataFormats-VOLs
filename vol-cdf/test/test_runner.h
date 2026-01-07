#ifndef TEST_CDF_H
#define TEST_CDF_H

#include <hdf5.h>

/* CDF functionality tests */
int OpenCDFTest(const char *filename);
int ReadCDFTest(const char *filename);

#endif /* TEST_CDF_H */
