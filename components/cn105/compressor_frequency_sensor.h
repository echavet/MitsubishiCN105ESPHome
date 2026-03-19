#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"


namespace esphome {

    class CompressorFrequencySensor : public sensor::Sensor, public Component {
    public:
        CompressorFrequencySensor() = default;
    };

}
