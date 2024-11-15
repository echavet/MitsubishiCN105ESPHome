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

void CN105Climate::set_stage_sensor(esphome::text_sensor::TextSensor* Stage_sensor) {
    this->Stage_sensor_ = Stage_sensor;
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