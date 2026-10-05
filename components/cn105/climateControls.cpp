#include "cn105.h"
#include "Globals.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>
#include <utility>

using namespace esphome;


void CN105Climate::checkPendingWantedSettings() {
    // Already in-flight — don't log or re-send
    if (this->wantedSettings.hasBeenSent) {
        return;
    }

    long now = CUSTOM_MILLIS;
    if (!(this->wantedSettings.hasChanged) || (now - this->wantedSettings.lastChange < this->debounce_delay_)) {
        return;
    }

    // Don't log if sendWantedSettings() will defer due to write throttle (300ms)
    if (now - this->lastSend <= 300) {
        return;
    }

    ESP_LOGI(LOG_ACTION_EVT_TAG, "checkPendingWantedSettings - wanted settings have changed, sending them to the heatpump...");
    this->sendWantedSettings();
}

void CN105Climate::checkPendingWantedRunStates() {
    // Already in-flight — don't log or re-send
    if (this->wantedRunStates.hasBeenSent) {
        return;
    }

    long now = CUSTOM_MILLIS;
    if (!(this->wantedRunStates.hasChanged) || (now - this->wantedRunStates.lastChange < this->debounce_delay_)) {
        return;
    }

    // Don't log if sendWantedRunStates() will defer due to write throttle (300ms)
    if (now - this->lastSend <= 300) {
        return;
    }

    ESP_LOGI(LOG_ACTION_EVT_TAG, "checkPendingWantedRunStates - wanted run states have changed, sending them to the heatpump...");
    this->sendWantedRunStates();
}

void logCheckWantedSettingsMutex(wantedHeatpumpSettings& settings) {

    if (settings.hasBeenSent) {
        ESP_LOGE("control", "Mutex lock faillure: wantedSettings should be locked while sending.");
        ESP_LOGD("control", "-- This is an assertion test on wantedSettings.hasBeenSent");
        ESP_LOGD("control", "-- wantedSettings.hasBeenSent = true is unexpected");
        ESP_LOGD("control", "-- should be false because mutex should prevent running this while sending");
        ESP_LOGD("control", "-- and mutex should be released only when hasBeenSent is false");
    }

}
void CN105Climate::controlDelegate(const esphome::climate::ClimateCall& call) {
    ESP_LOGD("control", "espHome control() interface method called...");
    bool updated = false;

    logCheckWantedSettingsMutex(this->wantedSettings);

    updated = this->processModeChange(call) || updated;
    updated = this->processTemperatureChange(call) || updated;
    updated = this->processFanChange(call) || updated;
    updated = this->processSwingChange(call) || updated;

    this->finalizeControlIfUpdated(updated);

    // Persist the (possibly new) mode + band so it can be restored after a reboot.
    if (updated) {
        this->save_setpoint_state_();
    }
}

bool CN105Climate::processModeChange(const esphome::climate::ClimateCall& call) {
    if (!call.get_mode().has_value()) {
        return false;
    }

    ESP_LOGD("control", "Mode change asked");
    this->mode = *call.get_mode();
    this->controlMode();
    // Do NOT call controlTemperature() here unconditionally.
    // A mode-only change (e.g. switching to DRY) should not emit a SET temperature
    // packet. If the call also includes a temperature change, processTemperatureChange()
    // will handle it. This prevents overwriting the user's last setpoint on mode changes.
    return true;
}

bool CN105Climate::processTemperatureChange(const esphome::climate::ClimateCall& call) {
    return this->profile_->process_temperature_change(call);
}

bool CN105Climate::processFanChange(const esphome::climate::ClimateCall& call) {
    if (!call.get_fan_mode().has_value()) {
        return false;
    }
    ESP_LOGD("control", "Fan change asked");
    this->fan_mode = *call.get_fan_mode();
    this->controlFan();
    return true;
}

bool CN105Climate::processSwingChange(const esphome::climate::ClimateCall& call) {
    if (!call.get_swing_mode().has_value()) {
        return false;
    }
    if (!this->profile_->allow_operation(cn105::ProfileFeature::SWING)) {
        return false;
    }
    ESP_LOGD("control", "Swing change asked");
    this->swing_mode = *call.get_swing_mode();
    this->controlSwing();
    return true;
}

void CN105Climate::finalizeControlIfUpdated(bool updated) {
    if (!updated) {
        return;
    }
    ESP_LOGD(LOG_ACTION_EVT_TAG, "clim.control() -> User changed something...");
    logCheckWantedSettingsMutex(this->wantedSettings);
    this->wantedSettings.hasChanged = true;
    this->wantedSettings.hasBeenSent = false;
    this->wantedSettings.lastChange = CUSTOM_MILLIS;
    this->debugSettings("control (wantedSettings)", this->wantedSettings);
    this->publish_state();
}

void CN105Climate::control(const esphome::climate::ClimateCall& call) {

#ifdef USE_ESP32
    std::lock_guard<std::mutex> guard(wantedSettingsMutex);
    this->controlDelegate(call);
#else    
    this->emulateMutex("CONTROL_WANTED_SETTINGS", std::bind(&CN105Climate::controlDelegate, this, call));
#endif    

}


/**
 * @brief Controls the swing modes based on user selection.
 *
 * This function handles the logic for CLIMATE_SWING_OFF, VERTICAL, HORIZONTAL, and BOTH.
 * It is designed to be safe for units that do not support horizontal swing (wideVane)
 * and provides an intuitive user experience by preserving static vane settings when possible.
 */
void CN105Climate::controlSwing() {
    this->profile_->control_swing();
}
void CN105Climate::controlFan() {
    this->profile_->control_fan();
}


void CN105Climate::controlTemperature() {
    this->profile_->control_temperature();
}

void CN105Climate::controlMode() {
    this->profile_->control_mode();
}

/**
 * Thanks to Bascht74 on issu #9 we know that the compressor frequency is not a good indicator of the heatpump being in operation
 * Because one can have multiple inside module for a single compressor.
 * Also, some heatpump does not support compressor frequency.
 * SO usage is deprecated.
*/


//inside the below we could implement an internal only HEAT_COOL doing the math with an offset or something
void CN105Climate::updateAction() {
    this->profile_->update_action();
}

climate::ClimateTraits CN105Climate::traits() {
    //ESP_LOGD(LOG_SETTINGS_TAG, "traits() called (dual: %d)", cn105_traits_requires_two_point(traits_));
    return traits_;
}


/**
 * Modify our supported traits.
 *
 * Returns:
 *   A reference to this class' supported climate::ClimateTraits.
 */
climate::ClimateTraits& CN105Climate::config_traits() {
    return traits_;
}


void CN105Climate::setModeSetting(const char* setting) {
    const char* value = this->profile_->mode_setting(setting);
    if (value != nullptr) this->wantedSettings.mode = value;
}

void CN105Climate::setPowerSetting(const char* setting) {
    int index = lookupByteMapIndex(POWER_MAP, 2, setting);
    if (index > -1) {
        wantedSettings.power = POWER_MAP[index];
    } else {
        wantedSettings.power = POWER_MAP[0];
    }
}

void CN105Climate::setFanSpeed(const char* setting) {
    const char* value = this->profile_->fan_setting(setting);
    if (value != nullptr) this->wantedSettings.fan = value;
}

void CN105Climate::setVaneSetting(const char* setting) {
    if (this->profile_->allow_operation(cn105::ProfileFeature::SWING)) {
        this->profile_->set_vane_setting(setting);
    }
}

void CN105Climate::setWideVaneSetting(const char* setting) {
    if (this->profile_->allow_operation(cn105::ProfileFeature::SWING)) {
        this->profile_->set_wide_vane_setting(setting);
    }
}

void CN105Climate::setAirflowControlSetting(const char* setting) {
    if (this->profile_->allow_operation(cn105::ProfileFeature::AUXILIARY_CONTROLS)) {
        this->profile_->set_airflow_setting(setting);
    }
}

void CN105Climate::set_remote_temperature(float setting) {
    if (!this->profile_->accepts_remote_temperature(setting)) return;
    // Each accepted sample refreshes the watchdog, even if its value is unchanged (#474).
    this->remoteTemperature_ = setting;
    this->shouldSendExternalTemperature_ = true;
    ESP_LOGD(LOG_REMOTE_TEMP, "setting remote temperature to %f", setting);
    this->pingExternalTemperature();
    if (setting > 0) this->startRemoteTempKeepAlive();
    else this->stopRemoteTempKeepAlive();
}

// --- HEAT_COOL band persistence (opt-in: supports.restore_setpoints) ----------------
// The dual-setpoint band is synthetic (a Mitsubishi unit stores only a single setpoint),
// so it is lost on reboot and the entity returns as hardware AUTO. When enabled, we keep
// {mode, low, high} in flash and re-seed it in setup() before the first settings read.

void CN105Climate::restore_setpoint_state_() {
    this->profile_->restore_setpoints();
}

void CN105Climate::save_setpoint_state_() {
    this->profile_->save_setpoints();
}
