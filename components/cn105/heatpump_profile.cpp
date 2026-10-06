#include "heatpump_profile.h"
#include "climate_action.h"
#include "cn105.h"
#include <algorithm>
#include <cmath>

using namespace esphome;
using namespace esphome::cn105;

namespace {

/// 0x09 status bytes can be model-specific. Keep previous value and log once
/// at DEBUG so a repeating unknown (issue #668) does not WARN-spam every poll.
void log_unknown_status_byte_once(const char* field, uint8_t byte_value) {
    static cn105_protocol::unknown_lookup_cache seen;
    if (cn105_protocol::first_unknown_lookup(seen, field, byte_value)) {
        ESP_LOGD("Decoder", "Unknown %s byte 0x%02X — keeping previous value", field, byte_value);
    }
}

}  // namespace

const char* HeatPumpProfile::mode_setting(const char* setting) const {
    const int index = cn105_protocol::lookup_index(MODE_MAP, 5, setting);
    return MODE_MAP[index >= 0 ? index : 0];
}

const char* HeatPumpProfile::fan_setting(const char* setting) const {
    const int index = cn105_protocol::lookup_index(FAN_MAP, 6, setting);
    return FAN_MAP[index >= 0 ? index : 0];
}

void HeatPumpProfile::decode_settings() {
    heatpumpSettings receivedSettings{};
    heatpumpRunStates receivedRunStates{};
    ESP_LOGD("Decoder", "[0x02 is settings]");

    receivedSettings.connected = true;

    const uint8_t power_byte = hp_.data[3];
    auto power_opt = cn105_protocol::lookup_value_opt(POWER_MAP, POWER, 2, power_byte);
    if (power_opt) {
        receivedSettings.power = *power_opt;
    } else {
        ESP_LOGW("Decoder", "Unknown power byte 0x%02X — keeping previous value", power_byte);
        receivedSettings.power = hp_.currentSettings.power
            ? hp_.currentSettings.power
            : POWER_MAP[0];  // default to "OFF" when no prior value exists
    }

    receivedSettings.iSee = (hp_.data[4] > 0x08);
    uint8_t modeByte = (receivedSettings.iSee ? (hp_.data[4] - 0x08) : hp_.data[4]);
    auto mode_opt = cn105_protocol::lookup_value_opt(MODE_MAP, MODE, 5, modeByte);
    if (mode_opt) {
        receivedSettings.mode = *mode_opt;
    } else {
        ESP_LOGW("Decoder", "Unknown mode byte 0x%02X — keeping previous value", modeByte);
        receivedSettings.mode = hp_.currentSettings.mode
            ? hp_.currentSettings.mode
            : MODE_MAP[4];  // default to "AUTO" when no prior value exists
    }

    ESP_LOGD("Decoder", "[Power : %s]", receivedSettings.power);
    ESP_LOGD("Decoder", "[iSee  : %d]", receivedSettings.iSee);
    ESP_LOGD("Decoder", "[Mode  : %s]", receivedSettings.mode);

    receivedSettings.temperature = this->decodeSettingsTemperature(hp_.data);

    ESP_LOGD("Decoder", "[Temp °C: %f]", receivedSettings.temperature);

    const uint8_t fan_byte = hp_.data[6];
    auto fan_opt = cn105_protocol::lookup_value_opt(FAN_MAP, FAN, 6, hp_.data[6]);
    if (fan_opt) {
        receivedSettings.fan = *fan_opt;
    } else {
        ESP_LOGW("Decoder", "Unknown fan byte 0x%02X — keeping previous value", fan_byte);
        receivedSettings.fan = hp_.currentSettings.fan
            ? hp_.currentSettings.fan
            : FAN_MAP[0];
    }
    ESP_LOGD("Decoder", "[Fan: %s]", receivedSettings.fan);

    {
        auto vane_opt = cn105_protocol::lookup_value_opt(VANE_MAP, VANE, 7, hp_.data[7]);
        if (vane_opt) {
            receivedSettings.vane = *vane_opt;
        } else {
            ESP_LOGW("Decoder", "Unknown vane byte 0x%02X — keeping previous value", hp_.data[7]);
            receivedSettings.vane = hp_.currentSettings.vane
                ? hp_.currentSettings.vane
                : VANE_MAP[0];  // default to "AUTO" when no prior value exists
        }
        ESP_LOGD("Decoder", "[Vane: %s]", receivedSettings.vane);
    }

    // --- START OF MODIFIED SECTION - Reverted widevane section back to more or less original state
    if ((hp_.data[10] != 0) && (hp_.traits_.supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL))) {    // wideVane is not always supported
        uint8_t wideVaneByte = hp_.data[10] & 0x0F;
        auto wideVane_opt = cn105_protocol::lookup_value_opt(WIDEVANE_MAP, WIDEVANE, 8, wideVaneByte);
        if (wideVane_opt) {
            receivedSettings.wideVane = *wideVane_opt;
        } else {
            ESP_LOGW("Decoder", "Unknown wideVane byte 0x%02X — keeping previous value", wideVaneByte);
            // Guard against null: on the first settings packet currentSettings.wideVane
            // is still nullptr, and an unknown byte here would otherwise propagate a null
            // pointer into the %s log below (and downstream), panicking the ESP32.
            receivedSettings.wideVane = hp_.currentSettings.wideVane
                ? hp_.currentSettings.wideVane
                : WIDEVANE_MAP[2];  // default to "|" (center) when no prior value exists
        }
        this->wide_vane_adjust_ = (hp_.data[10] & 0xF0) == 0x80 ? true : false;
        ESP_LOGD("Decoder", "[wideVane: %s (adj:%d)]", receivedSettings.wideVane, this->wide_vane_adjust_);
    } else {
        ESP_LOGD("Decoder", "widevane is not supported");
    }
    // --- END OF MODIFIED SECTION ---

    if (hp_.iSee_sensor_ != nullptr) {
        hp_.iSee_sensor_->publish_state(receivedSettings.iSee);
    }

    // --- TARGET HUMIDITY (byte 12 of 0x02 settings packet) ---
    // Some premium models (e.g. MSZ-LN series) store a target humidity
    // percentage in data[12]. This value changes when the mode is switched
    // via the IR remote (e.g. COOL→70%, DRY→50%, HEAT→40%).
    // Not all models populate this byte — it may read 0x00 on unsupported units.
    if (hp_.target_humidity_sensor_ != nullptr) {
        uint8_t raw_humidity = hp_.data[12];
        if (raw_humidity > 0 && raw_humidity <= 100) {
            float humidity_pct = static_cast<float>(raw_humidity);
            if (hp_.target_humidity_sensor_->get_raw_state() != humidity_pct) {
                ESP_LOGD("Decoder", "[Target Humidity: %.0f%%]", humidity_pct);
                hp_.target_humidity_sensor_->publish_state(humidity_pct);
            }
        } else if (raw_humidity != 0) {
            ESP_LOGD("Decoder", "[Target Humidity byte out of range: 0x%02X]", raw_humidity);
        }
    }

    // --- AIRFLOW CONTROL START
    if (hp_.airflow_control_select_ != nullptr) {
        if (hp_.data[10] == 0x80) {
            if (receivedSettings.iSee) {
                auto airflow_opt = cn105_protocol::lookup_value_opt(AIRFLOW_CONTROL_MAP, AIRFLOW_CONTROL, 3, hp_.data[14]);
                if (airflow_opt) {
                    receivedRunStates.airflow_control = *airflow_opt;
                } else {
                    ESP_LOGW("Decoder", "Unknown airflow_control byte 0x%02X — keeping previous value", hp_.data[14]);
                    receivedRunStates.airflow_control = hp_.currentRunStates.airflow_control;
                }
            } else {
                // For some reason data[10] is 0x80, but the i-See sensor is not active.
                // Some units let us do this, but the real mode is unknown (might be powersave) and the i-See sensor does not get activated.
                //receivedRunStates.airflow_control = "N/A";
                ESP_LOGD("Decoder", "i-See sensor not present/active.");
                receivedRunStates.airflow_control = AIRFLOW_CONTROL_MAP[0];
            }
        } else {
            receivedRunStates.airflow_control = AIRFLOW_CONTROL_MAP[0];
        }
        if (!hp_.currentRunStates.airflow_control || strcmp(receivedRunStates.airflow_control, hp_.currentRunStates.airflow_control) != 0) {
            hp_.currentRunStates.airflow_control = receivedRunStates.airflow_control;
            hp_.airflow_control_select_->publish_state(receivedRunStates.airflow_control);
        }
    }

    // --- AIRFLOW CONTROL END

    hp_.heatpumpUpdate(receivedSettings);
}

void HeatPumpProfile::decode_status() {
    //FC 62 01 30 10 06 00 00 1A 01 00 00 00 00 00 00 00 00 00 00 00 3C
    //MSZ-RW25VGHZ-SC1 / MUZ-RW25VGHZ-SC1
    //FC 62 01 30 10 06 00 00 00 01 00 08 05 50 00 00 42 00 00 00 00 B7
    //                           OP IP IP EU EU       ??
    // OP = operating status (1 = compressor running, 0 = standby)
    // IP = Current input power in Watts (16-bit decimal)
    // EU = energy usage
    //      (used energy in kWh = value/10)
    //      TODO: Currently the maximum size of the counter is not known and
    //            if the counter extends to other bytes.
    // ?? = unknown bytes that appear to have a fixed/constant value
    heatpumpStatus receivedStatus{};
    ESP_LOGD("Decoder", "[0x06 is status]");
    //hp_.last_received_packet_sensor->publish_state("0x62-> 0x06: Data -> Heatpump Status");

    // reset counter (because a reply indicates it is connected)
    hp_.nonResponseCounter = 0;
    {
        receivedStatus.operating = hp_.data[4];
        // Some models (e.g. PAA/PUZ combo) report noise on this byte while not operating; set report_when_idle false to
        // force the frequency to 0 whenever this unit is not operating. For multi-head systems, it may be useful to report
        // compressor frequency because they share an outdoor compressor which may be running even when this indoor unit is not.
        receivedStatus.compressorFrequency = HeatPumpProtocol::compressor_frequency(hp_.data, hp_.compressor_frequency_report_when_idle_);
    }
    receivedStatus.inputPower = hp_.convert_input_power_to_W(float((hp_.data[5] << 8) | hp_.data[6]));
    receivedStatus.kWh = hp_.convert_energy_usage_to_kWh(float((hp_.data[7] << 8) | hp_.data[8]));

    // no change with this packet to roomTemperature
    receivedStatus.roomTemperature = hp_.currentStatus.roomTemperature;
    receivedStatus.outsideAirTemperature = hp_.currentStatus.outsideAirTemperature;
    receivedStatus.runtimeHours = hp_.currentStatus.runtimeHours;
    hp_.statusChanged(receivedStatus);
}

bool HeatPumpProfile::decode_submode() {
    bool climate_changed = false;

    ESP_LOGD("Decoder", "[0x09 is sub modes]");

    heatpumpSettings receivedSettings{};

    // Use std::optional lookups — keep previous value on unknown bytes
    auto stage_opt = cn105_protocol::lookup_value_opt(STAGE_MAP, STAGE, 7, hp_.data[4]);
    if (stage_opt) {
        receivedSettings.stage = *stage_opt;
    } else {
        log_unknown_status_byte_once("stage", hp_.data[4]);
        receivedSettings.stage = hp_.currentSettings.stage
            ? hp_.currentSettings.stage
            : STAGE_MAP[0];  // default to "IDLE" when no prior value exists
    }

    auto sub_mode_opt = cn105_protocol::lookup_value_opt(SUB_MODE_MAP, SUB_MODE, 6, hp_.data[3]);
    if (sub_mode_opt) {
        receivedSettings.sub_mode = *sub_mode_opt;
    } else {
        log_unknown_status_byte_once("sub_mode", hp_.data[3]);
        receivedSettings.sub_mode = hp_.currentSettings.sub_mode
            ? hp_.currentSettings.sub_mode
            : SUB_MODE_MAP[0];  // default to "NORMAL" when no prior value exists
    }

    auto auto_sub_mode_opt = cn105_protocol::lookup_value_opt(AUTO_SUB_MODE_MAP, AUTO_SUB_MODE, 7, hp_.data[5]);
    if (auto_sub_mode_opt) {
        receivedSettings.auto_sub_mode = *auto_sub_mode_opt;
    } else {
        log_unknown_status_byte_once("auto_sub_mode", hp_.data[5]);
        receivedSettings.auto_sub_mode = hp_.currentSettings.auto_sub_mode
            ? hp_.currentSettings.auto_sub_mode
            : AUTO_SUB_MODE_MAP[0];  // default to "AUTO_OFF" when no prior value exists
    }

    ESP_LOGD("Decoder", "[Stage : %s]", receivedSettings.stage);
    ESP_LOGD("Decoder", "[Sub Mode  : %s]", receivedSettings.sub_mode);
    ESP_LOGD("Decoder", "[Auto Mode Sub Mode  : %s]", receivedSettings.auto_sub_mode);

    //hp_.heatpumpUpdate(receivedSettings);
    if (hp_.stage_sensor_ != nullptr) {
        if (!hp_.currentSettings.stage || strcmp(receivedSettings.stage, hp_.currentSettings.stage) != 0) {
            hp_.currentSettings.stage = receivedSettings.stage;
            hp_.stage_sensor_->publish_state(receivedSettings.stage);

            // If using stage as operating fallback, update action immediately when stage changes
            // and publish to Home Assistant
            if (hp_.use_stage_for_operating_status_) {
                this->update_action();
                climate_changed = true;
            }
        }
    }
    if (HeatPumpProtocol::apply_sub_mode(hp_.currentSettings, receivedSettings.sub_mode,
                                       [this]() { this->update_action(); })) {
        if (hp_.Sub_mode_sensor_ != nullptr) {
            hp_.Sub_mode_sensor_->publish_state(receivedSettings.sub_mode);
        }
        // The 0x09 sub-mode can change independently of the normal status packet.
        // Ask the driver to publish the refreshed action even without a sub-mode sensor.
        climate_changed = true;
    }
    if (hp_.Auto_sub_mode_sensor_ != nullptr && (!hp_.currentSettings.auto_sub_mode || strcmp(receivedSettings.auto_sub_mode, hp_.currentSettings.auto_sub_mode) != 0)) {
        hp_.currentSettings.auto_sub_mode = receivedSettings.auto_sub_mode;
        hp_.Auto_sub_mode_sensor_->publish_state(receivedSettings.auto_sub_mode);
    }

    return climate_changed;
}

void HeatPumpProfile::decode_hvac_options() {
    //MSZ-LN25VG2W
    //FC 62 01 30 10 42 01 01 01 00 00 00 00 00 00 00 00 00 00 00 00 18
    //                  AP NM CL
    // AP = air purifier (1 = on, 0 = off)
    // NM = night mode (1 = on, 0 = off)
    // CL = circulator (1 = on, 0 = off) ! MIGHT BE SAME BYTE AS ECONOCOOL - NEEDS TESTING !
    heatpumpRunStates receivedRunStates{};
    ESP_LOGD("Decoder", "[0x42 is HVAC options]");

    if (hp_.air_purifier_switch_ != nullptr) {
        receivedRunStates.air_purifier = hp_.data[1];
        ESP_LOGD("Decoder", "[Air purifier : %s]", receivedRunStates.air_purifier ? "ON" : "OFF");
        if (receivedRunStates.air_purifier != hp_.currentRunStates.air_purifier || receivedRunStates.air_purifier != hp_.air_purifier_switch_->state) {
            hp_.currentRunStates.air_purifier = receivedRunStates.air_purifier;
            hp_.air_purifier_switch_->publish_state(receivedRunStates.air_purifier);
        }
    }
    if (hp_.night_mode_switch_ != nullptr) {
        receivedRunStates.night_mode = hp_.data[2];
        ESP_LOGD("Decoder", "[Night mode : %s]", receivedRunStates.night_mode ? "ON" : "OFF");
        if (receivedRunStates.night_mode != hp_.currentRunStates.night_mode || receivedRunStates.night_mode != hp_.night_mode_switch_->state) {
            hp_.currentRunStates.night_mode = receivedRunStates.night_mode;
            hp_.night_mode_switch_->publish_state(receivedRunStates.night_mode);
        }
    }
    if (hp_.circulator_switch_ != nullptr) {
        receivedRunStates.circulator = hp_.data[3];
        ESP_LOGD("Decoder", "[Circulator : %s]", receivedRunStates.circulator ? "ON" : "OFF");
        if (receivedRunStates.circulator != hp_.currentRunStates.circulator || receivedRunStates.circulator != hp_.circulator_switch_->state) {
            hp_.currentRunStates.circulator = receivedRunStates.circulator;
            hp_.circulator_switch_->publish_state(receivedRunStates.circulator);
        }
    }
}

void HeatPumpProfile::apply_received_settings(heatpumpSettings& settings) {

    if ((hp_.wantedSettings.mode == nullptr) && (hp_.wantedSettings.power == nullptr)) {        // to prevent overwriting a user demand
        this->reconcile_power_mode(settings, true);
    }

    this->update_action();       // update action info on HA climate component

    if (hp_.wantedSettings.fan == nullptr) {  // to prevent overwriting a user demand
        this->reconcile_fan(settings, true);
    }

    if (hp_.wantedSettings.vane == nullptr) { // to prevent overwriting a user demand
        this->checkVaneSettings(settings);
    }

    if (hp_.wantedSettings.wideVane == nullptr) { // to prevent overwriting a user demand
        this->checkWideVaneSettings(settings);
    }

    if (this->shouldApplyIncomingSetpoint(settings)) {
        this->update_target_temperatures(settings.temperature);
        hp_.currentSettings.temperature = settings.temperature;
    }

    hp_.currentSettings.iSee = settings.iSee;

    hp_.currentSettings.connected = true;

}

void HeatPumpProfile::reconcile_power_mode(heatpumpSettings& settings, bool updateCurrentSettings) {
    hp_.mode = HeatPumpProtocol::reconcile_power_mode(settings, hp_.currentSettings, hp_.mode,
        hp_.supports_dual_setpoint_, updateCurrentSettings);
}

void HeatPumpProfile::checkVaneSettings(heatpumpSettings& settings, bool updateCurrentSettings) {
    if (this->shouldIgnoreIncomingVane(settings)) {
        this->updateExtraSelectComponents(settings);
        return;
    }

    if (hp_.hasChanged(hp_.currentSettings.vane, settings.vane, "vane")) {
        ESP_LOGI(LOG_SETTINGS_TAG, "vane setting changed");

        //hp_.debugSettings("settings", settings);

        if (updateCurrentSettings) {
            //ESP_LOGD(LOG_SETTINGS_TAG, "updating currentSetting with new value");
            hp_.currentSettings.vane = settings.vane;
        }

        if (strcmp(settings.vane, "SWING") == 0) {
            if ((hp_.currentSettings.wideVane != nullptr) && (strcmp(hp_.currentSettings.wideVane, "SWING") == 0)) {
                hp_.swing_mode = climate::CLIMATE_SWING_BOTH;
            } else {
                hp_.swing_mode = climate::CLIMATE_SWING_VERTICAL;
            }
        } else {
            if ((hp_.currentSettings.wideVane != nullptr) && (strcmp(hp_.currentSettings.wideVane, "SWING") == 0)) {
                hp_.swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
            } else {
                hp_.swing_mode = climate::CLIMATE_SWING_OFF;
            }
        }
        ESP_LOGD(LOG_SETTINGS_TAG, "Swing mode is: %i", hp_.swing_mode);
    }

    this->updateExtraSelectComponents(settings);
}

void HeatPumpProfile::checkWideVaneSettings(heatpumpSettings& settings, bool updateCurrentSettings) {

    /* ******** HANDLE MITSUBISHI VANE CHANGES ********
     * VANE_MAP[7]        = {"AUTO", "1", "2", "3", "4", "5", "SWING"};
     * WIDEVANE_MAP[8]   = { "<<", "<",  "|",  ">",  ">>", "<>", "SWING", "AIRFLOW CONTROL" }
     */

    if (hp_.hasChanged(hp_.currentSettings.wideVane, settings.wideVane, "wideVane")) {    // widevane setting change ?
        ESP_LOGI(TAG, "widevane setting changed");
        hp_.debugSettings("settings", settings);

        // here I hope that the vane and widevane are always sent together
        if (updateCurrentSettings) {
            hp_.currentSettings.wideVane = settings.wideVane;
        }

        if (strcmp(settings.wideVane, "SWING") == 0) {
            if ((hp_.currentSettings.vane != nullptr) && (strcmp(hp_.currentSettings.vane, "SWING") == 0)) {
                hp_.swing_mode = climate::CLIMATE_SWING_BOTH;
            } else {
                hp_.swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
            }
        } else {
            if ((hp_.currentSettings.vane != nullptr) && (strcmp(hp_.currentSettings.vane, "SWING") == 0)) {
                hp_.swing_mode = climate::CLIMATE_SWING_VERTICAL;
            } else {
                hp_.swing_mode = climate::CLIMATE_SWING_OFF;
            }
        }
        ESP_LOGD(TAG, "Swing mode is: %i", hp_.swing_mode);
    }

    /*if (hp_.hasChanged(hp_.van_orientation->state.c_str(), settings.vane, "select vane")) {
        ESP_LOGI(TAG, "vane setting (extra select component) changed");
        hp_.van_orientation->publish_state(currentSettings.vane);
    }*/

    this->updateExtraSelectComponents(settings);
}

void HeatPumpProfile::updateExtraSelectComponents(heatpumpSettings& settings) {

    if (hp_.vertical_vane_select_ != nullptr) {
        if (hp_.hasChanged(hp_.vertical_vane_select_->current_option(), settings.vane, "select vane")) {
            ESP_LOGI(TAG, "vane setting (extra select component) changed");
            hp_.vertical_vane_select_->publish_state(settings.vane);
        }
    }
    if (hp_.horizontal_vane_select_ != nullptr) {
        if (hp_.hasChanged(hp_.horizontal_vane_select_->current_option(), settings.wideVane, "select wideVane")) {
            ESP_LOGI(TAG, "widevane setting (extra select component) changed");
            hp_.horizontal_vane_select_->publish_state(settings.wideVane);
        }
    }
}

float HeatPumpProfile::decodeSettingsTemperature(const uint8_t* data) {
    return HeatPumpProtocol::decode_temperature(data, hp_.currentSettings.temperature,
        hp_.use_msz_a24na_setpoint_table_, this->use_temperature_encoding_b_, this->use_temperature_encoding_b_latched_);
}

bool HeatPumpProfile::hasPendingUserTemperature() const {
    return (hp_.wantedSettings.temperature != -1.0f) &&
           (hp_.wantedSettings.hasChanged) &&
           (!hp_.wantedSettings.hasBeenSent);
}

bool HeatPumpProfile::isWithinPostSendGrace() const {
    if (!hp_.wantedSettings.hasBeenSent) return false;
    uint32_t graceMs = hp_.update_interval_ + DEFER_SCHEDULE_UPDATE_LOOP_DELAY;
    return (CUSTOM_MILLIS - hp_.wantedSettings.lastChange) < graceMs;
}

bool HeatPumpProfile::disagreesWithLastUserSetpoint(float incoming) const {
    return cn105_protocol::setpoint_disagrees_within_grace(incoming, hp_.wantedSettings.last_user_temperature,
        hp_.wantedSettings.last_user_temperature_ms, CUSTOM_MILLIS, RECEIVED_SETPOINT_GRACE_WINDOW_MS);
}

bool HeatPumpProfile::shouldApplyIncomingSetpoint(const heatpumpSettings& settings) {
    if (hp_.wantedSettings.temperature != -1) return false;
    if (this->hasPendingUserTemperature()) {
        ESP_LOGD(LOG_SETTINGS_TAG, "Ignoring setpoint: pending user temp");
        return false;
    }
    if (this->isWithinPostSendGrace()) {
        ESP_LOGD(LOG_SETTINGS_TAG, "Ignoring setpoint: post-send grace");
        return false;
    }
    if (this->disagreesWithLastUserSetpoint(settings.temperature)) {
        ESP_LOGD(LOG_SETTINGS_TAG, "Ignoring setpoint: disagrees with user (%.1f vs %.1f)",
            settings.temperature, hp_.wantedSettings.last_user_temperature);
        return false;
    }
    return true;
}

bool HeatPumpProfile::shouldIgnoreIncomingVane(const heatpumpSettings& settings) const {
    return cn105_protocol::vane_disagrees_within_grace(settings.vane, hp_.wantedSettings.last_user_vane,
        hp_.wantedSettings.last_user_vane_ms, CUSTOM_MILLIS, RECEIVED_SETPOINT_GRACE_WINDOW_MS);
}

void HeatPumpProfile::encode_control(uint8_t* packet) {
    HeatPumpProtocol::encode_control(packet, hp_.wantedSettings, hp_.currentSettings, this->use_temperature_encoding_b_,
        hp_.use_msz_a24na_setpoint_table_, this->wide_vane_adjust_,
        hp_.vane_type_ == CN105Climate::VaneType::SPLIT_HORIZONTAL);
}

void HeatPumpProfile::apply_wanted_settings() {

    if ((hp_.wantedSettings.mode != nullptr) || (hp_.wantedSettings.power != nullptr)) {
        this->reconcile_power_mode(hp_.wantedSettings, false);
        this->update_action();       // update action info on HA climate component
    }

    if (hp_.wantedSettings.fan != nullptr) {
        this->reconcile_fan(hp_.wantedSettings, false);
    }

    if (((hp_.wantedSettings.vane != nullptr) || (hp_.wantedSettings.wideVane != nullptr))) {
        if (hp_.wantedSettings.vane == nullptr) { // to prevent a nullpointer error
            hp_.wantedSettings.vane = hp_.currentSettings.vane;
        }
        if (hp_.wantedSettings.wideVane == nullptr) { // to prevent a nullpointer error
            hp_.wantedSettings.wideVane = hp_.currentSettings.wideVane;
        }

        this->checkVaneSettings(hp_.wantedSettings, false);
    }

    // HA Temp — only update if this SET includes an explicit temperature change;
    // otherwise the stale currentSettings.temperature would overwrite the UI.
    if (hp_.wantedSettings.temperature != -1.0f) {
        this->update_target_temperatures(hp_.getTemperatureSetting());
    }

}

void HeatPumpProfile::control_mode() {

    switch (hp_.mode) {
    case climate::CLIMATE_MODE_COOL:
        ESP_LOGI("control", "changing mode to COOL");
        hp_.setModeSetting("COOL");
        hp_.setPowerSetting("ON");
        break;
    case climate::CLIMATE_MODE_HEAT:
        ESP_LOGI("control", "changing mode to HEAT");
        hp_.setModeSetting("HEAT");
        hp_.setPowerSetting("ON");

        break;
    case climate::CLIMATE_MODE_DRY:
        ESP_LOGI("control", "changing mode to DRY");
        hp_.setModeSetting("DRY");
        hp_.setPowerSetting("ON");

        break;

    case climate::CLIMATE_MODE_HEAT_COOL:
        ESP_LOGI("control", "changing mode to HEAT_COOL (hardware AUTO)");
        hp_.setModeSetting("AUTO");
        hp_.setPowerSetting("ON");
        break;

    case climate::CLIMATE_MODE_AUTO:
        ESP_LOGI("control", "changing mode to AUTO");
        hp_.setModeSetting("AUTO");
        hp_.setPowerSetting("ON");

        break;
    case climate::CLIMATE_MODE_FAN_ONLY:
        ESP_LOGI("control", "changing mode to FAN_ONLY");
        hp_.setModeSetting("FAN");
        hp_.setPowerSetting("ON");
        break;
    case climate::CLIMATE_MODE_OFF:
        ESP_LOGI("control", "changing mode to OFF");
        hp_.setPowerSetting("OFF");
        break;
    default:
        ESP_LOGW("control", "unsupported mode");
    }
}

bool HeatPumpProfile::process_temperature_change(const esphome::climate::ClimateCall& call) {
    // Vérifier si une température est fournie selon les traits
    // En modes AUTO/DRY, accepter aussi target_temperature même en dual setpoint
    bool tempHasValue = (call.get_target_temperature_low().has_value() ||
        call.get_target_temperature_high().has_value() || call.get_target_temperature().has_value());
    /*
    bool tempHasValue = cn105_traits_requires_two_point(hp_.traits_) ?
        (
            call.get_target_temperature_low().has_value() ||
            call.get_target_temperature_high().has_value() ||
            ((hp_.mode == climate::CLIMATE_MODE_AUTO || hp_.mode == climate::CLIMATE_MODE_DRY) &&
                call.get_target_temperature().has_value())
            ) :
        call.get_target_temperature().has_value();
    */

    if (!tempHasValue) {
        return false;
    } else {
        ESP_LOGD("control", "A temperature setpoint value has been provided...");
    }

    float temp_low = NAN;
    float temp_high = NAN;
    float temp_single = NAN;
    if (call.get_target_temperature_low().has_value()) {
        temp_low = hp_.fahrenheitSupport_.normalizeUiTemperatureToHeatpumpTemperature(*call.get_target_temperature_low());
    }
    if (call.get_target_temperature_high().has_value()) {
        temp_high = hp_.fahrenheitSupport_.normalizeUiTemperatureToHeatpumpTemperature(*call.get_target_temperature_high());
    }
    if (call.get_target_temperature().has_value()) {
        temp_single = hp_.fahrenheitSupport_.normalizeUiTemperatureToHeatpumpTemperature(*call.get_target_temperature());
    }

    if (cn105_traits_requires_two_point(hp_.traits_)) {
        ESP_LOGD("control", "Processing with dual setpoint support...");
        if (call.get_target_temperature_low().has_value() && call.get_target_temperature_high().has_value()) {
            this->handleDualSetpointBoth(temp_low, temp_high);
        } else if (call.get_target_temperature_low().has_value()) {
            this->handleDualSetpointLowOnly(temp_low);
        } else if (call.get_target_temperature_high().has_value()) {
            this->handleDualSetpointHighOnly(temp_high);
        } else if (call.get_target_temperature().has_value() &&
            (hp_.mode == climate::CLIMATE_MODE_AUTO || hp_.mode == climate::CLIMATE_MODE_DRY)) {
            this->handleSingleTargetInAutoOrDry(temp_single);
        }
    } else {
        ESP_LOGD("control", "Processing without dual setpoint support...");
        if (call.get_target_temperature().has_value()) {
            hp_.setTargetTemperature(temp_single);
            ESP_LOGI("control", "Setting heatpump setpoint : %.1f", hp_.getTargetTemperature());
        }
    }

    this->control_temperature();
    ESP_LOGD("control", "controlled temperature to: %.1f", hp_.wantedSettings.temperature);
    return true;
}

void HeatPumpProfile::control_temperature() {
    float setting;

    // Utiliser la logique appropriée selon les traits
    switch (hp_.mode) {
    case climate::CLIMATE_MODE_HEAT_COOL:
        // Mode HEAT_COOL (new): displays 2 sliders
        // BUT sends AUTO command to Mitsubishi hardware
        // with internal deadband logic
        if (cn105_traits_requires_two_point(hp_.traits_)) {
            if ((!std::isnan(hp_.currentSettings.temperature)) && (hp_.currentSettings.temperature > 0)) {
                // Initialize if values are missing
                if (std::isnan(hp_.getTargetTemperatureLow())) {
                    hp_.setTargetTemperatureLow(hp_.currentSettings.temperature - 2.0f);
                }
                if (std::isnan(hp_.getTargetTemperatureHigh())) {
                    hp_.setTargetTemperatureHigh(hp_.currentSettings.temperature + 2.0f);
                }
                ESP_LOGI("control", "Initializing HEAT_COOL mode temps from current PAC temp: %.1f -> [%.1f - %.1f]",
                    hp_.currentSettings.temperature, hp_.getTargetTemperatureLow(), hp_.getTargetTemperatureHigh());
            }
            // In HEAT_COOL, use deadband to calculate 'setting'
            float current = hp_.getCurrentTemperature();
            if (!std::isnan(current)) {
                float low = hp_.getTargetTemperatureLow();
                float high = hp_.getTargetTemperatureHigh();
                setting = HeatPumpProtocol::deadband_setting(current, low, high);
                ESP_LOGD("control", "HEAT_COOL deadband: current=%.1f, low=%.1f, high=%.1f => setting=%.1f", current, low, high, setting);
            } else {
                // fallback
                setting = hp_.getTargetTemperature();
            }
        } else {
            setting = hp_.getTargetTemperature();
        }
        break;

    case climate::CLIMATE_MODE_AUTO:
        // Mode AUTO (legacy): keeps original behavior
        // Ignore dual setpoint here if possible, or take median
        // But for Mitsu AUTO, a single setpoint matters.
        setting = hp_.getTargetTemperature();
        // If forced to dual point by global trait, take the median
        if (cn105_traits_requires_two_point(hp_.traits_)) {
            if (!std::isnan(hp_.getTargetTemperatureLow()) && !std::isnan(hp_.getTargetTemperatureHigh())) {
                setting = (hp_.getTargetTemperatureLow() + hp_.getTargetTemperatureHigh()) / 2.0f;
            }
        }
        ESP_LOGD("control", "AUTO mode (legacy) : using target temperature: %.1f", setting);
        break;

    case climate::CLIMATE_MODE_HEAT:
        // Mode HEAT : using low target temperature
        if (cn105_traits_requires_two_point(hp_.traits_)) {
            setting = hp_.getTargetTemperatureLow();
        } else {
            setting = hp_.getTargetTemperature();
        }
        ESP_LOGD("control", "HEAT mode : getting temperature (low/target): %1.f", setting);
        break;
    case climate::CLIMATE_MODE_COOL:
        // Mode COOL : using high target temperature
        if (cn105_traits_requires_two_point(hp_.traits_)) {
            setting = hp_.getTargetTemperatureHigh();
        } else {
            setting = hp_.getTargetTemperature();
        }
        ESP_LOGD("control", "COOL mode : getting temperature (high/target): %1.f", setting);
        break;
    case climate::CLIMATE_MODE_DRY:
        // Mode DRY : using high target temperature
        if (cn105_traits_requires_two_point(hp_.traits_)) {
            setting = hp_.getTargetTemperatureHigh();
        } else {
            setting = hp_.getTargetTemperature();
        }
        ESP_LOGD("control", "DRY mode : getting temperature (high/target): %1.f", setting);
        break;
    default:
        // Other modes : use median temperature
        if (cn105_traits_requires_two_point(hp_.traits_)) {
            setting = (hp_.getTargetTemperatureLow() + hp_.getTargetTemperatureHigh()) / 2.0f;
        } else {
            setting = hp_.getTargetTemperature();
        }
        ESP_LOGD("control", "DEFAULT mode : getting temperature median:%1.f", setting);
        break;
    }

    setting = hp_.calculateTemperatureSetting(setting);
    hp_.wantedSettings.temperature = setting;
    // Track last user temperature command: this persists across resetSettings()
    // so we can apply a grace window before accepting PAC-reported setpoints.
    hp_.wantedSettings.last_user_temperature = setting;
    hp_.wantedSettings.last_user_temperature_ms = CUSTOM_MILLIS;
    ESP_LOGI("control", "setting wanted temperature to %.1f (tracked as last_user_temperature)", setting);
}

void HeatPumpProfile::control_swing() {
    // Check if horizontal vane (wideVane) is supported by this unit at the beginning.
    bool wideVaneSupported = hp_.traits_.supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);
    bool vane_is_swing = (hp_.currentSettings.vane != nullptr) && (strcmp(hp_.currentSettings.vane, "SWING") == 0);
    bool wide_is_swing = (hp_.currentSettings.wideVane != nullptr) && (strcmp(hp_.currentSettings.wideVane, "SWING") == 0);

    switch (hp_.swing_mode) {
    case climate::CLIMATE_SWING_OFF:
        // When swing is turned OFF, conditionally set vanes to a default static position.
        // This only sets default position if swing was previously enabled
        if (vane_is_swing) {
            hp_.setVaneSetting("AUTO");
        }
        if (wideVaneSupported && wide_is_swing) {
            hp_.setWideVaneSetting("|");
        }
        break;

    case climate::CLIMATE_SWING_VERTICAL:
        // Turn on vertical swing.
        hp_.setVaneSetting("SWING");
        // If horizontal swing was also on AND is supported, turn it off to a default static position.
        // This correctly handles switching from BOTH to VERTICAL, while preserving any user's
        // static horizontal setting if it wasn't swinging.
        if (wideVaneSupported && wide_is_swing) {
            hp_.setWideVaneSetting("|");
        }
        break;

    case climate::CLIMATE_SWING_HORIZONTAL:
        // If vertical swing was on, turn it off to a default static position.
        // This correctly handles switching from BOTH to HORIZONTAL, while preserving any user's
        // static vertical setting if it wasn't swinging.
        if (vane_is_swing) {
            hp_.setVaneSetting("AUTO");
        }
        // Turn on horizontal swing, but only if the unit supports it.
        if (wideVaneSupported) {
            hp_.setWideVaneSetting("SWING");
        }
        break;

    case climate::CLIMATE_SWING_BOTH:
        // Turn on vertical swing.
        hp_.setVaneSetting("SWING");
        // Turn on horizontal swing, but only if the unit supports it.
        if (wideVaneSupported) {
            hp_.setWideVaneSetting("SWING");
        }
        break;

    default:
        ESP_LOGW(TAG, "control - received unsupported swing mode request.");
        break;
    }
}

void HeatPumpProfile::update_action() {
    ESP_LOGV(TAG, "updating action back to espHome...");

    if (cn105_traits_requires_two_point(hp_.traits())) {
        this->sanitize_setpoints();
    }
    // Keep the upstream #722 override in the payload profile, including legacy fallback.
    if (const auto sub_mode_action = cn105_climate::action_from_sub_mode(hp_.mode, hp_.currentSettings.sub_mode)) {
        hp_.action = *sub_mode_action;
    } else {
        switch (hp_.mode) {
        case climate::CLIMATE_MODE_HEAT:
            //hp_.setActionIfOperatingAndCompressorIsActiveTo(climate::CLIMATE_ACTION_HEATING);
            this->setActionIfOperatingTo(climate::CLIMATE_ACTION_HEATING);
            break;
        case climate::CLIMATE_MODE_COOL:
            //hp_.setActionIfOperatingAndCompressorIsActiveTo(climate::CLIMATE_ACTION_COOLING);
            this->setActionIfOperatingTo(climate::CLIMATE_ACTION_COOLING);
            break;
        case climate::CLIMATE_MODE_HEAT_COOL:
            if (hp_.traits().supports_mode(climate::CLIMATE_MODE_HEAT) &&
                hp_.traits().supports_mode(climate::CLIMATE_MODE_COOL)) {
                // Logique Deadband pour HEAT_COOL
                if (hp_.getCurrentTemperature() >= hp_.getTargetTemperatureHigh()) {
                    this->setActionIfOperatingTo(climate::CLIMATE_ACTION_COOLING);
                } else if (hp_.getCurrentTemperature() <= hp_.getTargetTemperatureLow()) {
                    this->setActionIfOperatingTo(climate::CLIMATE_ACTION_HEATING);
                } else {
                    this->setActionIfOperatingTo(climate::CLIMATE_ACTION_IDLE);
                }
            } else {
                this->setActionIfOperatingTo(climate::CLIMATE_ACTION_FAN);
            }
            break;

        case climate::CLIMATE_MODE_AUTO:

            if (hp_.traits().supports_mode(climate::CLIMATE_MODE_HEAT) &&
                hp_.traits().supports_mode(climate::CLIMATE_MODE_COOL)) {
                // If the unit supports both heating and cooling
                if (hp_.getCurrentTemperature() >= hp_.getTargetTemperatureHigh()) {
                    this->setActionIfOperatingTo(climate::CLIMATE_ACTION_COOLING);
                } else if (hp_.getCurrentTemperature() <= hp_.getTargetTemperatureLow()) {
                    this->setActionIfOperatingTo(climate::CLIMATE_ACTION_HEATING);
                } else {
                    this->setActionIfOperatingTo(climate::CLIMATE_ACTION_IDLE);
                }
            } else if (hp_.traits().supports_mode(climate::CLIMATE_MODE_COOL)) {
                // If the unit only supports cooling
                if (hp_.getCurrentTemperature() < hp_.getTargetTemperatureHigh()) {
                    // If the temperature meets or exceeds the target, switch to fan-only mode
                    this->setActionIfOperatingTo(climate::CLIMATE_ACTION_IDLE);
                } else {
                    // Otherwise, continue cooling
                    this->setActionIfOperatingTo(climate::CLIMATE_ACTION_COOLING);
                }
            } else if (hp_.traits().supports_mode(climate::CLIMATE_MODE_HEAT)) {
                // If the unit only supports heating
                if (hp_.getCurrentTemperature() >= hp_.getTargetTemperatureLow()) {
                    // If the temperature meets or exceeds the target, switch to fan-only mode
                    this->setActionIfOperatingTo(climate::CLIMATE_ACTION_IDLE);
                } else {
                    // Otherwise, continue heating
                    this->setActionIfOperatingTo(climate::CLIMATE_ACTION_HEATING);
                }
            } else {
                ESP_LOGE(TAG, "AUTO mode is not supported by this unit");
                this->setActionIfOperatingTo(climate::CLIMATE_ACTION_FAN);
            }
            break;

        case climate::CLIMATE_MODE_DRY:
            //hp_.setActionIfOperatingAndCompressorIsActiveTo(climate::CLIMATE_ACTION_DRYING);
            this->setActionIfOperatingTo(climate::CLIMATE_ACTION_DRYING);
            break;
        case climate::CLIMATE_MODE_FAN_ONLY:
            hp_.action = climate::CLIMATE_ACTION_FAN;
            break;
        default:
            hp_.action = climate::CLIMATE_ACTION_OFF;
        }
    }

    ESP_LOGD(TAG, "Climate mode is: %i", hp_.mode);
    ESP_LOGD(TAG, "Climate action is: %i", hp_.action);
}

bool HeatPumpProfile::accepts_remote_temperature(float setting) {
    if (!HeatPumpProtocol::accepts_remote_temperature(setting)) {
        ESP_LOGW(LOG_REMOTE_TEMP, "Remote temperature is NaN, ignoring.");
        return false;
    }
    return true;
}

bool HeatPumpProfile::can_control_airflow() const {
    return HeatPumpProtocol::can_control_airflow(hp_.currentSettings);
}


void HeatPumpProfile::setActionIfOperatingTo(climate::ClimateAction action_if_operating) {
    hp_.action = HeatPumpProtocol::operating_action(hp_.currentStatus.operating,
        hp_.use_stage_for_operating_status_, hp_.currentSettings.stage, action_if_operating);
}

void HeatPumpProfile::handleDualSetpointBoth(float low, float high) {
    ESP_LOGD("control", "handleDualSetpointBoth - low: %.1f, high: %.1f", low, high);
    hp_.setTargetTemperatureLow(low);
    hp_.setTargetTemperatureHigh(high);
    this->last_dual_setpoint_side_ = 'N';
    this->last_dual_setpoint_change_ms_ = CUSTOM_MILLIS;
    hp_.currentSettings.dual_low_target = hp_.getTargetTemperatureLow();
    hp_.currentSettings.dual_high_target = hp_.getTargetTemperatureHigh();
}

void HeatPumpProfile::handleDualSetpointLowOnly(float low) {
    ESP_LOGD("control", "handleDualSetpointLowOnly - LOW: %.1f", low);
    if (this->last_dual_setpoint_side_ == 'H' && (CUSTOM_MILLIS - this->last_dual_setpoint_change_ms_) < UI_SETPOINT_ANTIREBOUND_MS) {
        ESP_LOGD("control", "IGNORED low setpoint due to UI anti-rebound after high change");
        return;
    }
    if (!std::isnan(hp_.getTargetTemperatureLow()) && fabsf(low - hp_.getTargetTemperatureLow()) < 0.05f) {
        ESP_LOGD("control", "IGNORED low setpoint: no effective change vs current low target");
        return;
    }
    hp_.setTargetTemperatureLow(low);
    if (hp_.mode == climate::CLIMATE_MODE_AUTO) {
        const float amplitude = 4.0f;
        hp_.setTargetTemperatureHigh(hp_.getTargetTemperatureLow() + amplitude);
        ESP_LOGD("control", "mode auto: sliding high to preserve amplitude %.1f => [%.1f - %.1f]", amplitude, hp_.getTargetTemperatureLow(), hp_.getTargetTemperatureHigh());
    }
    this->last_dual_setpoint_side_ = 'L';
    this->last_dual_setpoint_change_ms_ = CUSTOM_MILLIS;
    hp_.currentSettings.dual_low_target = hp_.getTargetTemperatureLow();
    hp_.currentSettings.dual_high_target = hp_.getTargetTemperatureHigh();
}

void HeatPumpProfile::handleDualSetpointHighOnly(float high) {
    ESP_LOGI("control", "HIGH: handleDualSetpointHighOnly - HIGH : %.1f", high);
    if (this->last_dual_setpoint_side_ == 'L' && (CUSTOM_MILLIS - this->last_dual_setpoint_change_ms_) < UI_SETPOINT_ANTIREBOUND_MS) {
        ESP_LOGD("control", "ignored high setpoint due to UI anti-rebound after low change");
        return;
    }
    if (!std::isnan(hp_.getTargetTemperatureHigh()) && fabsf(high - hp_.getTargetTemperatureHigh()) < 0.05f) {
        ESP_LOGD("control", "ignored high setpoint: no effective change vs current high target");
        return;
    }
    hp_.setTargetTemperatureHigh(high);
    if (hp_.mode == climate::CLIMATE_MODE_AUTO) {
        const float amplitude = 4.0f;
        hp_.setTargetTemperatureLow(hp_.getTargetTemperatureHigh() - amplitude);
        ESP_LOGD("control", "mode auto: sliding low to preserve amplitude %.1f => [%.1f - %.1f]", amplitude, hp_.getTargetTemperatureLow(), hp_.getTargetTemperatureHigh());
    }
    this->last_dual_setpoint_side_ = 'H';
    this->last_dual_setpoint_change_ms_ = CUSTOM_MILLIS;
    hp_.currentSettings.dual_low_target = hp_.getTargetTemperatureLow();
    hp_.currentSettings.dual_high_target = hp_.getTargetTemperatureHigh();
}

void HeatPumpProfile::handleSingleTargetInAutoOrDry(float requested) {
    ESP_LOGD("control", "handleSingleTargetInAutoOrDry - SINGLE: %.1f", requested);
    if (hp_.mode == climate::CLIMATE_MODE_AUTO) {
        const float half_span = 2.0f;
        hp_.setTargetTemperatureLow(requested - half_span);
        hp_.setTargetTemperatureHigh(requested + half_span);
        this->last_dual_setpoint_side_ = 'N';
        this->last_dual_setpoint_change_ms_ = CUSTOM_MILLIS;
        hp_.currentSettings.dual_low_target = hp_.getTargetTemperatureLow();
        hp_.currentSettings.dual_high_target = hp_.getTargetTemperatureHigh();
        hp_.setTargetTemperature(requested);
        ESP_LOGD("control", "AUTO received single target: median=%.1f => [%.1f - %.1f]", requested, hp_.getTargetTemperatureLow(), hp_.getTargetTemperatureHigh());
    }
    if (hp_.mode == climate::CLIMATE_MODE_DRY) {
        hp_.setTargetTemperatureHigh(requested);
        if (std::isnan(hp_.getTargetTemperatureLow())) {
            hp_.setTargetTemperatureLow(requested);
        }
        this->last_dual_setpoint_side_ = 'H';
        this->last_dual_setpoint_change_ms_ = CUSTOM_MILLIS;
        hp_.currentSettings.dual_low_target = hp_.getTargetTemperatureLow();
        hp_.currentSettings.dual_high_target = hp_.getTargetTemperatureHigh();
        hp_.setTargetTemperature(requested);
        ESP_LOGD("control", "DRY received single target: high=%.1f (low now %.1f)", hp_.getTargetTemperatureHigh(), hp_.getTargetTemperatureLow());
    }
}

void HeatPumpProfile::restore_setpoints() {
    if (!hp_.restore_setpoints_) {
        return;
    }
    this->setpoint_pref_ = global_preferences->make_preference<SetpointState>(
        hp_.get_object_id_hash() ^ 0x53504E54UL);  // XOR 'SPNT'
    this->setpoint_pref_ready_ = true;

    SetpointState s{};
    const bool loaded = this->setpoint_pref_.load(&s);
    const auto restored = HeatPumpProtocol::restored_setpoint_state(hp_.restore_setpoints_, loaded, s, hp_.supports_dual_setpoint_);
    if (!restored) {
        if (!loaded) ESP_LOGI("restore", "restore_setpoints: no saved HEAT_COOL band in flash");
        else if (!HeatPumpProtocol::restored_setpoint_state(true, true, s, true))
            ESP_LOGD("restore", "restore_setpoints: saved state is not a restorable HEAT_COOL band (mode=%u)", s.mode);
        else ESP_LOGW("restore", "restore_setpoints: saved HEAT_COOL band found but dual_setpoint not enabled; ignoring");
        return;
    }
    // Preserve the restored HEAT_COOL band when hardware reports HEAT, COOL or AUTO.
    // OFF and the other supported mode transitions are still reconciled normally.
    hp_.mode = climate::CLIMATE_MODE_HEAT_COOL;
    hp_.setTargetTemperatureLow(s.target_low);
    hp_.setTargetTemperatureHigh(s.target_high);
    ESP_LOGI("restore", "restore_setpoints: restored HEAT_COOL band low=%.1f high=%.1f", s.target_low, s.target_high);
}

void HeatPumpProfile::save_setpoints() {
    if (!hp_.restore_setpoints_ || !this->setpoint_pref_ready_) {
        return;
    }
    const auto s = HeatPumpProtocol::saved_setpoint_state(hp_.mode, hp_.getTargetTemperatureLow(), hp_.getTargetTemperatureHigh());
    if (this->setpoint_pref_.save(&s)) {
        ESP_LOGD("restore", "restore_setpoints: saved mode=%u low=%.1f high=%.1f", s.mode, s.target_low, s.target_high);
    }
}

void HeatPumpProfile::sanitize_setpoints() {
    if (!cn105_traits_requires_two_point(hp_.traits_)) {
        return;
    }
    ESP_LOGD(LOG_DUAL_SP_TAG, "sanitizing dual setpoints...");
    // Si une borne est NaN, la reconstruire à partir de l'autre borne ou d'une valeur raisonnable
    bool lowIsNaN = std::isnan(hp_.getTargetTemperatureLow());
    bool highIsNaN = std::isnan(hp_.getTargetTemperatureHigh());

    if (lowIsNaN && highIsNaN) {
        // Rien à faire si on n'a aucune info; essayer currentSettings.temperature si valide
        if (!std::isnan(hp_.currentSettings.temperature) && hp_.currentSettings.temperature > 0) {
            hp_.setTargetTemperatureLow(hp_.currentSettings.temperature - 2.0f);
            hp_.setTargetTemperatureHigh(hp_.currentSettings.temperature + 2.0f);
        } else {
            ESP_LOGD(LOG_DUAL_SP_TAG, "No known temperature, using default values 18.0f - 22.0f");
            hp_.setTargetTemperatureLow(18.0f);
            hp_.setTargetTemperatureHigh(22.0f);
        }

        ESP_LOGD(LOG_DUAL_SP_TAG, "AUTO sanitized dual setpoints [%.1f - %.1f]",
            hp_.getTargetTemperatureLow(), hp_.getTargetTemperatureHigh());

        return;
    }

    if (lowIsNaN && !highIsNaN) {
        // Reconstruire low à partir de high
        hp_.setTargetTemperatureLow((hp_.mode == climate::CLIMATE_MODE_AUTO)
            ? (hp_.getTargetTemperatureHigh() - 4.0f)
            : hp_.getTargetTemperatureHigh()); // en HEAT/COOL, une seule consigne peut suffire
    } else if (!lowIsNaN && highIsNaN) {
        // Reconstruire high à partir de low
        hp_.setTargetTemperatureHigh((hp_.mode == climate::CLIMATE_MODE_AUTO)
            ? (hp_.getTargetTemperatureLow() + 4.0f)
            : hp_.getTargetTemperatureLow());
    }

    ESP_LOGD(LOG_DUAL_SP_TAG, "AUTO sanitized dual setpoints [%.1f - %.1f]",
        hp_.getTargetTemperatureLow(), hp_.getTargetTemperatureHigh());
}

void HeatPumpProfile::encode_remote_temperature(uint8_t* packet) {
    HeatPumpProtocol::encode_remote_temperature(packet, hp_.remoteTemperature_);
}

void HeatPumpProfile::encode_run_states(uint8_t* packet) {
    HeatPumpProtocol::encode_run_states(packet, hp_.wantedRunStates, hp_.currentRunStates);
}


void HeatPumpProfile::control_fan() {

    switch (hp_.fan_mode.value()) {
    case climate::CLIMATE_FAN_OFF:
        hp_.setPowerSetting("OFF");
        break;
    case climate::CLIMATE_FAN_QUIET:
        hp_.setFanSpeed("QUIET");
        break;
    case climate::CLIMATE_FAN_DIFFUSE:
        hp_.setFanSpeed("QUIET");
        break;
    case climate::CLIMATE_FAN_LOW:
        hp_.setFanSpeed("1");
        break;
    case climate::CLIMATE_FAN_MEDIUM:
        hp_.setFanSpeed("2");
        break;
    case climate::CLIMATE_FAN_MIDDLE:
        hp_.setFanSpeed("3");
        break;
    case climate::CLIMATE_FAN_HIGH:
        hp_.setFanSpeed("4");
        break;
    case climate::CLIMATE_FAN_ON:
    case climate::CLIMATE_FAN_AUTO:
    default:
        hp_.setFanSpeed("AUTO");
        break;
    }
}

void HeatPumpProfile::reconcile_fan(heatpumpSettings& settings, bool updateCurrentSettings) {
    /*
         * ******* HANDLE FAN CHANGES ********
         *
         * const char* FAN_MAP[6]         = {"AUTO", "QUIET", "1", "2", "3", "4"};
         */
         // hp_.currentSettings.fan== NULL is true when it is the first time we get en answer from hp

    if (hp_.hasChanged(hp_.currentSettings.fan, settings.fan, "fan")) { // fan setting change ?
        ESP_LOGI(TAG, "fan setting changed");
        if (updateCurrentSettings) {
            hp_.currentSettings.fan = settings.fan;
        }

        if (strcmp(settings.fan, "QUIET") == 0) {
            hp_.fan_mode = climate::CLIMATE_FAN_QUIET;
        } else if (strcmp(settings.fan, "1") == 0) {
            hp_.fan_mode = climate::CLIMATE_FAN_LOW;
        } else if (strcmp(settings.fan, "2") == 0) {
            hp_.fan_mode = climate::CLIMATE_FAN_MEDIUM;
        } else if (strcmp(settings.fan, "3") == 0) {
            hp_.fan_mode = climate::CLIMATE_FAN_MIDDLE;
        } else if (strcmp(settings.fan, "4") == 0) {
            hp_.fan_mode = climate::CLIMATE_FAN_HIGH;
        } else { //case "AUTO" or default:
            hp_.fan_mode = climate::CLIMATE_FAN_AUTO;
        }
        if (hp_.fan_mode.has_value()) {
            ESP_LOGD(TAG, "Fan mode is: %i", static_cast<int>(hp_.fan_mode.value()));
        } else {
            ESP_LOGD(TAG, "Fan mode is not set");
        }
    }
}

void HeatPumpProfile::update_target_temperatures(float temperature) {
    const bool two_point = cn105_traits_requires_two_point(hp_.traits());
    HeatPumpProtocol::reconcile_target_temperatures(hp_, hp_.mode, two_point, temperature);
    if (two_point && (hp_.mode == climate::CLIMATE_MODE_AUTO || hp_.mode == climate::CLIMATE_MODE_HEAT_COOL)) {
        hp_.currentSettings.dual_low_target = hp_.getTargetTemperatureLow();
        hp_.currentSettings.dual_high_target = hp_.getTargetTemperatureHigh();
    }
    ESP_LOGD(LOG_SETTINGS_TAG, "Reconciled target %.1f with mode %u", temperature, static_cast<unsigned>(hp_.mode));
}

void HeatPumpProfile::apply_functions() {

    // Called after 2nd packet has arrived.

    char states[256];
    states[0] = '\0';  // Initialize as empty string
    size_t remaining = sizeof(states);
    char* pos = states;

    heatpumpFunctionCodes codes = hp_.functions.getAllCodes();
    for (int i = 0; i < MAX_FUNCTION_CODE_COUNT; ++i) {
        if (codes.valid[i]) {
            int code = codes.code[i];
            int value = hp_.functions.getValue(code);
            if (value > 0) {  // only values 1, 2, 3 are valid -- 0 values mean something the device does not support
                int written = snprintf(pos, remaining, "%i: %i ", code, value);
                if (written < 0 || static_cast<size_t>(written) >= remaining) {
                    // Buffer full or error
                    break;
                }
                pos += written;
                remaining -= written;
            }
        }
    }

    // Publish the results of all the codes in the Functions sensor
    if (hp_.Functions_sensor_ != nullptr) {
        hp_.Functions_sensor_->publish_state(states);
    }

    // Update Hardware Settings Selects
    for (auto* setting : hp_.hardware_settings_) {
        int val = hp_.functions.getValue(setting->get_code());
        if (val > 0) {
            setting->update_state_from_value(val);
        } else {
            ESP_LOGD(LOG_HARDWARE_SELECT_TAG, "Code %d received unknown value: %d", setting->get_code(), val);
        }
    }
}

bool HeatPumpProfile::encode_functions(uint8_t* first, uint8_t* second, const heatpumpFunctions& functions) {
    return HeatPumpProtocol::encode_functions(first, second, functions);
}

float HeatPumpProfile::calculate_temperature_setting(float setting) const {
    return HeatPumpProtocol::calculate_temperature_setting(setting, this->use_temperature_encoding_b_,
        hp_.use_msz_a24na_setpoint_table_);
}

void HeatPumpProfile::set_wide_vane_setting(const char* setting) {
    int index = hp_.lookupByteMapIndex(WIDEVANE_MAP, 8, setting);
    if (index > -1) {
        hp_.wantedSettings.wideVane = WIDEVANE_MAP[index];
    } else {
        hp_.wantedSettings.wideVane = WIDEVANE_MAP[0];
    }
}

void HeatPumpProfile::set_airflow_setting(const char* setting) {
    int index = hp_.lookupByteMapIndex(AIRFLOW_CONTROL_MAP, 3, setting);
    if (index > -1) {
        hp_.wantedRunStates.airflow_control = AIRFLOW_CONTROL_MAP[index];
    } else {
        hp_.wantedRunStates.airflow_control = AIRFLOW_CONTROL_MAP[0];
    }
}

void HeatPumpProfile::set_vane_setting(const char* setting) {
    HeatPumpProtocol::set_vane_setting(hp_.wantedSettings, setting, CUSTOM_MILLIS);
    if (hp_.wantedSettings.last_user_vane != nullptr) {
        ESP_LOGD("control", "Tracked last_user_vane: %s", hp_.wantedSettings.last_user_vane);
    }
}

const char* HeatPumpProfile::wide_vane_setting() {
    return HeatPumpProtocol::wide_vane_setting(hp_.wantedSettings, hp_.currentSettings);
}


void HeatPumpProfile::apply_wanted_run_states() {
    const auto states = HeatPumpProtocol::optimistic_run_states(hp_.wantedRunStates, hp_.currentRunStates);
    if (hp_.airflow_control_select_ != nullptr && hp_.wantedRunStates.airflow_control != nullptr) {
        if (hp_.hasChanged(hp_.airflow_control_select_->current_option(), states.airflow_control, "select airflow control")) {
            ESP_LOGI(TAG, "airflow control setting changed");
            hp_.airflow_control_select_->publish_state(states.airflow_control);
        }
    }
    if (hp_.air_purifier_switch_ != nullptr && hp_.wantedRunStates.air_purifier > -1) {
        if (hp_.air_purifier_switch_->state != states.air_purifier) {
            ESP_LOGI(TAG, "air purifier setting changed");
            hp_.air_purifier_switch_->publish_state(states.air_purifier);
        }
    }
    if (hp_.night_mode_switch_ != nullptr && hp_.wantedRunStates.night_mode > -1) {
        if (hp_.night_mode_switch_->state != states.night_mode) {
            ESP_LOGI(TAG, "night mode setting changed");
            hp_.night_mode_switch_->publish_state(states.night_mode);
        }
    }
    if (hp_.circulator_switch_ != nullptr && hp_.wantedRunStates.circulator > -1) {
        if (hp_.circulator_switch_->state != states.circulator) {
            ESP_LOGI(TAG, "circulator setting changed");
            hp_.circulator_switch_->publish_state(states.circulator);
        }
    }
}

void HeatPumpProfile::debug_functions(const uint8_t* packet, unsigned int length) {
    if (length < 2) return; // Pas de données à décoder

    std::string output;
    output.reserve(length * 8); // Pré-allocation pour éviter les réallocations

    char buffer[16];

    // On commence à i=1 pour sauter l'octet de commande (0x20 ou 0x22)
    for (unsigned int i = 1; i < length; i++) {
        uint8_t byte = packet[i];

        // Logique de décodage Mitsubishi (copiée de heatpumpFunctions)
        int code = ((byte >> 2) & 0xff) + 100;
        int value = byte & 3;

        // Formatage "Code:Valeur" (ex: " 102:3")
        snprintf(buffer, sizeof(buffer), " %d:%d", code, value);
        output += buffer;
    }

    // Affichage avec le tag LOG_FUNCTIONS_TAG (défini dans cn105_types.h)
    // Affiche par exemple : [FUNCTIONS] Decoded 20: 101:1 102:3 103:2 ...
    ESP_LOGD(LOG_FUNCTIONS_TAG, "Decoded %02X:%s", packet[0], output.c_str());
}

bool HeatPumpProfile::decode_functions(uint8_t code, const uint8_t* data, size_t length) {
    this->debug_functions(data, length);
    return HeatPumpProtocol::decode_functions(hp_.functions, code, data, length);
}
