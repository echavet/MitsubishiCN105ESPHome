#include <gtest/gtest.h>
#include "heat_cool_controller.h"
#include "heatpump_profile_protocol.h"

using namespace cn105_protocol;
using namespace esphome::climate;
using esphome::cn105::HeatPumpProtocol;

namespace {
class HeatCoolControllerTest : public ::testing::Test {
protected:
    HeatCoolController controller;
    float room = 22, low = 21, high = 23;
    const char *reported = "AUTO";
    bool pending = false;
    uint32_t now = 100, poll_ms = 2000;

    std::optional<HeatCoolCommand> update(bool user_command = false) {
        return controller.update(room, low, high, reported, pending, user_command, now, poll_ms);
    }
    void expect_command(HeatCoolDrive drive, float setpoint, bool user_command = false) {
        const auto command = update(user_command);
        ASSERT_TRUE(command.has_value());
        EXPECT_EQ(command->drive, drive);
        EXPECT_FLOAT_EQ(command->setpoint, setpoint);
    }
    void report_direction(uint8_t byte) {
        const auto submode = lookup_value_opt(AUTO_SUB_MODE_MAP, AUTO_SUB_MODE, 7, byte);
        ASSERT_TRUE(submode.has_value());
        controller.direction_report(auto_direction(*submode), now, poll_ms);
    }
};

TEST_F(HeatCoolControllerTest, AmbiguousAutoReportsDoNotEnableControlByDefault) {
    for (auto byte : {0x01, 0x03}) {
        report_direction(byte); // synthetic 0x09 payload field from issue #768
        EXPECT_FALSE(HeatPumpProtocol::manages_heat_cool(false, true, CLIMATE_MODE_HEAT_COOL));
        EXPECT_FLOAT_EQ(HeatPumpProtocol::deadband_setting(21.5f, low, high), 21.5f);
    }
    EXPECT_TRUE(HeatPumpProtocol::manages_heat_cool(true, true, CLIMATE_MODE_HEAT_COOL));
    EXPECT_FALSE(HeatPumpProtocol::manages_heat_cool(true, false, CLIMATE_MODE_HEAT_COOL));
    for (auto mode : {CLIMATE_MODE_OFF, CLIMATE_MODE_AUTO, CLIMATE_MODE_HEAT, CLIMATE_MODE_COOL, CLIMATE_MODE_FAN_ONLY, CLIMATE_MODE_DRY})
        EXPECT_FALSE(HeatPumpProtocol::manages_heat_cool(true, true, mode));
}

TEST_F(HeatCoolControllerTest, InBandHeadFollowsDirectionWithoutRequestingExplicitHeatOrCool) {
    expect_command(HeatCoolDrive::AUTO, 22);
    report_direction(0x02);
    expect_command(HeatCoolDrive::AUTO, 21);
    EXPECT_FALSE(update());
    // Another head calls for cooling. This head remains in band.
    room = 22.5f;
    report_direction(0x01);
    expect_command(HeatCoolDrive::AUTO, 23);
    report_direction(0x01);
    EXPECT_FALSE(update());
}

TEST_F(HeatCoolControllerTest, MissingDirectionExpiresOnceEvenWithConstantRoomAndAcrossWrap) {
    now = UINT32_MAX - 30000;
    report_direction(0x01);
    expect_command(HeatCoolDrive::AUTO, 23);
    now += 50000;
    report_direction(0x01); // a valid identical report refreshes the clock
    now += 59999;
    EXPECT_FALSE(update());
    ++now;
    expect_command(HeatCoolDrive::AUTO, 22);
    EXPECT_FALSE(update());
    report_direction(0x01);
    expect_command(HeatCoolDrive::AUTO, 23);
}

TEST_F(HeatCoolControllerTest, SlowPollingAllowsThreeIntervalsBeforeExpiry) {
    poll_ms = 30000;
    report_direction(0x01);
    expect_command(HeatCoolDrive::AUTO, 23);
    now += 89999;
    EXPECT_FALSE(update());
    ++now;
    expect_command(HeatCoolDrive::AUTO, 22);
}

TEST_F(HeatCoolControllerTest, RoomSourceStepAndRecoveryEmitOnceAtEachTransition) {
    room = NAN;
    EXPECT_FALSE(update());
    room = 22;
    expect_command(HeatCoolDrive::AUTO, 22);
    room = 20.5f;
    expect_command(HeatCoolDrive::HEAT, 21);
    reported = "HEAT";
    EXPECT_FALSE(update());
    room = 21;
    now += 599999;
    EXPECT_FALSE(update());
    ++now;
    expect_command(HeatCoolDrive::AUTO, 22);
    reported = "AUTO";
    room = 23.5f;
    expect_command(HeatCoolDrive::COOL, 23);
}

TEST_F(HeatCoolControllerTest, NewBandCanReleaseDwellAndStaleReadbackCannotOverrideIt) {
    room = 20;
    expect_command(HeatCoolDrive::HEAT, 21);
    low = 19;
    expect_command(HeatCoolDrive::AUTO, 21, true);
    controller.await_user_readback(21);
    room = 18;
    pending = true;
    EXPECT_FALSE(update());
    pending = false;
    controller.settings_report("HEAT", 21);
    controller.settings_report("AUTO", 22);
    EXPECT_FALSE(update());
    controller.settings_report("AUTO", 21);
    expect_command(HeatCoolDrive::HEAT, 19);
}

TEST_F(HeatCoolControllerTest, MissingReadbackHasBoundedWait) {
    expect_command(HeatCoolDrive::AUTO, 22, true);
    controller.await_user_readback(22);
    room = 20;
    now += 59999;
    EXPECT_FALSE(update());
    ++now;
    expect_command(HeatCoolDrive::HEAT, 21);
}

TEST_F(HeatCoolControllerTest, ConflictingModeReadbackIsRateLimited) {
    expect_command(HeatCoolDrive::AUTO, 22);
    reported = "COOL";
    now += 59999;
    EXPECT_FALSE(update());
    ++now;
    expect_command(HeatCoolDrive::AUTO, 22);
    ++now;
    EXPECT_FALSE(update());
    now += 59999;
    expect_command(HeatCoolDrive::AUTO, 22);
}

TEST_F(HeatCoolControllerTest, RebootAdoptsRunningModeAndDisconnectClearsDirection) {
    for (auto mode : {HeatCoolDrive::AUTO, HeatCoolDrive::HEAT, HeatCoolDrive::COOL}) {
        controller = {};
        reported = heat_cool_mode_name(mode);
        float target = 22;
        if (mode == HeatCoolDrive::HEAT) target = low;
        if (mode == HeatCoolDrive::COOL) target = high;
        controller.adopt(reported, target, now);
        EXPECT_EQ(controller.drive(), mode);
        EXPECT_FALSE(update());
    }
    controller = {};
    reported = nullptr;
    EXPECT_FALSE(update());
    controller.adopt("AUTO", 23, now);
    reported = "AUTO";
    expect_command(HeatCoolDrive::AUTO, 22);
}
TEST_F(HeatCoolControllerTest, StaleDirectionCannotSuppressAnOutOfBandRequest) {
    report_direction(0x02);
    expect_command(HeatCoolDrive::AUTO, 21);
    room = 20.5f;
    EXPECT_FALSE(update());
    now += 60000;
    expect_command(HeatCoolDrive::HEAT, 21);
}

TEST_F(HeatCoolControllerTest, ReturningAfterLostReportsDoesNotReviveOldEdge) {
    report_direction(0x01);
    expect_command(HeatCoolDrive::AUTO, 23);
    now += 120000;
    report_direction(0x00); // first report after the gap has no direction
    expect_command(HeatCoolDrive::AUTO, 22);
}

TEST_F(HeatCoolControllerTest, HardwareOffReleasesBandControlAndPreservesBothEdges) {
    report_direction(0x01);
    expect_command(HeatCoolDrive::AUTO, 23);
    const auto mode = HeatPumpProtocol::resolve_mode("OFF", "AUTO", CLIMATE_MODE_HEAT_COOL, true);
    EXPECT_EQ(mode, CLIMATE_MODE_OFF);
    EXPECT_FALSE(HeatPumpProtocol::manages_heat_cool(true, true, mode));
    const auto band = idle_dual_band(20, 23.5f, 23, [](float t) { return std::round(t * 2) / 2; });
    EXPECT_EQ(band.low, 20);
    EXPECT_EQ(band.high, 23.5f);
}

}  // namespace
