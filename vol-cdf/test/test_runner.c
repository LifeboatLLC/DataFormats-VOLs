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
        //num_failures += (OpenGeoTIFFTest(GRAYSCALE_FILENAME) != 0 ? 1 : 0);
        num_failures += (OpenCDFTest("example1") != 0 ? 1 : 0);


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
