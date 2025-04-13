#pragma once

#include "esphome/components/button/button.h"
#include "esphome/core/component.h"

namespace esphome {

    class FunctionsButton : public button::Button, public Component {
    public:
        using CallbackFunction = std::function<void()>;

        FunctionsButton() {}

        // This callback function links the button press to the Climate component
        void setCallbackFunction(CallbackFunction&& callback) {
            this->callBackFunction = std::move(callback);
        }

    protected:
        void press_action() override {
            if (callBackFunction) {
                callBackFunction(); // Trigger the callback function
            }
        }

    private:
        CallbackFunction callBackFunction;

    };

}