/// test_vane_setpoint_persistence.cpp — Tests for vane and setpoint persistence
/// fixes (issues #283, #512, #204, #446).
///
/// Uses shared predicates from cn105_protocol.h to avoid logic duplication.
#include <gtest/gtest.h>
#include "cn105_protocol.h"
#include "cn105_types.h"

namespace {

// Mirror of wantedHeatpumpSettings for testing resetSettings behavior
struct TestWantedSettings {
    const char* vane = nullptr;
    float temperature = -1.0f;
    const char* last_user_vane = nullptr;
    float last_user_temperature = -1.0f;
    uint32_t last_user_vane_ms = 0;
    uint32_t last_user_temperature_ms = 0;

    void resetSettings() {
        vane = nullptr;
        temperature = -1.0f;
    }
};

const char* setVane(TestWantedSettings& ws, const char* setting, uint32_t now_ms) {
    for (int i = 0; i < 7; i++) {
        if (strcasecmp(VANE_MAP[i], setting) == 0) {
            ws.vane = VANE_MAP[i];
            ws.last_user_vane = VANE_MAP[i];
            ws.last_user_vane_ms = now_ms;
            return VANE_MAP[i];
        }
    }
    return nullptr;
}

const char* vaneForPacket(const TestWantedSettings& ws) {
    return ws.vane ? ws.vane : ws.last_user_vane;
}

}  // namespace

// ══════════════════════════════════════════════════════════════
// VANE PERSISTENCE TESTS
// ══════════════════════════════════════════════════════════════

TEST(VanePersistenceTest, LastUserVaneSurvivesReset) {
    TestWantedSettings ws;
    setVane(ws, "↓↓", 1000);
    EXPECT_STREQ(ws.vane, "↓↓");
    EXPECT_STREQ(ws.last_user_vane, "↓↓");

    ws.resetSettings();

    EXPECT_EQ(ws.vane, nullptr);
    EXPECT_STREQ(ws.last_user_vane, "↓↓");
}

TEST(VanePersistenceTest, TempOnlyPacketUsesLastUserVane) {
    TestWantedSettings ws;
    setVane(ws, "↓↓", 1000);
    ws.resetSettings();
    ws.temperature = 24.0f;

    EXPECT_STREQ(vaneForPacket(ws), "↓↓");
}

TEST(VanePersistenceTest, ExplicitVaneOverridesLastUser) {
    TestWantedSettings ws;
    setVane(ws, "↓↓", 1000);
    ws.resetSettings();
    setVane(ws, "↑", 2000);

    EXPECT_STREQ(vaneForPacket(ws), "↑");
}

TEST(VanePersistenceTest, GraceWindowIgnoresPACDisagreement) {
    using cn105_protocol::vane_disagrees_within_grace;

    EXPECT_TRUE(vane_disagrees_within_grace("AUTO", "↓↓", 1000, 2000, 3000));
    EXPECT_FALSE(vane_disagrees_within_grace("↓↓", "↓↓", 1000, 2000, 3000));
    EXPECT_FALSE(vane_disagrees_within_grace("AUTO", "↓↓", 1000, 5000, 3000));
    EXPECT_FALSE(vane_disagrees_within_grace("AUTO", nullptr, 0, 2000, 3000));
}

// ══════════════════════════════════════════════════════════════
// SETPOINT PERSISTENCE TESTS
// ══════════════════════════════════════════════════════════════

TEST(SetpointGraceTest, LastUserTempSurvivesReset) {
    TestWantedSettings ws;
    ws.temperature = 22.0f;
    ws.last_user_temperature = 22.0f;
    ws.last_user_temperature_ms = 1000;

    ws.resetSettings();

    EXPECT_FLOAT_EQ(ws.temperature, -1.0f);
    EXPECT_FLOAT_EQ(ws.last_user_temperature, 22.0f);
}

TEST(SetpointGraceTest, GraceWindowIgnoresPACDisagreement) {
    using cn105_protocol::setpoint_disagrees_within_grace;

    EXPECT_TRUE(setpoint_disagrees_within_grace(17.0f, 22.0f, 1000, 2000, 3000));
    EXPECT_FALSE(setpoint_disagrees_within_grace(22.5f, 22.0f, 1000, 2000, 3000));
    EXPECT_FALSE(setpoint_disagrees_within_grace(17.0f, 22.0f, 1000, 5000, 3000));
    EXPECT_FALSE(setpoint_disagrees_within_grace(17.0f, -1.0f, 0, 2000, 3000));
}

TEST(SetpointGraceTest, Issue204StylePacketIgnoredDuringGrace) {
    using cn105_protocol::setpoint_disagrees_within_grace;
    EXPECT_TRUE(setpoint_disagrees_within_grace(17.0f, 22.0f, 1000, 2000, 3000));
}

// ══════════════════════════════════════════════════════════════
// TEMPERATURE ENCODING TESTS
// ══════════════════════════════════════════════════════════════

TEST(TemperatureEncodingTest, Data11_0x80_IsUnused_NotZeroDegrees) {
    EXPECT_TRUE(cn105_protocol::is_temp_byte_unused(0x80));
    EXPECT_FALSE(cn105_protocol::is_temp_byte_unused(0x00));
    EXPECT_FALSE(cn105_protocol::is_temp_byte_unused(0xAC));
}

TEST(TemperatureEncodingTest, EncodingB_22Degrees) {
    // 22°C in encoding B: (0xAC - 128) / 2 = (172 - 128) / 2 = 22
    float temp = cn105_protocol::decode_temperature(0x00, 0xAC);
    EXPECT_FLOAT_EQ(temp, 22.0f);
}

TEST(TemperatureEncodingTest, EncodingB_HalfDegree) {
    // 22.5°C in encoding B: (0xAD - 128) / 2 = 22.5
    float temp = cn105_protocol::decode_temperature(0x00, 0xAD);
    EXPECT_FLOAT_EQ(temp, 22.5f);
}

TEST(TemperatureEncodingTest, EncodingA_FallbackWhenB_IsZero) {
    // When data[11]=0, encoding A with offset 10: data[5]=12 → 12+10=22
    float temp = cn105_protocol::decode_temperature(12, 0x00, 10);
    EXPECT_FLOAT_EQ(temp, 22.0f);
}

TEST(TemperatureEncodingTest, EncodingB_TakesPrecedence) {
    // Even with valid data[5], non-zero data[11] uses encoding B
    float temp = cn105_protocol::decode_temperature(0x05, 0xB0);
    EXPECT_FLOAT_EQ(temp, 24.0f);
}
