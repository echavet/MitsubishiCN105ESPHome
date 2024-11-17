#include "cn105.h"
#include "Globals.h"

using namespace esphome;


void CN105Climate::checkPendingWantedSettings() {
    long now = CUSTOM_MILLIS;
    if (!(this->wantedSettings.hasChanged) || (now - this->wantedSettings.lastChange < this->debounce_delay_)) {
        return;
    }

    ESP_LOGI(LOG_ACTION_EVT_TAG, "checkPendingWantedSettings - wanted settings have changed, sending them to the heatpump...");
    this->sendWantedSettings();
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

    // Traiter les commandes de climatisation ici
    if (call.get_mode().has_value()) {
        ESP_LOGD("control", "Mode change asked");
        // Changer le mode de climatisation
        this->mode = *call.get_mode();
        updated = true;
        controlMode();
    }

    if (call.get_target_temperature().has_value()) {
        // Changer la température cible
        ESP_LOGI("control", "Setting heatpump setpoint : %.1f", *call.get_target_temperature());
        this->target_temperature = *call.get_target_temperature();
        updated = true;
        controlTemperature();
    }

    if (call.get_target_temperature_low().has_value()) {
        // Changer la température cible
        ESP_LOGI("control", "Setting heatpump low setpoint : %.1f", *call.get_target_temperature_low());
        this->target_temperature_low = *call.get_target_temperature_low();
        updated = true;
        controlTemperature();
    }
    if (call.get_target_temperature_high().has_value()) {
        // Changer la température cible
        ESP_LOGI("control", "Setting heatpump high setpoint : %.1f", *call.get_target_temperature_high());
        this->target_temperature_high = *call.get_target_temperature_high();
        updated = true;
        controlTemperature();
    }

    if (call.get_fan_mode().has_value()) {
        ESP_LOGD("control", "Fan change asked");
        // Changer le mode de ventilation
        this->fan_mode = *call.get_fan_mode();
        updated = true;
        this->controlFan();
    }
    if (call.get_swing_mode().has_value()) {
        ESP_LOGD("control", "Swing change asked");
        // Changer le mode de balancement
        this->swing_mode = *call.get_swing_mode();
        updated = true;
        this->controlSwing();
    }

    if (updated) {
        ESP_LOGD(LOG_ACTION_EVT_TAG, "clim.control() -> User changed something...");
        logCheckWantedSettingsMutex(this->wantedSettings);
        this->wantedSettings.hasChanged = true;
        this->wantedSettings.hasBeenSent = false;
        this->wantedSettings.lastChange = CUSTOM_MILLIS;
        this->debugSettings("control (wantedSettings)", this->wantedSettings);
    }

}

void CN105Climate::control(const esphome::climate::ClimateCall& call) {

#ifdef USE_ESP32
    std::lock_guard<std::mutex> guard(wantedSettingsMutex);
    this->controlDelegate(call);
#else    
    this->emulateMutex("CONTROL_WANTED_SETTINGS", std::bind(&CN105Climate::controlDelegate, this, call));
#endif    

}


void CN105Climate::controlSwing() {
    switch (this->swing_mode) {                 //setVaneSetting supports:  AUTO 1 2 3 4 5 and SWING
    case climate::CLIMATE_SWING_OFF:
        this->setVaneSetting("AUTO");
        this->setWideVaneSetting("|");
        break;
    case climate::CLIMATE_SWING_VERTICAL:
        this->setVaneSetting("SWING");
        this->setWideVaneSetting("|");
        break;
    case climate::CLIMATE_SWING_HORIZONTAL:
        this->setVaneSetting("AUTO");
        this->setWideVaneSetting("SWING");
        break;
    case climate::CLIMATE_SWING_BOTH:
        this->setVaneSetting("SWING");
        this->setWideVaneSetting("SWING");
        break;
    default:
        ESP_LOGW(TAG, "control - received unsupported swing mode request.");
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
    float setting = this->target_temperature;

    float cool_setpoint = this->target_temperature_low;
    float heat_setpoint = this->target_temperature_high;
    //float humidity_setpoint = this->target_humidity;

    if (!this->tempMode) {
        this->wantedSettings.temperature = this->lookupByteMapIndex(TEMP_MAP, 16, (int)(setting + 0.5)) > -1 ? setting : TEMP_MAP[0];
    } else {
        setting = setting * 2;
        setting = round(setting);
        setting = setting / 2;
        this->wantedSettings.temperature = setting < 10 ? 10 : (setting > 31 ? 31 : setting);
    }
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


void CN105Climate::setActionIfOperatingTo(climate::ClimateAction action) {
    if (currentStatus.operating) {
        this->action = action;
    } else {
        this->action = climate::CLIMATE_ACTION_IDLE;
    }
    ESP_LOGD(TAG, "setting action to -> %d", this->action);
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
        //this->setActionIfOperatingAndCompressorIsActiveTo(
        this->setActionIfOperatingTo(
            (this->current_temperature > this->target_temperature ?
                climate::CLIMATE_ACTION_COOLING :
                climate::CLIMATE_ACTION_HEATING));
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
    int index = lookupByteMapIndex(WIDEVANE_MAP, 7, setting);
    if (index > -1) {
        wantedSettings.wideVane = WIDEVANE_MAP[index];
    } else {
        wantedSettings.wideVane = WIDEVANE_MAP[0];
    }
}



