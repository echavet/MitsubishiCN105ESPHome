#pragma once
#include <cstdint>
#include "esphome/core/version.h"

// ESPHome's public enum values. Tests exercise production profiles, not copied algorithms.
namespace esphome::climate {
enum ClimateMode : uint8_t {
    CLIMATE_MODE_OFF = 0, CLIMATE_MODE_HEAT_COOL = 1, CLIMATE_MODE_COOL = 2,
    CLIMATE_MODE_HEAT = 3, CLIMATE_MODE_FAN_ONLY = 4, CLIMATE_MODE_DRY = 5, CLIMATE_MODE_AUTO = 6
};
enum ClimateFanMode : uint8_t {
    CLIMATE_FAN_ON = 0, CLIMATE_FAN_OFF = 1, CLIMATE_FAN_AUTO = 2,
    CLIMATE_FAN_LOW = 3, CLIMATE_FAN_MEDIUM = 4, CLIMATE_FAN_HIGH = 5,
    CLIMATE_FAN_MIDDLE = 6, CLIMATE_FAN_FOCUS = 7, CLIMATE_FAN_DIFFUSE = 8,
    CLIMATE_FAN_QUIET = 9
};
enum ClimateAction : uint8_t {
    CLIMATE_ACTION_OFF = 0, CLIMATE_ACTION_COOLING = 2, CLIMATE_ACTION_HEATING = 3,
    CLIMATE_ACTION_IDLE = 4, CLIMATE_ACTION_DRYING = 5, CLIMATE_ACTION_FAN = 6,
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 3, 0)
    CLIMATE_ACTION_DEFROSTING = 7
#endif
};
}
