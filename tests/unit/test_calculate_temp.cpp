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

TEST(CalculateTempTest, EncodingA_ClampsMin_Below16) {
    // TEMP_MAP is stored 31..16, so a lookup miss used to return TEMP_MAP[0] = 31°C.
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(15.0f, false), 16.0f);
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(13.0f, false), 16.0f);
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(10.0f, false), 16.0f);
}

TEST(CalculateTempTest, EncodingA_ClampsMin_15_4_DoesNotJumpTo31) {
    // (int)(15.4 + 0.5) = 15, which is not in TEMP_MAP. Must clamp to 16, not TEMP_MAP[0].
    float result = HeatPumpProtocol::calculate_temperature_setting(15.4f, false);
    EXPECT_FLOAT_EQ(result, 16.0f);
}

TEST(CalculateTempTest, EncodingA_ClampsMin_15_5) {
    // 15.5 would previously round to 16 and pass through unclamped. Clamp first → 16.
    float result = HeatPumpProtocol::calculate_temperature_setting(15.5f, false);
    EXPECT_FLOAT_EQ(result, 16.0f);
}

TEST(CalculateTempTest, EncodingA_ClampsMax_Above31) {
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(32.0f, false), 31.0f);
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(50.0f, false), 31.0f);
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(99.0f, false), 31.0f);
}

TEST(CalculateTempTest, EncodingA_InRangeWholeUnchanged) {
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(16.0f, false), 16.0f);
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(20.0f, false), 20.0f);
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(24.0f, false), 24.0f);
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(31.0f, false), 31.0f);
}

TEST(CalculateTempTest, EncodingA_InRangeHalfUnchanged) {
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(16.5f, false), 16.5f);
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(22.5f, false), 22.5f);
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(30.5f, false), 30.5f);
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

TEST(CalculateTempTest, EncodingA_NaN_Passthrough) {
    EXPECT_TRUE(std::isnan(HeatPumpProtocol::calculate_temperature_setting(NAN, false)));
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

TEST(CalculateTempTest, EncodingB_BelowEncodingAMin_Unchanged) {
    // Encoding B allows 10–31. Values below Encoding A's 16°C floor must not be raised.
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(10.0f, true), 10.0f);
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(15.0f, true), 15.0f);
}

TEST(CalculateTempTest, EncodingB_NaN_Passthrough) {
    EXPECT_TRUE(std::isnan(HeatPumpProtocol::calculate_temperature_setting(NAN, true)));
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

TEST(CalculateTempTest, A24na_InRangeUnchanged) {
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(16.0f, false, true), 16.0f);
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(24.0f, false, true), 24.0f);
    EXPECT_FLOAT_EQ(HeatPumpProtocol::calculate_temperature_setting(31.0f, false, true), 31.0f);
}

TEST(CalculateTempTest, A24na_NaN_Passthrough) {
    EXPECT_TRUE(std::isnan(HeatPumpProtocol::calculate_temperature_setting(NAN, false, true)));
}
