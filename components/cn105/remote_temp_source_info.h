#pragma once

#include "esphome/components/text_sensor/text_sensor.h"
#include "esphome/core/component.h"


namespace esphome {

    class RemoteTempSourceInfo : public text_sensor::TextSensor, public Component {
    public:
        RemoteTempSourceInfo() {}
    };

}
