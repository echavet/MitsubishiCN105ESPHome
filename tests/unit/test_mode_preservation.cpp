/// test_mode_preservation.cpp — Regression tests for the HEAT_COOL mode-clobber
/// bug in checkPowerAndModeSettings (components/cn105/hp_readings.cpp).
///
/// Bug: a dual-setpoint unit driven in HEAT_COOL runs the heat pump in hardware
/// AUTO; the unit then reports its *active operating direction* ("HEAT"/"COOL")
/// in the settings packet. The unguarded HEAT/COOL branches used to overwrite
/// this->mode, dropping the user out of HEAT_COOL and collapsing the dual band.
/// Only the AUTO branch was guarded. The fix guards HEAT and COOL the same way,
/// gated on supports_dual_setpoint_ so genuine single-setpoint builds are
/// unaffected.
///
/// Exercises the production profile mode resolver.
#include <gtest/gtest.h>
#include "heatpump_profile_protocol.h"

using namespace esphome::climate;
using esphome::cn105::HeatPumpProtocol;


// ── The bug repro: in HEAT_COOL, a reported operating direction must not flip us ──

TEST(ModePreservationTest, HeatCool_Preserved_WhenUnitReportsCool) {
    EXPECT_EQ(HeatPumpProtocol::resolve_mode("ON", "COOL", CLIMATE_MODE_HEAT_COOL, /*dual=*/true),
              CLIMATE_MODE_HEAT_COOL);
}

TEST(ModePreservationTest, HeatCool_Preserved_WhenUnitReportsHeat) {
    EXPECT_EQ(HeatPumpProtocol::resolve_mode("ON", "HEAT", CLIMATE_MODE_HEAT_COOL, /*dual=*/true),
              CLIMATE_MODE_HEAT_COOL);
}

TEST(ModePreservationTest, HeatCool_Preserved_WhenUnitReportsAuto) {
    // Pins the pre-existing AUTO guard.
    EXPECT_EQ(HeatPumpProtocol::resolve_mode("ON", "AUTO", CLIMATE_MODE_HEAT_COOL, /*dual=*/true),
              CLIMATE_MODE_HEAT_COOL);
}

// ── A genuine user COOL/HEAT command still applies ──
// (processModeChange sets this->mode before this read-path runs, and the
// read-path is skipped entirely while a user demand is pending; once it clears,
// current already equals the user's chosen mode, so the guard does not trap it.)

TEST(ModePreservationTest, UserCool_StillApplies) {
    EXPECT_EQ(HeatPumpProtocol::resolve_mode("ON", "COOL", CLIMATE_MODE_COOL, /*dual=*/true),
              CLIMATE_MODE_COOL);
}

TEST(ModePreservationTest, UserHeat_StillApplies) {
    EXPECT_EQ(HeatPumpProtocol::resolve_mode("ON", "HEAT", CLIMATE_MODE_HEAT, /*dual=*/true),
              CLIMATE_MODE_HEAT);
}

// ── Single-setpoint builds are unaffected by the guard ──

TEST(ModePreservationTest, SingleSetpointBuild_CoolApplies) {
    EXPECT_EQ(HeatPumpProtocol::resolve_mode("ON", "COOL", CLIMATE_MODE_HEAT_COOL, /*dual=*/false),
              CLIMATE_MODE_COOL);
}

TEST(ModePreservationTest, SingleSetpointBuild_HeatApplies) {
    EXPECT_EQ(HeatPumpProtocol::resolve_mode("ON", "HEAT", CLIMATE_MODE_HEAT_COOL, /*dual=*/false),
              CLIMATE_MODE_HEAT);
}

// ── Other modes / power unchanged ──

TEST(ModePreservationTest, Dry_AppliesEvenFromHeatCool) {
    EXPECT_EQ(HeatPumpProtocol::resolve_mode("ON", "DRY", CLIMATE_MODE_HEAT_COOL, /*dual=*/true),
              CLIMATE_MODE_DRY);
}

TEST(ModePreservationTest, Fan_AppliesEvenFromHeatCool) {
    EXPECT_EQ(HeatPumpProtocol::resolve_mode("ON", "FAN", CLIMATE_MODE_HEAT_COOL, /*dual=*/true),
              CLIMATE_MODE_FAN_ONLY);
}

TEST(ModePreservationTest, PowerOff_AlwaysOff) {
    EXPECT_EQ(HeatPumpProtocol::resolve_mode("OFF", "COOL", CLIMATE_MODE_HEAT_COOL, /*dual=*/true),
              CLIMATE_MODE_OFF);
    EXPECT_EQ(HeatPumpProtocol::resolve_mode("OFF", "HEAT", CLIMATE_MODE_HEAT, /*dual=*/false),
              CLIMATE_MODE_OFF);
}
