#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"


namespace esphome {

    class InputPowerSensor : public sensor::Sensor, public Component {
    public:
        InputPowerSensor() {
            this->set_state_class(sensor::StateClass::STATE_CLASS_MEASUREMENT);
            this->set_accuracy_decimals(0);
        }
    };

}
