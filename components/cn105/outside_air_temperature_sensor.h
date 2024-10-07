#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"


namespace esphome {

    class OutsideAirTemperatureSensor : public sensor::Sensor, public Component {
    public:
        OutsideAirTemperatureSensor() {
            this->set_unit_of_measurement("°C");
            this->set_accuracy_decimals(1);
        }
    };

}
