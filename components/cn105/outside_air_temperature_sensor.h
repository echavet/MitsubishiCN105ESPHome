#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"


namespace esphome {

    class OutsideAirTemperatureSensor : public sensor::Sensor, public Component {
    public:
        OutsideAirTemperatureSensor() {
            this->set_state_class(sensor::StateClass::STATE_CLASS_MEASUREMENT);
            this->set_accuracy_decimals(1);
        }
    };

}
