#pragma once

#include <algorithm>
#include <vector>

namespace esphome {
    enum class FahrenheitMode {
        OFF = 0,
        STANDARD = 1,
        ALT = 2
    };

    class FahrenheitSupport {
    public:
        void setUseFahrenheitSupportMode(FahrenheitMode mode) {
            fahrenheit_mode_ = mode;
        }

        float normalizeHeatpumpTemperatureToUiTemperature(const float c) {
            if (fahrenheit_mode_ == FahrenheitMode::OFF || c == NAN) {
                return c; // If disabled, return the Celsius value as is.
            }

            // Select the appropriate conversion table based on mode
            const std::vector<std::pair<float, float>>& localTable =
                (fahrenheit_mode_ == FahrenheitMode::ALT) ? fahrenheitToCelsiusTableAlt : fahrenheitToCelsiusTable;

            auto it = std::upper_bound(
                localTable.begin(),
                localTable.end(),
                std::make_pair(0.0f, c),
                [](const std::pair<float, float>& a, const std::pair<float, float>& b) {
                    return a.second < b.second;  // compare Celsius
                }
            );

            if (it == localTable.begin() || it == localTable.end()) {
                return c;
            }

            auto prev = it;
            --prev;
            float fahrenheitResult = std::abs(prev->second - c) < std::abs(it->second - c) ? prev->first : it->first;

            return (fahrenheitResult - 32.0f) / 1.8f;
        }

        float normalizeUiTemperatureToHeatpumpTemperature(const float c) {
            if (fahrenheit_mode_ == FahrenheitMode::OFF || c == NAN) {
                return c; // If disabled, return the Celsius value as is.
            }

            // Select the appropriate conversion table based on mode
            const std::vector<std::pair<float, float>>& localTable =
                (fahrenheit_mode_ == FahrenheitMode::ALT) ? fahrenheitToCelsiusTableAlt : fahrenheitToCelsiusTable;

            float fahrenheitInput = (c * 1.8f) + 32.0f;

            // Due to vagaries of floating point math across architectures, we can't
            // just look up `c` in the map -- we're very unlikely to find a matching
            // value. Instead, we find the first value greater than `c`, and the
            // next-lowest value in the map. We return whichever `c` is closer to.
            auto it = std::upper_bound(localTable.begin(), localTable.end(), std::make_pair(fahrenheitInput, 0.0f),
                [](const std::pair<float, float>& a, const std::pair<float, float>& b) {
                    return a.first < b.first;
                });
            if (it == localTable.begin() || it == localTable.end()) {
                return c;
            }

            auto prev = it;
            --prev;

            return std::abs(prev->first - fahrenheitInput) < std::abs(it->first - fahrenheitInput) ? prev->second : it->second;
        }

    private:
        FahrenheitMode fahrenheit_mode_ = FahrenheitMode::OFF;

        // Given a temperature in Celsius that was converted from Fahrenheit, converts
        // it to the Celsius value (at half-degree precision) that matches what
        // Mitsubishi thermostats would have converted the Fahrenheit value to. For
        // instance, 72°F is 22.22°C, but this class returns 22.5°C.
        const std::vector<std::pair<float, float>> fahrenheitToCelsiusTable = {
            {61, 16.0}, {62, 16.5}, {63, 17.0}, {64, 17.5}, {65, 18.0},
            {66, 18.5}, {67, 19.0}, {68, 20.0}, {69, 21.0}, {70, 21.5},
            {71, 22.0}, {72, 22.5}, {73, 23.0}, {74, 23.5}, {75, 24.0},
            {76, 24.5}, {77, 25.0}, {78, 25.5}, {79, 26.0}, {80, 26.5},
            {81, 27.0}, {82, 27.5}, {83, 28.0}, {84, 28.5}, {85, 29.0},
            {86, 29.5}, {87, 30.0}, {88, 30.5}
        };
        const std::vector<std::pair<float, float>> fahrenheitToCelsiusTableAlt = {
            {61, 16.0}, {62, 16.5}, {63, 17.0}, {64, 18.0}, {65, 18.5},
            {66, 19.0}, {67, 19.5}, {68, 20.0}, {69, 20.5}, {70, 21.0},
            {71, 21.5}, {72, 22.0}, {73, 23.0}, {74, 23.5}, {75, 24.0},
            {76, 24.5}, {77, 25.0}, {78, 25.5}, {79, 26.0}, {80, 26.5},
            {81, 27.0}, {82, 28.0}, {83, 28.5}, {84, 29.0}, {85, 29.5},
            {86, 30.0}, {87, 30.5}, {88, 31.0}
        };
    };
}
