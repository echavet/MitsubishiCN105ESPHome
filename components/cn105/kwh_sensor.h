#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"


namespace esphome {

    class kWhSensor : public sensor::Sensor, public Component {
    public:
        kWhSensor() {
            this->set_unit_of_measurement("kWh");
            this->set_device_class("energy");
            this->set_state_class(sensor::StateClass::STATE_CLASS_TOTAL_INCREASING);
            this->set_accuracy_decimals(1);
        }
    };

}
