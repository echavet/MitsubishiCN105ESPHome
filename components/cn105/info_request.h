#pragma once

#include <cstdint>
#include <functional>

namespace esphome {

    class CN105Climate; // forward declaration

    struct InfoRequest {
        const char* id;
        const char* description;
        uint8_t code;                 // e.g. 0x02, 0x03, 0x06, 0x09, 0x42
        uint8_t maxFailures;          // disable after this many soft failures
        uint8_t failures;             // current failure count
        bool disabled;                // permanently disabled when not supported
        bool awaiting;                // awaiting a matching response
        uint32_t soft_timeout_ms;     // optional: skip forward on timeout without blocking cycle

        // Optional condition to decide whether this request should be sent in this device/config
        std::function<bool(const CN105Climate&)> canSend;

        // Optional response handler invoked when the matching response (code) is received
        std::function<void(CN105Climate&)> onResponse;

        InfoRequest(
            const char* id,
            const char* description,
            uint8_t code,
            uint8_t maxFailures = 3,
            uint32_t soft_timeout_ms = 0
        ) : id(id), description(description), code(code), maxFailures(maxFailures), failures(0), disabled(false), awaiting(false), soft_timeout_ms(soft_timeout_ms), canSend(nullptr), onResponse(nullptr) {}
    };
}


