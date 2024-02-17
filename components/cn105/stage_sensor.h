#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"


namespace esphome {

    class StageSensor : public text_sensor::TextSensor, public Component {
    public:
        StageSensor() {}
    };

}