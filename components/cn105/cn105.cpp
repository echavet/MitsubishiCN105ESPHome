
#include "cn105.h"

using namespace esphome;


CN105Climate::CN105Climate(uart::UARTComponent* uart) :
    UARTDevice(uart) {

    this->traits_.set_supports_action(true);
    this->traits_.set_supports_current_temperature(true);
    this->traits_.set_supports_two_point_target_temperature(false);
    this->traits_.set_visual_min_temperature(ESPMHP_MIN_TEMPERATURE);
    this->traits_.set_visual_max_temperature(ESPMHP_MAX_TEMPERATURE);
    this->traits_.set_visual_temperature_step(ESPMHP_TEMPERATURE_STEP);


    this->isUARTConnected_ = false;
    this->tempMode = false;
    this->wideVaneAdj = false;
    this->functions = heatpumpFunctions();
    this->autoUpdate = false;
    this->firstRun = true;
    this->externalUpdate = false;
    this->lastSend = 0;
    this->infoMode = 0;
    this->lastConnectRqTimeMs = 0;
    this->currentStatus.operating = false;
    this->currentStatus.compressorFrequency = NAN;
    this->currentStatus.inputPower = NAN;
    this->currentStatus.kWh = NAN;
    this->currentStatus.runtimeHours = NAN;
    this->tx_pin_ = -1;
    this->rx_pin_ = -1;

    this->horizontal_vane_select_ = nullptr;
    this->vertical_vane_select_ = nullptr;
    this->compressor_frequency_sensor_ = nullptr;
    this->input_power_sensor_ = nullptr;
    this->kwh_sensor_ = nullptr;
    this->runtime_hours_sensor_ = nullptr;

    this->powerRequestWithoutResponses = 0;     // power request is not supported by all heatpump #112

    this->remote_temp_timeout_ = 4294967295;    // uint32_t max
    this->generateExtraComponents();
    this->loopCycle.init();
    this->wantedSettings.resetSettings();
#ifndef USE_ESP32
    this->wantedSettingsMutex = false;
#endif
}



void CN105Climate::set_baud_rate(int baud) {
    this->baud_ = baud;
    ESP_LOGI(TAG, "setting baud rate to: %d", baud);
}

void CN105Climate::set_tx_rx_pins(uint8_t tx_pin, uint8_t rx_pin) {
    this->tx_pin_ = tx_pin;
    this->rx_pin_ = rx_pin;
    ESP_LOGI(TAG, "setting tx_pin: %d rx_pin: %d", tx_pin, rx_pin);

}

void CN105Climate::pingExternalTemperature() {
    this->set_timeout(SHEDULER_REMOTE_TEMP_TIMEOUT, this->remote_temp_timeout_, [this]() {
        ESP_LOGW(LOG_ACTION_EVT_TAG, "Remote temperature timeout occured, fall back to internal temperature!");
        this->set_remote_temperature(0);
        });
}

void CN105Climate::set_remote_temp_timeout(uint32_t timeout) {
    this->remote_temp_timeout_ = timeout;
    if (timeout == 4294967295) {
        ESP_LOGI(LOG_ACTION_EVT_TAG, "set_remote_temp_timeout is set to never.");
    } else {
        //ESP_LOGI(LOG_ACTION_EVT_TAG, "set_remote_temp_timeout is set to %lu", timeout);
        log_info_uint32(LOG_ACTION_EVT_TAG, "set_remote_temp_timeout is set to ", timeout);

        this->pingExternalTemperature();
    }
}

void CN105Climate::set_debounce_delay(uint32_t delay) {
    this->debounce_delay_ = delay;
    //ESP_LOGI(LOG_ACTION_EVT_TAG, "set_debounce_delay is set to %lu", delay);
    log_info_uint32(LOG_ACTION_EVT_TAG, "set_debounce_delay is set to ", delay);
}

float CN105Climate::get_compressor_frequency() {
    return currentStatus.compressorFrequency;
}
float CN105Climate::get_input_power() {
    return currentStatus.inputPower;
}
float CN105Climate::get_kwh() {
    return currentStatus.kWh;
}
float CN105Climate::get_runtime_hours() {
    return currentStatus.runtimeHours;
}
bool CN105Climate::is_operating() {
    return currentStatus.operating;
}


// SERIAL_8E1
void CN105Climate::setupUART() {

    //ESP_LOGI(TAG, "setupUART() with baudrate %lu", this->parent_->get_baud_rate());
    log_info_uint32(TAG, "setupUART() with baudrate ", this->parent_->get_baud_rate());
    this->setHeatpumpConnected(false);
    this->isUARTConnected_ = false;

    // just for debugging purpose, a way to use a button i, yaml to trigger a reconnect
    this->uart_setup_switch = true;

    if (this->parent_->get_data_bits() == 8 &&
        this->parent_->get_parity() == uart::UART_CONFIG_PARITY_EVEN &&
        this->parent_->get_stop_bits() == 1) {
        ESP_LOGD(TAG, "UART est configuré en SERIAL_8E1");
        this->isUARTConnected_ = true;
        this->initBytePointer();
    } else {
        ESP_LOGW(TAG, "UART n'est pas configuré en SERIAL_8E1");
    }

}

void CN105Climate::setHeatpumpConnected(bool state) {
    this->isHeatpumpConnected_ = state;
    if (this->hp_uptime_connection_sensor_ != nullptr) {
        if (state) {
            this->hp_uptime_connection_sensor_->start();
            ESP_LOGD(TAG, "starting hp_uptime_connection_sensor_ uptime chrono");
        } else {
            this->hp_uptime_connection_sensor_->stop();
            ESP_LOGD(TAG, "stopping hp_uptime_connection_sensor_ uptime chrono");
        }
    }
}
void CN105Climate::disconnectUART() {
    ESP_LOGD(TAG, "disconnectUART()");
    this->uart_setup_switch = false;
    this->setHeatpumpConnected(false);
    //this->isHeatpumpConnected_ = false;
    //this->isUARTConnected_ = false;
    this->firstRun = true;
    this->publish_state();

}

void CN105Climate::reconnectUART() {
    ESP_LOGD(TAG, "reconnectUART()");
    this->lastReconnectTimeMs = CUSTOM_MILLIS;
    this->disconnectUART();
    this->setupUART();
    this->sendFirstConnectionPacket();
}


void CN105Climate::reconnectIfConnectionLost() {

    long reconnectTimeMs = CUSTOM_MILLIS - this->lastReconnectTimeMs;

    if (reconnectTimeMs < this->update_interval_) {
        return;
    }

    if (!this->isHeatpumpConnectionActive()) {
        long connectTimeMs = CUSTOM_MILLIS - this->lastConnectRqTimeMs;
        if (connectTimeMs > this->update_interval_) {
            long lrTimeMs = CUSTOM_MILLIS - this->lastResponseMs;
            ESP_LOGW(TAG, "Heatpump has not replied for %ld s", lrTimeMs / 1000);
            ESP_LOGI(TAG, "We think Heatpump is not connected anymore..");
            this->reconnectUART();
        }
    }
}


bool CN105Climate::isHeatpumpConnectionActive() {
    long lrTimeMs = CUSTOM_MILLIS - this->lastResponseMs;

    // if (lrTimeMs > MAX_DELAY_RESPONSE_FACTOR * this->update_interval_) {
    //     ESP_LOGV(TAG, "Heatpump has not replied for %ld s", lrTimeMs / 1000);
    //     ESP_LOGV(TAG, "We think Heatpump is not connected anymore..");
    //     this->disconnectUART();
    // }

    return  (lrTimeMs < MAX_DELAY_RESPONSE_FACTOR * this->update_interval_);
}
