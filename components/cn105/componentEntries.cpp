#include "cn105.h"

using namespace esphome;

/**
 * This method is call by the esphome framework to initialize the component
 * We don't try to connect to the heater here because errors could not be logged fine because the
 * UART is used for communication with the heatpump
 * setupUART will handle the
*/
void CN105Climate::setup() {

    ESP_LOGD(TAG, "Component initialization: setup call");
    this->current_temperature = NAN;

    this->target_temperature = NAN;

    this->fan_mode = climate::CLIMATE_FAN_OFF;
    this->swing_mode = climate::CLIMATE_SWING_OFF;
    this->initBytePointer();
    this->lastResponseMs = CUSTOM_MILLIS;

    // initialize diagnostic stats
    this->nbCompleteCycles_ = 0;
    this->nbCycles_ = 0;
    this->nbHeatpumpConnections_ = 0;

    ESP_LOGI(TAG, "tx_pin: %d rx_pin: %d", this->tx_pin_, this->rx_pin_);
    //ESP_LOGI(TAG, "remote_temp_timeout is set to %lu", this->remote_temp_timeout_);
    log_info_uint32(TAG, "remote_temp_timeout is set to ", this->remote_temp_timeout_);
    //ESP_LOGI(TAG, "debounce_delay is set to %lu", this->debounce_delay_);
    log_info_uint32(TAG, "debounce_delay is set to ", this->debounce_delay_);

    this->setupUART();
    this->sendFirstConnectionPacket();
}


/**
 * @brief Executes the main loop for the CN105Climate component.
 * This function is called repeatedly in the main program loop.
 */
void CN105Climate::loop() {
    if (!this->processInput()) {                                            // if we don't get any input: no read op
        if ((this->wantedSettings.hasChanged) && (!this->loopCycle.isCycleRunning())) {
            this->checkPendingWantedSettings();
        } else if ((this->wantedRunStates.hasChanged) && (!this->loopCycle.isCycleRunning())) {
            this->checkPendingWantedRunStates();
        }else {
            if (this->loopCycle.isCycleRunning()) {                         // if we are  running an update cycle
                this->loopCycle.checkTimeout(this->update_interval_);
            } else { // we are not running a cycle
                if (this->loopCycle.hasUpdateIntervalPassed(this->get_update_interval())) {
                    this->buildAndSendRequestsInfoPackets();            // initiate an update cycle with this->cycleStarted();
                }
            }
        }
    }
}

uint32_t CN105Climate::get_update_interval() const { return this->update_interval_; }
void CN105Climate::set_update_interval(uint32_t update_interval) {
    //ESP_LOGD(TAG, "Setting update interval to %lu", update_interval);
    log_debug_uint32(TAG, "Setting update interval to ", update_interval);

    this->update_interval_ = update_interval;
    this->autoUpdate = (update_interval != 0);
}
