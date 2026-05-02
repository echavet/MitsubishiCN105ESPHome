/// test_checksum.cpp — Tests de non-régression pour checkSum()
/// Deps: cn105_types.h (constantes CONNECT), esphome_stubs.h
#include <gtest/gtest.h>
#include "esphome_stubs.h"
#include "cn105_types.h"

// Standalone reimplementation of checkSum (copy from hp_writings.cpp)
// This avoids pulling in the entire CN105Climate class.
static uint8_t checkSum(uint8_t bytes[], int len) {
    uint8_t sum = 0;
    for (int i = 0; i < len; i++) {
        sum += bytes[i];
    }
    return (0xfc - sum) & 0xff;
}

// ---- Tests ----

TEST(CheckSumTest, ConnectPacketHasCorrectChecksum) {
    // CONNECT constant: {0xfc, 0x5a, 0x01, 0x30, 0x02, 0xca, 0x01, 0xa8}
    // Last byte (0xa8) IS the checksum of the first 7 bytes
    uint8_t packet[CONNECT_LEN];
    memcpy(packet, CONNECT, CONNECT_LEN);

    uint8_t expected = packet[CONNECT_LEN - 1]; // 0xa8
    uint8_t computed = checkSum(packet, CONNECT_LEN - 1);

    EXPECT_EQ(computed, expected)
        << "checkSum of CONNECT packet should equal its last byte (0xA8)";
}

TEST(CheckSumTest, InfoPacketSettings) {
    // Info request for settings (0x02):
    // FC 42 01 30 10 02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 7B
    uint8_t packet[22] = {0xFC, 0x42, 0x01, 0x30, 0x10, 0x02, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    uint8_t computed = checkSum(packet, 21);
    EXPECT_EQ(computed, 0x7B);
}

TEST(CheckSumTest, AllZeroPacket) {
    uint8_t packet[10] = {0};
    uint8_t computed = checkSum(packet, 10);
    // sum=0, (0xfc - 0) & 0xff = 0xfc
    EXPECT_EQ(computed, 0xFC);
}

TEST(CheckSumTest, OverflowHandling) {
    // sum of all bytes = 0xFF * 4 = 0x3FC, truncated to uint8_t = 0xFC
    // (0xfc - 0xfc) & 0xff = 0x00
    uint8_t packet[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    uint8_t computed = checkSum(packet, 4);
    EXPECT_EQ(computed, 0x00);
}

TEST(CheckSumTest, SingleBytePacket) {
    uint8_t packet[1] = {0x42};
    uint8_t computed = checkSum(packet, 1);
    // (0xfc - 0x42) & 0xff = 0xBA
    EXPECT_EQ(computed, 0xBA);
}
