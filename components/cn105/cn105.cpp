
#include "cn105.h"
#ifdef USE_ESP32
#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

using namespace esphome;


CN105Climate::CN105Climate(uart::UARTComponent* uart) :
    UARTDevice(uart) {

    // Active les flags de fonctionnalités via l'API moderne (évite les setters dépréciés)
    this->traits_.add_feature_flags(
        climate::CLIMATE_SUPPORTS_ACTION |
        climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE
    );
    // supports_two_point_target_temperature sera défini dans setup() selon les modes supportés
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
    this->airflow_control_select_ = nullptr;
    this->compressor_frequency_sensor_ = nullptr;
    this->input_power_sensor_ = nullptr;
    this->kwh_sensor_ = nullptr;
    this->runtime_hours_sensor_ = nullptr;

    this->air_purifier_switch_ = nullptr;
    this->night_mode_switch_ = nullptr;
    this->circulator_switch_ = nullptr;

    this->powerRequestWithoutResponses = 0;     // power request is not supported by all heatpump #112

    this->remote_temp_timeout_ = 4294967295;    // uint32_t max
    this->generateExtraComponents();
    this->loopCycle.init();
    this->wantedSettings.resetSettings();
    this->wantedRunStates.resetSettings();
#ifndef USE_ESP32
    this->wantedSettingsMutex = false;
#endif

    // Register info requests once at construction time
    this->registerInfoRequests();
}

void CN105Climate::registerInfoRequests() {
    info_requests_.clear();
    // 0x02 Settings
    InfoRequest r_settings("settings", "Settings", 0x02, 3, 0);
    r_settings.onResponse = [this](CN105Climate& self) { (void)self; this->getSettingsFromResponsePacket(); };
    info_requests_.push_back(r_settings);

    // 0x03 Room temperature
    InfoRequest r_room("room_temp", "Room temperature", 0x03, 3, 0);
    r_room.onResponse = [this](CN105Climate& self) { (void)self; this->getRoomTemperatureFromResponsePacket(); };
    info_requests_.push_back(r_room);

    // 0x42 HVAC options (conditional)
    InfoRequest r_hvac_opts("hvac_options", "HVAC options", 0x42, 3, 500);
    r_hvac_opts.canSend = [this](const CN105Climate& self) {
        (void)self; // unused
        return (this->air_purifier_switch_ != nullptr || this->night_mode_switch_ != nullptr || this->circulator_switch_ != nullptr);
        };
    r_hvac_opts.onResponse = [this](CN105Climate& self) { (void)self; this->getHVACOptionsFromResponsePacket(); };
    info_requests_.push_back(r_hvac_opts);

    // 0x06 Status
    InfoRequest r_status("status", "Status", 0x06, 3, 0);
    r_status.onResponse = [this](CN105Climate& self) { (void)self; this->getOperatingAndCompressorFreqFromResponsePacket(); };
    info_requests_.push_back(r_status);

    // 0x09 Standby/Power (existing non-support behavior)
    InfoRequest r_power("standby", "Power/Standby", 0x09, 3, 500);
    r_power.onResponse = [this](CN105Climate& self) { (void)self; this->getPowerFromResponsePacket(); };
    info_requests_.push_back(r_power);

    // Optional placeholders 0x04/0x05 disabled by default
    InfoRequest r_unknown("unknown", "Unknown", 0x04, 1, 0);
    r_unknown.disabled = true;
    info_requests_.push_back(r_unknown);

    InfoRequest r_timers("timers", "Timers", 0x05, 1, 0);
    r_timers.disabled = true;
    info_requests_.push_back(r_timers);

    current_request_index_ = -1;
}

void CN105Climate::sendInfoRequest(uint8_t code) {
    for (size_t i = 0; i < info_requests_.size(); ++i) {
        auto& req = info_requests_[i];
        if (req.code != code) continue;
        if (req.disabled) { return; }
        if (req.canSend && !req.canSend(*this)) { return; }
        ESP_LOGD(LOG_CYCLE_TAG, "Sending %s (0x%02X)", req.description, req.code);
        req.awaiting = true;
        this->buildAndSendInfoPacket(req.code);
        if (req.soft_timeout_ms > 0) {
            uint8_t code_copy = req.code;
            const std::string tname = req.timeout_name.empty() ? (std::string("info_timeout_") + std::to_string(code_copy)) : req.timeout_name;
            this->set_timeout(tname.c_str(), req.soft_timeout_ms, [this, code_copy]() {
                // If still awaiting this code's response, consider it a soft failure and move on
                for (auto& r : this->info_requests_) {
                    if (r.code == code_copy && r.awaiting) {
                        r.awaiting = false;
                        r.failures++;
                        ESP_LOGW(LOG_CYCLE_TAG, "Soft timeout for %s (0x%02X), failures: %d", r.description, r.code, r.failures);
                        if (r.failures >= r.maxFailures) {
                            r.disabled = true;
                            ESP_LOGW(LOG_CYCLE_TAG, "%s (0x%02X) disabled (not supported)", r.description, r.code);
                        }
                        this->sendNextAfter(code_copy);
                        break;
                    }
                }
                });
        }
        current_request_index_ = static_cast<int>(i);
        return;
    }
}

void CN105Climate::markResponseSeenFor(uint8_t code) {
    for (auto& req : info_requests_) {
        if (req.code == code) {
            req.awaiting = false;
            req.failures = 0;
            ESP_LOGD(LOG_CYCLE_TAG, "Receiving %s (0x%02X)", req.description, req.code);
            if (req.onResponse) {
                req.onResponse(*this);
            }
            return;
        }
    }
}

void CN105Climate::sendNextAfter(uint8_t code) {
    // Find current index (by code) then try next activable entries in order
    int start = -1;
    for (size_t i = 0; i < info_requests_.size(); ++i) {
        if (info_requests_[i].code == code) { start = static_cast<int>(i); break; }
    }
    int idx = (start < 0) ? 0 : start + 1;
    for (; idx < static_cast<int>(info_requests_.size()); ++idx) {
        auto& req = info_requests_[idx];
        if (req.disabled) continue;
        if (req.canSend && !req.canSend(*this)) continue;
        this->sendInfoRequest(req.code);
        return;
    }
    // No more requests → end cycle
    this->terminateCycle();
}

bool CN105Climate::processInfoResponse(uint8_t code) {
    // Cherche si le code est géré par l'orchestrateur
    bool handled = false;
    for (auto& req : info_requests_) {
        if (req.code == code) {
            handled = true;
            break;
        }
    }
    if (!handled) return false;

    this->markResponseSeenFor(code);
    this->sendNextAfter(code);
    return true;
}



void CN105Climate::set_baud_rate(int baud) {
    this->baud_ = baud;
    ESP_LOGI(TAG, "setting baud rate to: %d", baud);
}

void CN105Climate::set_tx_rx_pins(int tx_pin, int rx_pin) {
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
bool CN105Climate::is_air_purifier() {
    return currentRunStates.air_purifier;
}
bool CN105Climate::is_night_mode() {
    return currentRunStates.night_mode;
}
bool CN105Climate::is_circulator() {
    return currentRunStates.circulator;
}

// SERIAL_8E1
void CN105Climate::setupUART() {

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
    this->force_low_level_uart_reinit();
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

void CN105Climate::force_low_level_uart_reinit() {
#ifdef USE_ESP32
    // Réinit basse couche: reconfigurer le contrôleur utilisé par UARTComponent
    // On utilise le port passé par set_uart_port (fallback UART0 si inconnu)
    const uart_port_t port = (this->uart_port_ == 1) ? UART_NUM_1 :
#ifdef UART_NUM_2
    (this->uart_port_ == 2) ? UART_NUM_2 :
#endif
        UART_NUM_0;

    ESP_LOGI(TAG, "Forcing low-level UART reinit on port %d (tx=%d, rx=%d)", (int)port, this->tx_pin_, this->rx_pin_);

    // IMPORTANT: ne pas supprimer/réinstaller le driver ici pour éviter conflit avec UARTComponent
    // On reconfigure in-place et on assainit les GPIO
    if (this->tx_pin_ >= 0) gpio_reset_pin((gpio_num_t)this->tx_pin_);
    if (this->rx_pin_ >= 0) gpio_reset_pin((gpio_num_t)this->rx_pin_);
    CUSTOM_DELAY(2);

    // Paramètres SERIAL_8E1 @ 2400 bauds (valeurs issues de la config UARTComponent)
    uart_config_t cfg = {};
    cfg.baud_rate = this->parent_ ? (int)this->parent_->get_baud_rate() : 2400;
    cfg.data_bits = UART_DATA_8_BITS;
    cfg.parity = UART_PARITY_EVEN;
    cfg.stop_bits = UART_STOP_BITS_1;
    cfg.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    cfg.rx_flow_ctrl_thresh = 0;

    uart_param_config(port, &cfg);

    // Reconfigurer les pins si connues; sinon GPIO1/2 (Atom S3 yaml)
    int tx = (this->tx_pin_ >= 0) ? this->tx_pin_ : 1;
    int rx = (this->rx_pin_ >= 0) ? this->rx_pin_ : 2;
    esp_err_t pin_err = uart_set_pin(port, (gpio_num_t)tx, (gpio_num_t)rx, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (pin_err != ESP_OK) {
        ESP_LOGE(TAG, "uart_set_pin failed: %s", esp_err_to_name(pin_err));
    }

    // RX idle high: assurer un pull-up (utile à bas débit/8E1)
    if (this->rx_pin_ >= 0) {
        gpio_set_pull_mode((gpio_num_t)this->rx_pin_, GPIO_PULLUP_ONLY);
    }

    // S'assurer du mode UART classique
    uart_set_mode(port, UART_MODE_UART);

    // Attendre que toute TX en cours finisse (si driver déjà installé)
    uart_wait_tx_done(port, pdMS_TO_TICKS(20));

    // Fixer la source d'horloge UART (bas débits peuvent être sensibles)
#if defined(UART_SCLK_XTAL)
    uart_set_sclk(port, UART_SCLK_XTAL);
#elif defined(UART_SCLK_APB)
    uart_set_sclk(port, UART_SCLK_APB);
#endif
    // Re-forcer explicitement le baud après sclk
    uart_set_baudrate(port, cfg.baud_rate);

    // Assainir inversion/flow control
    uart_set_line_inverse(port, UART_SIGNAL_INV_DISABLE);
    uart_set_hw_flow_ctrl(port, UART_HW_FLOWCTRL_DISABLE, 0);

    // Timeout RX court pour vider rapidement
    uart_set_rx_timeout(port, 2);

    // Purger les buffers pour éviter les résidus
    uart_flush_input(port);
    CUSTOM_DELAY(2);

    // Diagnostics
    uint32_t eff_baud = 0;
    uart_get_baudrate(port, &eff_baud);
    ESP_LOGD(TAG, "UART effective baud=%lu tx_pin=%d rx_pin=%d", (unsigned long)eff_baud, this->tx_pin_, this->rx_pin_);
#else
    // Pas d’ESP32: rien à faire
#endif
}
