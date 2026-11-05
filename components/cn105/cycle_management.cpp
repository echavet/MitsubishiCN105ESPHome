#include "cycle_management.h"
#include "cn105.h"
#include "Globals.h"

using namespace esphome;

void cycleManagement::checkTimeout(unsigned int update_interval) {
    if (doesCycleTimeOut(update_interval)) {                          // does it last too long ?                    
        ESP_LOGW(TAG, "Cycle timeout, resetting cycle...");
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

    // signed delta is millis()-rollover safe; > 0 means the deferred timestamp is in the past
    if ((int32_t)(CUSTOM_MILLIS - lastCompleteCycleMs) > 0) {    // we check this because of defering mecanism
        // a complete cycle is done
        lastCompleteCycleMs = CUSTOM_MILLIS;      // to prevent next inteval from ticking too soon
    }

    ESP_LOGI(LOG_CYCLE_TAG, "6: Cycle ended in %.1f seconds (with timeout?: %s)",
        (lastCompleteCycleMs - lastCycleStartMs) / 1000.0, timedOut ? "YES" : " NO");

}

bool cycleManagement::hasUpdateIntervalPassed(unsigned int update_interval) {
    // signed delta handles both millis() rollover (~49.7 days) and a deferred (future) lastCompleteCycleMs
    int32_t elapsed = (int32_t)(CUSTOM_MILLIS - lastCompleteCycleMs);
    return elapsed > (int32_t)update_interval;
}

bool cycleManagement::doesCycleTimeOut(unsigned int update_interval) {
    int32_t elapsed = (int32_t)(CUSTOM_MILLIS - lastCycleStartMs);   // rollover safe
    return elapsed > (int32_t)((2 * update_interval) + 1000);
}
