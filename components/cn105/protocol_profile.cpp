#include "protocol_profile.h"
#include "cn105.h"
#include "heatpump_profile.h"
#include "lossnay_profile.h"

namespace esphome::cn105 {

heatpumpStatus Cn105ProtocolProfile::decode_room_status(const uint8_t* data, const heatpumpStatus& previous) const {
    if (data[6] == 0 && !cn105_protocol::lookup_value_opt(ROOM_TEMP_MAP, ROOM_TEMP, 32, data[3]))
        ESP_LOGW("Decoder", "Unknown room_temp byte 0x%02X — keeping previous value", data[3]);
    return CommonProtocol::decode_room_status(data, previous);
}

std::unique_ptr<Cn105ProtocolProfile> make_protocol_profile(CN105Climate& climate, bool lossnay) {
    if (lossnay) return std::make_unique<LossnayProfile>(climate);
    return std::make_unique<HeatPumpProfile>(climate);
}

}  // namespace esphome::cn105
