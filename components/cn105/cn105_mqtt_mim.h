#pragma once

#include "esphome/core/component.h"
#include "esphome/components/mqtt/mqtt_client.h"
#include <vector>

namespace esphome {
    namespace cn105 {

        class CN105Climate;  // Forward declaration

        class CN105MqttMim : public Component {
        public:
            void set_parent(CN105Climate* parent) { this->parent_ = parent; }
            void set_topic_prefix(const std::string& topic_prefix) { this->trace_topic_ = topic_prefix; }
            void set_mim_forward_buffer_size(uint32_t size) { this->forward_buffer_size_ = size; }

            void setup() override;
            void dump_config() override;
            float get_setup_priority() const override { return setup_priority::LATE; }
            void on_hvac_response_received(const std::vector<uint8_t>& data);

        protected:
            void on_mim_command_received_(const std::string& topic, const std::string& payload);  // Renommé pour clarté

            CN105Climate* parent_ = nullptr;
            std::string trace_topic_ = "debug/cn105/trames";
            uint32_t forward_buffer_size_ = 256;
            std::vector<uint8_t> forward_buffer_;
        };

    }  // namespace cn105
}  // namespace esphome