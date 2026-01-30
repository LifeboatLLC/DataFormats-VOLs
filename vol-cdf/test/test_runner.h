#ifndef TEST_CDF_H
#define TEST_CDF_H

#include <hdf5.h>

/* CDF functionality tests */
int OpenCDFTest(const char *filename);
int ReadCDFTest(const char *filename);
int DatatypeConversionTest(const char *filename);
int ReadVariableAttributeTest(const char *filename);
int ReadGlobalArrayAttributeTest(const char *filename);
int ReadIndexedGlobalAttributeTest(const char *filename);

#endif /* TEST_CDF_H */
