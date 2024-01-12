// TODO: extra_components: add a last_request_datetime et last_response_datetime
// TODO: instead of having to entry points, control and update, we should only record control modifications and run them in the nexte update call

#include "cn105.h"

using namespace esphome;


CN105Climate::CN105Climate(HardwareSerial* hw_serial)
    : hw_serial_(hw_serial) {
    this->traits_.set_supports_action(true);
    this->traits_.set_supports_current_temperature(true);
    this->traits_.set_supports_two_point_target_temperature(false);
    this->traits_.set_visual_min_temperature(ESPMHP_MIN_TEMPERATURE);
    this->traits_.set_visual_max_temperature(ESPMHP_MAX_TEMPERATURE);
    this->traits_.set_visual_temperature_step(ESPMHP_TEMPERATURE_STEP);


    this->traits_.set_supported_modes({
        //climate::CLIMATE_MODE_AUTO,
        climate::CLIMATE_MODE_COOL,
        climate::CLIMATE_MODE_DRY,
        climate::CLIMATE_MODE_FAN_ONLY,
        climate::CLIMATE_MODE_HEAT,
        climate::CLIMATE_MODE_OFF });

    this->traits_.set_supported_fan_modes({
        climate::CLIMATE_FAN_AUTO,
        climate::CLIMATE_FAN_QUIET,
        climate::CLIMATE_FAN_LOW,
        climate::CLIMATE_FAN_HIGH,
        climate::CLIMATE_FAN_MEDIUM });


    this->traits_.set_supported_swing_modes({
        //climate::CLIMATE_SWING_BOTH,
        //climate::CLIMATE_SWING_HORIZONTAL,
        climate::CLIMATE_SWING_VERTICAL,
        climate::CLIMATE_SWING_OFF });


    this->isConnected_ = false;
    this->tempMode = false;
    this->wideVaneAdj = false;
    this->functions = heatpumpFunctions();
    this->autoUpdate = false;
    this->firstRun = true;
    this->externalUpdate = false;
    this->lastSend = 0;
    this->infoMode = 0;
    this->currentStatus.operating = false;
    this->currentStatus.compressorFrequency = -1;

    generateExtraComponents();

}




void CN105Climate::set_baud_rate(int baud) {
    this->baud_ = baud;
    ESP_LOGD(TAG, "set_baud_rate()");
    ESP_LOGD(TAG, "baud: %d", baud);
}

// void CN105Climate::set_wifi_connected_state(bool state) {

//     ESP_LOGD(TAG, "setting WiFi connection state do %d", state);

//     this->wifi_connected_state_ = state;
//     if (state) {
//         // Réinitialiser la variable de temporisation et planifier la fin du délai        
//         this->setWifiDelayCompleted(false);
//         this->set_timeout("wifi_delay", 5000, [this]() { this->setWifiDelayCompleted(true); });
//     }
// }



void CN105Climate::check_logger_conflict_() {
#ifdef USE_LOGGER
    if (this->get_hw_serial_() == logger::global_logger->get_hw_serial()) {
        ESP_LOGW(TAG, "  You're using the same serial port for logging"
            " and the MitsubishiHeatPump component. Please disable"
            " logging over the serial port by setting"
            " logger:baud_rate to 0.");

        //this->mark_failed();

    }
#endif
}

int CN105Climate::get_compressor_frequency() {
    return currentStatus.compressorFrequency;
}
bool CN105Climate::is_operating() {
    return currentStatus.operating;
}



void CN105Climate::setupUART() {
    // Votre code ici
    ESP_LOGI(TAG, "setupUART() with baudrate %d", this->baud_);

    this->isHeatpumpConnected_ = false;
    this->isConnected_ = false;

    ESP_LOGI(
        TAG,
        "hw_serial(%p) is &Serial(%p)? %s",
        this->get_hw_serial_(),
        &Serial,
        YESNO(this->get_hw_serial_() == &Serial)
    );

    this->check_logger_conflict_();

    this->uart_setup_switch = true;

    if (this->get_hw_serial_() != NULL) {
        ESP_LOGD(TAG, "Serial->begin...");
        this->get_hw_serial_()->begin(this->baud_, SERIAL_8E1);
        this->isConnected_ = true;
        this->initBytePointer();
    } else {
        ESP_LOGE(TAG, "L'UART doit être défini.");
    }

    //this->publish_state();
}


void CN105Climate::disconnectUART() {
    ESP_LOGD(TAG, "disconnectUART()");
    this->uart_setup_switch = false;

    this->isHeatpumpConnected_ = false;
    this->isConnected_ = false;
    this->cancel_timeout(SHEDULER_INTERVAL_SYNC_NAME);
    this->publish_state();
    if (this->get_hw_serial_() != NULL) {
        this->get_hw_serial_()->end();
    } else {
        ESP_LOGE(TAG, "L'UART doit être défini.");
    }
}


bool CN105Climate::isHeatpumpConnectionActive() {
    long lrTimeMs = CUSTOM_MILLIS - this->lastResponseMs;

    if (lrTimeMs > MAX_DELAY_RESPONSE) {
        ESP_LOGW(TAG, "Heatpump has not replied for %d ms", lrTimeMs);
        ESP_LOGI(TAG, "We think Heatpump is not connected anymore..");
    }

    return  (lrTimeMs < MAX_DELAY_RESPONSE);
}
//#endregion climate
