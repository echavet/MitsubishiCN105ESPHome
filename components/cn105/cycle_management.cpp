#include "cycle_management.h"
#include "cn105.h"
#include "Globals.h"

bool cycleManagement::isCycleRunning() {
    return cycleRunning;
}

void cycleManagement::init() {
    cycleRunning = false;
    lastCompleteCycleMs = CUSTOM_MILLIS;
}

void cycleManagement::deferCycle() {

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
    uint32_t delay = DEFER_SCHEDULE_UPDATE_LOOP_DELAY * 3;
#else
    uint32_t delay = DEFER_SCHEDULE_UPDATE_LOOP_DELAY;
#endif

    ESP_LOGI(LOG_CYCLE_TAG, "Defering cycle trigger of %d ms", delay);
    // forces the lastCompleteCycle offset of delay ms to allow a longer rest time
    lastCompleteCycleMs = CUSTOM_MILLIS + delay;

}
void cycleManagement::cycleStarted() {
    ESP_LOGI(LOG_CYCLE_TAG, "1: Cycle start");
    lastCycleStartMs = CUSTOM_MILLIS;
    cycleRunning = true;
}

void cycleManagement::cycleEnded(bool timedOut) {
    cycleRunning = false;

    if (lastCompleteCycleMs < CUSTOM_MILLIS) {    // we check this because of defering mecanism
        // a complete cycle is done
        lastCompleteCycleMs = CUSTOM_MILLIS;      // to prevent next inteval from ticking too soon
    }

    ESP_LOGI(LOG_CYCLE_TAG, "6: Cycle ended in %.1f seconds (with timeout?: %s)",
        (lastCompleteCycleMs - lastCycleStartMs) / 1000.0, timedOut ? "YES" : " NO");
}

bool cycleManagement::hasUpdateIntervalPassed(unsigned int update_interval) {
    return (CUSTOM_MILLIS - lastCompleteCycleMs) > update_interval;
}

bool cycleManagement::doesCycleTimeOut(unsigned int update_interval) {
    return (CUSTOM_MILLIS - lastCycleStartMs) > (5 * update_interval) + 6000;
}