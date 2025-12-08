#include "hardware_setting_select.h"
#include "Globals.h"
#include "esphome/core/log.h"

namespace esphome {

    HardwareSettingSelect::HardwareSettingSelect(int code, const std::map<int, std::string>& options)
        : code_(code), mapping_(options) {
        for (auto const& [val, label] : options) {
            reverse_mapping_[label] = val;
        }
    }

    void HardwareSettingSelect::setCallbackFunction(CallbackFunction&& callback) {
        this->callback_ = std::move(callback);
    }

    int HardwareSettingSelect::get_code() const { return code_; }

    void HardwareSettingSelect::update_state_from_value(int value) {
        ESP_LOGD(LOG_HARDWARE_SELECT_TAG, "Code %d update request with value: %d", this->code_, value);
        if (this->mapping_.count(value)) {
            std::string new_state = this->mapping_[value];
            if (this->state != new_state) {
                ESP_LOGI(LOG_HARDWARE_SELECT_TAG, "Code %d state changed: %s -> %s (val: %d)", this->code_, this->state.c_str(), new_state.c_str(), value);
                this->publish_state(new_state);
            } else {
                ESP_LOGD(LOG_HARDWARE_SELECT_TAG, "Code %d state unchanged: %s (val: %d)", this->code_, new_state.c_str(), value);
            }
        } else {
            ESP_LOGW(LOG_HARDWARE_SELECT_TAG, "Code %d received unknown value: %d", this->code_, value);
        }
    }

    void HardwareSettingSelect::control(const std::string& value) {
        ESP_LOGD(LOG_HARDWARE_SELECT_TAG, "Code %d control request: %s", this->code_, value.c_str());
        if (this->reverse_mapping_.count(value)) {
            int int_value = this->reverse_mapping_[value];
            if (callback_) {
                ESP_LOGD(LOG_HARDWARE_SELECT_TAG, "Code %d calling callback with val: %d", this->code_, int_value);
                callback_(value, int_value);
            } else {
                ESP_LOGW(LOG_HARDWARE_SELECT_TAG, "Code %d no callback registered!", this->code_);
            }
        } else {
            ESP_LOGW(LOG_HARDWARE_SELECT_TAG, "Code %d control value not found in mapping: %s", this->code_, value.c_str());
        }
    }

}  // namespace esphome

