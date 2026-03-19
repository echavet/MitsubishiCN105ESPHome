#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"


namespace esphome {

    class OutsideAirTemperatureSensor : public sensor::Sensor, public Component {
    public:
        OutsideAirTemperatureSensor() = default;
    };

}
