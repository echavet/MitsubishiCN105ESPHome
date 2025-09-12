#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"


namespace esphome {
    namespace cn105 {
        class StageSensor : public text_sensor::TextSensor, public Component {
        public:
            StageSensor() {}
        };

    }
}