/// test_frame_parser.cpp — Unit tests for FrameParser (Phase 3A).
/// Deps: frame_parser.h, cn105_types.h
///
/// TDD: these tests were written BEFORE the FrameParser implementation.
#include <gtest/gtest.h>
#include "frame_parser.h"

using namespace cn105_protocol;

// ════════════════════════════════════════════════════════════════
// Basic lifecycle
// ════════════════════════════════════════════════════════════════

TEST(FrameParser, InitialState) {
    FrameParser parser;
    EXPECT_FALSE(parser.frame_complete());
    EXPECT_EQ(parser.command(), 0);
    EXPECT_EQ(parser.data_length(), 0);
}

TEST(FrameParser, ResetClearsState) {
    FrameParser parser;
    // Feed start byte then reset
    parser.feed(0xFC);
    parser.reset();
    EXPECT_FALSE(parser.frame_complete());
    EXPECT_EQ(parser.command(), 0);
}

// ════════════════════════════════════════════════════════════════
// Real frame: Settings response (OFF, HEAT, 19.5°C)
// From live capture: FC 62 01 30 10 02 00 00 00 01 1C 00 00 00 00 03 A7 00 00 00 00 94
// ════════════════════════════════════════════════════════════════

TEST(FrameParser, CompleteSettingsFrame) {
    FrameParser parser;
    const uint8_t frame[] = {
        0xFC, 0x62, 0x01, 0x30, 0x10,                               // header (5 bytes)
        0x02, 0x00, 0x00, 0x00, 0x01, 0x1C, 0x00, 0x00,             // data (16 bytes)
        0x00, 0x00, 0x03, 0xA7, 0x00, 0x00, 0x00, 0x00,
        0x94                                                          // checksum
    };

    for (size_t i = 0; i < sizeof(frame) - 1; ++i) {
        EXPECT_FALSE(parser.frame_complete()) << "Should not be complete at byte " << i;
        parser.feed(frame[i]);
    }
    // Feed the checksum byte
    parser.feed(frame[sizeof(frame) - 1]);
    EXPECT_TRUE(parser.frame_complete());
    EXPECT_TRUE(parser.checksum_valid());
    EXPECT_EQ(parser.command(), 0x62);
    EXPECT_EQ(parser.data_length(), 16);

    // Verify payload access
    const uint8_t* data = parser.data();
    EXPECT_EQ(data[0], 0x02);  // settings type
    EXPECT_EQ(data[3], 0x00);  // power OFF
    EXPECT_EQ(data[4], 0x01);  // mode HEAT
}

// ════════════════════════════════════════════════════════════════
// Real frame: CONNECT handshake
// FC 5A 01 30 02 CA 01 A8
// ════════════════════════════════════════════════════════════════

TEST(FrameParser, ConnectPacket) {
    FrameParser parser;
    const uint8_t frame[] = {0xFC, 0x5A, 0x01, 0x30, 0x02, 0xCA, 0x01, 0xA8};

    for (auto b : frame) parser.feed(b);
    EXPECT_TRUE(parser.frame_complete());
    EXPECT_TRUE(parser.checksum_valid());
    EXPECT_EQ(parser.command(), 0x5A);
    EXPECT_EQ(parser.data_length(), 2);
}

// ════════════════════════════════════════════════════════════════
// Real frame: CONNECT response (ACK)
// FC 7A 01 30 01 00 54
// ════════════════════════════════════════════════════════════════

TEST(FrameParser, ConnectAck) {
    FrameParser parser;
    const uint8_t frame[] = {0xFC, 0x7A, 0x01, 0x30, 0x01, 0x00, 0x54};

    for (auto b : frame) parser.feed(b);
    EXPECT_TRUE(parser.frame_complete());
    EXPECT_TRUE(parser.checksum_valid());
    EXPECT_EQ(parser.command(), 0x7A);
    EXPECT_EQ(parser.data_length(), 1);
    EXPECT_EQ(parser.data()[0], 0x00);
}

// ════════════════════════════════════════════════════════════════
// Real frame: SET remote temp (22.8°C)
// FC 41 01 30 10 07 01 1E AE 00 00 00 00 00 00 00 00 00 00 00 00 AA
// ════════════════════════════════════════════════════════════════

TEST(FrameParser, SetRemoteTemp) {
    FrameParser parser;
    const uint8_t frame[] = {
        0xFC, 0x41, 0x01, 0x30, 0x10,
        0x07, 0x01, 0x1E, 0xAE, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0xAA
    };

    for (auto b : frame) parser.feed(b);
    EXPECT_TRUE(parser.frame_complete());
    EXPECT_TRUE(parser.checksum_valid());
    EXPECT_EQ(parser.command(), 0x41);
    EXPECT_EQ(parser.data()[0], 0x07);  // remote temp type
}

// ════════════════════════════════════════════════════════════════
// Error handling
// ════════════════════════════════════════════════════════════════

TEST(FrameParser, GarbageBeforeStartByte) {
    FrameParser parser;
    // Feed garbage then a valid connect ACK
    parser.feed(0x00);
    parser.feed(0xFF);
    parser.feed(0x42);
    EXPECT_FALSE(parser.frame_complete());

    const uint8_t frame[] = {0xFC, 0x7A, 0x01, 0x30, 0x01, 0x00, 0x54};
    for (auto b : frame) parser.feed(b);
    EXPECT_TRUE(parser.frame_complete());
    EXPECT_TRUE(parser.checksum_valid());
}

TEST(FrameParser, BadChecksum) {
    FrameParser parser;
    // Valid connect ACK but with wrong checksum (0x55 instead of 0x54)
    const uint8_t frame[] = {0xFC, 0x7A, 0x01, 0x30, 0x01, 0x00, 0x55};
    for (auto b : frame) parser.feed(b);
    EXPECT_TRUE(parser.frame_complete());
    EXPECT_FALSE(parser.checksum_valid());
}

TEST(FrameParser, BufferOverflowProtection) {
    FrameParser parser;
    // Header claiming data_length=0xFF (way too large)
    parser.feed(0xFC);
    parser.feed(0x62);
    parser.feed(0x01);
    parser.feed(0x30);
    parser.feed(0xFF);  // data_length = 255 > MAX_DATA_BYTES
    // Parser should have reset itself
    EXPECT_FALSE(parser.frame_complete());
}

TEST(FrameParser, ReusableAfterComplete) {
    FrameParser parser;
    // First frame
    const uint8_t frame1[] = {0xFC, 0x7A, 0x01, 0x30, 0x01, 0x00, 0x54};
    for (auto b : frame1) parser.feed(b);
    EXPECT_TRUE(parser.frame_complete());

    // Reset and parse a second frame
    parser.reset();
    const uint8_t frame2[] = {0xFC, 0x5A, 0x01, 0x30, 0x02, 0xCA, 0x01, 0xA8};
    for (auto b : frame2) parser.feed(b);
    EXPECT_TRUE(parser.frame_complete());
    EXPECT_EQ(parser.command(), 0x5A);
}

// ════════════════════════════════════════════════════════════════
// Full poll cycle: 6 sequential frames from real capture
// ════════════════════════════════════════════════════════════════

TEST(FrameParser, SequentialFrames) {
    FrameParser parser;
    // RESPONSE:Settings + RESPONSE:RoomTemp back-to-back
    const uint8_t settings[] = {
        0xFC, 0x62, 0x01, 0x30, 0x10,
        0x02, 0x00, 0x00, 0x01, 0x01, 0x1A, 0x03, 0x07,
        0x00, 0x00, 0x03, 0xAB, 0x00, 0x00, 0x00, 0x00,
        0x87
    };
    const uint8_t roomtemp[] = {
        0xFC, 0x62, 0x01, 0x30, 0x10,
        0x03, 0x00, 0x00, 0x0D, 0x00, 0x00, 0xAE, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x9F
    };

    // Parse first frame
    for (auto b : settings) parser.feed(b);
    EXPECT_TRUE(parser.frame_complete());
    EXPECT_EQ(parser.data()[0], 0x02);  // settings

    // Reset and parse second
    parser.reset();
    for (auto b : roomtemp) parser.feed(b);
    EXPECT_TRUE(parser.frame_complete());
    EXPECT_EQ(parser.data()[0], 0x03);  // room temp
    EXPECT_EQ(parser.data()[3], 0x0D);  // 23°C encoding A
}
