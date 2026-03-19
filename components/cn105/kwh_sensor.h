#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"


namespace esphome {

    class kWhSensor : public sensor::Sensor, public Component {
    public:
        kWhSensor() {
            this->set_state_class(sensor::StateClass::STATE_CLASS_TOTAL_INCREASING);
            this->set_accuracy_decimals(1);
        }
    };

}
