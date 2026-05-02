/// test_types.cpp — Tests de non-régression pour les structs du protocole CN105
/// Deps: cn105_types.h, esphome_stubs.h
#include <gtest/gtest.h>
#include "esphome_stubs.h"
#include "cn105_types.h"

// ========================================================
// heatpumpSettings tests
// ========================================================

TEST(HeatpumpSettingsTest, ResetSettings_NullsAndDefaults) {
    heatpumpSettings s{};
    s.power = "ON";
    s.mode = "COOL";
    s.temperature = 24.0f;
    s.fan = "AUTO";
    s.vane = "SWING";
    s.wideVane = "|";

    s.resetSettings();

    EXPECT_EQ(s.power, nullptr);
    EXPECT_EQ(s.mode, nullptr);
    EXPECT_FLOAT_EQ(s.temperature, -1.0f);
    EXPECT_FLOAT_EQ(s.dual_low_target, -100.0f);
    EXPECT_FLOAT_EQ(s.dual_high_target, -100.0f);
    EXPECT_EQ(s.fan, nullptr);
    EXPECT_EQ(s.vane, nullptr);
    EXPECT_EQ(s.wideVane, nullptr);
}

TEST(HeatpumpSettingsTest, EqualityOperator_IdenticalSettings) {
    heatpumpSettings a{};
    a.power = "ON";
    a.mode = "HEAT";
    a.temperature = 22.0f;
    a.fan = "AUTO";
    a.vane = "AUTO";
    a.wideVane = "|";

    heatpumpSettings b{};
    b.power = "ON";
    b.mode = "HEAT";
    b.temperature = 22.0f;
    b.fan = "AUTO";
    b.vane = "AUTO";
    b.wideVane = "|";

    EXPECT_TRUE(a == b);
}

TEST(HeatpumpSettingsTest, EqualityOperator_DifferentTemp) {
    heatpumpSettings a{};
    a.power = "ON";
    a.mode = "HEAT";
    a.temperature = 22.0f;
    a.fan = "AUTO";
    a.vane = "AUTO";
    a.wideVane = "|";

    heatpumpSettings b = a;
    b.temperature = 24.0f;

    EXPECT_FALSE(a == b);
}

TEST(HeatpumpSettingsTest, EqualityOperator_DifferentMode) {
    heatpumpSettings a{};
    a.power = "ON";
    a.mode = "HEAT";
    a.temperature = 22.0f;
    a.fan = "AUTO";
    a.vane = "AUTO";
    a.wideVane = "|";

    heatpumpSettings b = a;
    b.mode = "COOL";

    // operator== compares const char* pointers, not string content
    // Since "HEAT" and "COOL" are different string literals, pointers differ
    EXPECT_FALSE(a == b);
}

TEST(HeatpumpSettingsTest, InequalityOperator_Works) {
    heatpumpSettings a{};
    a.power = "ON";
    a.mode = "HEAT";
    a.temperature = 22.0f;
    a.fan = "AUTO";
    a.vane = "AUTO";
    a.wideVane = "|";

    heatpumpSettings b = a;
    b.temperature = 25.0f;

    EXPECT_TRUE(a != b);
}

// ========================================================
// wantedHeatpumpSettings tests
// ========================================================

TEST(WantedHeatpumpSettingsTest, ResetSettings_IncludesFlags) {
    wantedHeatpumpSettings ws{};
    ws.hasChanged = true;
    ws.hasBeenSent = true;
    ws.power = "ON";
    ws.temperature = 24.0f;

    ws.resetSettings();

    EXPECT_FALSE(ws.hasChanged);
    EXPECT_FALSE(ws.hasBeenSent);
    EXPECT_EQ(ws.power, nullptr);
    EXPECT_FLOAT_EQ(ws.temperature, -1.0f);
}

// ========================================================
// heatpumpStatus tests
// ========================================================

TEST(HeatpumpStatusTest, EqualityWithNaN) {
    // NaN == NaN should be true in our custom operator
    heatpumpStatus a{};
    a.roomTemperature = NAN;
    a.outsideAirTemperature = NAN;
    a.operating = false;
    a.compressorFrequency = 0;
    a.inputPower = 0;
    a.kWh = 0;
    a.runtimeHours = 0;

    heatpumpStatus b = a;

    EXPECT_TRUE(a == b) << "Two statuses with NaN should be equal";
}

TEST(HeatpumpStatusTest, InequalityWithDifferentTemp) {
    heatpumpStatus a{};
    a.roomTemperature = 21.0f;
    a.outsideAirTemperature = NAN;
    a.operating = false;
    a.compressorFrequency = 0;
    a.inputPower = 0;
    a.kWh = 0;
    a.runtimeHours = 0;

    heatpumpStatus b = a;
    b.roomTemperature = 22.0f;

    EXPECT_TRUE(a != b);
}

TEST(HeatpumpStatusTest, InequalityWithOperatingDiff) {
    heatpumpStatus a{};
    a.roomTemperature = 21.0f;
    a.outsideAirTemperature = NAN;
    a.operating = false;
    a.compressorFrequency = 0;
    a.inputPower = 0;
    a.kWh = 0;
    a.runtimeHours = 0;

    heatpumpStatus b = a;
    b.operating = true;

    EXPECT_TRUE(a != b);
}

// ========================================================
// heatpumpRunStates tests
// ========================================================

TEST(HeatpumpRunStatesTest, ResetSettings) {
    heatpumpRunStates rs{};
    rs.air_purifier = 1;
    rs.night_mode = 1;
    rs.circulator = 1;
    rs.airflow_control = "DIRECT";

    rs.resetSettings();

    EXPECT_EQ(rs.air_purifier, -1);
    EXPECT_EQ(rs.night_mode, -1);
    EXPECT_EQ(rs.circulator, -1);
    EXPECT_EQ(rs.airflow_control, nullptr);
}

TEST(HeatpumpRunStatesTest, EqualityOperator) {
    heatpumpRunStates a{};
    a.air_purifier = 1;
    a.night_mode = 0;
    a.circulator = 1;
    a.airflow_control = "EVEN";

    heatpumpRunStates b = a;

    EXPECT_TRUE(a == b);
}

TEST(WantedRunStatesTest, ResetSettings_IncludesFlags) {
    wantedHeatpumpRunStates wrs{};
    wrs.hasChanged = true;
    wrs.hasBeenSent = true;
    wrs.air_purifier = 1;

    wrs.resetSettings();

    EXPECT_FALSE(wrs.hasChanged);
    EXPECT_FALSE(wrs.hasBeenSent);
    EXPECT_EQ(wrs.air_purifier, -1);
}
