#pragma once
#include "cn105_protocol.h"
#include "protocol_profile.h"
#include "esphome/components/climate/climate_mode.h"

static const uint8_t LOSSNAY_PROFILE = 0x34;
static const uint8_t LOSSNAY_MODE[3] = { 0x00, 0x01, 0x02 };
// Lossnay values are translated to the existing internal protocol names:
// HEAT = heat recovery, FAN = bypass, AUTO = automatic operation.
static const char* LOSSNAY_MODE_MAP[3] = { "HEAT", "FAN", "AUTO" };
static const uint8_t LOSSNAY_FAN[4] = { 0x01, 0x02, 0x03, 0x04 };
static const char* LOSSNAY_FAN_MAP[4] = { "1", "2", "3", "4" };

namespace cn105_protocol {
// ════════════════════════════════════════════════════════════════
// Lossnay profile fields
// ════════════════════════════════════════════════════════════════

static constexpr uint8_t LOSSNAY_POWER_MASK = 0x01;
static constexpr uint8_t LOSSNAY_MODE_MASK = 0x04;
static constexpr uint8_t LOSSNAY_FAN_MASK = 0x08;

struct LossnaySettingsBytes {
    uint8_t power;
    uint8_t mode;
    uint8_t fan;
};

/// Decode the confirmed fields from a Lossnay 0x02 response payload.
inline LossnaySettingsBytes decode_lossnay_settings(const uint8_t* data) {
    return {data[3], data[5], data[6]};
}

/// Decode the confirmed automatic-mode result from a Lossnay 0x09 payload.
inline std::optional<uint8_t> decode_lossnay_actual_mode(const uint8_t* data) {
    const uint8_t actual_mode = data[8];
    return actual_mode <= 0x01 ? std::optional<uint8_t>{actual_mode} : std::nullopt;
}

/// Apply confirmed Lossnay fields to a full 0x41 SET packet.
inline void encode_lossnay_control(
    uint8_t* packet,
    std::optional<uint8_t> power,
    std::optional<uint8_t> mode,
    std::optional<uint8_t> fan
) {
    if (power) {
        packet[8] = *power;
        packet[6] |= LOSSNAY_POWER_MASK;
    }
    if (mode) {
        packet[10] = *mode;
        packet[6] |= LOSSNAY_MODE_MASK;
    }
    if (fan) {
        packet[11] = *fan;
        packet[6] |= LOSSNAY_FAN_MASK;
    }
}

}  // namespace cn105_protocol

namespace esphome::cn105 {

// Confirmed Lossnay dialect, independently testable without UART/component lifecycle.
class LossnayProtocol {
 public:
    static bool supports(ProfileFeature) { return false; }
    static bool decode_functions(heatpumpFunctions&, uint8_t, const uint8_t*, size_t) { return false; }
    static bool encode_functions(uint8_t*, uint8_t*, const heatpumpFunctions&) { return false; }
    static void encode_run_states(uint8_t*) {}
    static const char* fan_setting(climate::ClimateFanMode mode) {
        switch (mode) {
            case climate::CLIMATE_FAN_LOW: return LOSSNAY_FAN_MAP[0];
            case climate::CLIMATE_FAN_MEDIUM: return LOSSNAY_FAN_MAP[1];
            case climate::CLIMATE_FAN_MIDDLE: return LOSSNAY_FAN_MAP[2];
            case climate::CLIMATE_FAN_HIGH: return LOSSNAY_FAN_MAP[3];
            default: return nullptr;
        }
    }
    static std::optional<climate::ClimateFanMode> fan_mode(const char* setting) {
        if (setting == nullptr) return std::nullopt;
        if (std::strcmp(setting, "1") == 0) return climate::CLIMATE_FAN_LOW;
        if (std::strcmp(setting, "2") == 0) return climate::CLIMATE_FAN_MEDIUM;
        if (std::strcmp(setting, "3") == 0) return climate::CLIMATE_FAN_MIDDLE;
        if (std::strcmp(setting, "4") == 0) return climate::CLIMATE_FAN_HIGH;
        return std::nullopt;
    }
    static heatpumpSettings decode_settings(const uint8_t* data, const heatpumpSettings& previous) {
        heatpumpSettings settings{};
        const auto raw = cn105_protocol::decode_lossnay_settings(data);
        const auto power = cn105_protocol::lookup_value_opt(POWER_MAP, POWER, 2, raw.power);
        const auto mode = cn105_protocol::lookup_value_opt(LOSSNAY_MODE_MAP, LOSSNAY_MODE, 3, raw.mode);
        const auto fan = cn105_protocol::lookup_value_opt(LOSSNAY_FAN_MAP, LOSSNAY_FAN, 4, raw.fan);
        settings.power = power ? *power : (previous.power ? previous.power : POWER_MAP[0]);
        settings.mode = mode ? *mode : (previous.mode ? previous.mode : LOSSNAY_MODE_MAP[2]);
        settings.fan = fan ? *fan : (previous.fan ? previous.fan : LOSSNAY_FAN_MAP[0]);
        settings.temperature = previous.temperature;
        settings.vane = previous.vane;
        settings.wideVane = previous.wideVane;
        settings.connected = true;
        return settings;
    }
    static bool accepts_handshake(uint8_t command, uint8_t profile) {
        return command == 0x7A && profile == LOSSNAY_PROFILE;
    }
    void reset() { actual_mode_.reset(); }
    bool decode_actual_mode(const uint8_t* data) {
        const auto next = cn105_protocol::decode_lossnay_actual_mode(data);
        if (next == actual_mode_) return false;
        actual_mode_ = next;
        return true;
    }
    climate::ClimateAction action(const char* power, climate::ClimateMode mode) const {
        if (power == nullptr || std::strcmp(power, "ON") != 0) return climate::CLIMATE_ACTION_OFF;
        if (mode == climate::CLIMATE_MODE_FAN_ONLY) return climate::CLIMATE_ACTION_FAN;
        if (mode == climate::CLIMATE_MODE_HEAT) return climate::CLIMATE_ACTION_HEATING;
        if (mode == climate::CLIMATE_MODE_AUTO) {
            if (!actual_mode_) return climate::CLIMATE_ACTION_IDLE;
            return *actual_mode_ == 0x01 ? climate::CLIMATE_ACTION_FAN : climate::CLIMATE_ACTION_HEATING;
        }
        return climate::CLIMATE_ACTION_OFF;
    }
    bool update_actual_action(const uint8_t* data, const char* power, climate::ClimateMode mode,
                              climate::ClimateAction& current_action) {
        if (!decode_actual_mode(data)) return false;
        const auto next = action(power, mode);
        if (next == current_action) return false;
        current_action = next;
        return true;
    }
    bool command_mode(climate::ClimateMode mode, wantedHeatpumpSettings& wanted) {
        switch (mode) {
            case climate::CLIMATE_MODE_OFF: wanted.power = POWER_MAP[0]; return true;
            case climate::CLIMATE_MODE_HEAT: wanted.mode = LOSSNAY_MODE_MAP[0]; break;
            case climate::CLIMATE_MODE_FAN_ONLY: wanted.mode = LOSSNAY_MODE_MAP[1]; break;
            case climate::CLIMATE_MODE_AUTO: reset(); wanted.mode = LOSSNAY_MODE_MAP[2]; break;
            default: return false;
        }
        // Include ON even if the last received settings still precede a rapid OFF command.
        wanted.power = POWER_MAP[1];
        return true;
    }
    climate::ClimateMode reconcile_power_mode(const heatpumpSettings& settings, heatpumpSettings& current,
                                               climate::ClimateMode mode, bool update_current) {
        const bool changed = field_changed(settings.power, current.power) ||
                             field_changed(settings.mode, current.mode);
        if (update_current) {
            if (settings.power != nullptr) current.power = settings.power;
            if (settings.mode != nullptr) current.mode = settings.mode;
        }
        if (!changed || settings.power == nullptr) return mode;
        if (std::strcmp(settings.power, "ON") != 0) return climate::CLIMATE_MODE_OFF;
        if (settings.mode == nullptr) return mode;
        if (std::strcmp(settings.mode, "HEAT") == 0) return climate::CLIMATE_MODE_HEAT;
        if (std::strcmp(settings.mode, "FAN") == 0) return climate::CLIMATE_MODE_FAN_ONLY;
        if (std::strcmp(settings.mode, "AUTO") == 0) {
            if (mode != climate::CLIMATE_MODE_AUTO) reset();
            return climate::CLIMATE_MODE_AUTO;
        }
        return mode;
    }
    static void encode_control(uint8_t* packet, const wantedHeatpumpSettings& wanted) {
        const auto encode = [](const char* value, const char* const* names, const uint8_t* bytes, int size)
            -> std::optional<uint8_t> {
            if (value == nullptr) return std::nullopt;
            for (int i = 0; i < size; ++i) if (std::strcmp(value, names[i]) == 0) return bytes[i];
            return std::nullopt;
        };
        cn105_protocol::encode_lossnay_control(packet, encode(wanted.power, POWER_MAP, POWER, 2),
            encode(wanted.mode, LOSSNAY_MODE_MAP, LOSSNAY_MODE, 3),
            encode(wanted.fan, LOSSNAY_FAN_MAP, LOSSNAY_FAN, 4));
    }
 private:
    static bool field_changed(const char* value, const char* previous) {
        return value != nullptr && (previous == nullptr || std::strcmp(value, previous) != 0);
    }
    std::optional<uint8_t> actual_mode_;
};

}  // namespace esphome::cn105
