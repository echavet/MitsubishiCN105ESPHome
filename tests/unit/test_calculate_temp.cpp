/// test_calculate_temp.cpp — Regression tests for the production temperature calculation
/// Deps: cn105_types.h, esphome_stubs.h
#include <gtest/gtest.h>
#include <cmath>
#include "esphome_stubs.h"
#include "cn105_types.h"

#include "heatpump_profile_protocol.h"
using esphome::cn105::HeatPumpProtocol;

// ========================================================
// Encoding A (tempMode = false) — uses TEMP_MAP lookup
// ========================================================

TEST(CalculateTempTest, EncodingA_ValidTemp_24) {
    // 24 is in TEMP_MAP → should return 24.0
    float result = HeatPumpProtocol::calculate_temperature_setting(24.0f, false);
    EXPECT_FLOAT_EQ(result, 24.0f);
}

TEST(CalculateTempTest, EncodingA_ValidTemp_16_Min) {
    float result = HeatPumpProtocol::calculate_temperature_setting(16.0f, false);
    EXPECT_FLOAT_EQ(result, 16.0f);
}

TEST(CalculateTempTest, EncodingA_ValidTemp_31_Max) {
    float result = HeatPumpProtocol::calculate_temperature_setting(31.0f, false);
    EXPECT_FLOAT_EQ(result, 31.0f);
}

TEST(CalculateTempTest, EncodingA_InvalidTemp_ReturnsFirstEntry) {
    // 99 is NOT in TEMP_MAP → returns TEMP_MAP[0] = 31
    float result = HeatPumpProtocol::calculate_temperature_setting(99.0f, false);
    EXPECT_FLOAT_EQ(result, static_cast<float>(TEMP_MAP[0]));
}

TEST(CalculateTempTest, EncodingA_HalfDegree_RoundsToNearest) {
    // 22.5 → rounds to 23 via (int)(22.5 + 0.5) = 23 → found in TEMP_MAP
    float result = HeatPumpProtocol::calculate_temperature_setting(22.5f, false);
    EXPECT_FLOAT_EQ(result, 22.5f); // returns the input if lookup succeeds
}

TEST(CalculateTempTest, EncodingA_HalfDegree_22_3_RoundsTo22) {
    // (int)(22.3 + 0.5) = (int)(22.8) = 22 → found in TEMP_MAP
    float result = HeatPumpProtocol::calculate_temperature_setting(22.3f, false);
    EXPECT_FLOAT_EQ(result, 22.3f); // returns the input since lookup for 22 succeeds
}

// ========================================================
// Encoding B (tempMode = true) — half-degree rounding + clamping
// ========================================================

TEST(CalculateTempTest, EncodingB_ExactHalfDegree_26_5) {
    float result = HeatPumpProtocol::calculate_temperature_setting(26.5f, true);
    EXPECT_FLOAT_EQ(result, 26.5f);
}

TEST(CalculateTempTest, EncodingB_RoundsToHalfDegree) {
    // 26.3 → round(2 * 26.3) / 2 = round(52.6) / 2 = 53 / 2 = 26.5
    float result = HeatPumpProtocol::calculate_temperature_setting(26.3f, true);
    EXPECT_FLOAT_EQ(result, 26.5f);
}

TEST(CalculateTempTest, EncodingB_ClampsMin_9) {
    float result = HeatPumpProtocol::calculate_temperature_setting(9.0f, true);
    EXPECT_FLOAT_EQ(result, 10.0f); // clamped to 10
}

TEST(CalculateTempTest, EncodingB_ClampsMin_Negative) {
    float result = HeatPumpProtocol::calculate_temperature_setting(-5.0f, true);
    EXPECT_FLOAT_EQ(result, 10.0f); // clamped to 10
}

TEST(CalculateTempTest, EncodingB_ClampsMax_32) {
    float result = HeatPumpProtocol::calculate_temperature_setting(32.0f, true);
    EXPECT_FLOAT_EQ(result, 31.0f); // clamped to 31
}

TEST(CalculateTempTest, EncodingB_ClampsMax_50) {
    float result = HeatPumpProtocol::calculate_temperature_setting(50.0f, true);
    EXPECT_FLOAT_EQ(result, 31.0f); // clamped to 31
}

TEST(CalculateTempTest, EncodingB_ExactMin_10) {
    float result = HeatPumpProtocol::calculate_temperature_setting(10.0f, true);
    EXPECT_FLOAT_EQ(result, 10.0f); // boundary — no clamp
}

TEST(CalculateTempTest, EncodingB_ExactMax_31) {
    float result = HeatPumpProtocol::calculate_temperature_setting(31.0f, true);
    EXPECT_FLOAT_EQ(result, 31.0f); // boundary — no clamp
}

// ========================================================
// MSZ-A24NA table (a24na = true)
// ========================================================

TEST(CalculateTempTest, A24na_ClampsMin) {
    float result = HeatPumpProtocol::calculate_temperature_setting(15.0f, false, true);
    EXPECT_FLOAT_EQ(result, 16.0f);
}

TEST(CalculateTempTest, A24na_ClampsMax) {
    float result = HeatPumpProtocol::calculate_temperature_setting(32.0f, false, true);
    EXPECT_FLOAT_EQ(result, 31.0f);
}

TEST(CalculateTempTest, A24na_RoundsToHalfDegree) {
    float result = HeatPumpProtocol::calculate_temperature_setting(16.3f, false, true);
    EXPECT_FLOAT_EQ(result, 16.5f);
}

TEST(CalculateTempTest, A24na_KeepsExactHalfDegree) {
    float result = HeatPumpProtocol::calculate_temperature_setting(21.5f, false, true);
    EXPECT_FLOAT_EQ(result, 21.5f);
}
