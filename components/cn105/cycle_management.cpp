#include "cn105.h"


bool CN105Climate::isCycleRunning() {
    return cycleRunning;
}

void CN105Climate::deferCycle() {

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_DEBUG
    uint32_t delay = DEFER_SCHEDULE_UPDATE_LOOP_DELAY * 3;
#else
    uint32_t delay = DEFER_SCHEDULE_UPDATE_LOOP_DELAY;
#endif

    ESP_LOGI(LOG_CYCLE_TAG, "Defering cycle trigger of %d ms", delay);
    // forces the lastCompleteCycle offset of delay ms to allow a longer rest time
    this->lastCompleteCycle += delay;

}
void CN105Climate::cycleStarted() {
    ESP_LOGI(LOG_CYCLE_TAG, "1: Cycle start");
    this->lastRequestInfo = CUSTOM_MILLIS;
    cycleRunning = true;
}
void CN105Climate::cycleEnded() {
    ESP_LOGI(LOG_CYCLE_TAG, "6: Cycle ends");
    cycleRunning = false;
    // a complete cycle is done
    this->lastCompleteCycle = CUSTOM_MILLIS;
}

bool CN105Climate::hasUpdateIntervalPassed() {
    return (CUSTOM_MILLIS - this->lastCompleteCycle) > this->update_interval_;
}

bool CN105Climate::didCycleTimeOut() {
    return (CUSTOM_MILLIS - this->lastRequestInfo) > (5 * this->update_interval_) + 4000;
}