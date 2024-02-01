#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"


namespace esphome {

    class ISeeSensor : public binary_sensor::BinarySensor, public Component {
    public:
        ISeeSensor() {}
    };

}