#include "grib2_vol_connector.h"
#include <hdf5.h>
//#define GRIB2_VOL_PLUGIN_PATH "/Users/elenapourmal/Working/NASA-VOLS/2025-12-18-vol-repo/vol-grib2/build/src"

int main() {
    hid_t vol_id, fapl_id, file_id, dset_id;

    // Tell the library where to find the GRIB2 VOL connector library
    // (May be skipped if HDF5_VOL_CONNECTOR/HDF5_PLUGIN_PATH are defined in env)
//    H5PLappend(GRIB2_VOL_PLUGIN_PATH);

    // Register the GRIB2 VOL connector
    vol_id = H5VLregister_connector_by_name(GRIB2_VOL_CONNECTOR_NAME, H5P_DEFAULT);

    // Create file access property list and set VOL connector
    fapl_id = H5Pcreate(H5P_FILE_ACCESS);
    H5Pset_vol(fapl_id, vol_id, NULL);

    // Open GRIB2 file
    file_id = H5Fopen("example-1.grib2", H5F_ACC_RDONLY, fapl_id);

    // Open image dataset
    //dset_id = H5Dopen2(file_id, "/image", H5P_DEFAULT);

    // Read data...

    // Cleanup
    //H5Dclose(dset_id);
    H5Fclose(file_id);
    H5Pclose(fapl_id);
    H5VLunregister_connector(vol_id);

    return 0;
}
