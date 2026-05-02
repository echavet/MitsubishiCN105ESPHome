/// test_lookup_tables.cpp — Tests de non-régression pour lookupByteMapValue/Index
/// Deps: cn105_types.h (tables MODE, FAN, VANE, TEMP, etc.), esphome_stubs.h
#include <gtest/gtest.h>
#include "esphome_stubs.h"
#include "cn105_types.h"

// Standalone reimplementations of lookup functions (copy from utils.cpp)
// Avoids pulling in CN105Climate class dependencies.

static const char* lookupByteMapValue(const char* valuesMap[], const uint8_t byteMap[],
                                       int len, uint8_t byteValue,
                                       const char* debugInfo = "",
                                       const char* defaultValue = nullptr) {
    for (int i = 0; i < len; i++) {
        if (byteMap[i] == byteValue) {
            return valuesMap[i];
        }
    }
    if (defaultValue != nullptr) {
        return defaultValue;
    } else {
        return valuesMap[0];
    }
}

static int lookupByteMapValue_int(const int valuesMap[], const uint8_t byteMap[],
                                   int len, uint8_t byteValue,
                                   const char* debugInfo = "") {
    for (int i = 0; i < len; i++) {
        if (byteMap[i] == byteValue) {
            return valuesMap[i];
        }
    }
    return valuesMap[0];
}

static int lookupByteMapIndex_int(const int valuesMap[], int len, int lookupValue,
                                   const char* debugInfo = "") {
    for (int i = 0; i < len; i++) {
        if (valuesMap[i] == lookupValue) {
            return i;
        }
    }
    return -1;
}

static int lookupByteMapIndex_str(const char* valuesMap[], int len,
                                   const char* lookupValue,
                                   const char* debugInfo = "") {
    for (int i = 0; i < len; i++) {
        if (strcasecmp(valuesMap[i], lookupValue) == 0) {
            return i;
        }
    }
    return -1;
}

// ---- MODE Tests ----

TEST(LookupTablesTest, ModeByteToString_Heat) {
    EXPECT_STREQ(lookupByteMapValue(MODE_MAP, MODE, 5, 0x01), "HEAT");
}

TEST(LookupTablesTest, ModeByteToString_Auto) {
    EXPECT_STREQ(lookupByteMapValue(MODE_MAP, MODE, 5, 0x08), "AUTO");
}

TEST(LookupTablesTest, ModeByteToString_Cool) {
    EXPECT_STREQ(lookupByteMapValue(MODE_MAP, MODE, 5, 0x03), "COOL");
}

TEST(LookupTablesTest, ModeByteToString_Dry) {
    EXPECT_STREQ(lookupByteMapValue(MODE_MAP, MODE, 5, 0x02), "DRY");
}

TEST(LookupTablesTest, ModeByteToString_Fan) {
    EXPECT_STREQ(lookupByteMapValue(MODE_MAP, MODE, 5, 0x07), "FAN");
}

TEST(LookupTablesTest, ModeByteToString_UnknownReturnsDefault) {
    const char* result = lookupByteMapValue(MODE_MAP, MODE, 5, 0xFF, "test", "UNKNOWN");
    EXPECT_STREQ(result, "UNKNOWN");
}

TEST(LookupTablesTest, ModeByteToString_UnknownNoDefaultReturnsIndex0) {
    // Without defaultValue → returns valuesMap[0] = "HEAT"
    const char* result = lookupByteMapValue(MODE_MAP, MODE, 5, 0xFF);
    EXPECT_STREQ(result, "HEAT");
}

// ---- MODE Index Tests ----

TEST(LookupTablesTest, ModeStringToIndex_Cool) {
    EXPECT_EQ(lookupByteMapIndex_str(MODE_MAP, 5, "COOL"), 2);
}

TEST(LookupTablesTest, ModeStringToIndex_CaseInsensitive) {
    EXPECT_EQ(lookupByteMapIndex_str(MODE_MAP, 5, "cool"), 2);
}

TEST(LookupTablesTest, ModeStringToIndex_NotFound) {
    EXPECT_EQ(lookupByteMapIndex_str(MODE_MAP, 5, "TURBO"), -1);
}

// ---- FAN Tests ----

TEST(LookupTablesTest, FanByteToString_Auto) {
    EXPECT_STREQ(lookupByteMapValue(FAN_MAP, FAN, 6, 0x00), "AUTO");
}

TEST(LookupTablesTest, FanByteToString_Quiet) {
    EXPECT_STREQ(lookupByteMapValue(FAN_MAP, FAN, 6, 0x01), "QUIET");
}

TEST(LookupTablesTest, FanByteToString_Speed4) {
    EXPECT_STREQ(lookupByteMapValue(FAN_MAP, FAN, 6, 0x06), "4");
}

// ---- VANE Tests ----

TEST(LookupTablesTest, VaneByteToString_Auto) {
    EXPECT_STREQ(lookupByteMapValue(VANE_MAP, VANE, 7, 0x00), "AUTO");
}

TEST(LookupTablesTest, VaneByteToString_Swing) {
    EXPECT_STREQ(lookupByteMapValue(VANE_MAP, VANE, 7, 0x07), "SWING");
}

// ---- TEMP Tests (int version) ----

TEST(LookupTablesTest, TempByteToInt_31) {
    int result = lookupByteMapValue_int(TEMP_MAP, TEMP, 16, 0x00);
    EXPECT_EQ(result, 31);
}

TEST(LookupTablesTest, TempByteToInt_16) {
    int result = lookupByteMapValue_int(TEMP_MAP, TEMP, 16, 0x0F);
    EXPECT_EQ(result, 16);
}

TEST(LookupTablesTest, TempIndexFromValue_24) {
    int idx = lookupByteMapIndex_int(TEMP_MAP, 16, 24);
    EXPECT_EQ(idx, 7); // TEMP_MAP[7] = 24
}

TEST(LookupTablesTest, TempIndexFromValue_NotFound) {
    int idx = lookupByteMapIndex_int(TEMP_MAP, 16, 99);
    EXPECT_EQ(idx, -1);
}

// ---- POWER Tests ----

TEST(LookupTablesTest, PowerByteToString_Off) {
    EXPECT_STREQ(lookupByteMapValue(POWER_MAP, POWER, 2, 0x00), "OFF");
}

TEST(LookupTablesTest, PowerByteToString_On) {
    EXPECT_STREQ(lookupByteMapValue(POWER_MAP, POWER, 2, 0x01), "ON");
}

// ---- Table Consistency Tests ----

TEST(LookupTablesTest, ModeTableConsistency_AllBytesUnique) {
    for (int i = 0; i < 5; i++) {
        for (int j = i + 1; j < 5; j++) {
            EXPECT_NE(MODE[i], MODE[j])
                << "MODE bytes at index " << i << " and " << j << " collide";
        }
    }
}

TEST(LookupTablesTest, FanTableConsistency_AllBytesUnique) {
    for (int i = 0; i < 6; i++) {
        for (int j = i + 1; j < 6; j++) {
            EXPECT_NE(FAN[i], FAN[j])
                << "FAN bytes at index " << i << " and " << j << " collide";
        }
    }
}

TEST(LookupTablesTest, VaneTableConsistency_AllBytesUnique) {
    for (int i = 0; i < 7; i++) {
        for (int j = i + 1; j < 7; j++) {
            EXPECT_NE(VANE[i], VANE[j])
                << "VANE bytes at index " << i << " and " << j << " collide";
        }
    }
}

// ---- WIDEVANE Tests ----

TEST(LookupTablesTest, WideVaneByteToString_Swing) {
    EXPECT_STREQ(lookupByteMapValue(WIDEVANE_MAP, WIDEVANE, 8, 0x0c), "SWING");
}

// ---- AIRFLOW CONTROL Tests ----

TEST(LookupTablesTest, AirflowControlByteToString_Even) {
    EXPECT_STREQ(lookupByteMapValue(AIRFLOW_CONTROL_MAP, AIRFLOW_CONTROL, 3, 0x00), "EVEN");
}

TEST(LookupTablesTest, AirflowControlByteToString_Direct) {
    EXPECT_STREQ(lookupByteMapValue(AIRFLOW_CONTROL_MAP, AIRFLOW_CONTROL, 3, 0x02), "DIRECT");
}
