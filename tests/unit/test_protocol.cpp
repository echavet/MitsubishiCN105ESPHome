/// test_protocol.cpp — Tests for cn105_protocol.h pure protocol functions.
/// Deps: cn105_protocol.h (production code, no mocks needed)
///
/// These tests validate the ACTUAL production functions, not standalone copies.
/// Any regression in cn105_protocol.h will be caught here.
#include <gtest/gtest.h>
#include "cn105_protocol.h"

using namespace cn105_protocol;

// ════════════════════════════════════════════════════════════════
// checksum() — production function
// ════════════════════════════════════════════════════════════════

TEST(ProtocolChecksum, ConnectPacket) {
    uint8_t pkt[] = {0xfc, 0x5a, 0x01, 0x30, 0x02, 0xca, 0x01};
    EXPECT_EQ(checksum(pkt, 7), 0xa8);
}

TEST(ProtocolChecksum, InfoPacket) {
    uint8_t pkt[] = {0xfc, 0x42, 0x01, 0x30, 0x10,
                     0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(checksum(pkt, 21), 0x7b);
}

TEST(ProtocolChecksum, ZeroLength) {
    uint8_t pkt[] = {};
    EXPECT_EQ(checksum(pkt, 0), 0xfc);
}

TEST(ProtocolChecksum, Overflow) {
    // 0xfc + 0x04 = 0x100 → wraps to 0x00 in uint8_t, checksum = (0xfc - 0x00) & 0xff = 0xfc
    uint8_t pkt[] = {0xfc, 0x04};
    EXPECT_EQ(checksum(pkt, 2), 0xfc);
}

TEST(ProtocolChecksum, RealSettingsResponse) {
    // Real captured frame: FC 62 01 30 10 02 00 00 00 08 09 00 01 00 00 03 AC 00 00 00 00 (9A)
    uint8_t pkt[] = {0xFC, 0x62, 0x01, 0x30, 0x10,
                     0x02, 0x00, 0x00, 0x00, 0x08, 0x09, 0x00, 0x01,
                     0x00, 0x00, 0x03, 0xAC, 0x00, 0x00, 0x00, 0x00};
    EXPECT_EQ(checksum(pkt, 21), 0x9A);
}

// ════════════════════════════════════════════════════════════════
// decode_temperature() — production function
// ════════════════════════════════════════════════════════════════

TEST(ProtocolDecodeTemp, EncodingB_22C) {
    // enc_b = 0xAC = 172 → (172-128)/2 = 22.0
    EXPECT_FLOAT_EQ(decode_temperature(0x09, 0xAC), 22.0f);
}

TEST(ProtocolDecodeTemp, EncodingB_19_5C) {
    // enc_b = 0xA7 = 167 → (167-128)/2 = 19.5
    EXPECT_FLOAT_EQ(decode_temperature(0x1C, 0xA7), 19.5f);
}

TEST(ProtocolDecodeTemp, EncodingB_IgnoresEncA) {
    // When enc_b != 0, enc_a is ignored
    EXPECT_FLOAT_EQ(decode_temperature(0x00, 0xAC), 22.0f);
    EXPECT_FLOAT_EQ(decode_temperature(0xFF, 0xAC), 22.0f);
}

TEST(ProtocolDecodeTemp, EncodingA_FallbackWithOffset10) {
    // enc_b == 0 → uses enc_a + offset (default 10)
    EXPECT_FLOAT_EQ(decode_temperature(0x0B, 0x00), 21.0f);   // 11 + 10
    EXPECT_FLOAT_EQ(decode_temperature(0x00, 0x00), 10.0f);   // 0 + 10
}

TEST(ProtocolDecodeTemp, EncodingA_CustomOffset) {
    // Settings use different offset (31 - enc_a for encoding A)
    EXPECT_FLOAT_EQ(decode_temperature(0x09, 0x00, 22), 31.0f);  // 9 + 22
}

TEST(ProtocolDecodeTemp, RoomTemp_BothEncodingsAgree) {
    // Real frame: data[3]=0x0B (enc_a), data[6]=0xAA (enc_b)
    float tempA = decode_temperature(0x0B, 0x00);       // 11 + 10 = 21.0
    float tempB = decode_temperature(0x0B, 0xAA);       // (170-128)/2 = 21.0
    EXPECT_FLOAT_EQ(tempA, tempB);
    EXPECT_FLOAT_EQ(tempB, 21.0f);
}

TEST(ProtocolDecodeTemp, EncodingB_MinValue) {
    // enc_b = 128 → (128-128)/2 = 0.0
    EXPECT_FLOAT_EQ(decode_temperature(0x00, 0x80), 0.0f);
}

TEST(ProtocolDecodeTemp, EncodingB_HalfDegree) {
    // enc_b = 0xA9 = 169 → (169-128)/2 = 20.5
    EXPECT_FLOAT_EQ(decode_temperature(0x00, 0xA9), 20.5f);
}

// ════════════════════════════════════════════════════════════════
// encode_temperature_b() — production function
// ════════════════════════════════════════════════════════════════

TEST(ProtocolEncodeTemp, RoundTrip_19_5) {
    uint8_t encoded = encode_temperature_b(19.5f);
    EXPECT_EQ(encoded, 0xA7);
    EXPECT_FLOAT_EQ(decode_temperature(0x00, encoded), 19.5f);
}

TEST(ProtocolEncodeTemp, RoundTrip_22_0) {
    uint8_t encoded = encode_temperature_b(22.0f);
    EXPECT_EQ(encoded, 0xAC);
    EXPECT_FLOAT_EQ(decode_temperature(0x00, encoded), 22.0f);
}

TEST(ProtocolEncodeTemp, RoundTrip_16_0) {
    uint8_t encoded = encode_temperature_b(16.0f);
    EXPECT_EQ(encoded, 0xA0);
    EXPECT_FLOAT_EQ(decode_temperature(0x00, encoded), 16.0f);
}

TEST(ProtocolEncodeTemp, RoundTrip_31_0) {
    uint8_t encoded = encode_temperature_b(31.0f);
    EXPECT_EQ(encoded, 0xBE);
    EXPECT_FLOAT_EQ(decode_temperature(0x00, encoded), 31.0f);
}

TEST(ProtocolEncodeTemp, HalfDegreeValues) {
    for (float t = 16.0f; t <= 31.0f; t += 0.5f) {
        uint8_t encoded = encode_temperature_b(t);
        float decoded = decode_temperature(0x00, encoded);
        EXPECT_FLOAT_EQ(decoded, t) << "Roundtrip failed for " << t << "°C";
    }
}

// ════════════════════════════════════════════════════════════════
// encode_remote_temperature() — production function
// ════════════════════════════════════════════════════════════════

TEST(ProtocolEncodeRemoteTemp, KeepAlive_20_9C) {
    uint8_t enc_a, enc_b;
    encode_remote_temperature(20.9f, enc_a, enc_b);
    // round(20.9*2) = round(41.8) = 42
    // enc_a = 42 - 16 = 26 = 0x1A
    // enc_b = 42 + 128 = 170 = 0xAA
    EXPECT_EQ(enc_a, 0x1A);
    EXPECT_EQ(enc_b, 0xAA);
}

TEST(ProtocolEncodeRemoteTemp, Exact_21_0C) {
    uint8_t enc_a, enc_b;
    encode_remote_temperature(21.0f, enc_a, enc_b);
    EXPECT_EQ(enc_a, 0x1A);  // 42 - 16
    EXPECT_EQ(enc_b, 0xAA);  // 42 + 128
}

TEST(ProtocolEncodeRemoteTemp, Low_10_0C) {
    uint8_t enc_a, enc_b;
    encode_remote_temperature(10.0f, enc_a, enc_b);
    // round(10.0*2) = 20
    EXPECT_EQ(enc_a, 0x04);  // 20 - 16
    EXPECT_EQ(enc_b, 0x94);  // 20 + 128
}

// ════════════════════════════════════════════════════════════════
// lookup_value() — production function
// ════════════════════════════════════════════════════════════════

// Import the protocol tables from cn105_types.h for testing
#include "cn105_types.h"

TEST(ProtocolLookup, PowerOff) {
    EXPECT_STREQ(lookup_value(POWER_MAP, POWER, 2, 0x00), "OFF");
}

TEST(ProtocolLookup, PowerOn) {
    EXPECT_STREQ(lookup_value(POWER_MAP, POWER, 2, 0x01), "ON");
}

TEST(ProtocolLookup, ModeHeat) {
    EXPECT_STREQ(lookup_value(MODE_MAP, MODE, 5, 0x01), "HEAT");
}

TEST(ProtocolLookup, ModeCool) {
    EXPECT_STREQ(lookup_value(MODE_MAP, MODE, 5, 0x03), "COOL");
}

TEST(ProtocolLookup, ModeAuto) {
    EXPECT_STREQ(lookup_value(MODE_MAP, MODE, 5, 0x08), "AUTO");
}

TEST(ProtocolLookup, FanAuto) {
    EXPECT_STREQ(lookup_value(FAN_MAP, FAN, 6, 0x00), "AUTO");
}

TEST(ProtocolLookup, FanQuiet) {
    EXPECT_STREQ(lookup_value(FAN_MAP, FAN, 6, 0x01), "QUIET");
}

TEST(ProtocolLookup, UnknownByteFallsBackToIndex0) {
    EXPECT_STREQ(lookup_value(MODE_MAP, MODE, 5, 0xFF), "HEAT");
}

TEST(ProtocolLookup, TempMapIndex0) {
    EXPECT_EQ(lookup_value(TEMP_MAP, TEMP, 16, 0x00), 31);
}

TEST(ProtocolLookup, TempMapIndex15) {
    EXPECT_EQ(lookup_value(TEMP_MAP, TEMP, 16, 0x0F), 16);
}

// ════════════════════════════════════════════════════════════════
// lookup_index() — production function
// ════════════════════════════════════════════════════════════════

TEST(ProtocolLookupIndex, FindsExistingInt) {
    EXPECT_EQ(lookup_index(TEMP_MAP, 16, 22), 9);  // 22°C is at index 9
}

TEST(ProtocolLookupIndex, ReturnsMinusOneForMissing) {
    EXPECT_EQ(lookup_index(TEMP_MAP, 16, 99), -1);
}

TEST(ProtocolLookupIndex, FindsExistingString) {
    EXPECT_EQ(lookup_index(MODE_MAP, 5, "COOL"), 2);
}

TEST(ProtocolLookupIndex, StringCaseInsensitive) {
    EXPECT_EQ(lookup_index(MODE_MAP, 5, "cool"), 2);
}

TEST(ProtocolLookupIndex, StringNotFound) {
    EXPECT_EQ(lookup_index(MODE_MAP, 5, "TURBO"), -1);
}
