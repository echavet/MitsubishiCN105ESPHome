#include "cn105_mqtt_mim.h"
#include "cn105.h"  // Include pour définition complète de CN105Climate
#include "esphome/components/mqtt/mqtt_client.h"
#include "esphome/core/log.h"

namespace esphome {
    namespace cn105 {

        static const char* const MIM_TAG = "cn105_mqtt_mim";

        void CN105MqttMim::setup() {
            auto* mqtt = mqtt::global_mqtt_client;
            if (mqtt == nullptr) {
                ESP_LOGW(MIM_TAG, "MQTT client not initialized");
                return;
            }
            mqtt->subscribe(trace_topic_, [this](const std::string& topic, const std::string& payload) {
                this->on_melcloud_command_received_(topic, payload);
                });
        }

        void CN105MqttMim::on_melcloud_command_received_(const std::string& topic, const std::string& payload) {
            ESP_LOGD(MIM_TAG, "Commande Melcloud reçue sur %s: %s", topic.c_str(), payload.c_str());

            std::vector<uint8_t> trame;
            for (size_t i = 0; i < payload.length(); i += 2) {
                trame.push_back(std::stoi(payload.substr(i, 2), nullptr, 16));
            }
            if (parent_ == nullptr) {
                ESP_LOGW(MIM_TAG, "Parent not set");
                return;
            }
            parent_->activate_passive_mode();
            parent_->send_raw_hvac_command(trame);
            mqtt::global_mqtt_client->publish(trace_topic_, "RX: " + payload);
            forward_buffer_.insert(forward_buffer_.end(), trame.begin(), trame.end());
            if (forward_buffer_.size() > forward_buffer_size_ / 2) {
                parent_->send_raw_hvac_command(forward_buffer_);
                mqtt::global_mqtt_client->publish(trace_topic_, "TX: " + payload);
                forward_buffer_.clear();
            }
        }

        void CN105MqttMim::on_hvac_response_received(const std::vector<uint8_t>& data) {
            std::string hex;
            for (uint8_t byte : data) {
                char buf[3];
                snprintf(buf, sizeof(buf), "%02X", byte);
                hex += buf;
            }
            ESP_LOGD(MIM_TAG, "Réponse HVAC reçue: %s", hex.c_str());
            mqtt::global_mqtt_client->publish(trace_topic_, "RX: " + hex);
        }

        void CN105MqttMim::dump_config() {
            ESP_LOGCONFIG(MIM_TAG, "CN105MqttMim:");
            ESP_LOGCONFIG(MIM_TAG, "  Topic Prefix: %s", trace_topic_.c_str());
            ESP_LOGCONFIG(MIM_TAG, "  Forward Buffer Size: %u", forward_buffer_size_);
        }

    }  // namespace cn105
}  // namespace esphome