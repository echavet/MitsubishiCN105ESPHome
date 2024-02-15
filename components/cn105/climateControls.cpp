#include "cn105.h"
#include "Globals.h"

using namespace esphome;


void CN105Climate::checkPendingWantedSettings() {

    long now = CUSTOM_MILLIS;

    if ((this->firstRun) || (this->wantedSettings.hasBeenSent) || (now - this->wantedSettings.lastChange < this->debounce_delay_)) {
        return;
    }
    if (this->currentSettings != this->wantedSettings) {

        if (this->wantedSettings.hasChanged) {
            if (!this->wantedSettings.hasBeenSent) {
                ESP_LOGD(TAG, "checkPendingWantedSettings - wanted settings have changed, sending them to the heatpump...");
                this->sendWantedSettings();
            }
        } else {
            ESP_LOGI(TAG, "checkPendingWantedSettings - detected a change from IR Remote Control");
            // if not wantedSettings.hasChanged this is because we've had a change from IR Remote Control
            // in this case wantedSettings should be updated to currentSettings
            this->wantedSettings = this->currentSettings;

            // TODO: chk, this shouldn't be necessary here
            this->wantedSettings.hasChanged = false;
            this->wantedSettings.hasBeenSent = false;
        }

    } else {
        if (this->wantedSettings.hasChanged) {
            ESP_LOGW(TAG, "checkPendingWantedSettings- SHOULDN'T HAPPEN :  this->wantedSettings.hasChanged is true but this->currentSettings == this->wantedSettings,");
            this->wantedSettings.hasChanged = false;
        }
    }

}

//#region climate
void CN105Climate::control(const esphome::climate::ClimateCall& call) {

    ESP_LOGD("control", "espHome control() interface method called...");
    bool updated = false;
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
        this->wantedSettings.hasChanged = true;
        this->wantedSettings.hasBeenSent = false;
        this->wantedSettings.lastChange = CUSTOM_MILLIS;
        this->debugSettings("control (wantedSettings)", this->wantedSettings);

        // we don't call sendWantedSettings() anymore because it will be called by the loop() method
        // just because we changed something doesn't mean we want to send it to the heatpump right away
        //ESP_LOGD(TAG, "User changed something, sending change to heatpump...");
        //this->sendWantedSettings();        
    }

    // send the update back to esphome: this will update the UI
    // this is not necessary because state will be published after the response and the call off settingsChanged() in the updateSuccess() method
    // this->publish_state();
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
    case climate::CLIMATE_MODE_HEAT_COOL:
        ESP_LOGI("control", "changing mode to HEAT_COOL");
        this->setModeSetting("AUTO");
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
    case climate::CLIMATE_MODE_HEAT_COOL:
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
    // Définir les caractéristiques de ton climatiseur ici

    return traits_;
    // auto traits = climate::ClimateTraits();

    // traits.set_supports_current_temperature(true);
    // traits.set_supports_two_point_target_temperature(false);
    // traits.set_supports_cool_mode(true);
    // traits.set_supports_heat_mode(true);
    // traits.set_supports_fan_mode_auto(true);
    // traits.set_supports_fan_mode_on(true);
    // traits.set_supports_fan_mode_off(true);
    // traits.set_supports_swing_mode_off(true);
    // traits.set_supports_swing_mode_both(true);
    // traits.set_visual_min_temperature(16);
    // traits.set_visual_max_temperature(30);
    // traits.set_visual_temperature_step(0.5f);
    // return traits;
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


void CN105Climate::on_horizontal_swing_change(const std::string& swing) {
    ESP_LOGD(TAG, "Setting vertical swing position: %s", swing.c_str());
    //this->setWideVaneSetting(swing.c_str());
    //this->wantedSettings.hasChanged = true;
    //this->wantedSettings.hasBeenSent = false;
}

void CN105Climate::on_vertical_swing_change(const std::string& swing) {
    ESP_LOGD(TAG, "Setting horizontal swing position: %s", swing.c_str());
    //this->setVaneSetting(swing.c_str());
    //this->wantedSettings.hasChanged = true;
    //this->wantedSettings.hasBeenSent = false;
}



