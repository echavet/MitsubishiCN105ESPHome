
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


    this->traits_.set_supported_modes({
        climate::CLIMATE_MODE_HEAT_COOL,
        climate::CLIMATE_MODE_COOL,
        climate::CLIMATE_MODE_DRY,
        climate::CLIMATE_MODE_FAN_ONLY,
        climate::CLIMATE_MODE_HEAT,
        climate::CLIMATE_MODE_OFF });

    this->traits_.set_supported_fan_modes({
        climate::CLIMATE_FAN_AUTO,
        climate::CLIMATE_FAN_MIDDLE,
        climate::CLIMATE_FAN_QUIET,
        climate::CLIMATE_FAN_LOW,
        climate::CLIMATE_FAN_HIGH,
        climate::CLIMATE_FAN_MEDIUM });


    this->traits_.set_supported_swing_modes({
        climate::CLIMATE_SWING_BOTH,
        climate::CLIMATE_SWING_HORIZONTAL,
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
    this->tx_pin_ = -1;
    this->rx_pin_ = -1;

    this->horizontal_vane_select_ = nullptr;
    this->vertical_vane_select_ = nullptr;
    this->compressor_frequency_sensor_ = nullptr;

    this->remote_temp_timeout_ = 4294967295;    // uint32_t max

    this->generateExtraComponents();

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

void CN105Climate::setExternalTemperatureCheckout() {
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
        ESP_LOGI(LOG_ACTION_EVT_TAG, "set_remote_temp_timeout is set to %d", timeout);
        this->setExternalTemperatureCheckout();
    }
}

void CN105Climate::set_debounce_delay(uint32_t delay) {
    this->debounce_delay_ = delay;
    ESP_LOGI(LOG_ACTION_EVT_TAG, "set_debounce_delay is set to %d", delay);
}

int CN105Climate::get_compressor_frequency() {
    return currentStatus.compressorFrequency;
}
bool CN105Climate::is_operating() {
    return currentStatus.operating;
}


// SERIAL_8E1
void CN105Climate::setupUART() {

    ESP_LOGI(TAG, "setupUART() with baudrate %d", this->parent_->get_baud_rate());

    this->isHeatpumpConnected_ = false;
    this->isConnected_ = false;

    // just for debugging purpose, a way to use a button i, yaml to trigger a reconnect
    this->uart_setup_switch = true;

    if (this->parent_->get_data_bits() == 8 &&
        this->parent_->get_parity() == uart::UART_CONFIG_PARITY_EVEN &&
        this->parent_->get_stop_bits() == 1) {
        ESP_LOGD("CustomComponent", "UART est configuré en SERIAL_8E1");
        this->isConnected_ = true;
        this->initBytePointer();
    } else {
        ESP_LOGW("CustomComponent", "UART n'est pas configuré en SERIAL_8E1");
    }

}


void CN105Climate::disconnectUART() {
    ESP_LOGD(TAG, "disconnectUART()");
    this->uart_setup_switch = false;

    this->isHeatpumpConnected_ = false;
    this->isConnected_ = false;
    this->firstRun = true;
    this->cancel_timeout(SHEDULER_INTERVAL_SYNC_NAME);
    this->publish_state();

}

void CN105Climate::reconnectUART() {
    ESP_LOGD(TAG, "reconnectUART()");
    this->disconnectUART();
    this->setupUART();
    this->sendFirstConnectionPacket();
}

bool CN105Climate::isHeatpumpConnectionActive() {
    long lrTimeMs = CUSTOM_MILLIS - this->lastResponseMs;

    if (lrTimeMs > MAX_DELAY_RESPONSE_FACTOR * this->update_interval_) {
        ESP_LOGW(TAG, "Heatpump has not replied for %ld s", lrTimeMs / 1000);
        ESP_LOGI(TAG, "We think Heatpump is not connected anymore..");
    }

    return  (lrTimeMs < MAX_DELAY_RESPONSE_FACTOR * this->update_interval_);
}

