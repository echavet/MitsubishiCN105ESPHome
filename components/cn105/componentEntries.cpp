#include "cn105.h"
#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif

using namespace esphome;

/**
 * This method is call by the esphome framework to initialize the component
 * We don't try to connect to the heater here because errors could not be logged fine because the
 * UART is used for communication with the heatpump
 * setupUART will handle the
*/
void CN105Climate::setup() {

    ESP_LOGD(TAG, "Component initialization: setup call");
    this->boot_ms_ = CUSTOM_MILLIS;
    this->current_temperature = NAN;
    this->target_temperature = NAN;
    this->target_temperature_low = NAN;
    this->target_temperature_high = NAN;
    this->fan_mode = climate::CLIMATE_FAN_OFF;
    this->swing_mode = climate::CLIMATE_SWING_OFF;
    this->initBytePointer();
    this->lastResponseMs = CUSTOM_MILLIS;

    // initialize diagnostic stats
    this->nbCompleteCycles_ = 0;
    this->nbCycles_ = 0;
    this->nbHeatpumpConnections_ = 0;

    // Register info requests here to ensure all dependencies (like hardware_settings) are ready
    this->registerInfoRequests();

    ESP_LOGI(TAG, "tx_pin: %d rx_pin: %d", this->tx_pin_, this->rx_pin_);
    //ESP_LOGI(TAG, "remote_temp_timeout is set to %lu", this->remote_temp_timeout_);
    log_info_uint32(TAG, "remote_temp_timeout is set to ", this->remote_temp_timeout_);
    //ESP_LOGI(TAG, "debounce_delay is set to %lu", this->debounce_delay_);
    log_info_uint32(TAG, "debounce_delay is set to ", this->debounce_delay_);

    // IMPORTANT: ne pas initier la connexion UART/CN105 dans setup().
    // On démarre la séquence dans loop() pour éviter de rater les premiers logs OTA.

    // Initialize the internal flag based on the static configuration provided by YAML/Python
    this->supports_dual_setpoint_ = this->traits_.has_feature_flags(climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE);
    ESP_LOGI(TAG, "Dual setpoint support configured: %s", this->supports_dual_setpoint_ ? "YES" : "NO");
}


/**
 * @brief Executes the main loop for the CN105Climate component.
 * This function is called repeatedly in the main program loop.
 */
void CN105Climate::loop() {
    // Bootstrap connexion CN105 (UART + CONNECT) depuis loop()
    this->maybe_start_connection_();

    // Tant que la connexion n'a pas réussi, on ne lance AUCUN cycle/écriture (sinon ça court-circuite le délai).
    // On continue quand même à lire/processer l'input afin de détecter le 0x7A/0x7B (connection success).
    const bool can_talk_to_hp = this->isHeatpumpConnected_;

    if (!this->processInput()) {                                            // if we don't get any input: no read op
        if (!can_talk_to_hp) {
            return;
        }
        if ((this->wantedSettings.hasChanged) && (!this->loopCycle.isCycleRunning())) {
            this->checkPendingWantedSettings();
        } else if ((this->wantedRunStates.hasChanged) && (!this->loopCycle.isCycleRunning())) {
            this->checkPendingWantedRunStates();
        } else {
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

void CN105Climate::maybe_start_connection_() {
    if (this->conn_bootstrap_started_) return;

    // Timeout global: au bout de 2 minutes on démarre même sans WiFi
    if (!this->conn_timeout_armed_) {
        this->conn_timeout_armed_ = true;
        this->set_timeout("cn105_bootstrap_timeout", 120000, [this]() {
            if (this->conn_bootstrap_started_) return;
            ESP_LOGW(LOG_CONN_TAG, "Bootstrap connexion: timeout 120s, démarrage CN105 malgré tout");
            this->conn_bootstrap_started_ = true;
            this->setupUART();
            this->sendFirstConnectionPacket();
            });
    }

#ifdef USE_WIFI
    if (wifi::global_wifi_component != nullptr && !wifi::global_wifi_component->is_connected()) {
        if (!this->conn_wait_logged_) {
            this->conn_wait_logged_ = true;
            ESP_LOGI(LOG_CONN_TAG, "Bootstrap connexion: attente WiFi avant init UART/CONNECT");
        }
        return;
    }
#endif

    // Délai de grâce pour laisser le flux de logs OTA se connecter (évite de rater la séquence CONNECT)
    const uint32_t grace_ms = this->conn_bootstrap_delay_ms_;
    const uint32_t elapsed = CUSTOM_MILLIS - this->boot_ms_;
    if (elapsed < grace_ms) {
        if (!this->conn_grace_logged_) {
            this->conn_grace_logged_ = true;
            ESP_LOGI(LOG_CONN_TAG, "Bootstrap connexion: délai de grâce %ums pour logs OTA", grace_ms);
        }
        return;
    }

    this->conn_bootstrap_started_ = true;
    ESP_LOGI(LOG_CONN_TAG, "Bootstrap connexion: init UART + envoi CONNECT (loop)");
    this->setupUART();
    this->sendFirstConnectionPacket();
}

uint32_t CN105Climate::get_update_interval() const { return this->update_interval_; }
void CN105Climate::set_update_interval(uint32_t update_interval) {
    //ESP_LOGD(TAG, "Setting update interval to %lu", update_interval);
    log_debug_uint32(TAG, "Setting update interval to ", update_interval);

    this->update_interval_ = update_interval;
    this->autoUpdate = (update_interval != 0);
}
