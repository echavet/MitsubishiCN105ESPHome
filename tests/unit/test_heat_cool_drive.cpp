/// test_heat_cool_drive.cpp — dual-setpoint HEAT_COOL runs hardware AUTO inside the band.
///
/// Heads on a shared outdoor unit can only run one direction at a time. A head in
/// explicit COOL holds the outdoor unit in cooling even while idle, so heads asking
/// for HEAT wait in STANDBY. Hardware AUTO follows the direction the outdoor unit is
/// already running. HEAT_COOL therefore stays in AUTO inside the band, with the
/// setpoint on the band edge AUTO is running toward, and uses explicit HEAT or COOL
/// only to bring the room back into the band.
#include <gtest/gtest.h>
#include <cmath>
#include "cn105_protocol.h"

using cn105_protocol::AutoDirection;
using cn105_protocol::HeatCoolDrive;
using cn105_protocol::heat_cool_auto_setpoint;
using cn105_protocol::heat_cool_drive;

namespace {
constexpr float kLow = 21.0f;
constexpr float kHigh = 23.0f;
constexpr float kMargin = 0.5f;
constexpr uint32_t kDwell = 10u * 60u * 1000u;
constexpr uint32_t kNow = 100u * 60u * 1000u;
constexpr uint32_t kLongAgo = kNow - kDwell;
constexpr uint32_t kJustNow = kNow - 1000u;

HeatCoolDrive decide(float room, HeatCoolDrive current,
                     AutoDirection auto_dir = AutoDirection::UNKNOWN,
                     uint32_t last_switch_ms = kLongAgo) {
    return heat_cool_drive(room, kLow, kHigh, current, auto_dir, kNow, last_switch_ms, kMargin, kDwell);
}
}  // namespace

// ── Entering HEAT_COOL ──

TEST(HeatCoolDriveTest, Enter_InBand_RunsAuto) {
    for (float room : {20.6f, 21.0f, 22.0f, 23.0f, 23.4f}) {
        EXPECT_EQ(decide(room, HeatCoolDrive::NONE, AutoDirection::UNKNOWN, kJustNow), HeatCoolDrive::AUTO) << room;
    }
}

TEST(HeatCoolDriveTest, Enter_PastAnEdge_RunsExplicit) {
    EXPECT_EQ(decide(20.5f, HeatCoolDrive::NONE, AutoDirection::UNKNOWN, kJustNow), HeatCoolDrive::HEAT);
    EXPECT_EQ(decide(23.5f, HeatCoolDrive::NONE, AutoDirection::UNKNOWN, kJustNow), HeatCoolDrive::COOL);
}

TEST(HeatCoolDriveTest, Enter_UnknownRoom_RunsAuto) {
    EXPECT_EQ(decide(NAN, HeatCoolDrive::NONE), HeatCoolDrive::AUTO);
}

// ── AUTO inside the band ──

TEST(HeatCoolDriveTest, Auto_StaysInBand_WhateverDirectionItFollows) {
    // 2026-09-26: an idle head held the outdoor unit in COOL all night. In AUTO the
    // head follows the outdoor unit instead, so it must stay AUTO inside the band.
    for (auto dir : {AutoDirection::HEAT, AutoDirection::COOL, AutoDirection::UNKNOWN}) {
        for (float room : {20.6f, 21.0f, 22.0f, 23.0f, 23.4f}) {
            EXPECT_EQ(decide(room, HeatCoolDrive::AUTO, dir), HeatCoolDrive::AUTO) << room;
        }
    }
}

TEST(HeatCoolDriveTest, Auto_TooCold_AlreadyHeating_StaysAuto) {
    EXPECT_EQ(decide(20.0f, HeatCoolDrive::AUTO, AutoDirection::HEAT), HeatCoolDrive::AUTO);
}

TEST(HeatCoolDriveTest, Auto_TooWarm_AlreadyCooling_StaysAuto) {
    EXPECT_EQ(decide(24.0f, HeatCoolDrive::AUTO, AutoDirection::COOL), HeatCoolDrive::AUTO);
}

TEST(HeatCoolDriveTest, Auto_TooCold_NotHeating_GoesExplicitHeat) {
    EXPECT_EQ(decide(20.5f, HeatCoolDrive::AUTO, AutoDirection::COOL), HeatCoolDrive::HEAT);
    EXPECT_EQ(decide(20.5f, HeatCoolDrive::AUTO, AutoDirection::UNKNOWN), HeatCoolDrive::HEAT);
}

TEST(HeatCoolDriveTest, Auto_TooWarm_NotCooling_GoesExplicitCool) {
    EXPECT_EQ(decide(23.5f, HeatCoolDrive::AUTO, AutoDirection::HEAT), HeatCoolDrive::COOL);
    EXPECT_EQ(decide(23.5f, HeatCoolDrive::AUTO, AutoDirection::UNKNOWN), HeatCoolDrive::COOL);
}

TEST(HeatCoolDriveTest, Auto_LeavesAtOnce_NoDwell) {
    EXPECT_EQ(decide(20.5f, HeatCoolDrive::AUTO, AutoDirection::COOL, kJustNow), HeatCoolDrive::HEAT);
}

// ── Explicit HEAT / COOL back to AUTO ──

TEST(HeatCoolDriveTest, Heat_HoldsUntilRoomReachesLowEdge) {
    EXPECT_EQ(decide(20.5f, HeatCoolDrive::HEAT), HeatCoolDrive::HEAT);
    EXPECT_EQ(decide(21.0f, HeatCoolDrive::HEAT), HeatCoolDrive::AUTO);
}

TEST(HeatCoolDriveTest, Cool_HoldsUntilRoomReachesHighEdge) {
    EXPECT_EQ(decide(23.5f, HeatCoolDrive::COOL), HeatCoolDrive::COOL);
    EXPECT_EQ(decide(23.0f, HeatCoolDrive::COOL), HeatCoolDrive::AUTO);
}

TEST(HeatCoolDriveTest, Explicit_WaitsForMinimumDwell) {
    EXPECT_EQ(decide(22.0f, HeatCoolDrive::HEAT, AutoDirection::UNKNOWN, kJustNow), HeatCoolDrive::HEAT);
    EXPECT_EQ(decide(22.0f, HeatCoolDrive::COOL, AutoDirection::UNKNOWN, kJustNow), HeatCoolDrive::COOL);
}

TEST(HeatCoolDriveTest, Explicit_OvershootPastBand_ReturnsToAuto) {
    // Heating overshoot past the high edge goes back to AUTO, never straight to COOL.
    EXPECT_EQ(decide(23.5f, HeatCoolDrive::HEAT), HeatCoolDrive::AUTO);
    EXPECT_EQ(decide(20.5f, HeatCoolDrive::COOL), HeatCoolDrive::AUTO);
}

TEST(HeatCoolDriveTest, Dwell_SurvivesMillisWrap) {
    const uint32_t last = 0xFFFFFFFFu - 1000u;  // 1 s before millis() wraps
    EXPECT_EQ(heat_cool_drive(22.0f, kLow, kHigh, HeatCoolDrive::HEAT, AutoDirection::UNKNOWN,
                              5000u, last, kMargin, kDwell),
              HeatCoolDrive::HEAT);
    EXPECT_EQ(heat_cool_drive(22.0f, kLow, kHigh, HeatCoolDrive::HEAT, AutoDirection::UNKNOWN,
                              last + kDwell, last, kMargin, kDwell),
              HeatCoolDrive::AUTO);
}

TEST(HeatCoolDriveTest, UnknownRoom_HoldsCurrent) {
    for (auto d : {HeatCoolDrive::AUTO, HeatCoolDrive::HEAT, HeatCoolDrive::COOL}) {
        EXPECT_EQ(decide(NAN, d), d);
    }
}

// ── AUTO setpoint ──

TEST(HeatCoolAutoSetpointTest, FollowsTheEdgeAutoRunsToward) {
    // 2026-09-25: AUTO followed the outdoor unit into heating with the setpoint at
    // the room temperature and heated past the band. At the low edge it idles.
    EXPECT_FLOAT_EQ(heat_cool_auto_setpoint(kLow, kHigh, AutoDirection::HEAT), kLow);
    EXPECT_FLOAT_EQ(heat_cool_auto_setpoint(kLow, kHigh, AutoDirection::COOL), kHigh);
}

TEST(HeatCoolAutoSetpointTest, NoDirectionYet_Midpoint) {
    EXPECT_FLOAT_EQ(heat_cool_auto_setpoint(kLow, kHigh, AutoDirection::UNKNOWN), 22.0f);
}
