#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"

namespace esphome {

    class FunctionsNumber : public number::Number, public Component {
    public:
        using CallbackFunction = std::function<void(float number)>;
        FunctionsNumber() {}

        // This callback function links the number changes back to the Climate component
        void setCallbackFunction(CallbackFunction&& callback) {
            this->callBackFunction = std::move(callback);
        }

    protected:
        void control(float x) override { // called when the number changes
            if (callBackFunction) {
                callBackFunction(x); // Trigger the callback function
            }
        }

    private:
        CallbackFunction callBackFunction;
    };
}