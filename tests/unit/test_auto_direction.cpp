/// test_auto_direction.cpp — Regression tests for the HEAT_COOL/AUTO hvac_action
/// bug in CN105Climate::updateAction (components/cn105/climateControls.cpp).
///
/// Bug: in HEAT_COOL (hardware AUTO) the action was inferred from the room
/// temperature versus the dual band. With the room at or above the high
/// setpoint and the compressor running, it reported COOLING while the unit's
/// own auto sub mode was AUTO_HEAT and the room was being heated.
/// The fix trusts the unit's reported direction and only falls back to the
/// band inference when the direction is unknown.
#include <gtest/gtest.h>
#include "cn105_protocol.h"

using cn105_protocol::AutoDirection;
using cn105_protocol::auto_direction;

TEST(AutoDirectionTest, AutoHeat_IsHeat) {
    EXPECT_EQ(auto_direction("AUTO_HEAT"), AutoDirection::HEAT);
}

TEST(AutoDirectionTest, AutoCool_IsCool) {
    EXPECT_EQ(auto_direction("AUTO_COOL"), AutoDirection::COOL);
}

TEST(AutoDirectionTest, NonDirectionalSubModes_AreUnknown) {
    for (const char* s : {"AUTO_OFF", "AUTO_LEADER", "AUTO_INACTIVE", "AUTO_IDLE", "AUTO_ACTIVE"}) {
        EXPECT_EQ(auto_direction(s), AutoDirection::UNKNOWN) << s;
    }
}

TEST(AutoDirectionTest, Null_IsUnknown) {
    EXPECT_EQ(auto_direction(nullptr), AutoDirection::UNKNOWN);
}
