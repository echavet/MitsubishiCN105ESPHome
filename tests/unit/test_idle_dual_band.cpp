/// test_idle_dual_band.cpp — a dual-setpoint band survives OFF and FAN_ONLY.
///
/// While the climate is OFF or FAN_ONLY the band waits for the next HEAT_COOL. The
/// unit still reports a single setpoint: whatever it last ran, which in HEAT_COOL is
/// usually one band edge. Re-centring the band on the rounded midpoint whenever that
/// setpoint differs moved a 20-23.5 band to 20.25-23.75 when a window automation
/// switched a unit off (2026-09-26), and HEAT_COOL then kept the shifted band.
#include <gtest/gtest.h>
#include <cmath>
#include "esphome_stubs.h"
#include "cn105_protocol.h"
#include "heatpump_profile_protocol.h"

using cn105_protocol::DualBand;
using cn105_protocol::idle_dual_band;

namespace {
// calculateTemperatureSetting, encoding B: nearest half degree
float halfDegree(float s) { return std::round(2.0f * s) / 2.0f; }

DualBand idle(float low, float high, float reported) {
    return idle_dual_band(low, high, reported, halfDegree);
}

// Celsius target storage for HeatPumpProtocol::reconcile_target_temperatures.
struct Targets {
    float low = NAN;
    float high = NAN;
    float single = NAN;
    float getTargetTemperatureLow() { return low; }
    float getTargetTemperatureHigh() { return high; }
    void setTargetTemperatureLow(float v) { low = v; }
    void setTargetTemperatureHigh(float v) { high = v; }
    void setTargetTemperature(float v) { single = v; }
    float calculateTemperatureSetting(float v) {
        return esphome::cn105::HeatPumpProtocol::calculate_temperature_setting(v, true);
    }
};
}  // namespace

TEST(IdleDualBandTest, DefinedBand_OffGridMidpoint_IsKept) {
    // Living Room: band 20-23.5 (midpoint 21.75 rounds to 22), unit last ran AUTO
    // toward the low edge and reports 20.
    const DualBand band = idle(20.0f, 23.5f, 20.0f);
    EXPECT_FLOAT_EQ(band.low, 20.0f);
    EXPECT_FLOAT_EQ(band.high, 23.5f);
}

TEST(IdleDualBandTest, DefinedBand_IsKept_WhateverTheUnitReports) {
    for (float reported : {10.0f, 20.0f, 21.5f, 22.0f, 23.5f, 31.0f}) {
        const DualBand band = idle(20.5f, 23.0f, reported);
        EXPECT_FLOAT_EQ(band.low, 20.5f) << reported;
        EXPECT_FLOAT_EQ(band.high, 23.0f) << reported;
    }
}

TEST(IdleDualBandTest, NoBand_StartsAtTheReportedSetpoint) {
    const DualBand band = idle(NAN, NAN, 21.0f);
    EXPECT_FLOAT_EQ(band.low, 21.0f);
    EXPECT_FLOAT_EQ(band.high, 21.0f);
}

TEST(IdleDualBandTest, MissingEdge_IsFilledAndCentredOnTheReportedSetpoint) {
    // Upstream behaviour for a half-known band is unchanged: the missing edge takes
    // the reported setpoint, then the band is centred on the rounded midpoint.
    DualBand band = idle(NAN, 23.0f, 20.0f);
    EXPECT_FLOAT_EQ(band.low, 20.0f);
    EXPECT_FLOAT_EQ(band.high, 23.0f);

    band = idle(20.0f, NAN, 22.0f);
    EXPECT_FLOAT_EQ(band.low, 20.0f);
    EXPECT_FLOAT_EQ(band.high, 22.0f);
}

TEST(IdleDualBandTest, SettingsPacketWhileOffOrFanOnly_KeepsTheBand) {
    // The production reconcile path for a settings packet: Living Room switched off
    // by the patio-door automation with a 20-23.5 band, unit reporting 20.
    for (auto mode : {esphome::climate::CLIMATE_MODE_OFF, esphome::climate::CLIMATE_MODE_FAN_ONLY}) {
        Targets t;
        t.low = 20.0f;
        t.high = 23.5f;
        esphome::cn105::HeatPumpProtocol::reconcile_target_temperatures(t, mode, true, 20.0f);
        EXPECT_FLOAT_EQ(t.low, 20.0f) << mode;
        EXPECT_FLOAT_EQ(t.high, 23.5f) << mode;
    }
}
