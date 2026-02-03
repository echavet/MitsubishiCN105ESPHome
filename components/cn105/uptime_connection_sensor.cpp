#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "uptime_connection_sensor.h"

namespace esphome {
    namespace cn105 {

        static const char* const TAG = "uptime.connection.sensor";
        
        void HpUpTimeConnectionSensor::update() {
            uint64_t now = this->get_uptime_ms_();

            if (this->connected_) {
                float seconds = (now - this->start_time_) / 1000.0f;
                this->publish_state(seconds);
            } else {
                this->publish_state(0);
            }
        }

        uint64_t HpUpTimeConnectionSensor::get_uptime_ms_() {
            const uint32_t ms = millis();
            const uint64_t ms_mask = (1ULL << 32) - 1ULL;
            const uint32_t last_ms = this->uptime_accumulator_ & ms_mask;
            if (ms < last_ms) {
                this->uptime_accumulator_ += ms_mask + 1ULL;
                ESP_LOGD(TAG, "Detected roll-over \xf0\x9f\xa6\x84");
            }
            this->uptime_accumulator_ &= ~ms_mask;
            this->uptime_accumulator_ |= ms;
            return this->uptime_accumulator_;
        }

        void HpUpTimeConnectionSensor::start() {
            this->start_time_ = this->get_uptime_ms_();
            this->connected_ = true;
            this->update();
        }

        void HpUpTimeConnectionSensor::stop() {
            this->connected_ = false;
            this->update();
        }

        std::string HpUpTimeConnectionSensor::unique_id() { return get_mac_address() + "-uptime-hp_connection"; }
        void HpUpTimeConnectionSensor::dump_config() { LOG_SENSOR("", "Uptime Connection Sensor", this); }

    }  // namespace cn105
}  // namespace esphome
