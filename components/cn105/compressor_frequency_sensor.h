#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"


namespace esphome {

    class CompressorFrequencySensor : public sensor::Sensor, public Component {
    public:
        CompressorFrequencySensor() {
            this->set_state_class(sensor::StateClass::STATE_CLASS_MEASUREMENT);
            this->set_accuracy_decimals(1);
        }
    };

}
