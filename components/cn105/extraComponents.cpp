#include "cn105.h"

using namespace esphome;


void CN105Climate::generateExtraComponents() {
    /*this->iSee_sensor = new binary_sensor::BinarySensor();
    this->iSee_sensor->set_name("iSee sensor");
    this->iSee_sensor->publish_initial_state(false);
    App.register_binary_sensor(this->iSee_sensor);*/

}

void CN105Climate::set_vertical_vane_select(
    VaneOrientationSelect* vertical_vane_select) {

    this->vertical_vane_select_ = vertical_vane_select;

    // builds option list from SwiCago vaneMap
    std::vector<std::string> vaneOptions(std::begin(VANE_MAP), std::end(VANE_MAP));
    this->vertical_vane_select_->traits.set_options(vaneOptions);

    this->vertical_vane_select_->setCallbackFunction([this](const char* setting) {

        ESP_LOGD("EVT", "vane.control() -> Demande un chgt de réglage de la vane: %s", setting);

        this->setVaneSetting(setting);
        this->wantedSettings.hasChanged = true;
        this->wantedSettings.hasBeenSent = false;
        this->wantedSettings.lastChange = CUSTOM_MILLIS;
        });

}

void CN105Climate::set_horizontal_vane_select(
    VaneOrientationSelect* horizontal_vane_select) {
    this->horizontal_vane_select_ = horizontal_vane_select;

    // builds option list from SwiCago wideVaneMap
    std::vector<std::string> wideVaneOptions(std::begin(WIDEVANE_MAP), std::end(WIDEVANE_MAP));
    this->horizontal_vane_select_->traits.set_options(wideVaneOptions);

    this->horizontal_vane_select_->setCallbackFunction([this](const char* setting) {
        ESP_LOGD("EVT", "wideVane.control() -> Demande un chgt de réglage de la wideVane: %s", setting);

        this->setWideVaneSetting(setting);
        this->wantedSettings.hasChanged = true;
        this->wantedSettings.hasBeenSent = false;
        this->wantedSettings.lastChange = CUSTOM_MILLIS;
    });

}

void CN105Climate::set_airflow_control_select(
    VaneOrientationSelect* airflow_control_select) {
    this->airflow_control_select_ = airflow_control_select;
        
    std::vector<std::string> airflowControlOptions(std::begin(AIRFLOW_CONTROL_MAP), std::end(AIRFLOW_CONTROL_MAP));
    this->airflow_control_select_->traits.set_options(airflowControlOptions);
    
    this->airflow_control_select_->setCallbackFunction([this](const char* setting) {
        if (strcmp(this->currentSettings.wideVane, lookupByteMapValue(WIDEVANE_MAP, WIDEVANE, 8, 0x80 & 0x0F)) == 0) {
            ESP_LOGD("EVT", "airFlow -> Request for change of airflow control setting: %s", setting);

            this->setAirflowControlSetting(setting);
            this->wantedRunStates.hasChanged = true;
            this->wantedRunStates.hasBeenSent = false;
            this->wantedRunStates.lastChange = CUSTOM_MILLIS;
        } else {
            this->airflow_control_select_->publish_state(this->currentRunStates.airflow_control);
        }
    });
}

void CN105Climate::set_compressor_frequency_sensor(
    sensor::Sensor* compressor_frequency_sensor) {
    this->compressor_frequency_sensor_ = compressor_frequency_sensor;
}

void CN105Climate::set_input_power_sensor(
    sensor::Sensor* input_power_sensor) {
    this->input_power_sensor_ = input_power_sensor;
}

void CN105Climate::set_kwh_sensor(
    sensor::Sensor* kwh_sensor) {
    this->kwh_sensor_ = kwh_sensor;
}

void CN105Climate::set_runtime_hours_sensor(
    sensor::Sensor* runtime_hours_sensor) {
    this->runtime_hours_sensor_ = runtime_hours_sensor;
}

void CN105Climate::set_outside_air_temperature_sensor(
    sensor::Sensor* outside_air_temperature_sensor) {
    this->outside_air_temperature_sensor_ = outside_air_temperature_sensor;
}

void CN105Climate::set_isee_sensor(esphome::binary_sensor::BinarySensor* iSee_sensor) {
    this->iSee_sensor_ = iSee_sensor;
}

void CN105Climate::set_stage_sensor(esphome::text_sensor::TextSensor* stage_sensor) {
    this->stage_sensor_ = stage_sensor;
}
void CN105Climate::set_use_stage_for_operating_status(bool value) {
    this->use_stage_for_operating_status_ = value;
    ESP_LOGI(TAG, "Using stage sensor as operating fallback: %s", value ? "true" : "false");
}

void CN105Climate::set_functions_sensor(esphome::text_sensor::TextSensor* Functions_sensor) {
    this->Functions_sensor_ = Functions_sensor;
}

void CN105Climate::set_functions_get_button(FunctionsButton* Button) {
    this->Functions_get_button_ = Button;
    this->Functions_get_button_->setCallbackFunction([this]() {
        ESP_LOGI(LOG_CYCLE_TAG, "Retrieving functions");
        // Get the settings from the heat pump
        this->getFunctions();
        // The response is handled in heatpumpFunctions.cpp
        });
}

void CN105Climate::set_functions_set_button(FunctionsButton* Button) {
    this->Functions_set_button_ = Button;
    this->Functions_set_button_->setCallbackFunction([this]() {

        if (!functions.isValid()) {
            if (this->Functions_sensor_ != nullptr) {
                this->Functions_sensor_->publish_state("Please get the functions first.");
            }
            return;
        }

        ESP_LOGI(LOG_CYCLE_TAG, "Setting code %i to value %i", this->functions_code_, this->functions_value_);
        functions.setValue(this->functions_code_, this->functions_value_);

        // Now send the codes.
        this->setFunctions(functions);

        });
}

void CN105Climate::set_functions_set_code(FunctionsNumber* Number) {
    this->Functions_set_code_ = Number;
    this->Functions_set_code_->setCallbackFunction([this](float x) {
        // store the code
        this->functions_code_ = (int)x;
        });

}
void CN105Climate::set_functions_set_value(FunctionsNumber* Number) {
    this->Functions_set_value_ = Number;
    this->Functions_set_value_->setCallbackFunction([this](float x) {
        // store the value
        this->functions_value_ = (int)x;
        });
}

void CN105Climate::set_air_purifier_switch(HVACOptionSwitch* Switch) {
    this->air_purifier_switch_ = Switch;
    this->air_purifier_switch_->setCallbackFunction([this](bool state) {
        this->wantedRunStates.air_purifier = state;
        
        this->wantedRunStates.hasChanged = true;
        this->wantedRunStates.hasBeenSent = false;
        this->wantedRunStates.lastChange = CUSTOM_MILLIS;
    });
}

void CN105Climate::set_night_mode_switch(HVACOptionSwitch* Switch) {
    this->night_mode_switch_ = Switch;
    this->night_mode_switch_->setCallbackFunction([this](bool state) {
        this->wantedRunStates.night_mode = state;
        
        this->wantedRunStates.hasChanged = true;
        this->wantedRunStates.hasBeenSent = false;
        this->wantedRunStates.lastChange = CUSTOM_MILLIS;
    });
}

void CN105Climate::set_circulator_switch(HVACOptionSwitch* Switch) { // only in HEAT mode? Manual says so, but it is possible to set the bit. The remote will not do it.
    this->circulator_switch_ = Switch;
    this->circulator_switch_->setCallbackFunction([this](bool state) {
        this->wantedRunStates.circulator = state;
        
        this->wantedRunStates.hasChanged = true;
        this->wantedRunStates.hasBeenSent = false;
        this->wantedRunStates.lastChange = CUSTOM_MILLIS;
    });
}

void CN105Climate::set_sub_mode_sensor(esphome::text_sensor::TextSensor* Sub_mode_sensor) {
    this->Sub_mode_sensor_ = Sub_mode_sensor;
}

void CN105Climate::set_auto_sub_mode_sensor(esphome::text_sensor::TextSensor* Auto_sub_mode_sensor) {
    this->Auto_sub_mode_sensor_ = Auto_sub_mode_sensor;
}

void CN105Climate::set_hp_uptime_connection_sensor(uptime::HpUpTimeConnectionSensor* hp_up_connection_sensor) {
    this->hp_uptime_connection_sensor_ = hp_up_connection_sensor;
}

void CN105Climate::set_use_fahrenheit_support_mode(bool value) {
    this->use_fahrenheit_support_mode_ = value;
    ESP_LOGI(TAG, "Fahrenheit compatibility mode enabled: %s", value ? "true" : "false");
}
