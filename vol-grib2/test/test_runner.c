/*
 * Test program for GRIB2 VOL connector
 * Tests reading a GRIB2 file through HDF5 interface
 */

#include "test_runner.h"
#include "grib2_vol_connector.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Test functions */
int main(int argc, char **argv)
{
    int num_failures = 0;
    const char *test_name = (argc > 1) ? argv[1] : "all";
    int run_all = (strcmp(test_name, "all") == 0);

    /* Run GRIB2 functionality tests (each test manages its own connector registration) */
    if (run_all || strcmp(test_name, "basic_open_close") == 0)
        num_failures += (OpenGRIB2BasicTest("example-1.grib2") != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "open_close") == 0)
        num_failures += (OpenGRIB2Test("example-1.grib2", "message_5/values") != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "link_exists") == 0)
        num_failures += (LinkExistsTest("example-1.grib2") != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "multi_link_exists") == 0)
        num_failures += (MultiLinkExistsTest("example-1.grib2") != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "link_attr_iterate") == 0)
        num_failures += (LinkAttrIterateTest("example-1.grib2") != 0 ? 1 : 0);

    if (run_all || strcmp(test_name, "attr_read_test") == 0)
        num_failures += (AttrGRIB2Test("example-1.grib2") != 0 ? 1 : 0);

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
