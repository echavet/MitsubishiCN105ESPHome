/// test_real_frames.cpp — Tests basés sur des trames UART réelles capturées en production.
/// Deps: cn105_types.h, esphome_stubs.h
///
/// Source: logs ESPHome d'une PAC Mitsubishi en fonctionnement réel.
/// Chaque test vérifie que notre logique de décodage/encodage produit
/// les mêmes résultats que ce qui a été observé sur le terrain.
#include <gtest/gtest.h>
#include "esphome_stubs.h"
#include "cn105_types.h"

// ── Helpers standalone (copies des fonctions pures) ──

static uint8_t checkSum(uint8_t bytes[], int len) {
    uint8_t sum = 0;
    for (int i = 0; i < len; i++) { sum += bytes[i]; }
    return (0xfc - sum) & 0xff;
}

static const char* lookupByteMapValue(const char* valuesMap[], const uint8_t byteMap[],
                                       int len, uint8_t byteValue) {
    for (int i = 0; i < len; i++) {
        if (byteMap[i] == byteValue) return valuesMap[i];
    }
    return valuesMap[0];
}

static int lookupByteMapValue_int(const int valuesMap[], const uint8_t byteMap[],
                                   int len, uint8_t byteValue) {
    for (int i = 0; i < len; i++) {
        if (byteMap[i] == byteValue) return valuesMap[i];
    }
    return valuesMap[0];
}

// ════════════════════════════════════════════════════════════════
// Trames réelles capturées — Cycle idle (AUTO OFF 22°C)
// ════════════════════════════════════════════════════════════════

// RESPONSE:Settings — FC 62 01 30 10 [02 00 00 00 08 09 00 01 00 00 03 AC 00 00 00 00] (9A)
static const uint8_t REAL_SETTINGS_IDLE[] = {
    0xFC, 0x62, 0x01, 0x30, 0x10,
    0x02, 0x00, 0x00, 0x00, 0x08, 0x09, 0x00, 0x01, 0x00, 0x00, 0x03, 0xAC, 0x00, 0x00, 0x00, 0x00,
    0x9A
};

// RESPONSE:RoomTemp — FC 62 01 30 10 [03 00 00 0B 00 00 AA 00 00 00 00 00 00 00 00 00] (A5)
static const uint8_t REAL_ROOMTEMP[] = {
    0xFC, 0x62, 0x01, 0x30, 0x10,
    0x03, 0x00, 0x00, 0x0B, 0x00, 0x00, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xA5
};

// RESPONSE:Status — FC 62 01 30 10 [06 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00] (57)
static const uint8_t REAL_STATUS_IDLE[] = {
    0xFC, 0x62, 0x01, 0x30, 0x10,
    0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x57
};

// RESPONSE:ErrorCode — FC 62 01 30 10 [04 00 00 00 80 00 00 00 00 00 00 00 00 00 00 00] (D9)
static const uint8_t REAL_ERRORCODE[] = {
    0xFC, 0x62, 0x01, 0x30, 0x10,
    0x04, 0x00, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xD9
};

// ════════════════════════════════════════════════════════════════
// Trames réelles — Commande utilisateur: HEAT 19.5°C
// ════════════════════════════════════════════════════════════════

// SET — FC 41 01 30 10 [01 07 00 01 01 00 00 00 00 00 00 00 00 00 A7 00] (CD)
static const uint8_t REAL_SET_HEAT_19_5[] = {
    0xFC, 0x41, 0x01, 0x30, 0x10,
    0x01, 0x07, 0x00, 0x01, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA7, 0x00,
    0xCD
};

// RESPONSE:Settings après SET — FC 62 01 30 10 [02 00 00 01 01 1C 00 01 00 00 03 A7 00 00 00 00] (92)
static const uint8_t REAL_SETTINGS_HEAT_19_5[] = {
    0xFC, 0x62, 0x01, 0x30, 0x10,
    0x02, 0x00, 0x00, 0x01, 0x01, 0x1C, 0x00, 0x01, 0x00, 0x00, 0x03, 0xA7, 0x00, 0x00, 0x00, 0x00,
    0x92
};

// SET OFF — FC 41 01 30 10 [01 05 00 00 00 00 00 00 00 00 00 00 00 00 AC 00] (CC)
static const uint8_t REAL_SET_OFF[] = {
    0xFC, 0x41, 0x01, 0x30, 0x10,
    0x01, 0x05, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAC, 0x00,
    0xCC
};

// SET Remote Temp — FC 41 01 30 10 [07 01 1A AA 00 00 00 00 00 00 00 00 00 00 00 00] (B2)
static const uint8_t REAL_SET_REMOTE_TEMP[] = {
    0xFC, 0x41, 0x01, 0x30, 0x10,
    0x07, 0x01, 0x1A, 0xAA, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0xB2
};

// ACK from HP — FC 61 01 30 10 [00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00] (5E)
static const uint8_t REAL_ACK[] = {
    0xFC, 0x61, 0x01, 0x30, 0x10,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x5E
};


// ════════════════════════════════════════════════════════════════
// Tests: Checksums de trames réelles
// ════════════════════════════════════════════════════════════════

TEST(RealFramesChecksum, SettingsIdle) {
    uint8_t pkt[22]; memcpy(pkt, REAL_SETTINGS_IDLE, 22);
    EXPECT_EQ(checkSum(pkt, 21), pkt[21]);
}

TEST(RealFramesChecksum, RoomTemp) {
    uint8_t pkt[22]; memcpy(pkt, REAL_ROOMTEMP, 22);
    EXPECT_EQ(checkSum(pkt, 21), pkt[21]);
}

TEST(RealFramesChecksum, StatusIdle) {
    uint8_t pkt[22]; memcpy(pkt, REAL_STATUS_IDLE, 22);
    EXPECT_EQ(checkSum(pkt, 21), pkt[21]);
}

TEST(RealFramesChecksum, ErrorCode) {
    uint8_t pkt[22]; memcpy(pkt, REAL_ERRORCODE, 22);
    EXPECT_EQ(checkSum(pkt, 21), pkt[21]);
}

TEST(RealFramesChecksum, SetHeat19_5) {
    uint8_t pkt[22]; memcpy(pkt, REAL_SET_HEAT_19_5, 22);
    EXPECT_EQ(checkSum(pkt, 21), pkt[21]);
}

TEST(RealFramesChecksum, SetOff) {
    uint8_t pkt[22]; memcpy(pkt, REAL_SET_OFF, 22);
    EXPECT_EQ(checkSum(pkt, 21), pkt[21]);
}

TEST(RealFramesChecksum, SetRemoteTemp) {
    uint8_t pkt[22]; memcpy(pkt, REAL_SET_REMOTE_TEMP, 22);
    EXPECT_EQ(checkSum(pkt, 21), pkt[21]);
}

TEST(RealFramesChecksum, Ack) {
    uint8_t pkt[22]; memcpy(pkt, REAL_ACK, 22);
    EXPECT_EQ(checkSum(pkt, 21), pkt[21]);
}

// ════════════════════════════════════════════════════════════════
// Tests: Décodage Settings (0x02) — trames réelles
// Payload offset: data[0]=byte[5], data[3]=byte[8], etc.
// ════════════════════════════════════════════════════════════════

class RealSettingsDecode : public ::testing::Test {
protected:
    // Extract payload (bytes after header FC 62 01 30 10)
    static const uint8_t* payload(const uint8_t* frame) { return frame + 5; }
};

TEST_F(RealSettingsDecode, IdleState_PowerOff) {
    const uint8_t* data = payload(REAL_SETTINGS_IDLE);
    // data[3] = 0x00 → POWER_MAP lookup → "OFF"
    EXPECT_STREQ(lookupByteMapValue(POWER_MAP, POWER, 2, data[3]), "OFF");
}

TEST_F(RealSettingsDecode, IdleState_ModeAuto) {
    const uint8_t* data = payload(REAL_SETTINGS_IDLE);
    // data[4] = 0x08 → iSee check: 0x08 > 0x08? false → mode = MODE lookup for 0x08 = "AUTO"
    bool iSee = data[4] > 0x08;
    EXPECT_FALSE(iSee);
    uint8_t modeVal = iSee ? (data[4] - 0x08) : data[4];
    EXPECT_STREQ(lookupByteMapValue(MODE_MAP, MODE, 5, modeVal), "AUTO");
}

TEST_F(RealSettingsDecode, IdleState_TempEncodingB_22C) {
    const uint8_t* data = payload(REAL_SETTINGS_IDLE);
    // data[11] = 0xAC ≠ 0 → encoding B: (172-128)/2 = 22.0°C
    EXPECT_NE(data[11], 0x00) << "data[11] should be non-zero for encoding B";
    float temp = (float)(data[11] - 128) / 2.0f;
    EXPECT_FLOAT_EQ(temp, 22.0f);
}

TEST_F(RealSettingsDecode, IdleState_FanAuto) {
    const uint8_t* data = payload(REAL_SETTINGS_IDLE);
    // data[6] = 0x00 → FAN_MAP lookup → "AUTO"
    // Note: FAN[0]=0x00 maps to FAN_MAP[0]="AUTO" but we need exact lookup
    EXPECT_STREQ(lookupByteMapValue(FAN_MAP, FAN, 6, data[6]), "AUTO");
}

TEST_F(RealSettingsDecode, IdleState_Vane) {
    const uint8_t* data = payload(REAL_SETTINGS_IDLE);
    // data[7] = 0x01 → VANE_MAP lookup → "↑↑" (confirmed by real logs: "vane: ↑↑")
    EXPECT_STREQ(lookupByteMapValue(VANE_MAP, VANE, 7, data[7]), "\xE2\x86\x91\xE2\x86\x91");
}

TEST_F(RealSettingsDecode, HeatState_PowerOn) {
    const uint8_t* data = payload(REAL_SETTINGS_HEAT_19_5);
    EXPECT_STREQ(lookupByteMapValue(POWER_MAP, POWER, 2, data[3]), "ON");
}

TEST_F(RealSettingsDecode, HeatState_ModeHeat) {
    const uint8_t* data = payload(REAL_SETTINGS_HEAT_19_5);
    bool iSee = data[4] > 0x08;
    EXPECT_FALSE(iSee);
    EXPECT_STREQ(lookupByteMapValue(MODE_MAP, MODE, 5, data[4]), "HEAT");
}

TEST_F(RealSettingsDecode, HeatState_TempEncodingB_19_5C) {
    const uint8_t* data = payload(REAL_SETTINGS_HEAT_19_5);
    // data[11] = 0xA7 → (167-128)/2 = 19.5°C
    EXPECT_NE(data[11], 0x00);
    float temp = (float)(data[11] - 128) / 2.0f;
    EXPECT_FLOAT_EQ(temp, 19.5f);
}

// ════════════════════════════════════════════════════════════════
// Tests: Décodage RoomTemp (0x03) — trame réelle
// ════════════════════════════════════════════════════════════════

TEST(RealRoomTempDecode, EncodingA_21C) {
    const uint8_t* data = REAL_ROOMTEMP + 5; // skip header
    // data[3] = 0x0B → encoding A: 0x0B + 10 = 21°C (old formula)
    // But also data[6] = 0xAA → encoding B: (170-128)/2 = 21.0°C
    // Both should agree
    float tempA = (float)data[3] + 10.0f; // legacy: data[3] + 10
    float tempB = (float)(data[6] - 128) / 2.0f;
    EXPECT_FLOAT_EQ(tempA, 21.0f);
    EXPECT_FLOAT_EQ(tempB, 21.0f);
    EXPECT_FLOAT_EQ(tempA, tempB) << "Encoding A and B should agree";
}

// ════════════════════════════════════════════════════════════════
// Tests: Décodage Status (0x06) — trame réelle
// ════════════════════════════════════════════════════════════════

TEST(RealStatusDecode, IdleNotOperating) {
    const uint8_t* data = REAL_STATUS_IDLE + 5;
    // data[4] = 0x00 → operating = false
    EXPECT_EQ(data[4], 0x00) << "operating byte should be 0 when idle";
}

// ════════════════════════════════════════════════════════════════
// Tests: Vérification SET packets — trames réelles
// ════════════════════════════════════════════════════════════════

TEST(RealSetPacketDecode, SetHeat19_5_ControlFlags) {
    const uint8_t* data = REAL_SET_HEAT_19_5 + 5;
    // data[1] = 0x07 → flags: power(0x01) + mode(0x02) + temp(0x04) = 0x07
    EXPECT_EQ(data[1], 0x07);
    EXPECT_EQ(data[1] & 0x01, 0x01) << "power flag set";
    EXPECT_EQ(data[1] & 0x02, 0x02) << "mode flag set";
    EXPECT_EQ(data[1] & 0x04, 0x04) << "temperature flag set";
}

TEST(RealSetPacketDecode, SetHeat19_5_PowerOn) {
    const uint8_t* data = REAL_SET_HEAT_19_5 + 5;
    // data[3] = 0x01 → POWER ON
    EXPECT_EQ(data[3], POWER[1]); // POWER[1] should be 0x01 = ON
}

TEST(RealSetPacketDecode, SetHeat19_5_ModeHeat) {
    const uint8_t* data = REAL_SET_HEAT_19_5 + 5;
    // data[4] = 0x01 → MODE HEAT
    EXPECT_EQ(data[4], MODE[0]); // MODE[0] should be 0x01 = HEAT
}

TEST(RealSetPacketDecode, SetHeat19_5_TempEncodingB) {
    const uint8_t* data = REAL_SET_HEAT_19_5 + 5;
    // data[14] = 0xA7 → encoding B: (19.5*2)+128 = 167 = 0xA7
    float target = 19.5f;
    int expected = (int)((target * 2) + 128);
    EXPECT_EQ(data[14], expected);
}

TEST(RealSetPacketDecode, SetOff_ControlFlags) {
    const uint8_t* data = REAL_SET_OFF + 5;
    // data[1] = 0x05 → flags: power(0x01) + temp(0x04) = 0x05
    EXPECT_EQ(data[1], 0x05);
    EXPECT_EQ(data[1] & 0x01, 0x01) << "power flag set";
    EXPECT_EQ(data[1] & 0x02, 0x00) << "mode flag NOT set";
    EXPECT_EQ(data[1] & 0x04, 0x04) << "temperature flag set";
}

TEST(RealSetPacketDecode, SetOff_PowerOff) {
    const uint8_t* data = REAL_SET_OFF + 5;
    EXPECT_EQ(data[3], POWER[0]); // POWER[0] = 0x00 = OFF
}

TEST(RealSetPacketDecode, SetOff_TempEncodingB_22C) {
    const uint8_t* data = REAL_SET_OFF + 5;
    // data[14] = 0xAC → (22.0*2)+128 = 172 = 0xAC
    float target = 22.0f;
    int expected = (int)((target * 2) + 128);
    EXPECT_EQ(data[14], expected);
}

// ════════════════════════════════════════════════════════════════
// Tests: Remote temperature packet — trame réelle
// ════════════════════════════════════════════════════════════════

TEST(RealRemoteTemp, KeepAlive20_9C) {
    const uint8_t* data = REAL_SET_REMOTE_TEMP + 5;
    // data[0] = 0x07 → remote temp command
    EXPECT_EQ(data[0], 0x07);
    // data[1] = 0x01 → enabled
    EXPECT_EQ(data[1], 0x01);
    // data[2] = 0x1A = 26 → round(20.9*2) - 16 = 42 - 16 = 26 ✓
    float remoteTemp = 20.9f;
    float rounded = round(remoteTemp * 2);
    EXPECT_EQ(data[2], static_cast<uint8_t>(rounded - 16));
    // data[3] = 0xAA = 170 → round(20.9*2) + 128 = 42 + 128 = 170 ✓
    EXPECT_EQ(data[3], static_cast<uint8_t>(rounded + 128));
}

// ════════════════════════════════════════════════════════════════
// Tests: Cohérence encode/decode roundtrip sur trames réelles
// ════════════════════════════════════════════════════════════════

TEST(RealFrameRoundtrip, SettingsToSet_TempConsistency) {
    // La trame Settings confirmée par la PAC doit avoir la même température
    // que celle envoyée dans le SET packet
    const uint8_t* setData = REAL_SET_HEAT_19_5 + 5;
    const uint8_t* respData = REAL_SETTINGS_HEAT_19_5 + 5;

    // SET: encoding B at data[14]
    float setTemp = (float)(setData[14] - 128) / 2.0f;
    // RESPONSE: encoding B at data[11]
    float respTemp = (float)(respData[11] - 128) / 2.0f;

    EXPECT_FLOAT_EQ(setTemp, respTemp)
        << "Temperature in SET packet must match RESPONSE confirmation";
    EXPECT_FLOAT_EQ(setTemp, 19.5f);
}

TEST(RealFrameRoundtrip, SettingsToSet_ModeConsistency) {
    const uint8_t* setData = REAL_SET_HEAT_19_5 + 5;
    const uint8_t* respData = REAL_SETTINGS_HEAT_19_5 + 5;

    // SET: mode at data[4], RESPONSE: mode at data[4]
    EXPECT_EQ(setData[4], respData[4])
        << "Mode byte in SET must match RESPONSE confirmation";
}
