/// frame_parser.h — Standalone UART frame parser for Mitsubishi CN105 protocol.
/// Deps: cn105_protocol.h (checksum), cn105_types.h (MAX_DATA_BYTES)
///
/// Extracts byte-by-byte frame assembly from CN105Climate into a pure,
/// testable class with no ESPHome dependency.
///
/// Usage:
///   FrameParser parser;
///   while (serial.available()) {
///       parser.feed(serial.read());
///       if (parser.frame_complete()) {
///           process(parser.command(), parser.data(), parser.data_length());
///           parser.reset();
///       }
///   }
#pragma once

#include <cstdint>
#include <cstring>
#include "cn105_protocol.h"
#include "cn105_types.h"

namespace cn105_protocol {

class FrameParser {
public:
    FrameParser() { reset(); }

    /// Feed one byte from the UART stream.
    /// After each call, check frame_complete() to see if a full frame is ready.
    void feed(uint8_t byte) {
        if (!found_start_) {
            if (byte == 0xFC) {
                found_start_ = true;
                bytes_read_ = 0;
                buffer_[bytes_read_++] = byte;
            }
            // Ignore garbage bytes before 0xFC
            return;
        }

        // Overflow protection
        if (bytes_read_ >= MAX_DATA_BYTES) {
            reset();
            return;
        }

        buffer_[bytes_read_] = byte;

        // At index 4, we know the data length
        if (bytes_read_ == 4) {
            data_length_ = byte;
            command_ = buffer_[1];

            // Sanity check: declared length must fit in buffer
            // Total frame = 5 (header) + data_length + 1 (checksum)
            if ((data_length_ + 6) > MAX_DATA_BYTES) {
                reset();
                return;
            }
        }

        // Check if we have a complete frame
        // Complete when bytes_read_ == 5 (header) + data_length_ (payload) + 1 (checksum) - 1 (0-indexed)
        if (data_length_ >= 0 && bytes_read_ == data_length_ + 5) {
            // Frame is complete (checksum byte is at buffer_[bytes_read_])
            checksum_byte_ = byte;
            frame_complete_ = true;
        } else {
            bytes_read_++;
        }
    }

    /// Reset the parser to accept a new frame.
    void reset() {
        found_start_ = false;
        frame_complete_ = false;
        bytes_read_ = 0;
        data_length_ = -1;
        command_ = 0;
        checksum_byte_ = 0;
    }

    /// True when a complete frame (header + payload + checksum) has been received.
    bool frame_complete() const { return frame_complete_; }

    /// True when the received checksum matches the computed one.
    /// Only valid after frame_complete() returns true.
    bool checksum_valid() const {
        if (!frame_complete_) return false;
        uint8_t computed = checksum(buffer_, data_length_ + 5);
        return computed == checksum_byte_;
    }

    /// The command byte (offset 1 in the frame header).
    uint8_t command() const { return command_; }

    /// The declared data length (offset 4 in the frame header).
    int data_length() const { return data_length_ < 0 ? 0 : data_length_; }

    /// Pointer to the payload data (starts at offset 5 in the buffer).
    /// Only valid after frame_complete() returns true.
    const uint8_t* data() const { return &buffer_[5]; }

    /// Pointer to the full raw frame buffer (header + payload, no checksum).
    const uint8_t* raw() const { return buffer_; }

    /// Total frame size including header, payload, and checksum.
    int frame_size() const {
        if (!frame_complete_) return 0;
        return data_length_ + 6; // 5 header + data + 1 checksum
    }

private:
    uint8_t buffer_[MAX_DATA_BYTES]{};
    bool found_start_ = false;
    bool frame_complete_ = false;
    int bytes_read_ = 0;
    int data_length_ = -1;
    uint8_t command_ = 0;
    uint8_t checksum_byte_ = 0;
};

} // namespace cn105_protocol
