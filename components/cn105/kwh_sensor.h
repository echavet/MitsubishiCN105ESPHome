#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"


namespace esphome {

    class kWhSensor : public sensor::Sensor, public Component {
    public:
        kWhSensor() = default;
    };

}
