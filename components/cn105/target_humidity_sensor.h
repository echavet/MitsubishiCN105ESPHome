// Diagnostic sensor: exposes the target humidity byte (data[12] of 0x02 packet).
// Depends on: esphome/components/sensor
#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"


namespace esphome {

    class TargetHumiditySensor : public sensor::Sensor, public Component {
    public:
        TargetHumiditySensor() = default;
    };

}
