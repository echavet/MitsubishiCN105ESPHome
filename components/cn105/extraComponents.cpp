#include "cn105.h"

using namespace esphome;


class VaneOrientationSelect : public select::Select {
public:
    VaneOrientationSelect(CN105Climate* parent) : parent_(parent) {}

    void control(const std::string& value) override {

        ESP_LOGD("VAN_CTRL", "Demande un chgt de réglage de la vane: %s", value.c_str());

        parent_->setVaneSetting(value.c_str()); // should be enough to trigger a sendWantedSettings

        // TODO: update thanks to new sendWantedSettings policy 
        // parent_->sendWantedSettings();

    }
private:
    CN105Climate* parent_;

};

void CN105Climate::generateExtraComponents() {
    this->compressor_frequency_sensor = new sensor::Sensor();
    this->compressor_frequency_sensor->set_name("Compressor Frequency");
    this->compressor_frequency_sensor->set_unit_of_measurement("Hz");
    this->compressor_frequency_sensor->set_accuracy_decimals(0);
    this->compressor_frequency_sensor->publish_state(0);

    // Enregistrer le capteur pour que ESPHome le gère
    App.register_sensor(compressor_frequency_sensor);

    this->iSee_sensor = new binary_sensor::BinarySensor();
    this->iSee_sensor->set_name("iSee sensor");
    this->iSee_sensor->publish_initial_state(false);
    App.register_binary_sensor(this->iSee_sensor);

    this->van_orientation = new  VaneOrientationSelect(this);
    this->van_orientation->set_name("Van orientation");

    //this->van_orientation->traits.set_options({ "AUTO", "1", "2", "3", "4", "5", "SWING" });    
    std::vector<std::string> vaneOptions(std::begin(VANE_MAP), std::end(VANE_MAP));
    this->van_orientation->traits.set_options(vaneOptions);

    App.register_select(this->van_orientation);

    /*this->last_sent_packet_sensor = new TextSensor();
    this->last_sent_packet_sensor->set_name("Last Sent Packet");
    this->last_sent_packet_sensor->set_entity_category(esphome::EntityCategory::ENTITY_CATEGORY_DIAGNOSTIC);
    App.register_text_sensor(this->last_sent_packet_sensor);


    this->last_received_packet_sensor = new text_sensor::TextSensor();
    this->last_received_packet_sensor->set_name("Last Received Packet");
    this->last_received_packet_sensor->set_entity_category(esphome::EntityCategory::ENTITY_CATEGORY_DIAGNOSTIC);
    App.register_text_sensor(this->last_received_packet_sensor);*/


}