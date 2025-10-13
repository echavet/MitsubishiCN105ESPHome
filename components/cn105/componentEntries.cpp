#include "cn105.h"

namespace esphome {
    namespace cn105 {

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

            if (this->mqtt_mim_handler_ != nullptr) {
                this->mqtt_mim_handler_->set_parent(this);
                ESP_LOGCONFIG(TAG, "CN105MqttMim parent set at runtime");
            }

            this->setupUART();
            this->sendFirstConnectionPacket();
        }


        /**
         * @brief Executes the main loop for the CN105Climate component.
         * This function is called repeatedly in the main program loop.
         */
         // Ajoutez cette nouvelle fonction au fichier.
        void CN105Climate::activate_passive_mode() {
            if (!this->passive_mode_active_) {
                ESP_LOGI(TAG, "Dispositif officiel détecté. Passage en mode passif pour %u ms.", this->passive_mode_duration_ms_);
            } else {
                ESP_LOGD(TAG, "Mode passif réinitialisé (nouveau paquet reçu).");
            }
            // On active le mode et on réinitialise le timer à chaque commande
            this->passive_mode_active_ = true;
            this->passive_mode_start_ms_ = CUSTOM_MILLIS;
        }


        /**
         * @brief Executes the main loop for the CN105Climate component.
         * This function is called repeatedly in the main program loop.
         */
        void CN105Climate::loop() {
            // --- GESTION DU TIMEOUT DU MODE PASSIF ---
            // Si on est en mode passif et que le temps est écoulé, on repasse en actif.
            if (this->passive_mode_active_ && (CUSTOM_MILLIS - this->passive_mode_start_ms_ > this->passive_mode_duration_ms_)) {
                ESP_LOGI(TAG, "Timeout du mode passif. Reprise du cycle de polling normal.");
                this->passive_mode_active_ = false;
            }

            // Le reste de votre logique de boucle existante
            if (!this->processInput()) {
                if ((this->wantedSettings.hasChanged) && (!this->loopCycle.isCycleRunning())) {
                    this->checkPendingWantedSettings();
                } else {
                    if (this->loopCycle.isCycleRunning()) {
                        this->loopCycle.checkTimeout(this->update_interval_);
                    } else {
                        // On ne lance un nouveau cycle que si on n'est PAS en mode passif
                        if (!this->is_in_passive_mode() && this->loopCycle.hasUpdateIntervalPassed(this->get_update_interval())) {
                            this->buildAndSendRequestsInfoPackets();
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
    }
}