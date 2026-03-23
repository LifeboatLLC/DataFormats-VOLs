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
/*
 * Test program for CDF VOL connector
 * Tests reading a CDF file through HDF5 interface
 */

#include "test_runner.h"
#include "cdf_vol_connector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test functions */
int main(int argc, char **argv)
{
    int num_failures = 0;
    const char *test_name = (argc > 1) ? argv[1] : "all";
    int run_all = (strcmp(test_name, "all") == 0);

    /* Run CDF functionality tests (each test manages its own connector registration) */
    if (run_all || strcmp(test_name, "open_close") == 0)
        num_failures += (OpenCDFTest("example1") != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "check_existence_and_open") == 0)
        num_failures += (CheckExistenceAndOpenTest() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "multi_links_exist") == 0)
        num_failures += (MultiLinksExistTest() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "link_attr_iterate_test") == 0)
        num_failures += (LinkAttrIterateTest() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "read_variable") == 0)
        num_failures += (ReadCDFVariableTest() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "dataset_datatype_conversion") == 0)
        num_failures += (DatasetDatatypeConversionTest() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "read_variable_attribute") == 0)
        num_failures += (ReadVariableAttributeTest() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "read_all_global_attributes") == 0)
        num_failures += (ReadUnindexedGlobalArrayAttributeTest() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "read_indexed_global_attribute") == 0)
        num_failures += (ReadIndexedGlobalAttributeTest() != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "read_basic_rVariable_and_rEntry") == 0)
        num_failures += (ReadBasicRVariableAndREntryTest() != 0 ? 1 : 0);

    if (num_failures == 0) {
        printf("\n%s: All tests completed successfully\n", test_name);
    } else {
        printf("\n%s: %d test(s) failed\n", test_name, num_failures);
    }

    return (num_failures == 0 ? 0 : 1);
error:
    printf("Error occurred during the testing process, aborting\n");
    return 1;
}
