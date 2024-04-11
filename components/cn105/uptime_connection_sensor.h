#pragma once
#include "esphome/components/uptime/uptime_sensor.h"

namespace esphome {
    namespace uptime {

        class HpUpTimeConnectionSensor : public UptimeSensor {
        public:
            void update() override;
            std::string unique_id() override;
            void dump_config() override;

            void start();           // connection established
            void stop();            // connection lost
        protected:
            bool connected_{ false };
        };
    }  // namespace uptime
}  // namespace esphome