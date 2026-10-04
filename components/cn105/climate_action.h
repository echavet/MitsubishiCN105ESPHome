#pragma once

#include <cstring>
#include <optional>
#include "esphome/components/climate/climate_mode.h"
#include "esphome/core/version.h"

namespace cn105_climate {

// No override means updateAction() should derive the action from the HVAC mode.
inline std::optional<esphome::climate::ClimateAction> action_from_sub_mode(
    esphome::climate::ClimateMode mode, const char* sub_mode) {
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 3, 0)
    using namespace esphome::climate;
    if ((mode == CLIMATE_MODE_HEAT || mode == CLIMATE_MODE_HEAT_COOL ||
         mode == CLIMATE_MODE_AUTO) &&
        sub_mode != nullptr && std::strcmp(sub_mode, "DEFROST") == 0) {
        return CLIMATE_ACTION_DEFROSTING;
    }
#endif
    return std::nullopt;
}

}  // namespace cn105_climate
