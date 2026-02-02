
#include "cn105.h"
#ifdef USE_ESP32
#include <driver/uart.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#endif

using namespace esphome;


CN105Climate::CN105Climate(uart::UARTComponent* uart) :
    UARTDevice(uart),
    scheduler_(
        // send_callback: envoie un paquet via buildAndSendInfoPacket
        [this](uint8_t code) { this->buildAndSendInfoPacket(code); },
        // timeout_callback: utilise set_timeout de Component
        [this](const std::string& name, uint32_t timeout_ms, std::function<void()> callback) {
            this->set_timeout(name.c_str(), timeout_ms, std::move(callback));
        },
        // terminate_callback: termine le cycle
        [this]() { this->terminateCycle(); },
        // context_callback: retourne this pour les callbacks canSend et onResponse
        [this]() -> CN105Climate* { return this; }
    ) {

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

    // Register info requests moved to setup() to ensure hardware_settings_ are populated
}

void CN105Climate::registerInfoRequests() {
    scheduler_.clear_requests();

    // 0x02 Settings
    // Soft timeout 500ms, high failure tolerance for noisy buses
    InfoRequest r_settings("settings", "Settings", 0x02, 50, 500);
    r_settings.onResponse = [this](CN105Climate& self) { (void)self; this->getSettingsFromResponsePacket(); };
    scheduler_.register_request(r_settings);

    // 0x03 Room temperature
    InfoRequest r_room("room_temp", "Room temperature", 0x03, 50, 500);
    r_room.onResponse = [this](CN105Climate& self) { (void)self; this->getRoomTemperatureFromResponsePacket(); };
    scheduler_.register_request(r_room);

    // 0x06 Status
    InfoRequest r_status("status", "Status", 0x06, 50, 500);
    r_status.onResponse = [this](CN105Climate& self) { (void)self; this->getOperatingAndCompressorFreqFromResponsePacket(); };
    scheduler_.register_request(r_status);

    // 0x09 Standby/Power
    InfoRequest r_power("standby", "Power/Standby", 0x09, 3, 500);
    r_power.onResponse = [this](CN105Climate& self) { (void)self; this->getPowerFromResponsePacket(); };
    scheduler_.register_request(r_power);

    // 0x42 HVAC options
    InfoRequest r_hvac_opts("hvac_options", "HVAC options", 0x42, 3, 500);
    r_hvac_opts.canSend = [this](const CN105Climate& self) {
        (void)self;
        return (this->air_purifier_switch_ != nullptr || this->night_mode_switch_ != nullptr || this->circulator_switch_ != nullptr);
        };
    r_hvac_opts.onResponse = [this](CN105Climate& self) { (void)self; this->getHVACOptionsFromResponsePacket(); };
    scheduler_.register_request(r_hvac_opts);

    // Placeholders
    InfoRequest r_unknown("unknown", "Unknown", 0x04, 1, 0);
    r_unknown.disabled = true;
    scheduler_.register_request(r_unknown);

    InfoRequest r_timers("timers", "Timers", 0x05, 1, 0);
    r_timers.disabled = true;
    scheduler_.register_request(r_timers);

    // Appel vers la nouvelle méthode dédiée
    this->registerHardwareSettingsRequests();
}

void CN105Climate::registerHardwareSettingsRequests() {
    if (!this->hardware_settings_.empty()) {
        ESP_LOGI(LOG_FUNCTIONS_TAG, "Registering function settings requests (0x20/0x22) with interval %u ms", this->hardware_settings_interval_ms_);
        uint32_t interval = this->hardware_settings_interval_ms_;

        // Helper Lambda : Vérifie l'incompatibilité et désactive tout si nécessaire
        auto check_and_disable = [](CN105Climate& self, uint8_t code) -> bool {
            if (self.data[0] != code) return false;

            bool all_zeros = true;
            // Sur certaines unités (ex: SEZ), les codes peuvent être présents avec une valeur à 0
            // tant que la session n'est pas en mode installateur. La présence de l'octet (code+valeur)
            // suffit à valider le support.
            for (int i = 1; i < self.dataLength; i++) {
                if (self.data[i] != 0) {
                    all_zeros = false;
                    break;
                }
            }

            if (all_zeros) {
                ESP_LOGW(LOG_FUNCTIONS_TAG, "Response 0x%02X contains only zeros. Feature not supported by unit. Disabling.", code);

                // 1. Désactiver la requête via le scheduler
                self.scheduler_.disable_request(code);

                // 2. Marquer les composants graphiques comme "Failed" (Unavailable)
                ESP_LOGD(LOG_FUNCTIONS_TAG, "Marking Hardware Setting Selects as failed.");
                for (auto* setting : self.hardware_settings_) {
                    setting->set_enabled(false);
                }

                return false;
            }
            return true;
            };

        // --- Part 1 (0x20) ---
        InfoRequest r_funcs1("functions1", "Functions Part 1", 0x20, 3, 0, interval, LOG_FUNCTIONS_TAG);
        r_funcs1.onResponse = [this, check_and_disable](CN105Climate& self) {
            // Log the raw packet and decoded pairs even if the unit returns all zeros
            self.hpPacketDebug(self.data, self.dataLength, "RX 0x20");
            self.hpFunctionsDebug(self.data, self.dataLength);
            if (check_and_disable(self, 0x20)) {
                self.functions.setData1(&self.data[1]);
                ESP_LOGD(LOG_FUNCTIONS_TAG, "Got functions packet 1 (via InfoRequest)");
            }
            };
        scheduler_.register_request(r_funcs1);

        // --- Part 2 (0x22) ---
        InfoRequest r_funcs2("functions2", "Functions Part 2", 0x22, 3, 0, interval, LOG_FUNCTIONS_TAG);
        r_funcs2.onResponse = [this, check_and_disable](CN105Climate& self) {
            // Log the raw packet and decoded pairs even if the unit returns all zeros
            self.hpPacketDebug(self.data, self.dataLength, "RX 0x22");
            self.hpFunctionsDebug(self.data, self.dataLength);
            if (check_and_disable(self, 0x22)) {
                self.functions.setData2(&self.data[1]);
                ESP_LOGD(LOG_FUNCTIONS_TAG, "Got functions packet 2 (via InfoRequest)");
                self.functionsArrived();
            }
            };
        scheduler_.register_request(r_funcs2);

    } else {
        ESP_LOGD(LOG_FUNCTIONS_TAG, "No hardware settings configured in YAML, skipping 0x20/0x22 requests");
    }
}

// Les méthodes sendInfoRequest, markResponseSeenFor, sendNextAfter et processInfoResponse
// ont été déplacées dans RequestScheduler pour respecter le principe de responsabilité unique (SRP).



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
        ESP_LOGW(LOG_REMOTE_TEMP, "Remote temperature timeout occured, fall back to internal temperature!");
        this->stopRemoteTempKeepAlive();
        this->set_remote_temperature(0);
        });
}

void CN105Climate::set_remote_temp_timeout(uint32_t timeout) {
    this->remote_temp_timeout_ = timeout;
    if (timeout == 4294967295) {
        ESP_LOGI(LOG_REMOTE_TEMP, "set_remote_temp_timeout is set to never.");
    } else {
        //ESP_LOGI(LOG_ACTION_EVT_TAG, "set_remote_temp_timeout is set to %lu", timeout);
        log_info_uint32(LOG_REMOTE_TEMP, "set_remote_temp_timeout is set to ", timeout);

        this->pingExternalTemperature();
    }
}

void CN105Climate::set_remote_temp_keepalive_interval(uint32_t interval_ms) {
    this->remote_temp_keepalive_interval_ms_ = interval_ms;
    if (interval_ms == 0) {
        ESP_LOGI(LOG_REMOTE_TEMP, "Remote temperature keep-alive disabled.");
    } else {
        log_info_uint32(LOG_REMOTE_TEMP, "Remote temperature keep-alive interval set to ", interval_ms);
    }
}

void CN105Climate::startRemoteTempKeepAlive() {
    // Don't start if keep-alive is disabled or already active
    if (this->remote_temp_keepalive_interval_ms_ == 0) {
        ESP_LOGD(LOG_REMOTE_TEMP, "Keep-alive disabled, not starting.");
        return;
    }
    if (this->remote_temp_keepalive_active_) {
        ESP_LOGV(LOG_REMOTE_TEMP, "Keep-alive already active.");
        return;
    }

    this->remote_temp_keepalive_active_ = true;
    log_info_uint32(LOG_REMOTE_TEMP, "Starting remote temperature keep-alive with interval ", this->remote_temp_keepalive_interval_ms_);

    this->set_interval(SCHEDULER_REMOTE_TEMP_KEEPALIVE, this->remote_temp_keepalive_interval_ms_, [this]() {
        if (this->remoteTemperature_ > 0 && this->isHeatpumpConnected_) {
            ESP_LOGD(LOG_REMOTE_TEMP, "Keep-alive: re-sending remote temperature %.1f", this->remoteTemperature_);
            // Send the temperature packet without resetting the watchdog timeout
            // (watchdog is only reset when HA sends a new value via set_remote_temperature)
            this->sendRemoteTemperaturePacket();
        } else {
            ESP_LOGV(LOG_REMOTE_TEMP, "Keep-alive skipped: remoteTemp=%.1f, connected=%d",
                this->remoteTemperature_, this->isHeatpumpConnected_);
        }
        });
}

void CN105Climate::stopRemoteTempKeepAlive() {
    if (!this->remote_temp_keepalive_active_) {
        return;
    }
    this->remote_temp_keepalive_active_ = false;
    this->cancel_interval(SCHEDULER_REMOTE_TEMP_KEEPALIVE);
    ESP_LOGI(LOG_REMOTE_TEMP, "Stopped remote temperature keep-alive.");
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
    ESP_LOGI(LOG_CONN_TAG, "setupUART(): baud=%d tx=%d rx=%d (UART port=%d)", this->parent_->get_baud_rate(), this->tx_pin_, this->rx_pin_, this->uart_port_);
    this->setHeatpumpConnected(false);
    this->isUARTConnected_ = false;

    // just for debugging purpose, a way to use a button i, yaml to trigger a reconnect
    this->uart_setup_switch = true;

    if (this->parent_->get_data_bits() == 8 &&
        this->parent_->get_parity() == uart::UART_CONFIG_PARITY_EVEN &&
        this->parent_->get_stop_bits() == 1) {
        ESP_LOGI(LOG_CONN_TAG, "UART configuré en SERIAL_8E1");
        this->isUARTConnected_ = true;
        this->initBytePointer();
    } else {
        ESP_LOGW(LOG_CONN_TAG, "UART n'est pas configuré en SERIAL_8E1");
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
    // Désactivé: le fallback UART bas-niveau (ESP-IDF 5.4.x) peut interférer avec les
    // tests de handshake/fallback. On laisse UARTComponent gérer la réinit standard.
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
