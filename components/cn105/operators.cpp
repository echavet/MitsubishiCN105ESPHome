#include "cn105.h"
//#region operators
bool operator==(const heatpumpSettings& lhs, const heatpumpSettings& rhs) {
    return lhs.power == rhs.power &&
        lhs.mode == rhs.mode &&
        lhs.temperature == rhs.temperature &&
        lhs.fan == rhs.fan &&
        lhs.vane == rhs.vane &&
        lhs.wideVane == rhs.wideVane &&
        lhs.iSee == rhs.iSee;
}

bool operator!=(const heatpumpSettings& lhs, const heatpumpSettings& rhs) {
    return lhs.power != rhs.power ||
        lhs.mode != rhs.mode ||
        lhs.temperature != rhs.temperature ||
        lhs.fan != rhs.fan ||
        lhs.vane != rhs.vane ||
        lhs.wideVane != rhs.wideVane ||
        lhs.iSee != rhs.iSee;
}

bool operator!(const heatpumpSettings& settings) {
    return !settings.power &&
        !settings.mode &&
        !settings.temperature &&
        !settings.fan &&
        !settings.vane &&
        !settings.wideVane &&
        !settings.iSee;
}

bool operator==(const heatpumpTimers& lhs, const heatpumpTimers& rhs) {
    return lhs.mode == rhs.mode &&
        lhs.onMinutesSet == rhs.onMinutesSet &&
        lhs.onMinutesRemaining == rhs.onMinutesRemaining &&
        lhs.offMinutesSet == rhs.offMinutesSet &&
        lhs.offMinutesRemaining == rhs.offMinutesRemaining;
}

bool operator!=(const heatpumpTimers& lhs, const heatpumpTimers& rhs) {
    return lhs.mode != rhs.mode ||
        lhs.onMinutesSet != rhs.onMinutesSet ||
        lhs.onMinutesRemaining != rhs.onMinutesRemaining ||
        lhs.offMinutesSet != rhs.offMinutesSet ||
        lhs.offMinutesRemaining != rhs.offMinutesRemaining;
}
//#endregion operators