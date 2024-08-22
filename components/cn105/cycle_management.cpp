#include "cycle_management.h"
#include "cn105.h"
#include "Globals.h"



void cycleManagement::checkTimeout(unsigned int update_interval) {
    if (doesCycleTimeOut(update_interval)) {                          // does it last too long ?                    
        ESP_LOGW(TAG, "Cycle timeout, reseting cycle...");
        cycleEnded(true);
    }
}


bool cycleManagement::isCycleRunning() {
    return cycleRunning;
}

void cycleManagement::init() {
    cycleRunning = false;
    lastCompleteCycleMs = CUSTOM_MILLIS;
}

void cycleManagement::deferCycle() {

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
    uint32_t delay = DEFER_SCHEDULE_UPDATE_LOOP_DELAY * 2;
#else
    uint32_t delay = DEFER_SCHEDULE_UPDATE_LOOP_DELAY;
#endif

    //ESP_LOGI(LOG_CYCLE_TAG, "Defering cycle trigger of %lu ms", delay);
    log_info_uint32(LOG_CYCLE_TAG, "Defering cycle trigger of  ", delay, " ms");
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
    if (CUSTOM_MILLIS < lastCompleteCycleMs) return false;      // must be checked because operands are they are unsigned
    return (CUSTOM_MILLIS - lastCompleteCycleMs) > update_interval;
}

bool cycleManagement::doesCycleTimeOut(unsigned int update_interval) {
    if (CUSTOM_MILLIS < lastCycleStartMs) return false;         // must be checked because operands are they are unsigned
    return (CUSTOM_MILLIS - lastCycleStartMs) > (2 * update_interval) + 1000;
}