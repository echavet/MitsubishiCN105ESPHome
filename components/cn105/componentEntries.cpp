#include "cn105.h"



/**
 * This method is call by the esphome framework to initialize the component
 * We don't try to connect to the heater here because errors could not be logged fine because the
 * UART is used for communication with the heatpump
 * setupUART will handle the
*/
void CN105Climate::setup() {
    // Votre code ici
    ESP_LOGD(TAG, "Initialisation du composant: appel de setup()");
    this->current_temperature = NAN;
    this->target_temperature = NAN;
    this->fan_mode = climate::CLIMATE_FAN_OFF;
    this->swing_mode = climate::CLIMATE_SWING_OFF;
    this->initBytePointer();

    ESP_LOGI(
        TAG,
        "hw_serial(%p) is &Serial(%p)? %s",
        this->get_hw_serial_(),
        &Serial,
        YESNO(this->get_hw_serial_() == &Serial)
    );

    ESP_LOGI(TAG, "bauds: %d", this->baud_);

    this->check_logger_conflict_();

    this->setupUART();
    this->sendFirstConnectionPacket();
}


/**
 * delays for 12s the call of setup() by esphome to let the time for OTA logs to initialize
*/
bool CN105Climate::can_proceed() {
    if (!this->init_delay_initiated_) {
        ESP_LOGI(TAG, "delaying setup process for 12 seconds..");
        this->init_delay_initiated_ = true;
        this->init_delay_completed_ = false;
        this->set_timeout("init_delay", 12000, [this]() { this->init_delay_completed_ = true; });
    }

    if (this->init_delay_completed_) {
        ESP_LOGI(TAG, "delay expired setup will start...");
    }
    return this->init_delay_completed_;
}

/**
 * Main component loop: used exclusively to process the input from the heatpump
 * No write to the heatpump is done here
*/
void CN105Climate::loop() {
    this->processInput();
}



/**
 * Programs the sending of a request
*/
void CN105Climate::programUpdateInterval() {
    if (autoUpdate) {
        ESP_LOGD(TAG, "Autoupdate is ON --> creating a loop for reccurent updates...");

        ESP_LOGI(TAG, "Programming update interval : %d", this->get_update_interval());

        this->cancel_timeout(SHEDULER_INTERVAL_SYNC_NAME);     // in case a loop is already programmed


        this->set_timeout(SHEDULER_INTERVAL_SYNC_NAME, this->get_update_interval(), [this]() {

            this->buildAndSendRequestsInfoPackets();


            });
    }
}

uint32_t CN105Climate::get_update_interval() const { return this->update_interval_; }
void CN105Climate::set_update_interval(uint32_t update_interval) {
    this->update_interval_ = update_interval;
    this->autoUpdate = (update_interval != 0);
}