#include "cn105.h"
#include "Globals.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <vector>
#include <utility>

using namespace esphome;


void CN105Climate::checkPendingWantedSettings() {
    long now = CUSTOM_MILLIS;
    if (!(this->wantedSettings.hasChanged) || (now - this->wantedSettings.lastChange < this->debounce_delay_)) {
        return;
    }

    ESP_LOGI(LOG_ACTION_EVT_TAG, "checkPendingWantedSettings - wanted settings have changed, sending them to the heatpump...");
    this->sendWantedSettings();
}

void CN105Climate::checkPendingWantedRunStates() {
    long now = CUSTOM_MILLIS;
    if (!(this->wantedRunStates.hasChanged) || (now - this->wantedRunStates.lastChange < this->debounce_delay_)) {
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
}

bool CN105Climate::processModeChange(const esphome::climate::ClimateCall& call) {
    if (!call.get_mode().has_value()) {
        return false;
    }

    ESP_LOGD("control", "Mode change asked");
    this->mode = *call.get_mode();
    this->controlMode();
    this->controlTemperature();
    return true;
}

void CN105Climate::handleDualSetpointBoth(float low, float high) {
    this->target_temperature_low = low;
    this->target_temperature_high = high;
    ESP_LOGD("control", "received both target_temperature_low and target_temperature_high: %.1f - %.1f", this->target_temperature_low, this->target_temperature_high);
    this->last_dual_setpoint_side_ = 'N';
    this->last_dual_setpoint_change_ms_ = CUSTOM_MILLIS;
    this->currentSettings.dual_low_target = this->target_temperature_low;
    this->currentSettings.dual_high_target = this->target_temperature_high;
}

void CN105Climate::handleDualSetpointLowOnly(float low) {
    ESP_LOGI("control", "received only target_temperature_low: %.1f", low);
    if (this->last_dual_setpoint_side_ == 'H' && (CUSTOM_MILLIS - this->last_dual_setpoint_change_ms_) < UI_SETPOINT_ANTIREBOUND_MS) {
        ESP_LOGD("control", "ignored low setpoint due to UI anti-rebound after high change");
        return;
    }
    if (!std::isnan(this->target_temperature_low) && fabsf(low - this->target_temperature_low) < 0.05f) {
        ESP_LOGD("control", "ignored low setpoint: no effective change vs current low target");
        return;
    }
    this->target_temperature_low = low;
    if (this->mode == climate::CLIMATE_MODE_AUTO) {
        const float amplitude = 4.0f;
        this->target_temperature_high = this->target_temperature_low + amplitude;
        ESP_LOGD("control", "mode auto: sliding high to preserve amplitude %.1f => [%.1f - %.1f]", amplitude, this->target_temperature_low, this->target_temperature_high);
    }
    this->last_dual_setpoint_side_ = 'L';
    this->last_dual_setpoint_change_ms_ = CUSTOM_MILLIS;
    this->currentSettings.dual_low_target = this->target_temperature_low;
    this->currentSettings.dual_high_target = this->target_temperature_high;
}

void CN105Climate::handleDualSetpointHighOnly(float high) {
    ESP_LOGI("control", "received only target_temperature_high: %.1f", high);
    if (this->last_dual_setpoint_side_ == 'L' && (CUSTOM_MILLIS - this->last_dual_setpoint_change_ms_) < UI_SETPOINT_ANTIREBOUND_MS) {
        ESP_LOGD("control", "ignored high setpoint due to UI anti-rebound after low change");
        return;
    }
    if (!std::isnan(this->target_temperature_high) && fabsf(high - this->target_temperature_high) < 0.05f) {
        ESP_LOGD("control", "ignored high setpoint: no effective change vs current high target");
        return;
    }
    this->target_temperature_high = high;
    if (this->mode == climate::CLIMATE_MODE_AUTO) {
        const float amplitude = 4.0f;
        this->target_temperature_low = this->target_temperature_high - amplitude;
        ESP_LOGD("control", "mode auto: sliding low to preserve amplitude %.1f => [%.1f - %.1f]", amplitude, this->target_temperature_low, this->target_temperature_high);
    }
    this->last_dual_setpoint_side_ = 'H';
    this->last_dual_setpoint_change_ms_ = CUSTOM_MILLIS;
    this->currentSettings.dual_low_target = this->target_temperature_low;
    this->currentSettings.dual_high_target = this->target_temperature_high;
}

void CN105Climate::handleSingleTargetInAutoOrDry(float requested) {
    if (this->mode == climate::CLIMATE_MODE_AUTO) {
        const float half_span = 2.0f;
        this->target_temperature_low = requested - half_span;
        this->target_temperature_high = requested + half_span;
        this->last_dual_setpoint_side_ = 'N';
        this->last_dual_setpoint_change_ms_ = CUSTOM_MILLIS;
        this->currentSettings.dual_low_target = this->target_temperature_low;
        this->currentSettings.dual_high_target = this->target_temperature_high;
        this->target_temperature = requested;
        ESP_LOGD("control", "AUTO received single target: median=%.1f => [%.1f - %.1f]", requested, this->target_temperature_low, this->target_temperature_high);
    } else {
        this->target_temperature_high = requested;
        if (std::isnan(this->target_temperature_low)) {
            this->target_temperature_low = requested;
        }
        this->last_dual_setpoint_side_ = 'H';
        this->last_dual_setpoint_change_ms_ = CUSTOM_MILLIS;
        this->currentSettings.dual_low_target = this->target_temperature_low;
        this->currentSettings.dual_high_target = this->target_temperature_high;
        this->target_temperature = requested;
        ESP_LOGD("control", "DRY received single target: high=%.1f (low now %.1f)", this->target_temperature_high, this->target_temperature_low);
    }
}

bool CN105Climate::processTemperatureChange(const esphome::climate::ClimateCall& call) {
    // Vérifier si une température est fournie selon les traits
    // En modes AUTO/DRY, accepter aussi target_temperature même en dual setpoint
    bool tempHasValue = this->traits_.get_supports_two_point_target_temperature() ?
        (
            call.get_target_temperature_low().has_value() ||
            call.get_target_temperature_high().has_value() ||
            ((this->mode == climate::CLIMATE_MODE_AUTO || this->mode == climate::CLIMATE_MODE_DRY) &&
                call.get_target_temperature().has_value())
            ) :
        call.get_target_temperature().has_value();

    if (!tempHasValue) {
        return false;
    }

    if (this->traits_.get_supports_two_point_target_temperature()) {
        if (call.get_target_temperature_low().has_value() && call.get_target_temperature_high().has_value()) {
            this->handleDualSetpointBoth(*call.get_target_temperature_low(), *call.get_target_temperature_high());
        } else if (call.get_target_temperature_low().has_value()) {
            this->handleDualSetpointLowOnly(*call.get_target_temperature_low());
        } else if (call.get_target_temperature_high().has_value()) {
            this->handleDualSetpointHighOnly(*call.get_target_temperature_high());
        } else if (call.get_target_temperature().has_value() &&
            (this->mode == climate::CLIMATE_MODE_AUTO || this->mode == climate::CLIMATE_MODE_DRY)) {
            this->handleSingleTargetInAutoOrDry(*call.get_target_temperature());
        }
        ESP_LOGI("control", "Setting heatpump low temp : %.1f - high temp : %.1f", this->target_temperature_low, this->target_temperature_high);
    } else {
        if (call.get_target_temperature().has_value()) {
            this->target_temperature = *call.get_target_temperature();
            ESP_LOGI("control", "Setting heatpump setpoint : %.1f", this->target_temperature);
        }
    }


    this->controlTemperature();
    ESP_LOGD("control", "controlled temperature to: %.1f", this->wantedSettings.temperature);
    return true;
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
    // Check if horizontal vane (wideVane) is supported by this unit at the beginning.
    bool wideVaneSupported = this->traits_.supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL);

    switch (this->swing_mode) {
    case climate::CLIMATE_SWING_OFF:
        // When swing is turned OFF, conditionally set vanes to a default static position.
        // This only sets default position if swing was previously enabled
        if (strcmp(currentSettings.vane, "SWING") == 0) {
            this->setVaneSetting("AUTO");
        }
        if (wideVaneSupported && strcmp(currentSettings.wideVane, "SWING") == 0) {
            this->setWideVaneSetting("|");
        }
        break;

    case climate::CLIMATE_SWING_VERTICAL:
        // Turn on vertical swing.
        this->setVaneSetting("SWING");
        // If horizontal swing was also on AND is supported, turn it off to a default static position.
        // This correctly handles switching from BOTH to VERTICAL, while preserving any user's
        // static horizontal setting if it wasn't swinging.
        if (wideVaneSupported && strcmp(currentSettings.wideVane, "SWING") == 0) {
            this->setWideVaneSetting("|");
        }
        break;

    case climate::CLIMATE_SWING_HORIZONTAL:
        // If vertical swing was on, turn it off to a default static position.
        // This correctly handles switching from BOTH to HORIZONTAL, while preserving any user's
        // static vertical setting if it wasn't swinging.
        if (strcmp(currentSettings.vane, "SWING") == 0) {
            this->setVaneSetting("AUTO");
        }
        // Turn on horizontal swing, but only if the unit supports it.
        if (wideVaneSupported) {
            this->setWideVaneSetting("SWING");
        }
        break;

    case climate::CLIMATE_SWING_BOTH:
        // Turn on vertical swing.
        this->setVaneSetting("SWING");
        // Turn on horizontal swing, but only if the unit supports it.
        if (wideVaneSupported) {
            this->setWideVaneSetting("SWING");
        }
        break;

    default:
        ESP_LOGW(TAG, "control - received unsupported swing mode request.");
        break;
    }
}
void CN105Climate::controlFan() {

    switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_OFF:
        this->setPowerSetting("OFF");
        break;
    case climate::CLIMATE_FAN_QUIET:
        this->setFanSpeed("QUIET");
        break;
    case climate::CLIMATE_FAN_DIFFUSE:
        this->setFanSpeed("QUIET");
        break;
    case climate::CLIMATE_FAN_LOW:
        this->setFanSpeed("1");
        break;
    case climate::CLIMATE_FAN_MEDIUM:
        this->setFanSpeed("2");
        break;
    case climate::CLIMATE_FAN_MIDDLE:
        this->setFanSpeed("3");
        break;
    case climate::CLIMATE_FAN_HIGH:
        this->setFanSpeed("4");
        break;
    case climate::CLIMATE_FAN_ON:
    case climate::CLIMATE_FAN_AUTO:
    default:
        this->setFanSpeed("AUTO");
        break;
    }
}


void CN105Climate::controlTemperature() {
    float setting;




    // Utiliser la logique appropriée selon les traits
    if (this->traits_.get_supports_two_point_target_temperature()) {
        //this->sanitizeDualSetpoints();
        // Dual setpoint : choisir la bonne consigne selon le mode
        switch (this->mode) {
        case climate::CLIMATE_MODE_AUTO:

            if (this->traits_.get_supports_two_point_target_temperature()) {
                if ((!std::isnan(currentSettings.temperature)) && (currentSettings.temperature > 0)) {
                    this->target_temperature_low = currentSettings.temperature - 2.0f;
                    this->target_temperature_high = currentSettings.temperature + 2.0f;
                    ESP_LOGI("control", "Initializing AUTO mode temps from current PAC temp: %.1f -> [%.1f - %.1f]",
                        currentSettings.temperature, this->target_temperature_low, this->target_temperature_high);
                    //this->publish_state();
                } else {
                    this->sanitizeDualSetpoints();
                }
            }


            // Mode AUTO : utiliser la moyenne des deux bornes            
            setting = (this->target_temperature_low + this->target_temperature_high) / 2.0f;
            ESP_LOGD("control", "AUTO mode : getting median temperature low:%1.f, high:%1.f, result:%.1f", this->target_temperature_low, this->target_temperature_high, setting);
            break;

        case climate::CLIMATE_MODE_HEAT:
            // Mode HEAT : using low target temperature
            this->sanitizeDualSetpoints();
            setting = this->target_temperature_low;
            ESP_LOGD("control", "HEAT mode : getting temperature low:%1.f", this->target_temperature_low);
            break;
        case climate::CLIMATE_MODE_COOL:
            // Mode COOL : using high target temperature
            this->sanitizeDualSetpoints();
            setting = this->target_temperature_high;
            ESP_LOGD("control", "COOL mode : getting temperature high:%1.f", this->target_temperature_high);
            break;
        case climate::CLIMATE_MODE_DRY:
            // Mode DRY : using high target temperature
            this->sanitizeDualSetpoints();
            setting = this->target_temperature_high;
            ESP_LOGD("control", "COOL mode : getting temperature high:%1.f", this->target_temperature_high);
            break;
        default:
            // Other modes : use median temperature
            this->sanitizeDualSetpoints();
            setting = (this->target_temperature_low + this->target_temperature_high) / 2.0f;
            ESP_LOGD("control", "DEFAULT mode : getting temperature median:%1.f", setting);
            break;
        }
    } else {
        // Single setpoint : utiliser target_temperature
        setting = this->target_temperature;
    }

    setting = this->calculateTemperatureSetting(setting);
    this->wantedSettings.temperature = setting;
    ESP_LOGI("control", "setting wanted temperature to %.1f", setting);
}



void CN105Climate::controlMode() {
    switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
        ESP_LOGI("control", "changing mode to COOL");
        this->setModeSetting("COOL");
        this->setPowerSetting("ON");
        break;
    case climate::CLIMATE_MODE_HEAT:
        ESP_LOGI("control", "changing mode to HEAT");
        this->setModeSetting("HEAT");
        this->setPowerSetting("ON");

        break;
    case climate::CLIMATE_MODE_DRY:
        ESP_LOGI("control", "changing mode to DRY");
        this->setModeSetting("DRY");
        this->setPowerSetting("ON");

        break;

    case climate::CLIMATE_MODE_AUTO:
        ESP_LOGI("control", "changing mode to AUTO");
        this->setModeSetting("AUTO");
        this->setPowerSetting("ON");

        break;
    case climate::CLIMATE_MODE_FAN_ONLY:
        ESP_LOGI("control", "changing mode to FAN_ONLY");
        this->setModeSetting("FAN");
        this->setPowerSetting("ON");
        break;
    case climate::CLIMATE_MODE_OFF:
        ESP_LOGI("control", "changing mode to OFF");
        this->setPowerSetting("OFF");
        break;
    default:
        ESP_LOGW("control", "unsupported mode");
    }
}


void CN105Climate::setActionIfOperatingTo(climate::ClimateAction action_if_operating) {


    ESP_LOGD(LOG_OPERATING_STATUS_TAG, "Setting action to %d (effective_operating: %s, use_stage_fallback: %s, current_stage: %s)",
        static_cast<int>(this->action),
        this->currentStatus.operating ? "true" : "false",
        this->use_stage_for_operating_status_ ? "yes" : "no",
        getIfNotNull(this->currentSettings.stage, "N/A"));


    if (this->use_stage_for_operating_status_) {
        ESP_LOGD(LOG_OPERATING_STATUS_TAG, "using stage for operating status because use_stage_for_operating_status_ is true");
        if (this->currentSettings.stage != nullptr &&
            strcmp(this->currentSettings.stage, STAGE_MAP[0 /*IDLE*/]) != 0) {
            this->action = action_if_operating;
            ESP_LOGD(LOG_OPERATING_STATUS_TAG, "stage is active");
        } else {
            ESP_LOGD(LOG_OPERATING_STATUS_TAG, "stage is iddle or null");
            this->action = climate::CLIMATE_ACTION_IDLE;
        }
    } else {
        ESP_LOGD(LOG_OPERATING_STATUS_TAG, "using currentStatus.operating for operating status because use_stage_for_operating_status_ is false");
        this->action = this->currentStatus.operating ? action_if_operating : climate::CLIMATE_ACTION_IDLE;
    }


}

/**
 * Thanks to Bascht74 on issu #9 we know that the compressor frequency is not a good indicator of the heatpump being in operation
 * Because one can have multiple inside module for a single compressor.
 * Also, some heatpump does not support compressor frequency.
 * SO usage is deprecated.
*/
void CN105Climate::setActionIfOperatingAndCompressorIsActiveTo(climate::ClimateAction action) {

    ESP_LOGW(TAG, "Warning: the use of compressor frequency as an active indicator is deprecated. Please use operating status instead.");

    if (currentStatus.compressorFrequency <= 0) {
        this->action = climate::CLIMATE_ACTION_IDLE;
    } else {
        this->setActionIfOperatingTo(action);
    }
}

//inside the below we could implement an internal only HEAT_COOL doing the math with an offset or something
void CN105Climate::updateAction() {
    ESP_LOGV(TAG, "updating action back to espHome...");
    if (this->traits().get_supports_two_point_target_temperature()) {
        this->sanitizeDualSetpoints();
    }
    switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT:
        //this->setActionIfOperatingAndCompressorIsActiveTo(climate::CLIMATE_ACTION_HEATING);       
        this->setActionIfOperatingTo(climate::CLIMATE_ACTION_HEATING);
        break;
    case climate::CLIMATE_MODE_COOL:
        //this->setActionIfOperatingAndCompressorIsActiveTo(climate::CLIMATE_ACTION_COOLING);
        this->setActionIfOperatingTo(climate::CLIMATE_ACTION_COOLING);
        break;
    case climate::CLIMATE_MODE_AUTO:

        if (this->traits().supports_mode(climate::CLIMATE_MODE_HEAT) &&
            this->traits().supports_mode(climate::CLIMATE_MODE_COOL)) {
            // If the unit supports both heating and cooling
            if (this->current_temperature >= this->target_temperature_high) {
                this->setActionIfOperatingTo(climate::CLIMATE_ACTION_COOLING);
            } else if (this->current_temperature <= this->target_temperature_low) {
                this->setActionIfOperatingTo(climate::CLIMATE_ACTION_HEATING);
            } else {
                this->setActionIfOperatingTo(climate::CLIMATE_ACTION_IDLE);
            }
        } else if (this->traits().supports_mode(climate::CLIMATE_MODE_COOL)) {
            // If the unit only supports cooling
            if (this->current_temperature < this->target_temperature_high) {
                // If the temperature meets or exceeds the target, switch to fan-only mode
                this->setActionIfOperatingTo(climate::CLIMATE_ACTION_IDLE);
            } else {
                // Otherwise, continue cooling
                this->setActionIfOperatingTo(climate::CLIMATE_ACTION_COOLING);
            }
        } else if (this->traits().supports_mode(climate::CLIMATE_MODE_HEAT)) {
            // If the unit only supports heating
            if (this->current_temperature >= this->target_temperature_low) {
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
        //this->setActionIfOperatingAndCompressorIsActiveTo(climate::CLIMATE_ACTION_DRYING);
        this->setActionIfOperatingTo(climate::CLIMATE_ACTION_DRYING);
        break;
    case climate::CLIMATE_MODE_FAN_ONLY:
        this->action = climate::CLIMATE_ACTION_FAN;
        break;
    default:
        this->action = climate::CLIMATE_ACTION_OFF;
    }

    ESP_LOGD(TAG, "Climate mode is: %i", this->mode);
    ESP_LOGD(TAG, "Climate action is: %i", this->action);
}

climate::ClimateTraits CN105Climate::traits() {
    //ESP_LOGD(LOG_SETTINGS_TAG, "traits() called (dual: %d)", traits_.get_supports_two_point_target_temperature());
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
    int index = lookupByteMapIndex(MODE_MAP, 5, setting);
    if (index > -1) {
        wantedSettings.mode = MODE_MAP[index];
    } else {
        wantedSettings.mode = MODE_MAP[0];
    }
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
    int index = lookupByteMapIndex(FAN_MAP, 6, setting);
    if (index > -1) {
        wantedSettings.fan = FAN_MAP[index];
    } else {
        wantedSettings.fan = FAN_MAP[0];
    }
}

void CN105Climate::setVaneSetting(const char* setting) {
    int index = lookupByteMapIndex(VANE_MAP, 7, setting);
    if (index > -1) {
        wantedSettings.vane = VANE_MAP[index];
    } else {
        wantedSettings.vane = VANE_MAP[0];
    }
}

void CN105Climate::setWideVaneSetting(const char* setting) {
    int index = lookupByteMapIndex(WIDEVANE_MAP, 8, setting);
    if (index > -1) {
        wantedSettings.wideVane = WIDEVANE_MAP[index];
    } else {
        wantedSettings.wideVane = WIDEVANE_MAP[0];
    }
}

void CN105Climate::setAirflowControlSetting(const char* setting) {
    int index = lookupByteMapIndex(AIRFLOW_CONTROL_MAP, 3, setting);
    if (index > -1) {
        wantedRunStates.airflow_control = AIRFLOW_CONTROL_MAP[index];
    } else {
        wantedRunStates.airflow_control = AIRFLOW_CONTROL_MAP[0];
    }
}

void CN105Climate::set_remote_temperature(float setting) {
    this->shouldSendExternalTemperature_ = true;
    if (use_fahrenheit_support_mode_) {
        setting = this->fahrenheitSupport_.normalizeCelsiusForConversionFromFahrenheit(setting);
    }
    this->remoteTemperature_ = setting;
    ESP_LOGD(LOG_REMOTE_TEMP, "setting remote temperature to %f", this->remoteTemperature_);
}

