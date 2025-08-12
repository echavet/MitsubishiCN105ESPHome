#pragma once

#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

namespace esphome {

    class HVACOptionSwitch : public switch_::Switch, public Component {
    public:
        
        using CallbackFunction = std::function<void(bool state)>;

        //HVACOptionSwitch() {}

        // This callback function links the button press to the Climate component
        void setCallbackFunction(CallbackFunction&& callback) {
            this->callBackFunction = std::move(callback);
        }

    protected:
        void write_state(bool state) override {
            if (callBackFunction) {
                callBackFunction(state); // Trigger the callback function
            }
        }

    private:
        CallbackFunction callBackFunction;

    };

}