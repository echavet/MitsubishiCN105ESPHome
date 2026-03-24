#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"


namespace esphome {

    class InputPowerSensor : public sensor::Sensor, public Component {
    public:
        InputPowerSensor() = default;
    };

}
