/// test_temperature.cpp — Tests de non-régression pour le décodage/encodage température
/// Deps: cn105_types.h (tables TEMP/TEMP_MAP), esphome_stubs.h
#include <gtest/gtest.h>
#include "esphome_stubs.h"
#include "cn105_types.h"

// Standalone reimplementation of lookup for TEMP_MAP
static int lookupByteMapValue_int(const int valuesMap[], const uint8_t byteMap[],
                                   int len, uint8_t byteValue) {
    for (int i = 0; i < len; i++) {
        if (byteMap[i] == byteValue) {
            return valuesMap[i];
        }
    }
    return valuesMap[0];
}

// ========================================================
// Temperature DECODING tests (mirroring hp_readings.cpp logic)
// ========================================================

// Encoding A: data[11] == 0x00, temperature is in data[5] via TEMP_MAP lookup
// Encoding B: data[11] != 0x00, temperature = (data[11] - 128) / 2.0

TEST(TemperatureDecodeTest, EncodingA_Data5_0x00_Returns31) {
    // TEMP[0] = 0x00, TEMP_MAP[0] = 31
    uint8_t data5 = 0x00;
    int result = lookupByteMapValue_int(TEMP_MAP, TEMP, 16, data5);
    EXPECT_EQ(result, 31);
}

TEST(TemperatureDecodeTest, EncodingA_Data5_0x05_Returns26) {
    // TEMP[5] = 0x05, TEMP_MAP[5] = 26
    uint8_t data5 = 0x05;
    int result = lookupByteMapValue_int(TEMP_MAP, TEMP, 16, data5);
    EXPECT_EQ(result, 26);
}

TEST(TemperatureDecodeTest, EncodingA_Data5_0x0F_Returns16) {
    // TEMP[15] = 0x0F, TEMP_MAP[15] = 16
    uint8_t data5 = 0x0F;
    int result = lookupByteMapValue_int(TEMP_MAP, TEMP, 16, data5);
    EXPECT_EQ(result, 16);
}

TEST(TemperatureDecodeTest, EncodingB_0xA5_Returns18_5) {
    // data[11] = 0xA5 = 165 → (165 - 128) / 2.0 = 18.5
    uint8_t data11 = 0xA5;
    float temp = (float)(data11 - 128) / 2.0f;
    EXPECT_FLOAT_EQ(temp, 18.5f);
}

TEST(TemperatureDecodeTest, EncodingB_0xB4_Returns26_0) {
    // data[11] = 0xB4 = 180 → (180 - 128) / 2.0 = 26.0
    uint8_t data11 = 0xB4;
    float temp = (float)(data11 - 128) / 2.0f;
    EXPECT_FLOAT_EQ(temp, 26.0f);
}

TEST(TemperatureDecodeTest, EncodingB_0xB5_Returns26_5) {
    // data[11] = 0xB5 = 181 → (181 - 128) / 2.0 = 26.5
    uint8_t data11 = 0xB5;
    float temp = (float)(data11 - 128) / 2.0f;
    EXPECT_FLOAT_EQ(temp, 26.5f);
}

TEST(TemperatureDecodeTest, EncodingB_0x94_Returns10_0_Min) {
    // data[11] = 0x94 = 148 → (148 - 128) / 2.0 = 10.0
    uint8_t data11 = 0x94;
    float temp = (float)(data11 - 128) / 2.0f;
    EXPECT_FLOAT_EQ(temp, 10.0f);
}

TEST(TemperatureDecodeTest, EncodingB_0xBE_Returns31_0_Max) {
    // data[11] = 0xBE = 190 → (190 - 128) / 2.0 = 31.0
    uint8_t data11 = 0xBE;
    float temp = (float)(data11 - 128) / 2.0f;
    EXPECT_FLOAT_EQ(temp, 31.0f);
}

// ========================================================
// Temperature ENCODING tests (mirroring hp_writings.cpp logic)
// ========================================================

// Encoding A: packet[10] = TEMP[idx] where TEMP_MAP[idx] == target
// Encoding B: packet[19] = (int)((target * 2) + 128)

TEST(TemperatureEncodeTest, EncodingA_23C_CorrectByte) {
    // 23°C → TEMP_MAP[8] = 23, so TEMP[8] = 0x08
    float target = 23.0f;
    int expected_idx = -1;
    for (int i = 0; i < 16; i++) {
        if (TEMP_MAP[i] == (int)target) {
            expected_idx = i;
            break;
        }
    }
    ASSERT_NE(expected_idx, -1) << "23°C should be found in TEMP_MAP";
    EXPECT_EQ(TEMP[expected_idx], 0x08);
}

TEST(TemperatureEncodeTest, EncodingB_26C_CorrectByte) {
    float target = 26.0f;
    int encoded = (int)((target * 2) + 128);
    EXPECT_EQ(encoded, 180); // 0xB4
}

TEST(TemperatureEncodeTest, EncodingB_26_5C_CorrectByte) {
    float target = 26.5f;
    int encoded = (int)((target * 2) + 128);
    EXPECT_EQ(encoded, 181); // 0xB5
}

TEST(TemperatureEncodeTest, EncodingB_10C_Min) {
    float target = 10.0f;
    int encoded = (int)((target * 2) + 128);
    EXPECT_EQ(encoded, 148); // 0x94
}

TEST(TemperatureEncodeTest, EncodingB_31C_Max) {
    float target = 31.0f;
    int encoded = (int)((target * 2) + 128);
    EXPECT_EQ(encoded, 190); // 0xBE
}

// ========================================================
// Roundtrip tests (encode then decode must give same value)
// ========================================================

TEST(TemperatureRoundtripTest, EncodingB_Roundtrip_18_5) {
    float original = 18.5f;
    int encoded = (int)((original * 2) + 128);
    float decoded = (float)(encoded - 128) / 2.0f;
    EXPECT_FLOAT_EQ(decoded, original);
}

TEST(TemperatureRoundtripTest, EncodingB_Roundtrip_WholeRange) {
    // Test every half-degree from 10.0 to 31.0
    for (float t = 10.0f; t <= 31.0f; t += 0.5f) {
        int encoded = (int)((t * 2) + 128);
        float decoded = (float)(encoded - 128) / 2.0f;
        EXPECT_FLOAT_EQ(decoded, t) << "Roundtrip failed for " << t << "°C";
    }
}
