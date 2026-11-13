#include "lossnay_profile.h"
#include "cn105.h"

using namespace esphome;
using namespace esphome::cn105;

// Lossnay protocol reverse-engineering and captures: @lewinjh, original PR #721.
bool LossnayProfile::allow_operation(ProfileFeature feature) {
    if (feature == ProfileFeature::TEMPERATURE) {
        if (!target_warning_logged_) {
            ESP_LOGW("control", "Ignoring Lossnay target-temperature changes: no confirmed setpoint field");
            target_warning_logged_ = true;
        }
    } else {
        ESP_LOGW(TAG, "Ignoring unsupported Lossnay operation (%u)", static_cast<unsigned>(feature));
    }
    return false;
}

const char* LossnayProfile::mode_setting(const char* setting) const {
    const int index = cn105_protocol::lookup_index(LOSSNAY_MODE_MAP, 3, setting);
    if (index >= 0) return LOSSNAY_MODE_MAP[index];
    ESP_LOGW("control", "Ignoring unsupported Lossnay mode %s", setting);
    return nullptr;
}

const char* LossnayProfile::fan_setting(const char* setting) const {
    const int index = cn105_protocol::lookup_index(LOSSNAY_FAN_MAP, 4, setting);
    if (index >= 0) return LOSSNAY_FAN_MAP[index];
    ESP_LOGW("control", "Ignoring unsupported Lossnay fan mode %s", setting);
    return nullptr;
}

void LossnayProfile::decode_settings() {
    auto settings = LossnayProtocol::decode_settings(hp_.data, hp_.currentSettings);
    hp_.heatpumpUpdate(settings);
}

void LossnayProfile::decode_status() {
    heatpumpStatus status = hp_.currentStatus;
    hp_.nonResponseCounter = 0;
    status.operating = false;
    status.compressorFrequency = 0;
    status.inputPower = hp_.convert_input_power_to_W(float((hp_.data[5] << 8) | hp_.data[6]));
    status.kWh = hp_.convert_energy_usage_to_kWh(float((hp_.data[7] << 8) | hp_.data[8]));
    hp_.statusChanged(status);
}

bool LossnayProfile::decode_submode() {
    // data[7] is fixed 0x40; data[8] is actual mode; data[9]/[10] repeat fan.
    return protocol_.update_actual_action(hp_.data, hp_.currentSettings.power, hp_.mode, hp_.action);
}

bool LossnayProfile::decode_functions(uint8_t code, const uint8_t* data, size_t length) {
    return LossnayProtocol::decode_functions(hp_.functions, code, data, length);
}

void LossnayProfile::encode_control(uint8_t* packet) {
    LossnayProtocol::encode_control(packet, hp_.wantedSettings);
}

void LossnayProfile::reconcile_power_mode(heatpumpSettings& settings, bool update_current) {
    hp_.mode = protocol_.reconcile_power_mode(settings, hp_.currentSettings, hp_.mode, update_current);
}

void LossnayProfile::apply_received_settings(heatpumpSettings& settings) {
    if (hp_.wantedSettings.mode == nullptr && hp_.wantedSettings.power == nullptr) {
        this->reconcile_power_mode(settings, true);
    }
    this->update_action();
    if (hp_.wantedSettings.fan == nullptr) this->reconcile_fan(settings, true);
    hp_.currentSettings.iSee = false;
    hp_.currentSettings.connected = true;
}

void LossnayProfile::apply_wanted_settings() {
    if (hp_.wantedSettings.mode != nullptr || hp_.wantedSettings.power != nullptr) {
        this->reconcile_power_mode(hp_.wantedSettings, false);
        this->update_action();
    }
    if (hp_.wantedSettings.fan != nullptr) this->reconcile_fan(hp_.wantedSettings, false);
}

void LossnayProfile::control_mode() {
    if (!protocol_.command_mode(hp_.mode, hp_.wantedSettings)) {
        ESP_LOGW("control", "Ignoring unsupported Lossnay climate mode");
    }
}

bool LossnayProfile::process_temperature_change(const climate::ClimateCall& call) {
    if (call.get_target_temperature().has_value() || call.get_target_temperature_low().has_value() ||
        call.get_target_temperature_high().has_value()) {
        this->allow_operation(ProfileFeature::TEMPERATURE);
    }
    return false;
}

void LossnayProfile::update_action() {
    hp_.action = protocol_.action(hp_.currentSettings.power, hp_.mode);
}

void LossnayProfile::control_fan() {
    const char* setting = LossnayProtocol::fan_setting(hp_.fan_mode.value());
    if (setting != nullptr) hp_.setFanSpeed(setting);
    else ESP_LOGW("control", "Ignoring unsupported Lossnay fan mode");
}

void LossnayProfile::reconcile_fan(heatpumpSettings& settings, bool update_current) {
    if (!hp_.hasChanged(hp_.currentSettings.fan, settings.fan, "fan")) return;
    const auto mode = LossnayProtocol::fan_mode(settings.fan);
    if (!mode) return;
    if (update_current) hp_.currentSettings.fan = settings.fan;
    hp_.fan_mode = *mode;
}
