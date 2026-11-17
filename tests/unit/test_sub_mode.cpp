/// test_sub_mode.cpp — Defrost sub-mode bitmask decoding (#758 / PR #766).
/// Deps: heatpump_profile_protocol.h (production HeatPumpProtocol::decode_sub_mode)
///
/// Packet 0x09 data[3] is a bitmask. The filter reminder bit (0x01) can stay
/// set for weeks; when another bit is set it must be ignored so DEFROST/PREHEAT
/// still decode. Bare 0x01 stays WARMUP (same as before the bitmask change).
#include <gtest/gtest.h>
#include "heatpump_profile_protocol.h"

using esphome::cn105::HeatPumpProtocol;

TEST(SubModeBitmask, DefrostAlone) {
    auto result = HeatPumpProtocol::decode_sub_mode(0x02);
    ASSERT_TRUE(result.has_value());
    EXPECT_STREQ(*result, "DEFROST");
}

TEST(SubModeBitmask, DefrostWithFilterReminder) {
    auto result = HeatPumpProtocol::decode_sub_mode(0x03);
    ASSERT_TRUE(result.has_value());
    EXPECT_STREQ(*result, "DEFROST");
}

TEST(SubModeBitmask, PreheatHeatingStartWithFilterReminder) {
    auto result = HeatPumpProtocol::decode_sub_mode(0x05);
    ASSERT_TRUE(result.has_value());
    EXPECT_STREQ(*result, "PREHEAT");
}

TEST(SubModeBitmask, FilterReminderAloneStaysWarmup) {
    auto result = HeatPumpProtocol::decode_sub_mode(0x01);
    ASSERT_TRUE(result.has_value());
    EXPECT_STREQ(*result, "WARMUP");
}

TEST(SubModeBitmask, UnchangedMappings) {
    auto normal = HeatPumpProtocol::decode_sub_mode(0x00);
    ASSERT_TRUE(normal.has_value());
    EXPECT_STREQ(*normal, "NORMAL");

    auto preheat = HeatPumpProtocol::decode_sub_mode(0x04);
    ASSERT_TRUE(preheat.has_value());
    EXPECT_STREQ(*preheat, "PREHEAT");

    auto standby = HeatPumpProtocol::decode_sub_mode(0x08);
    ASSERT_TRUE(standby.has_value());
    EXPECT_STREQ(*standby, "STANDBY");

    auto off = HeatPumpProtocol::decode_sub_mode(0x10);
    ASSERT_TRUE(off.has_value());
    EXPECT_STREQ(*off, "OFF");
}
