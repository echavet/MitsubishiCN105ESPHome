#pragma once
#include "cn105_protocol.h"
#include "cn105_types.h"
#include <cstdio>

namespace esphome::cn105 {

// Payload fields shared by the heat-pump and Lossnay dialects.
struct CommonProtocol {
    static heatpumpStatus decode_room_status(const uint8_t* data, const heatpumpStatus& previous) {
        heatpumpStatus status = previous;
        status.outsideAirTemperature = data[5] > 1 ? (data[5] - 128) / 2.0f : NAN;
        if (data[6] != 0) {
            status.roomTemperature = (static_cast<int>(data[6]) - 128) / 2.0f;
        } else {
            const auto room = cn105_protocol::lookup_value_opt(ROOM_TEMP_MAP, ROOM_TEMP, 32, data[3]);
            if (room) status.roomTemperature = static_cast<float>(*room);
        }
        status.runtimeHours = static_cast<float>((data[11] << 16) | (data[12] << 8) | data[13]) / 60;
        return status;
    }

    static std::string decode_error(const uint8_t* data) {
        // Bit 7 announces error-reporting availability, rather than an error.
        const uint8_t code = data[4] & 0x7F;
        const uint8_t sub = data[5];
        if (code == 0 && sub == 0) return "No Error";
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "Error 0x%02X sub 0x%02X", code, sub);
        return buffer;
    }
};

}  // namespace esphome::cn105
