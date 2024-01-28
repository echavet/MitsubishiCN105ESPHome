#include "cn105.h"

using namespace esphome;
/*
class VaneOrientationSelect : public select::Select {
public:
    VaneOrientationSelect(CN105Climate* parent) : parent_(parent) {}

    void control(const std::string& value) override {

        ESP_LOGD("EVT", "vane.control() -> Demande un chgt de réglage de la vane: %s", value.c_str());

        parent_->setVaneSetting(value.c_str()); // should be enough to trigger a sendWantedSettings
        parent_->wantedSettings.hasChanged = true;
        parent_->wantedSettings.hasBeenSent = false;
        // now updated thanks to new sendWantedSettings policy
        // parent_->sendWantedSettings();

    }
private:
    CN105Climate* parent_;

};*/

void CN105Climate::generateExtraComponents() {
    /*this->compressor_frequency_sensor = new sensor::Sensor();
    this->compressor_frequency_sensor->set_name("Compressor Frequency");
    this->compressor_frequency_sensor->set_unit_of_measurement("Hz");
    this->compressor_frequency_sensor->set_accuracy_decimals(0);
    this->compressor_frequency_sensor->publish_state(0);

    // Enregistrer le capteur pour que ESPHome le gère
    App.register_sensor(compressor_frequency_sensor);*/

    this->iSee_sensor = new binary_sensor::BinarySensor();
    this->iSee_sensor->set_name("iSee sensor");
    this->iSee_sensor->publish_initial_state(false);
    App.register_binary_sensor(this->iSee_sensor);

    /*this->van_orientation = new  VaneOrientationSelect(this);
    this->van_orientation->set_name("Van orientation");

    //this->van_orientation->traits.set_options({ "AUTO", "1", "2", "3", "4", "5", "SWING" });
    std::vector<std::string> vaneOptions(std::begin(VANE_MAP), std::end(VANE_MAP));
    this->van_orientation->traits.set_options(vaneOptions);

    App.register_select(this->van_orientation);*/

    /*this->last_sent_packet_sensor = new TextSensor();
    this->last_sent_packet_sensor->set_name("Last Sent Packet");
    this->last_sent_packet_sensor->set_entity_category(esphome::EntityCategory::ENTITY_CATEGORY_DIAGNOSTIC);
    App.register_text_sensor(this->last_sent_packet_sensor);


    this->last_received_packet_sensor = new text_sensor::TextSensor();
    this->last_received_packet_sensor->set_name("Last Received Packet");
    this->last_received_packet_sensor->set_entity_category(esphome::EntityCategory::ENTITY_CATEGORY_DIAGNOSTIC);
    App.register_text_sensor(this->last_received_packet_sensor);*/


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
        });

    /*this->vertical_vane_select_->add_on_state_callback(
        [this](const std::string& value, size_t index) {
            //if (value == this->vertical_swing_state_) return;
            this->on_vertical_swing_change(value);
        });*/
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
        });

    /*this->horizontal_vane_select_->add_on_state_callback(
        [this](const std::string& value, size_t index) {
            //if (value == this->horizontal_swing_state_) return;
            this->on_horizontal_swing_change(value);
        });*/
}

void CN105Climate::set_compressor_frequency_sensor(
    sensor::Sensor* compressor_frequency_sensor) {
    this->compressor_frequency_sensor_ = compressor_frequency_sensor;
}
