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
    ESP_LOGD("control", "handleDualSetpointBoth - low: %.1f, high: %.1f", low, high);
    this->setTargetTemperatureLow(low);
    this->setTargetTemperatureHigh(high);
    this->last_dual_setpoint_side_ = 'N';
    this->last_dual_setpoint_change_ms_ = CUSTOM_MILLIS;
    this->currentSettings.dual_low_target = this->getTargetTemperatureLow();
    this->currentSettings.dual_high_target = this->getTargetTemperatureHigh();
}

void CN105Climate::handleDualSetpointLowOnly(float low) {
    ESP_LOGD("control", "handleDualSetpointLowOnly - LOW: %.1f", low);
    if (this->last_dual_setpoint_side_ == 'H' && (CUSTOM_MILLIS - this->last_dual_setpoint_change_ms_) < UI_SETPOINT_ANTIREBOUND_MS) {
        ESP_LOGD("control", "IGNORED low setpoint due to UI anti-rebound after high change");
        return;
    }
    if (!std::isnan(this->getTargetTemperatureLow()) && fabsf(low - this->getTargetTemperatureLow()) < 0.05f) {
        ESP_LOGD("control", "IGNORED low setpoint: no effective change vs current low target");
        return;
    }
    this->setTargetTemperatureLow(low);
    if (this->mode == climate::CLIMATE_MODE_HEAT_COOL) {
        const float amplitude = 4.0f;
        this->setTargetTemperatureHigh(this->getTargetTemperatureLow() + amplitude);
        ESP_LOGD("control", "mode heat_cool: sliding high to preserve amplitude %.1f => [%.1f - %.1f]", amplitude, this->getTargetTemperatureLow(), this->getTargetTemperatureHigh());
    }
    this->last_dual_setpoint_side_ = 'L';
    this->last_dual_setpoint_change_ms_ = CUSTOM_MILLIS;
    this->currentSettings.dual_low_target = this->getTargetTemperatureLow();
    this->currentSettings.dual_high_target = this->getTargetTemperatureHigh();
}

void CN105Climate::handleDualSetpointHighOnly(float high) {
    ESP_LOGI("control", "HIGH: handleDualSetpointHighOnly - HIGH : %.1f", high);
    if (this->last_dual_setpoint_side_ == 'L' && (CUSTOM_MILLIS - this->last_dual_setpoint_change_ms_) < UI_SETPOINT_ANTIREBOUND_MS) {
        ESP_LOGD("control", "ignored high setpoint due to UI anti-rebound after low change");
        return;
    }
    if (!std::isnan(this->getTargetTemperatureHigh()) && fabsf(high - this->getTargetTemperatureHigh()) < 0.05f) {
        ESP_LOGD("control", "ignored high setpoint: no effective change vs current high target");
        return;
    }
    this->setTargetTemperatureHigh(high);
    if (this->mode == climate::CLIMATE_MODE_HEAT_COOL) {
        const float amplitude = 4.0f;
        this->setTargetTemperatureLow(this->getTargetTemperatureHigh() - amplitude);
        ESP_LOGD("control", "mode heat_cool: sliding low to preserve amplitude %.1f => [%.1f - %.1f]", amplitude, this->getTargetTemperatureLow(), this->getTargetTemperatureHigh());
    }
    this->last_dual_setpoint_side_ = 'H';
    this->last_dual_setpoint_change_ms_ = CUSTOM_MILLIS;
    this->currentSettings.dual_low_target = this->getTargetTemperatureLow();
    this->currentSettings.dual_high_target = this->getTargetTemperatureHigh();
}

void CN105Climate::handleSingleTargetInAutoOrDry(float requested) {
    ESP_LOGD("control", "handleSingleTargetInAutoOrDry - SINGLE: %.1f", requested);
    if (this->mode == climate::CLIMATE_MODE_HEAT_COOL) {
        const float half_span = 2.0f;
        this->setTargetTemperatureLow(requested - half_span);
        this->setTargetTemperatureHigh(requested + half_span);
        this->last_dual_setpoint_side_ = 'N';
        this->last_dual_setpoint_change_ms_ = CUSTOM_MILLIS;
        this->currentSettings.dual_low_target = this->getTargetTemperatureLow();
        this->currentSettings.dual_high_target = this->getTargetTemperatureHigh();
        this->setTargetTemperature(requested);
        ESP_LOGD("control", "HEAT_COOL received single target: median=%.1f => [%.1f - %.1f]", requested, this->getTargetTemperatureLow(), this->getTargetTemperatureHigh());
    }
    if (this->mode == climate::CLIMATE_MODE_DRY) {
        this->setTargetTemperatureHigh(requested);
        if (std::isnan(this->getTargetTemperatureLow())) {
            this->setTargetTemperatureLow(requested);
        }
        this->last_dual_setpoint_side_ = 'H';
        this->last_dual_setpoint_change_ms_ = CUSTOM_MILLIS;
        this->currentSettings.dual_low_target = this->getTargetTemperatureLow();
        this->currentSettings.dual_high_target = this->getTargetTemperatureHigh();
        this->setTargetTemperature(requested);
        ESP_LOGD("control", "DRY received single target: high=%.1f (low now %.1f)", this->getTargetTemperatureHigh(), this->getTargetTemperatureLow());
    }
}

bool CN105Climate::processTemperatureChange(const esphome::climate::ClimateCall& call) {
    // Vérifier si une température est fournie selon les traits
    // En modes AUTO/DRY, accepter aussi target_temperature même en dual setpoint
    bool tempHasValue = (call.get_target_temperature_low().has_value() ||
        call.get_target_temperature_high().has_value() || call.get_target_temperature().has_value());
    /*
    bool tempHasValue = this->traits_.get_supports_two_point_target_temperature() ?
        (
            call.get_target_temperature_low().has_value() ||
            call.get_target_temperature_high().has_value() ||
            ((this->mode == climate::CLIMATE_MODE_AUTO || this->mode == climate::CLIMATE_MODE_DRY) &&
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
        temp_low = this->fahrenheitSupport_.normalizeUiTemperatureToHeatpumpTemperature(*call.get_target_temperature_low());
    }
    if (call.get_target_temperature_high().has_value()) {
        temp_high = this->fahrenheitSupport_.normalizeUiTemperatureToHeatpumpTemperature(*call.get_target_temperature_high());
    }
    if (call.get_target_temperature().has_value()) {
        temp_single = this->fahrenheitSupport_.normalizeUiTemperatureToHeatpumpTemperature(*call.get_target_temperature());
    }

    if (this->traits_.has_feature_flags(climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
        ESP_LOGD("control", "Processing with dual setpoint support...");
        if (call.get_target_temperature_low().has_value() && call.get_target_temperature_high().has_value()) {
            this->handleDualSetpointBoth(temp_low, temp_high);
        } else if (call.get_target_temperature_low().has_value()) {
            this->handleDualSetpointLowOnly(temp_low);
        } else if (call.get_target_temperature_high().has_value()) {
            this->handleDualSetpointHighOnly(temp_high);
        } else if (call.get_target_temperature().has_value() &&
            (this->mode == climate::CLIMATE_MODE_HEAT_COOL || this->mode == climate::CLIMATE_MODE_DRY)) {
            this->handleSingleTargetInAutoOrDry(temp_single);
        }
    } else {
        ESP_LOGD("control", "Processing without dual setpoint support...");
        if (call.get_target_temperature().has_value()) {
            this->setTargetTemperature(temp_single);
            ESP_LOGI("control", "Setting heatpump setpoint : %.1f", this->getTargetTemperature());
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
    bool vane_is_swing = (this->currentSettings.vane != nullptr) && (strcmp(this->currentSettings.vane, "SWING") == 0);
    bool wide_is_swing = (this->currentSettings.wideVane != nullptr) && (strcmp(this->currentSettings.wideVane, "SWING") == 0);

    switch (this->swing_mode) {
    case climate::CLIMATE_SWING_OFF:
        // When swing is turned OFF, conditionally set vanes to a default static position.
        // This only sets default position if swing was previously enabled
        if (vane_is_swing) {
            this->setVaneSetting("AUTO");
        }
        if (wideVaneSupported && wide_is_swing) {
            this->setWideVaneSetting("|");
        }
        break;

    case climate::CLIMATE_SWING_VERTICAL:
        // Turn on vertical swing.
        this->setVaneSetting("SWING");
        // If horizontal swing was also on AND is supported, turn it off to a default static position.
        // This correctly handles switching from BOTH to VERTICAL, while preserving any user's
        // static horizontal setting if it wasn't swinging.
        if (wideVaneSupported && wide_is_swing) {
            this->setWideVaneSetting("|");
        }
        break;

    case climate::CLIMATE_SWING_HORIZONTAL:
        // If vertical swing was on, turn it off to a default static position.
        // This correctly handles switching from BOTH to HORIZONTAL, while preserving any user's
        // static vertical setting if it wasn't swinging.
        if (vane_is_swing) {
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
    if (this->traits_.has_feature_flags(climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
        this->sanitizeDualSetpoints();
        // Dual setpoint : choisir la bonne consigne selon le mode
        switch (this->mode) {
        case climate::CLIMATE_MODE_HEAT_COOL:
            if (this->traits_.has_feature_flags(climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
                float low = this->getTargetTemperatureLow();
                float high = this->getTargetTemperatureHigh();
                float current = this->getCurrentTemperature();

                // Initialize setpoints if not defined
                if (std::isnan(low) || std::isnan(high)) {
                    if ((!std::isnan(currentSettings.temperature)) && (currentSettings.temperature > 0)) {
                        if (std::isnan(low)) this->setTargetTemperatureLow(currentSettings.temperature - 2.0f);
                        if (std::isnan(high)) this->setTargetTemperatureHigh(currentSettings.temperature + 2.0f);
                        low = this->getTargetTemperatureLow();
                        high = this->getTargetTemperatureHigh();
                        ESP_LOGI("control", "Initializing HEAT_COOL setpoints from heat pump temp: %.1f -> [%.1f - %.1f]",
                            currentSettings.temperature, low, high);
                    } else {
                        // Fallback to defaults when no heat pump temperature available
                        if (std::isnan(low)) this->setTargetTemperatureLow(ESPMHP_DEFAULT_LOW_SETPOINT);
                        if (std::isnan(high)) this->setTargetTemperatureHigh(ESPMHP_DEFAULT_HIGH_SETPOINT);
                        low = this->getTargetTemperatureLow();
                        high = this->getTargetTemperatureHigh();
                        ESP_LOGW("control", "HEAT_COOL: using default setpoints [%.1f - %.1f]", low, high);
                    }
                }

                // Deadband algorithm: send LOW, HIGH, or current temp to keep heat pump idle
                setting = this->calculateDeadbandSetpoint(current, low, high);
                if (std::isnan(setting)) {
                    ESP_LOGD("control", "HEAT_COOL: no current temp yet, waiting for heat pump data");
                    return;
                }
                ESP_LOGD("control", "HEAT_COOL: deadband -> setting=%.1f (current=%.1f, range=[%.1f-%.1f])",
                         setting, current, low, high);
            } else {
                setting = this->getTargetTemperature();
            }
            break;

        case climate::CLIMATE_MODE_HEAT:
            // Mode HEAT : using low target temperature
            setting = this->getTargetTemperatureLow();
            ESP_LOGD("control", "HEAT mode : getting temperature low:%1.f", this->getTargetTemperatureLow());
            break;
        case climate::CLIMATE_MODE_COOL:
            // Mode COOL : using high target temperature
            setting = this->getTargetTemperatureHigh();
            ESP_LOGD("control", "COOL mode : getting temperature high:%1.f", this->getTargetTemperatureHigh());
            break;
        case climate::CLIMATE_MODE_DRY:
            // Mode DRY : using high target temperature
            setting = this->getTargetTemperatureHigh();
            ESP_LOGD("control", "COOL mode : getting temperature high:%1.f", this->getTargetTemperatureHigh());
            break;
        default:
            // Other modes : use median temperature
            if (this->traits_.has_feature_flags(climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
                setting = (this->getTargetTemperatureLow() + this->getTargetTemperatureHigh()) / 2.0f;
            } else {
                setting = this->getTargetTemperature();
            }
            ESP_LOGD("control", "DEFAULT mode : getting temperature median:%1.f", setting);
            break;
        }
    } else {
        // Single setpoint : utiliser target_temperature
        setting = this->getTargetTemperature();
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

    case climate::CLIMATE_MODE_HEAT_COOL:
        // HEAT_COOL from Home Assistant maps to AUTO on Mitsubishi heat pump
        ESP_LOGI("control", "changing mode to HEAT_COOL (Mitsubishi AUTO)");
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

    // Determine if stage indicates activity (for fallback logic)
    bool stage_is_active = this->use_stage_for_operating_status_ &&
        this->currentSettings.stage != nullptr &&
        strcmp(this->currentSettings.stage, STAGE_MAP[0 /*IDLE*/]) != 0;

    ESP_LOGD(LOG_OPERATING_STATUS_TAG, "Setting action (operating: %s, stage_fallback_enabled: %s, stage: %s, stage_is_active: %s)",
        this->currentStatus.operating ? "true" : "false",
        this->use_stage_for_operating_status_ ? "yes" : "no",
        getIfNotNull(this->currentSettings.stage, "N/A"),
        stage_is_active ? "yes" : "no");

    // True fallback logic: operating OR (fallback enabled AND stage is active)
    // This handles cases like 2-stage heating where compressor may be off but gas heating is active
    if (this->currentStatus.operating) {
        // Primary: compressor is running
        this->action = action_if_operating;
        ESP_LOGD(LOG_OPERATING_STATUS_TAG, "Action set by operating status (compressor running)");
    } else if (stage_is_active) {
        // Fallback: compressor not running but stage indicates activity (e.g., gas heating)
        this->action = action_if_operating;
        ESP_LOGD(LOG_OPERATING_STATUS_TAG, "Action set by stage fallback (stage: %s)", this->currentSettings.stage);
    } else {
        // Neither operating nor stage indicates activity
        this->action = climate::CLIMATE_ACTION_IDLE;
        ESP_LOGD(LOG_OPERATING_STATUS_TAG, "Action set to IDLE (no activity detected)");
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
    if (this->traits().has_feature_flags(climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
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
    case climate::CLIMATE_MODE_HEAT_COOL:
        // HEAT_COOL mode (mapped from Mitsubishi AUTO)
        if (this->traits().supports_mode(climate::CLIMATE_MODE_HEAT) &&
            this->traits().supports_mode(climate::CLIMATE_MODE_COOL)) {
            // If the unit supports both heating and cooling
            if (this->getCurrentTemperature() > this->getTargetTemperatureHigh()) {
                this->setActionIfOperatingTo(climate::CLIMATE_ACTION_COOLING);
            } else if (this->getCurrentTemperature() < this->getTargetTemperatureLow()) {
                this->setActionIfOperatingTo(climate::CLIMATE_ACTION_HEATING);
            } else {
                this->setActionIfOperatingTo(climate::CLIMATE_ACTION_IDLE);
            }
        } else if (this->traits().supports_mode(climate::CLIMATE_MODE_COOL)) {
            // If the unit only supports cooling
            if (this->getCurrentTemperature() < this->getTargetTemperatureHigh()) {
                // If the temperature meets or exceeds the target, switch to fan-only mode
                this->setActionIfOperatingTo(climate::CLIMATE_ACTION_IDLE);
            } else {
                // Otherwise, continue cooling
                this->setActionIfOperatingTo(climate::CLIMATE_ACTION_COOLING);
            }
        } else if (this->traits().supports_mode(climate::CLIMATE_MODE_HEAT)) {
            // If the unit only supports heating
            if (this->getCurrentTemperature() >= this->getTargetTemperatureLow()) {
                // If the temperature meets or exceeds the target, switch to fan-only mode
                this->setActionIfOperatingTo(climate::CLIMATE_ACTION_IDLE);
            } else {
                // Otherwise, continue heating
                this->setActionIfOperatingTo(climate::CLIMATE_ACTION_HEATING);
            }
        } else {
            ESP_LOGE(TAG, "HEAT_COOL mode is not supported by this unit");
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
    if (std::isnan(setting)) {
        ESP_LOGW(LOG_REMOTE_TEMP, "Remote temperature is NaN, ignoring.");
        return;
    }

    // Toujours renvoyer la température distante lorsqu’un nouvel échantillon arrive,
    // même si la valeur n’a pas changé, afin d’éviter que l’unité Mitsubishi
    // ne repasse sur la sonde interne faute de mise à jour régulière (#474).
    this->remoteTemperature_ = setting;
    this->shouldSendExternalTemperature_ = true;

    // Update current_temperature immediately for deadband algorithm in heat_cool mode and HA display
    this->setCurrentTemperature(setting);

    ESP_LOGD(LOG_REMOTE_TEMP, "setting remote temperature to %f", this->remoteTemperature_);

    // React to temperature change immediately in HEAT_COOL mode
    this->checkDeadbandAndSend();
}

float CN105Climate::calculateDeadbandSetpoint(float current, float low, float high) {
    if (std::isnan(current)) {
        return NAN;
    }
    if (current < low) {
        return low;
    }
    if (current > high) {
        return high;
    }
    return current;  // In deadband: follow room temp
}

void CN105Climate::checkDeadbandAndSend() {
    // Only in HEAT_COOL mode with dual_setpoint
    if (this->mode != climate::CLIMATE_MODE_HEAT_COOL) return;
    if (!this->traits_.has_feature_flags(climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) return;

    float low = this->getTargetTemperatureLow();
    float high = this->getTargetTemperatureHigh();
    float current = this->getCurrentTemperature();

    // Check that values are defined
    if (std::isnan(low) || std::isnan(high)) return;

    float newSetting = this->calculateDeadbandSetpoint(current, low, high);
    if (std::isnan(newSetting)) {
        ESP_LOGD("control", "DEADBAND: current temp is NaN, skipping");
        return;
    }
    ESP_LOGD("control", "DEADBAND: setting=%.1f (current=%.1f, range=[%.1f-%.1f])",
             newSetting, current, low, high);

    // Send only if significantly different from last setpoint (avoid float precision issues)
    if (fabsf(newSetting - this->currentSettings.temperature) > 0.01f) {
        this->wantedSettings.temperature = this->calculateTemperatureSetting(newSetting);
        this->wantedSettings.hasChanged = true;
        this->wantedSettings.hasBeenSent = false;
        this->wantedSettings.lastChange = CUSTOM_MILLIS;
    }
}
