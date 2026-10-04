#pragma once

#include <cstdint>
#include "esphome/core/version.h"

// Minimal ESPHome enums for exercising the production action mapping.
namespace esphome::climate {

enum ClimateMode : uint8_t {
    CLIMATE_MODE_OFF = 0,
    CLIMATE_MODE_HEAT_COOL = 1,
    CLIMATE_MODE_COOL = 2,
    CLIMATE_MODE_HEAT = 3,
    CLIMATE_MODE_FAN_ONLY = 4,
    CLIMATE_MODE_DRY = 5,
    CLIMATE_MODE_AUTO = 6,
};

enum ClimateAction : uint8_t {
    CLIMATE_ACTION_OFF = 0,
    CLIMATE_ACTION_COOLING = 2,
    CLIMATE_ACTION_HEATING = 3,
    CLIMATE_ACTION_IDLE = 4,
    CLIMATE_ACTION_DRYING = 5,
    CLIMATE_ACTION_FAN = 6,
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 3, 0)
    CLIMATE_ACTION_DEFROSTING = 7,
#endif
};

}  // namespace esphome::climate
