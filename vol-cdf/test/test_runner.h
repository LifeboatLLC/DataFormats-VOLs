#ifndef TEST_CDF_H
#define TEST_CDF_H

#include <hdf5.h>

/* CDF functionality tests */
int OpenCDFTest(const char *filename);
int CheckExistenceAndOpenTest(void);
int MultiLinksExistTest(void);
int LinkAttrIterateTest(void);
int ReadCDFVariableTest(void);
int DatasetDatatypeConversionTest(void);
int ReadVariableAttributeTest(void);
int ReadUnindexedGlobalArrayAttributeTest(void);
int ReadIndexedGlobalAttributeTest(void);
int ReadBasicRVariableAndREntryTest(void);

#endif /* TEST_CDF_H */
