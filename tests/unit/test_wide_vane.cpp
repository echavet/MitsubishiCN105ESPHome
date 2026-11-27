/// test_wide_vane.cpp — Settings byte 10 (wide vane) decode and the AUTO position.
/// Deps: heatpump_profile_protocol.h, cn105_protocol.h, cn105_types.h
///
/// Real frames: Trane NTXWMT18A112AA (Mitsubishi MSZ-FH family, i-See, motorized
/// wide vane), ESP8266, 2026-11-06. Each frame is a 0x62 settings response.
#include <gtest/gtest.h>
#include "heatpump_profile_protocol.h"
#include "cn105_protocol.h"
#include "cn105_types.h"

using esphome::cn105::HeatPumpProtocol;

TEST(WideVaneDecode, DocumentedPositions) {
    EXPECT_STREQ(HeatPumpProtocol::decode_wide_vane(0x01), "←←");
    EXPECT_STREQ(HeatPumpProtocol::decode_wide_vane(0x02), "←");
    EXPECT_STREQ(HeatPumpProtocol::decode_wide_vane(0x03), "|");
    EXPECT_STREQ(HeatPumpProtocol::decode_wide_vane(0x04), "→");
    EXPECT_STREQ(HeatPumpProtocol::decode_wide_vane(0x05), "→→");
    EXPECT_STREQ(HeatPumpProtocol::decode_wide_vane(0x08), "←→");
    EXPECT_STREQ(HeatPumpProtocol::decode_wide_vane(0x0C), "SWING");
}

TEST(WideVaneDecode, ZeroIsAutoNotUnsupported) {
    EXPECT_STREQ(HeatPumpProtocol::decode_wide_vane(0x00), "AUTO");
}

TEST(WideVaneDecode, HighBitWithZeroPositionIsAirflowControl) {
    EXPECT_STREQ(HeatPumpProtocol::decode_wide_vane(0x80), "AIRFLOW CONTROL");
}

TEST(WideVaneDecode, AdjustBitKeepsPosition) {
    EXPECT_STREQ(HeatPumpProtocol::decode_wide_vane(0x83), "|");
    EXPECT_STREQ(HeatPumpProtocol::decode_wide_vane(0x8C), "SWING");
}

TEST(WideVaneDecode, UnknownByteIsNull) {
    EXPECT_EQ(HeatPumpProtocol::decode_wide_vane(0x06), nullptr);
    EXPECT_EQ(HeatPumpProtocol::decode_wide_vane(0x0F), nullptr);
}

TEST(WideVaneEncode, AutoIsWrittenAsZero) {
    wantedHeatpumpSettings wanted{};
    wanted.wideVane = WIDEVANE_MAP[8];
    uint8_t packet[22]{};
    packet[18] = 0xFF;
    HeatPumpProtocol::encode_control(packet, wanted, {}, false, false, false, false);
    EXPECT_EQ(packet[18], 0x00);
    EXPECT_EQ(packet[7], CONTROL_PACKET_2[0]);
}

TEST(WideVaneEncode, PositionsAndAutoRoundTrip) {
    // AIRFLOW CONTROL (index 7) is excluded: it is written as 0x00 and the unit reports 0x80.
    for (int i : {0, 1, 2, 3, 4, 5, 6, 8}) {
        wantedHeatpumpSettings wanted{};
        wanted.wideVane = WIDEVANE_MAP[i];
        uint8_t packet[22]{};
        HeatPumpProtocol::encode_control(packet, wanted, {}, false, false, false, false);
        EXPECT_STREQ(HeatPumpProtocol::decode_wide_vane(packet[18]), WIDEVANE_MAP[i]) << "index " << i;
    }
}

struct RealSettingsFrame {
    uint8_t data[16];
    uint8_t checksum;
    const char* wide_vane;
    bool iSee;
};

TEST(WideVaneRealFrames, NTXWMT18A112AA) {
    const RealSettingsFrame frames[] = {
        // i-See off, vanes in auto. Before this fix the decoder dropped it as "not supported".
        {{0x02, 0x00, 0x00, 0x01, 0x03, 0x19, 0x00, 0x00, 0x00, 0x00, 0x00, 0xAD, 0x00, 0x00, 0x00, 0x00}, 0x91, "AUTO", false},
        // i-See on, AUTO set from Home Assistant, first frame after a reboot.
        {{0x02, 0x00, 0x00, 0x01, 0x0B, 0x19, 0x01, 0x07, 0x00, 0x00, 0x00, 0xAD, 0x00, 0x00, 0x00, 0x00}, 0x81, "AUTO", true},
        // Center set from Home Assistant.
        {{0x02, 0x00, 0x00, 0x01, 0x03, 0x19, 0x00, 0x00, 0x00, 0x00, 0x03, 0xAD, 0x00, 0x00, 0x00, 0x00}, 0x8E, "|", false},
        // i-See on, INDIRECT/DIRECT from the remote.
        {{0x02, 0x00, 0x00, 0x01, 0x0B, 0x19, 0x00, 0x00, 0x00, 0x00, 0x80, 0xAD, 0x00, 0x00, 0x00, 0x00}, 0x09, "AIRFLOW CONTROL", true},
        // WIDE VANE button cycle from the remote.
        {{0x02, 0x00, 0x00, 0x01, 0x0B, 0x19, 0x00, 0x00, 0x00, 0x00, 0x01, 0xAD, 0x00, 0x00, 0x00, 0x00}, 0x88, "←←", true},
        {{0x02, 0x00, 0x00, 0x01, 0x0B, 0x19, 0x00, 0x00, 0x00, 0x00, 0x02, 0xAD, 0x00, 0x00, 0x00, 0x00}, 0x87, "←", true},
        {{0x02, 0x00, 0x00, 0x01, 0x0B, 0x19, 0x00, 0x00, 0x00, 0x00, 0x03, 0xAD, 0x00, 0x00, 0x00, 0x00}, 0x86, "|", true},
        {{0x02, 0x00, 0x00, 0x01, 0x0B, 0x19, 0x00, 0x00, 0x00, 0x00, 0x04, 0xAD, 0x00, 0x00, 0x00, 0x00}, 0x85, "→", true},
        {{0x02, 0x00, 0x00, 0x01, 0x0B, 0x19, 0x00, 0x00, 0x00, 0x00, 0x05, 0xAD, 0x00, 0x00, 0x00, 0x00}, 0x84, "→→", true},
        {{0x02, 0x00, 0x00, 0x01, 0x0B, 0x19, 0x00, 0x00, 0x00, 0x00, 0x0C, 0xAD, 0x00, 0x00, 0x00, 0x00}, 0x7D, "SWING", true},
    };
    for (const auto& f : frames) {
        uint8_t pkt[22] = {0xFC, 0x62, 0x01, 0x30, 0x10};
        std::copy(std::begin(f.data), std::end(f.data), pkt + 5);
        EXPECT_EQ(cn105_protocol::checksum(pkt, 21), f.checksum);
        EXPECT_EQ(f.data[4] > 0x08, f.iSee);
        EXPECT_STREQ(HeatPumpProtocol::decode_wide_vane(f.data[10]), f.wide_vane);
    }
}
