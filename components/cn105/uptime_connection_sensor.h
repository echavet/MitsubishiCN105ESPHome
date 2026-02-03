#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
    namespace cn105 {

        class HpUpTimeConnectionSensor : public sensor::Sensor, public PollingComponent {
        public:
            void update() override;
            std::string unique_id();
            void dump_config() override;

            void start();           // connection established
            void stop();            // connection lost
        protected:
            bool connected_{ false };
            uint64_t uptime_accumulator_{ 0 };
            uint64_t start_time_{ 0 };
            
            uint64_t get_uptime_ms_();
        };
    }  // namespace cn105
}  // namespace esphome
