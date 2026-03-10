#include <cdf.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(status, msg)                                                                         \
    if ((status) != CDF_OK) {                                                                      \
        fprintf(stderr, "Error: %s (status code: %ld)\n", msg, status);                            \
        char error_text[CDF_STATUSTEXT_LEN + 1];                                                   \
        CDFgetStatusText(status, error_text);                                                      \
        fprintf(stderr, "CDF ERROR: %s (status code: %ld)\n", error_text, status);                 \
        goto cleanup;                                                                              \
    }

int main(int argc, char *argv[])
{
    char *filename = "example2.cdf";
    printf("Creating CDF file: %s\n", filename);

    CDFid id = 0;
    CDFstatus status;
    long char_rVar_num, float_rVar_num, char_zVar_num, epoch16_zVar_num, tt2000_zVar_num;
    long vAttr1_num, vAttr2_num, vAttr3_num, gAttr1_num, gAttr2_num, gAttr3_num;
    
    long rVarNDims = 2;
    long rVarDimSizes[2] = {2, 2}; /* 2x2 array for rVariables */
    long host_encoding = HOST_ENCODING;
    long row_major = ROW_MAJOR; /* CDF VOL connector likely doesn't support COLUMN_MAJOR */

    /* Create a new CDF file with overwriting permitted */
    /* Use CDFcreate() over CDFcreateCDF for more arguments and flexibility */
    status = CDFcreate(filename, rVarNDims, rVarDimSizes, host_encoding, row_major, &id);
    if (status < CDF_OK) {
        if (status == CDF_EXISTS) {
            status = CDFopenCDF(filename, &id);
            if (status < CDF_OK)
                CHECK(status, "CDFopen failed");

            status = CDFdeleteCDF(id);
            if (status < CDF_OK)
                CHECK(status, "CDFdeleteCDF failed");

            status = CDFcreate(filename, rVarNDims, rVarDimSizes, host_encoding, row_major, &id);
            if (status < CDF_OK)
                CHECK(status, "CDFcreate failed");
            status = CDFsetFormat(id, SINGLE_FILE);
            if (status < CDF_OK)
                CHECK(status, "CDFsetFormat failed");
        } else {
            CHECK(status, "CDFcreate failed");
        }
    }

    // -----------------------------
    // Create a zVariable (char)
    // -----------------------------
    status = CDFcreatezVar(id, "zVar_char", CDF_UCHAR, 10, 0, NULL, VARY, NULL, &char_zVar_num);
    CHECK(status, "CDFcreatezVar failed");

    // -----------------------------
    // Add zVariable char data
    // -----------------------------
    long indices1D[1] = {0}; // start at top-left
    char char_values[10] = "zVar char";
    status = CDFputzVarData(id, char_zVar_num, 0, indices1D, char_values);
    CHECK(status, "CDFputzVarData failed - adding char data to zVariable");

    // -----------------------------
    // Create a zVariable (EPOCH16)
    // -----------------------------
    long epoch16_dimSizes[1] = {2};
    long epoch16_dimVarys[1] = {VARY};
    status = CDFcreatezVar(id, "zVar_epoch16", CDF_EPOCH16, 1, 1, epoch16_dimSizes, VARY,
                           epoch16_dimVarys, &epoch16_zVar_num);
    CHECK(status, "CDFcreatezVar epoch16 failed");

    // -----------------------------
    // Add zVariable EPOCH16 data
    // -----------------------------
    // First compute some example epoch16 values using the CDF library's internal functions to
    // ensure valid formatting
    double epoch16_values1[2];
    status =
        computeEPOCH16(2025L, 12L, 25L, 12L, 30L, 45L, 123L, 456L, 789L, 123L, epoch16_values1);
    CHECK(status, "computeEPOCH16 failed");

    /* Add the first epoch16 value */
    indices1D[0] = 0;
    status = CDFputzVarData(id, epoch16_zVar_num, 0, indices1D, epoch16_values1);
    CHECK(status, "CDFputzVarData epoch16 failed");

    // Compute second epoch16 value with different date/time for variety
    double epoch16_values2[2];
    status = computeEPOCH16(1990L, 1L, 1L, 0L, 0L, 0L, 0L, 0L, 0L, 0L, epoch16_values2);
    CHECK(status, "computeEPOCH16 failed");

    // Add the second epoch16 value
    indices1D[0] = 1;
    status = CDFputzVarData(id, epoch16_zVar_num, 0, indices1D, epoch16_values2);
    CHECK(status, "CDFputzVarData epoch16 failed");

    // NOW add the epoch16 values again as a second record
    // Second computed Epoch16 value first this time.
    indices1D[0] = 0;
    status = CDFputzVarData(id, epoch16_zVar_num, 1, indices1D, epoch16_values2);
    CHECK(status, "CDFputzVarData epoch16 failed - adding second record");

    // First Epoch16 value second this time.
    indices1D[0] = 1;
    status = CDFputzVarData(id, epoch16_zVar_num, 1, indices1D, epoch16_values1);
    CHECK(status, "CDFputzVarData epoch16 failed - adding second record");

    // -----------------------------
    // Create a zVariable (TT2000)
    // -----------------------------
    long tt2000_dimSizes[1] = {2};
    long tt2000_dimVarys[1] = {VARY};
    status = CDFcreatezVar(id, "zVar_tt2000", CDF_TIME_TT2000, 1, 1, tt2000_dimSizes, VARY,
                           tt2000_dimVarys, &tt2000_zVar_num);
    CHECK(status, "CDFcreatezVar tt2000 failed");

    // -----------------------------
    // Add zVariable TT2000 data
    // -----------------------------
    long long tt2000_value = 1234567890123456789LL;
    indices1D[0] = 0;
    status = CDFputzVarData(id, tt2000_zVar_num, 0, indices1D, &tt2000_value);
    CHECK(status, "CDFputzVarData tt2000 failed");

    indices1D[0] = 1;
    tt2000_value = 9223372036854775807LL;
    status = CDFputzVarData(id, tt2000_zVar_num, 0, indices1D, &tt2000_value);
    CHECK(status, "CDFputzVarData tt2000 failed");

    // -----------------------------
    // Create an rVariable (char)
    // -----------------------------
    long char_rVar_dimVarys[2] = {NOVARY, NOVARY};
    status =
        CDFvarCreate(id, "rVar_char", CDF_CHAR, 28, NOVARY, char_rVar_dimVarys, &char_rVar_num);
    CHECK(status, "CDFcreaterVar failed - rVar_char");
    // -----------------------------
    // Add rEntry (char) to rVar
    // -----------------------------
    long char_indices[2] = {0, 0};
    status = CDFvarPut(id, char_rVar_num, 0, char_indices, "rVariable character record.");
    CHECK(status, "CDFputrVarEntry failed - adding string rEntry to rVariable");

    // -----------------------------
    // Create a different rVariable (float)
    // -----------------------------
    long float_rVar_dimVarys[2] = {VARY, VARY};
    status =
        CDFvarCreate(id, "rVar_float2x2", CDF_FLOAT, 1, VARY, float_rVar_dimVarys, &float_rVar_num);
    CHECK(status, "CDFcreaterVar failed - rVar_Float");

    // -----------------------------
    // Add rEntry (float) to rVar in a 2x2 array for each record
    // -----------------------------
    float record1[2][2] = {{1.1f, 2.2f}, {3.3f, 4.4f}};
    long indices2D[2] = {0, 0}; // start at top-left
    long counts[2] = {2, 2};    // 2 rows × 2 cols
    long intervals[2] = {1, 1}; // no skipping
    long recStart = 0;
    long recCount = 1;
    long recInterval = 1;
    status = CDFvarHyperPut(id, float_rVar_num, recStart, recCount, recInterval, indices2D, counts,
                            intervals, record1);
    CHECK(status, "CDFvarHyperPut failed - adding 2x2 float array rEntry to rVariable");

    float record2[2][2] = {{5.5f, 6.6f}, {7.7f, 8.8f}}; /* Change values for second record */
    recStart = 1;                                       /* Start second record */
    status = CDFvarHyperPut(id, float_rVar_num, recStart, recCount, recInterval, indices2D, counts,
                            intervals, record2);
    CHECK(status,
          "CDFvarHyperPut failed - adding 2x2 float array rEntry to rVariable for second record");

    // -----------------------------
    // Create gAttr1
    // -----------------------------
    status = CDFcreateAttr(id, "gAttr1", GLOBAL_SCOPE, &gAttr1_num);
    CHECK(status, "CDFcreateAttr failed - gAttr1");

    // -----------------------------
    // Add string gEntry to gAttribute using CDFputAttrStrgEntry
    // -----------------------------
    char *string = "Second Example CDF";
    status = CDFputAttrStrgEntry(id, gAttr1_num, 0, string);
    CHECK(status, "CDFputAttrStrgEntry failed - adding string gEntry to gAttribute");

    // -----------------------------
    // Add string gEntry to gAttribute using normal CDFputAttrgEntry with string datatype
    // -----------------------------
    string = "Author: Lifeboat, LLC";
    status = CDFputAttrgEntry(id, gAttr1_num, 1, CDF_UCHAR, strlen(string), string);
    CHECK(status, "CDFputAttrgEntry failed - adding string gEntry to gAttribute");

    // -----------------------------
    // Add a double gEntry to gAttribute using CDFputAttrgEntry
    // -----------------------------
    double double_value = 3.141592653589793;
    status = CDFputAttrgEntry(id, gAttr1_num, 2, CDF_DOUBLE, 1, &double_value);
    CHECK(status, "CDFputAttrgEntry failed - adding double gEntry to gAttribute");

    // -----------------------------
    // Add a gEntry with Epoch16 data to gAttribute using CDFputAttrgEntry
    // -----------------------------
    double epoch16_values[4] = {epoch16_values2[0], epoch16_values2[1], epoch16_values1[0], epoch16_values1[1]};
    status = CDFputAttrgEntry(id, gAttr1_num, 3, CDF_EPOCH16, 2, epoch16_values);
    CHECK(status, "CDFputAttrgEntry with epoch16 failed");

    // -----------------------------
    // Add a gEntry with TT2000 data to gAttribute using CDFputAttrgEntry
    // Make sure to place it at an index that is not contiguous with the previous gEntries to test
    // non-contiguous gEntry handling
    // -----------------------------
    tt2000_value = 1625097600000000000LL;
    status = CDFputAttrgEntry(id, gAttr1_num, 20, CDF_TIME_TT2000, 1, &tt2000_value);
    CHECK(status, "CDFputAttrgEntry with tt2000 failed");

    // -----------------------------
    // Create gAttr2
    // -----------------------------
    status = CDFcreateAttr(id, "gAttr2", GLOBAL_SCOPE, &gAttr2_num);
    CHECK(status, "CDFcreateAttr failed - gAttr2");

    // -----------------------------
    // Add a double gEntry to gAttr2
    // -----------------------------
    status = CDFputAttrgEntry(id, gAttr2_num, 0, CDF_DOUBLE, 1, &double_value);
    CHECK(status, "CDFputAttrgEntry failed - adding double gEntry to gAttribute");

    // -----------------------------
    // Create gAttr3
    // -----------------------------
    status = CDFcreateAttr(id, "gAttr3", GLOBAL_SCOPE, &gAttr3_num);
    CHECK(status, "CDFcreateAttr failed - gAttr3");

    // -----------------------------
    // Add a tt2000 gEntry to gAttr3
    // -----------------------------
    status = CDFputAttrgEntry(id, gAttr3_num, 0, CDF_TIME_TT2000, 1, &tt2000_value);
    CHECK(status, "CDFputAttrgEntry failed - adding double gEntry to gAttribute");


    // -----------------------------
    // Create vAttribute1
    // -----------------------------
    status = CDFcreateAttr(id, "vAttribute1", VARIABLE_SCOPE, &vAttr1_num);
    CHECK(status, "CDFcreatezVarAttr failed to create vAttribute1 vAttribute");

    // -----------------------------
    // Create vAttribute2
    // -----------------------------
    status = CDFcreateAttr(id, "vAttribute2", VARIABLE_SCOPE, &vAttr2_num);
    CHECK(status, "CDFcreatezVarAttr failed to create vAttribute2 vAttribute");

    // -----------------------------
    // Create vAttribute3
    // -----------------------------
    status = CDFcreateAttr(id, "vAttribute3", VARIABLE_SCOPE, &vAttr3_num);
    CHECK(status, "CDFcreatezVarAttr failed to create vAttribute3 vAttribute");

    // -----------------------------
    // Add numerical zEntry to vAttribute1 for char zVariable using CDFputAttrzEntry
    // -----------------------------
    int16_t int16_value[2] = {-12345, 6789};
    status = CDFputAttrzEntry(id, vAttr1_num, char_zVar_num, CDF_INT2, 2, int16_value);
    CHECK(status, "CDFputAttrzEntry failed - adding int16 zEntry to vAttribute1 for char zVariable");

    // -----------------------------
    // Add a string type rEntry to vAttribute1 for rVar_char rVariable
    //------------------------------
    char *rEntry_string = "String rEntry for rVar_char";
    status = CDFputAttrrEntry(id, vAttr1_num, char_rVar_num, CDF_CHAR, strlen(rEntry_string), rEntry_string);
    CHECK(status, "CDFputAttrrEntry failed - adding string rEntry to vAttribute1 for rVar_char rVariable");

    // -----------------------------
    // Add a different type numerical zEntry to vAttribute2 for Epoch16 zVariable using CDFputAttrzEntry
    // -----------------------------
    /* Use the double value defined earlier */
    status = CDFputAttrzEntry(id, vAttr2_num, epoch16_zVar_num, CDF_REAL8, 1, &double_value);
    CHECK(status, "CDFputAttrzEntry failed - adding double zEntry to vAttribute2 for Epoch16 zVariable");

    // -----------------------------
    // Add string zEntry to vAttribute3 for Epoch16 zVariable using CDFputAttrStrzEntry
    // -----------------------------
    char *zEntry_string1 = "String entry for zVar_epoch16";
    status = CDFputAttrStrzEntry(id, vAttr3_num, epoch16_zVar_num, zEntry_string1);
    CHECK(status, "CDFputAttrStrzEntry failed - adding string to vAttribute3 for tt2000 zVariable");

    // -----------------------------
    // Add numerical rEntry to vAttribute2 for rVariable using CDFputAttrrEntry
    // -----------------------------
    float float_values[3] = {1.2345f, 2.3456f, 3.4567f};
    status = CDFputAttrrEntry(id, vAttr2_num, float_rVar_num, CDF_FLOAT, 3, float_values);
    CHECK(status, "CDFputAttrrEntry failed - adding float rEntry to vAttribute2 for rVariable");

    // -----------------------------
    // Add numerical zEntry to vAttribute2 for char zVariable using CDFputAttrzEntry
    // -----------------------------
    int8_t int8_value = -123;
    status = CDFputAttrzEntry(id, vAttr2_num, char_zVar_num, CDF_INT1, 1, &int8_value);
    CHECK(status, "CDFputAttrzEntry failed - adding int8 gEntry to vAttribute2 for char zVariable");

    // -----------------------------
    // Add numerical zEntry to vAttribute3 for char zVariable using CDFputAttrzEntry
    // -----------------------------
    int32_t int32_value = 9999999;
    status = CDFputAttrzEntry(id, vAttr2_num, char_zVar_num, CDF_INT4, 1, &int32_value);
    CHECK(status, "CDFputAttrzEntry failed - adding int8 gEntry to vAttribute2 for char zVariable");

cleanup:
    // -----------------------------
    // Close
    // -----------------------------
    if (id != 0) {
        status = CDFclose(id);
        if (status < CDF_OK) {
            char error_text[CDF_STATUSTEXT_LEN + 1];
            CDFgetStatusText(status, error_text);
            fprintf(stderr, "Error closing CDF file: %s (status code: %ld)\n", error_text, status);
            return 1;
        }
    }

    printf("CDF file created successfully.\n");
    return 0;
}