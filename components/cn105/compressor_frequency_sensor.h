#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"


namespace esphome {

    class CompressorFrequencySensor : public sensor::Sensor, public Component {
    public:
        CompressorFrequencySensor() {
            this->set_unit_of_measurement("Hz");
            this->set_device_class("frequency");
            this->set_state_class(sensor::StateClass::STATE_CLASS_MEASUREMENT);
            this->set_accuracy_decimals(1);
        }
    };

}
