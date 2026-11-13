/// test_setpoint_persistence.cpp — Regression tests for HEAT_COOL band
/// persistence across reboots (opt-in `supports.restore_setpoints`).
///
/// Bug: the dual-setpoint band is synthetic — a Mitsubishi unit stores only a
/// single setpoint and runs HEAT_COOL as hardware AUTO, so the user's low/high
/// band lives only in RAM. On reboot, setup() NANs target_temperature_low/high
/// (componentEntries.cpp) and the unit's hardware-AUTO report drives the entity
/// back to CLIMATE_MODE_AUTO (hp_readings.cpp), losing both the HEAT_COOL mode
/// and the band.
///
/// Fix (opt-in): persist {version, mode, low, high} to flash on every applied
/// control, and in setup() re-seed HEAT_COOL + the band before the first
/// settings read. The existing AUTO-branch guard in checkPowerAndModeSettings()
/// then keeps HEAT_COOL while hardware reports HEAT, COOL or AUTO; OFF
/// and the other supported mode transitions remain available.
///
/// Uses the production persisted record and restore decision; flash I/O stays in the profile.
#include <gtest/gtest.h>
#include "heatpump_profile_protocol.h"

using esphome::cn105::HeatPumpProtocol;
using SetpointState = HeatPumpProtocol::SetpointState;
using namespace esphome::climate;
constexpr uint8_t kStateVersion = 1;
static_assert(sizeof(SetpointState) == 10, "Preserve the existing packed flash record");

// ── Happy path: a saved HEAT_COOL band is restored exactly ──

TEST(SetpointPersistenceTest, RestoresHeatCoolBand) {
    auto s = HeatPumpProtocol::saved_setpoint_state(CLIMATE_MODE_HEAT_COOL, 20.0f, 24.0f);
    auto r = HeatPumpProtocol::restored_setpoint_state(/*restoreEnabled=*/true, /*loaded=*/true, s, /*dual=*/true);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r->mode, static_cast<uint8_t>(CLIMATE_MODE_HEAT_COOL));
    EXPECT_FLOAT_EQ(r->target_low, 20.0f);
    EXPECT_FLOAT_EQ(r->target_high, 24.0f);
}

TEST(SetpointPersistenceTest, SaveRestoreRoundTripIsLossless) {
    auto s = HeatPumpProtocol::saved_setpoint_state(CLIMATE_MODE_HEAT_COOL, 18.5f, 23.5f);
    auto r = HeatPumpProtocol::restored_setpoint_state(true, true, s, true);
    ASSERT_TRUE(r.has_value());
    EXPECT_FLOAT_EQ(r->target_low, 18.5f);
    EXPECT_FLOAT_EQ(r->target_high, 23.5f);
}

// ── Opt-in: nothing happens unless the feature is enabled ──

TEST(SetpointPersistenceTest, DisabledDoesNotRestore) {
    auto s = HeatPumpProtocol::saved_setpoint_state(CLIMATE_MODE_HEAT_COOL, 20.0f, 24.0f);
    EXPECT_FALSE(HeatPumpProtocol::restored_setpoint_state(/*restoreEnabled=*/false, true, s, true).has_value());
}

TEST(SetpointPersistenceTest, NoSavedStateDoesNotRestore) {
    SetpointState empty{};  // load() failed
    EXPECT_FALSE(HeatPumpProtocol::restored_setpoint_state(true, /*loaded=*/false, empty, true).has_value());
}

// ── Guards: never restore a stale / non-HEAT_COOL / invalid record ──

TEST(SetpointPersistenceTest, VersionMismatchDoesNotRestore) {
    SetpointState s{kStateVersion + 1u, static_cast<uint8_t>(CLIMATE_MODE_HEAT_COOL), 20.0f, 24.0f};
    EXPECT_FALSE(HeatPumpProtocol::restored_setpoint_state(true, true, s, true).has_value());
}

TEST(SetpointPersistenceTest, SingleModeDoesNotRestore) {
    // Single modes (HEAT/COOL) are persisted by the heat pump itself, so we
    // must not force HEAT_COOL when the last saved mode was single.
    auto s = HeatPumpProtocol::saved_setpoint_state(CLIMATE_MODE_COOL, 22.0f, 22.0f);
    EXPECT_FALSE(HeatPumpProtocol::restored_setpoint_state(true, true, s, true).has_value());
}

TEST(SetpointPersistenceTest, NanBandDoesNotRestore) {
    auto s = HeatPumpProtocol::saved_setpoint_state(CLIMATE_MODE_HEAT_COOL, NAN, 24.0f);
    EXPECT_FALSE(HeatPumpProtocol::restored_setpoint_state(true, true, s, true).has_value());
}

TEST(SetpointPersistenceTest, DualNotSupportedDoesNotRestore) {
    auto s = HeatPumpProtocol::saved_setpoint_state(CLIMATE_MODE_HEAT_COOL, 20.0f, 24.0f);
    EXPECT_FALSE(HeatPumpProtocol::restored_setpoint_state(true, true, s, /*dual=*/false).has_value());
}
