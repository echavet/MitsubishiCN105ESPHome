#include <gtest/gtest.h>
#include "climate_action.h"

using namespace esphome::climate;
using cn105_climate::action_from_sub_mode;

TEST(ClimateActionTest, DefrostOverridesHeatingCapableModes) {
    for (const auto mode : {CLIMATE_MODE_HEAT, CLIMATE_MODE_HEAT_COOL, CLIMATE_MODE_AUTO}) {
        SCOPED_TRACE(mode);
        const auto action = action_from_sub_mode(mode, "DEFROST");
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 3, 0)
        ASSERT_TRUE(action.has_value());
        EXPECT_EQ(*action, CLIMATE_ACTION_DEFROSTING);
#else
        // Older ESPHome has no defrost action; normal mode handling must run.
        EXPECT_FALSE(action.has_value());
#endif
    }
}

TEST(ClimateActionTest, DefrostDoesNotOverrideOtherModes) {
    for (const auto mode : {CLIMATE_MODE_OFF, CLIMATE_MODE_COOL, CLIMATE_MODE_DRY, CLIMATE_MODE_FAN_ONLY}) {
        SCOPED_TRACE(mode);
        EXPECT_FALSE(action_from_sub_mode(mode, "DEFROST").has_value());
    }
}

TEST(ClimateActionTest, OtherOrMissingSubModesUseNormalAction) {
    const char* sub_modes[] = {nullptr, "NORMAL", "WARMUP", "PREHEAT", "STANDBY", "OFF", "UNKNOWN", ""};
    for (const auto mode : {CLIMATE_MODE_HEAT, CLIMATE_MODE_HEAT_COOL, CLIMATE_MODE_AUTO}) {
        SCOPED_TRACE(mode);
        for (const char* sub_mode : sub_modes) {
            SCOPED_TRACE(sub_mode == nullptr ? "null" : sub_mode);
            EXPECT_FALSE(action_from_sub_mode(mode, sub_mode).has_value());
        }
    }
}
