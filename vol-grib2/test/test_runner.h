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

#ifndef TEST_GRIB2_H
#define TEST_GRIB2_H

#include <hdf5.h>

/* GRIB2 functionality tests */
int OpenGRIB2BasicTest(const char *filename);
int OpenGRIB2Test(const char *filename, const char *dsetname);
int LinkExistsTest(const char *filename);
int MultiLinkExistsTest(const char *filename);
int LinkAttrIterateTest(const char *filename);
int AttrGRIB2Test(const char *filename);

#endif /* TEST_GRIB2_H */
