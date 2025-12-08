#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include <map>
#include <vector>

namespace esphome {

    class HardwareSettingSelect : public select::Select, public Component {
    public:
        using CallbackFunction = std::function<void(const std::string& value, int int_value)>;

        HardwareSettingSelect(int code, const std::map<int, std::string>& options)
            : code_(code), mapping_(options) {
            std::vector<std::string> opts;
            for (auto const& [val, label] : options) {
                opts.push_back(label);
                reverse_mapping_[label] = val;
            }
            this->traits.set_options(opts);
        }

        void setCallbackFunction(CallbackFunction&& callback) {
            this->callback_ = std::move(callback);
        }

        int get_code() const { return code_; }

        void update_state_from_value(int value) {
            if (this->mapping_.count(value)) {
                std::string new_state = this->mapping_[value];
                if (this->state != new_state) {
                    this->publish_state(new_state);
                }
            }
        }

    protected:
        void control(const std::string& value) override {
            if (this->reverse_mapping_.count(value)) {
                int int_value = this->reverse_mapping_[value];
                if (callback_) {
                    callback_(value, int_value);
                }
            }
        }

        int code_;
        std::map<int, std::string> mapping_;
        std::map<std::string, int> reverse_mapping_;
        CallbackFunction callback_;
    };

}  // namespace esphome

