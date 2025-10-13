#include "cn105_mqtt_mim.h"
#include "cn105.h"  // Pour CN105Climate
#include "esphome/core/log.h"
#include <sstream>
#include <iomanip>

namespace esphome {
    namespace cn105 {

        static const char* const TAG_MIM = "cn105.mqtt_mim";

        void CN105MqttMim::setup() {
            if (mqtt::global_mqtt_client != nullptr) {
                std::string subscribe_topic = this->trace_topic_ + "/to_hvac";  // Suffixe pour écoute
                mqtt::global_mqtt_client->subscribe(subscribe_topic, [this](const std::string& topic, const std::string& payload) {
                    this->on_mim_command_received_(topic, payload);
                    });
                ESP_LOGCONFIG(TAG_MIM, "Souscription au topic MIM: %s", subscribe_topic.c_str());
            }
        }

        void CN105MqttMim::dump_config() {
            ESP_LOGCONFIG(TAG_MIM, "CN105 MQTT MIM:");
            ESP_LOGCONFIG(TAG_MIM, "  Topic de base: %s", this->trace_topic_.c_str());
            ESP_LOGCONFIG(TAG_MIM, "  Buffer size: %u", this->forward_buffer_size_);
        }

        void CN105MqttMim::on_mim_command_received_(const std::string& topic, const std::string& payload) {
            // Parser le payload en bytes (ex. hex string to vector<uint8_t>)
            std::vector<uint8_t> data;
            std::stringstream ss(payload);
            std::string byte_str;
            while (std::getline(ss, byte_str, ' ')) {  // Assumant payload comme "FC 41 ..." (hex séparé par espaces)
                uint8_t byte = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
                data.push_back(byte);
            }

            if (this->parent_ != nullptr) {
                this->parent_->send_raw_hvac_command(data);  // Forward vers le climatiseur
                this->parent_->activate_passive_mode();  // Réinitialise le timer du mode passif
                ESP_LOGD(TAG_MIM, "Commande MIM reçue et forwardée. Mode passif réinitialisé.");
            }
        }

        void CN105MqttMim::on_hvac_response_received(const std::vector<uint8_t>& data) {
            if (this->parent_ == nullptr || !this->parent_->is_in_passive_mode()) {
                ESP_LOGD(TAG_MIM, "Publication MIM ignorée car mode passif expiré ou parent non défini.");
                return;  // Ne publie pas si timeout expiré
            }

            // Construire le payload (ex. hex string)
            std::stringstream ss;
            for (size_t i = 0; i < data.size(); ++i) {
                ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(data[i]);
                if (i < data.size() - 1) ss << " ";
            }
            std::string payload = ss.str();

            std::string publish_topic = this->trace_topic_ + "/from_hvac";  // Suffixe pour publication
            mqtt::global_mqtt_client->publish(publish_topic, payload);
            ESP_LOGD(TAG_MIM, "Réponse HVAC publiée sur %s (mode passif actif).", publish_topic.c_str());
        }

    }  // namespace cn105
}  // namespace esphome