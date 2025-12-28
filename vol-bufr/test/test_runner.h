#ifndef TEST_BUFR_H
#define TEST_BUFR_H

#include <hdf5.h>

/* BUFR functionality tests */
int OpenBUFRTest(const char *filename);
int OpenBUFRDatasetTest(const char *filename, const char *dsetname);

#endif /* TEST_BUFR_H */
