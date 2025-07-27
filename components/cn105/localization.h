#pragma once

#include <algorithm>
#include <vector>

namespace esphome {
    class FahrenheitSupport {
        public:
            void setUseFahrenheitSupportMode(bool value) {
                use_fahrenheit_support_mode_ = value;
            }

            float normalizeCelsiusForConversionToFahrenheit(const float c) {
                if (!use_fahrenheit_support_mode_) {
                    return c; // If not in Fahrenheit support mode, return the Celsius value as is.
                }

                auto it = std::upper_bound(
                    fahrenheitToCelsiusTable.begin(),
                    fahrenheitToCelsiusTable.end(),
                    std::make_pair(0.0f, c),
                    [](const std::pair<float, float>& a, const std::pair<float, float>& b) {
                        return a.second < b.second;  // compare Celsius
                    }
                );

                if (it == fahrenheitToCelsiusTable.begin() || it == fahrenheitToCelsiusTable.end()) {
                    return c;
                }

                auto prev = it;
                --prev;
                float fahrenheitResult = std::abs(prev->second - c) < std::abs(it->second - c) ? prev->first : it->first;

                return (fahrenheitResult - 32.0f) / 1.8f;
            }

            float normalizeCelsiusForConversionFromFahrenheit(const float c) {
                if (!use_fahrenheit_support_mode_) {
                    return c; // If not in Fahrenheit support mode, return the Celsius value as is.
                }

                float fahrenheitInput = (c * 1.8f) + 32.0f;

                // Due to vagaries of floating point math across architectures, we can't
                // just look up `c` in the map -- we're very unlikely to find a matching
                // value. Instead, we find the first value greater than `c`, and the
                // next-lowest value in the map. We return whichever `c` is closer to.
                auto it = std::upper_bound(fahrenheitToCelsiusTable.begin(), fahrenheitToCelsiusTable.end(), std::make_pair(fahrenheitInput, 0.0f),
                    [](const std::pair<float, float>& a, const std::pair<float, float>& b) {
                        return a.first < b.first;
                    });
                if (it == fahrenheitToCelsiusTable.begin() || it == fahrenheitToCelsiusTable.end()) {
                    return c;
                }

                auto prev = it;
                --prev;

                return std::abs(prev->first - fahrenheitInput) < std::abs(it->first - fahrenheitInput) ? prev->second : it->second;
            }

        private:
            bool use_fahrenheit_support_mode_ = false;
            
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
    };
}