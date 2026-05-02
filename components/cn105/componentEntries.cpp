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
    this->parser_.reset();
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

    // IMPORTANT: do not initiate the UART/CN105 connection in setup().
    // We start the sequence in loop() to avoid missing the first OTA logs.

    // Initialize the internal flag based on the static configuration provided by YAML/Python
    this->supports_dual_setpoint_ = this->traits_.has_feature_flags(climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE);
    ESP_LOGI(TAG, "Dual setpoint support configured: %s", this->supports_dual_setpoint_ ? "YES" : "NO");
    ESP_LOGI(TAG, "Horizontal vanes configured: %d", this->horizontal_vanes_);
}


/**
 * @brief Executes the main loop for the CN105Climate component.
 * This function is called repeatedly in the main program loop.
 */
void CN105Climate::loop() {
    // Bootstrap connection CN105 (UART + CONNECT) from loop()
    this->maybe_start_connection_();

    // As long as the connection is not successful, we do not launch ANY cycle/write (otherwise it short-circuits the delay).
    // We still continue to read/process the input in order to detect 0x7A/0x7B (connection success).
    const bool can_talk_to_hp = this->isHeatpumpConnected();

    if (!this->processInput()) {                                            // if we don't get any input: no read op
        if (!can_talk_to_hp) {
            return;
        }
        if ((this->wantedSettings.hasChanged) && (!this->loopCycle.isCycleRunning())) {
            this->checkPendingWantedSettings();
        } else if ((this->wantedRunStates.hasChanged) && (!this->loopCycle.isCycleRunning())) {
            this->checkPendingWantedRunStates();
        } else if ((this->isSetFunctions_) && (!this->loopCycle.isCycleRunning())) {
            this->isSetFunctions_ = false;
            this->setFunctions(this->functions);
            // Also request to get function settings from heat pump to update UI with latest values.
            this->isGetFunctions_ = true;
        } else {
            if (this->loopCycle.isCycleRunning()) {                         // if we are  running an update cycle
                this->loopCycle.checkTimeout(this->update_interval_);
            } else { // we are not running a cycle
                if (this->loopCycle.hasUpdateIntervalPassed(this->get_update_interval())) {
                    if (this->isGetFunctions_) {
                        // Reactivate requests 0x20/0x22 and bypass interval timers.
                        // This must be done before starting a new cycle to prevent a race hazard of
                        // request 0x22 occurring before request 0x20.
                        this->scheduler_.enable_request(0x20);
                        this->scheduler_.timer_bypass(0x20);
                        this->scheduler_.enable_request(0x22);
                        this->scheduler_.timer_bypass(0x22);
                        this->isGetFunctions_ = false;
                    }
                    this->buildAndSendRequestsInfoPackets();            // initiate an update cycle with this->cycleStarted();
                }
            }
        }
    }
}

void CN105Climate::maybe_start_connection_() {
    switch (state_) {
        case DriverState::BOOT: {
            // Arm a 120s global timeout (fires once, forces connection even without WiFi)
            this->set_timeout("cn105_bootstrap_timeout", 120000, [this]() {
                if (state_ >= DriverState::CONNECTING) return;
                ESP_LOGW(LOG_CONN_TAG, "Bootstrap connexion: timeout 120s, démarrage CN105 malgré tout");
                this->setupUART();
                this->sendFirstConnectionPacket();
            });

#ifdef USE_WIFI
            if (wifi::global_wifi_component != nullptr && !wifi::global_wifi_component->is_connected()) {
                this->transition_to_(DriverState::WAIT_WIFI);
                ESP_LOGI(LOG_CONN_TAG, "Bootstrap connexion: attente WiFi avant init UART/CONNECT");
                return;
            }
#endif
            // WiFi ready (or no WiFi) — check grace delay
            this->transition_to_(DriverState::WAIT_GRACE);
            ESP_LOGI(LOG_CONN_TAG, "Bootstrap connexion: délai de grâce %ums pour logs OTA", this->conn_bootstrap_delay_ms_);
            return;
        }

        case DriverState::WAIT_WIFI: {
#ifdef USE_WIFI
            if (wifi::global_wifi_component != nullptr && !wifi::global_wifi_component->is_connected()) {
                return;  // still waiting
            }
#endif
            this->transition_to_(DriverState::WAIT_GRACE);
            ESP_LOGI(LOG_CONN_TAG, "Bootstrap connexion: WiFi connecté, délai de grâce %ums", this->conn_bootstrap_delay_ms_);
            return;
        }

        case DriverState::WAIT_GRACE: {
            const uint32_t elapsed = CUSTOM_MILLIS - this->boot_ms_;
            if (elapsed < this->conn_bootstrap_delay_ms_) {
                return;  // grace delay not elapsed yet
            }
            ESP_LOGI(LOG_CONN_TAG, "Bootstrap connexion: init UART + envoi CONNECT (loop)");
            this->setupUART();
            this->sendFirstConnectionPacket();
            // setupUART() transitions to CONNECTING if UART config is valid
            return;
        }

        case DriverState::CONNECTING:
        case DriverState::CONNECTED:
        case DriverState::DISCONNECTED:
            // Nothing to do — connection already started or managed elsewhere
            return;
    }
}

uint32_t CN105Climate::get_update_interval() const { return this->update_interval_; }
void CN105Climate::set_update_interval(uint32_t update_interval) {
    //ESP_LOGD(TAG, "Setting update interval to %lu", update_interval);
    log_debug_uint32(TAG, "Setting update interval to ", update_interval);

    this->update_interval_ = update_interval;
    this->autoUpdate = (update_interval != 0);
}
