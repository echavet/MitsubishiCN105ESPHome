#pragma once

#include "esphome/components/select/select.h"
#include "esphome/core/component.h"

namespace esphome {

    namespace cn105 {
        class VaneOrientationSelect : public select::Select, public Component {
        public:

            using CallbackFunction = std::function<void(const char* setting)>;


            void setCallbackFunction(CallbackFunction&& callback) {
                this->callBackFunction = std::move(callback);

            }


        protected:
            void control(const std::string& value) override {
                if (callBackFunction) {
                    callBackFunction(value.c_str()); // should be enough to trigger a sendWantedSettings
                }
            }

        private:
            CallbackFunction callBackFunction;

        };

    }
}// namespace esphome