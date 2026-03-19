#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"


namespace esphome {

    class RuntimeHoursSensor : public sensor::Sensor, public Component {
    public:
        RuntimeHoursSensor() = default;
    };

}
