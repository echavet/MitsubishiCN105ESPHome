#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"
#include "uptime_connection_sensor.h"

namespace esphome {
    namespace uptime {

        static const char* const TAG = "uptime.connection.sensor";
        void HpUpTimeConnectionSensor::update() {
            if (connected_) {
                UptimeSecondsSensor::update();
            } else {
                this->uptime_ = 0;
                this->publish_state(0);
            }
        }

        void HpUpTimeConnectionSensor::start() {
            this->uptime_ = 0;
            this->connected_ = true;
            this->update();
        }

        void HpUpTimeConnectionSensor::stop() {
            this->connected_ = false;
            this->update();
        }

        std::string HpUpTimeConnectionSensor::unique_id() { return get_mac_address() + "-uptime-hp_connection"; }
        void HpUpTimeConnectionSensor::dump_config() { LOG_SENSOR("", "Uptime Connection Sensor", this); }

    }  // namespace uptime
}  // namespace esphome
