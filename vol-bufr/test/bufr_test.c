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
 * Purpose:     This file defines tests which evluate basic BUFR file
 *              functionality through the BUFR VOL connector.
 */

#include "bufr_vol_connector.h"
#include "test_runner.h"
#include <H5PLpublic.h>

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Verify that BUFR file open/close operations work properly */
int OpenBUFRTest(const char *filename)
{
    hid_t vol_id = H5I_INVALID_HID;
    hid_t fapl_id = H5I_INVALID_HID;
    hid_t file_id = H5I_INVALID_HID;

    printf("Testing BUFR VOL connector open/close with file");

    /* Add the plugin path so HDF5 can find the connector */
#ifdef BUFR_VOL_PLUGIN_PATH
    if (H5PLappend(BUFR_VOL_PLUGIN_PATH) < 0) {
        printf("Failed to append plugin path\n");
        goto error;
    }
#endif

    /* Register the BUFR VOL connector */
    if ((vol_id = H5VLregister_connector_by_name(BUFR_VOL_CONNECTOR_NAME, H5P_DEFAULT)) < 0) {
        printf("Failed to register VOL connector\n");
        goto error;
    }

    /* Create file access property list */
    if ((fapl_id = H5Pcreate(H5P_FILE_ACCESS)) < 0) {
        printf("Failed to create FAPL\n");
        goto error;
    }

    /* Set the VOL connector */
    if (H5Pset_vol(fapl_id, vol_id, NULL) < 0) {
        printf("Failed to set VOL connector\n");
        goto error;
    }

    /* Open the BUFR file */
    if ((file_id = H5Fopen(filename, H5F_ACC_RDONLY, fapl_id)) < 0) {
        printf("Failed to open BUFR file\n");
        goto error;
    }

    /* Close the BUFR file */
    if (H5Fclose(file_id) < 0) {
        printf("Failed to close BUFR file\n");
        goto error;
    }

    /* Clean up*/
    if (H5Pclose(fapl_id) < 0) {
        printf("Failed to close FAPL\n");
        goto error;
    }

    /* Unregister VOL connector */
    if (H5VLunregister_connector(vol_id) < 0) {
        printf("Failed to unregister VOL connector\n");
        goto error;
    }

    printf("PASSED\n");
    return 0;

error:
    H5E_BEGIN_TRY
    {
        H5Pclose(fapl_id);
        H5Fclose(file_id);
        if (vol_id != H5I_INVALID_HID)
            H5VLunregister_connector(vol_id);
    }
    H5E_END_TRY;
    return -1;
}
