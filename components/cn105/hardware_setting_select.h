#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"
#include <map>
#include <vector>
#include <functional>
#include <string>

namespace esphome {

    class HardwareSettingSelect : public select::Select, public Component {
    public:
        using CallbackFunction = std::function<void(const std::string& value, int int_value)>;

        HardwareSettingSelect(int code, const std::map<int, std::string>& options);

        void setCallbackFunction(CallbackFunction&& callback);

        int get_code() const;

        void update_state_from_value(int value);

        void set_enabled(bool enabled);

        bool is_available();

    protected:
        void control(const std::string& value) override;

        int code_;
        std::map<int, std::string> mapping_;
        std::map<std::string, int> reverse_mapping_;
        CallbackFunction callback_;
        bool enabled_{ true };
    };

}  // namespace esphome
