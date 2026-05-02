/// test_calculate_temp.cpp — Tests de non-régression pour calculateTemperatureSetting()
/// Deps: cn105_types.h, esphome_stubs.h
#include <gtest/gtest.h>
#include <cmath>
#include "esphome_stubs.h"
#include "cn105_types.h"

// Standalone reimplementation of helpers needed by calculateTemperatureSetting
static int lookupByteMapIndex_int(const int valuesMap[], int len, int lookupValue) {
    for (int i = 0; i < len; i++) {
        if (valuesMap[i] == lookupValue) {
            return i;
        }
    }
    return -1;
}

// Standalone reimplementation of calculateTemperatureSetting (from utils.cpp)
// tempMode parameter replaces the `this->tempMode` member access
static float calculateTemperatureSetting(float setting, bool tempMode) {
    if (!tempMode) {
        return lookupByteMapIndex_int(TEMP_MAP, 16, (int)(setting + 0.5)) > -1 ? setting : TEMP_MAP[0];
    } else {
        setting = std::round(2.0f * setting) / 2.0f;  // Round to the nearest half-degree.
        return setting < 10 ? 10 : (setting > 31 ? 31 : setting);
    }
}

// ========================================================
// Encoding A (tempMode = false) — uses TEMP_MAP lookup
// ========================================================

TEST(CalculateTempTest, EncodingA_ValidTemp_24) {
    // 24 is in TEMP_MAP → should return 24.0
    float result = calculateTemperatureSetting(24.0f, false);
    EXPECT_FLOAT_EQ(result, 24.0f);
}

TEST(CalculateTempTest, EncodingA_ValidTemp_16_Min) {
    float result = calculateTemperatureSetting(16.0f, false);
    EXPECT_FLOAT_EQ(result, 16.0f);
}

TEST(CalculateTempTest, EncodingA_ValidTemp_31_Max) {
    float result = calculateTemperatureSetting(31.0f, false);
    EXPECT_FLOAT_EQ(result, 31.0f);
}

TEST(CalculateTempTest, EncodingA_InvalidTemp_ReturnsFirstEntry) {
    // 99 is NOT in TEMP_MAP → returns TEMP_MAP[0] = 31
    float result = calculateTemperatureSetting(99.0f, false);
    EXPECT_FLOAT_EQ(result, static_cast<float>(TEMP_MAP[0]));
}

TEST(CalculateTempTest, EncodingA_HalfDegree_RoundsToNearest) {
    // 22.5 → rounds to 23 via (int)(22.5 + 0.5) = 23 → found in TEMP_MAP
    float result = calculateTemperatureSetting(22.5f, false);
    EXPECT_FLOAT_EQ(result, 22.5f); // returns the input if lookup succeeds
}

TEST(CalculateTempTest, EncodingA_HalfDegree_22_3_RoundsTo22) {
    // (int)(22.3 + 0.5) = (int)(22.8) = 22 → found in TEMP_MAP
    float result = calculateTemperatureSetting(22.3f, false);
    EXPECT_FLOAT_EQ(result, 22.3f); // returns the input since lookup for 22 succeeds
}

// ========================================================
// Encoding B (tempMode = true) — half-degree rounding + clamping
// ========================================================

TEST(CalculateTempTest, EncodingB_ExactHalfDegree_26_5) {
    float result = calculateTemperatureSetting(26.5f, true);
    EXPECT_FLOAT_EQ(result, 26.5f);
}

TEST(CalculateTempTest, EncodingB_RoundsToHalfDegree) {
    // 26.3 → round(2 * 26.3) / 2 = round(52.6) / 2 = 53 / 2 = 26.5
    float result = calculateTemperatureSetting(26.3f, true);
    EXPECT_FLOAT_EQ(result, 26.5f);
}

TEST(CalculateTempTest, EncodingB_ClampsMin_9) {
    float result = calculateTemperatureSetting(9.0f, true);
    EXPECT_FLOAT_EQ(result, 10.0f); // clamped to 10
}

TEST(CalculateTempTest, EncodingB_ClampsMin_Negative) {
    float result = calculateTemperatureSetting(-5.0f, true);
    EXPECT_FLOAT_EQ(result, 10.0f); // clamped to 10
}

TEST(CalculateTempTest, EncodingB_ClampsMax_32) {
    float result = calculateTemperatureSetting(32.0f, true);
    EXPECT_FLOAT_EQ(result, 31.0f); // clamped to 31
}

TEST(CalculateTempTest, EncodingB_ClampsMax_50) {
    float result = calculateTemperatureSetting(50.0f, true);
    EXPECT_FLOAT_EQ(result, 31.0f); // clamped to 31
}

TEST(CalculateTempTest, EncodingB_ExactMin_10) {
    float result = calculateTemperatureSetting(10.0f, true);
    EXPECT_FLOAT_EQ(result, 10.0f); // boundary — no clamp
}

TEST(CalculateTempTest, EncodingB_ExactMax_31) {
    float result = calculateTemperatureSetting(31.0f, true);
    EXPECT_FLOAT_EQ(result, 31.0f); // boundary — no clamp
}
