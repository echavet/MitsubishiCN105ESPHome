#include "cn105.h"
#include <algorithm>  // for std::max

using namespace esphome;

uint8_t CN105Climate::checkSum(uint8_t bytes[], int len) {
    return cn105_protocol::checksum(bytes, len);
}


void CN105Climate::sendFirstConnectionPacket() {
    if (this->isUARTReady_()) {
        this->lastReconnectTimeMs = CUSTOM_MILLIS;          // marker to prevent to many reconnections
        this->setHeatpumpConnected(false);
        uint8_t packet[CONNECT_LEN];
        memcpy(packet, CONNECT, CONNECT_LEN);
        packet[3] = this->profile_->id();

        // Choix du mode de handshake: standard (0x5A) ou installateur (0x5B)
        packet[1] = (this->installer_mode_effective_) ? 0x5B : 0x5A;
        // CONNECT a un checksum pré-calculé dans la constante; si on modifie l'octet commande, on doit le recalculer.
        packet[CONNECT_LEN - 1] = checkSum(packet, CONNECT_LEN - 1);

        ESP_LOGI(LOG_CONN_TAG, "Envoi du paquet de connexion en mode %s (0x%02X), profile 0x%02X...",
            packet[1] == 0x5B ? "Installateur" : "Standard", packet[1], packet[3]);

        // Détails des octets en DEBUG sur le tag de connexion
        this->hpPacketDebug(packet, CONNECT_LEN, LOG_CONN_TAG);

        this->writePacket(packet, CONNECT_LEN, false);      // checkIsActive=false because it's the first packet and we don't have any reply yet

        this->lastSend = CUSTOM_MILLIS;
        this->lastConnectRqTimeMs = CUSTOM_MILLIS;
        this->nbHeatpumpConnections_++;

        // we wait for a 10s timeout to check if the hp has replied to connection packet
        this->set_timeout("checkFirstConnection", 10000, [this]() {
            if (!this->isHeatpumpConnected()) {
                ESP_LOGE(LOG_CONN_TAG, "--> Heatpump did not reply: NOT CONNECTED <--");
                // Fallback automatique: si le mode installateur est demandé mais que la PAC ignore 0x5B,
                // on retente une fois en mode standard (0x5A) pour préserver la connectivité.
                if (this->installer_mode_ && this->installer_mode_effective_ && !this->installer_mode_fallback_done_) {
                    this->installer_mode_effective_ = false;
                    this->installer_mode_fallback_done_ = true;
                    ESP_LOGW(LOG_CONN_TAG, "No reply to installer handshake (0x5B). Falling back to standard handshake (0x5A).");
                }
                ESP_LOGI(LOG_CONN_TAG, "Reinitializing UART and trying to connect again...");
                this->reconnectUART();
            }});

    } else {
        ESP_LOGE(LOG_CONN_TAG, "UART doesn't seem to be connected...");
        this->setupUART();
        // this delay to prevent a logging flood should never happen
        CUSTOM_DELAY(750);
    }
}

// void CN105Climate::statusChanged() {
//     ESP_LOGD(TAG, "hpStatusChanged ->");
//     this->current_temperature = currentStatus.roomTemperature;

//     ESP_LOGD(TAG, "t°: %f", currentStatus.roomTemperature);
//     ESP_LOGD(TAG, "operating: %d", currentStatus.operating);
//     ESP_LOGD(TAG, "compressor freq: %f", currentStatus.compressorFrequency);

//     this->updateAction();
//     this->publish_state();
// }

void CN105Climate::prepareInfoPacket(uint8_t* packet, int length) {
    ESP_LOGV(TAG, "preparing info packet...");

    memset(packet, 0, length * sizeof(uint8_t));

    for (int i = 0; i < INFOHEADER_LEN && i < length; i++) {
        packet[i] = INFOHEADER[i];
    }
    if (length > 3) {
        packet[3] = this->profile_->id();
    }
}

void CN105Climate::prepareSetPacket(uint8_t* packet, int length) {
    ESP_LOGV(TAG, "preparing Set packet...");
    memset(packet, 0, length * sizeof(uint8_t));

    for (int i = 0; i < HEADER_LEN && i < length; i++) {
        packet[i] = HEADER[i];
    }
    if (length > 3) {
        packet[3] = this->profile_->id();
    }
}

void CN105Climate::writePacket(uint8_t* packet, int length, bool checkIsActive) {

    if ((this->isUARTReady_()) &&
        (this->isHeatpumpConnectionActive() || (!checkIsActive))) {

        ESP_LOGD(TAG, "writing packet...");
        this->hpPacketDebug(packet, length, "WRITE");

        for (int i = 0; i < length; i++) {
            this->get_hw_serial_()->write_byte((uint8_t)packet[i]);
        }

        // Prevent sending wantedSettings too soon after writing for example the remote temperature update packet
        this->lastSend = CUSTOM_MILLIS;

    } else {
        ESP_LOGW(TAG, "could not write as asked, because UART is not connected");
        this->reconnectUART();
        ESP_LOGW(TAG, "delaying packet writing because we need to reconnect first...");
        if (length > PACKET_LEN) {
            ESP_LOGE(TAG, "Packet length %d exceeds PACKET_LEN %d, dropping.", length, PACKET_LEN);
            return;
        }
        memcpy(this->pending_packet_, packet, static_cast<size_t>(length));
        this->pending_packet_len_ = length;
        this->pending_check_is_active_ = checkIsActive;
        this->has_pending_packet_ = true;
        this->set_timeout("write", 4000, [this]() { this->try_write_pending_packet(); });
    }
}

void CN105Climate::try_write_pending_packet() {
    if (!this->has_pending_packet_) return;
    if (!this->isUARTReady_()) {
        this->reconnectUART();
        this->set_timeout("write", 2000, [this]() { this->try_write_pending_packet(); });
        return;
    }
    this->writePacket(this->pending_packet_, this->pending_packet_len_, this->pending_check_is_active_);
    this->has_pending_packet_ = false;
}

const char* CN105Climate::getModeSetting() {
    if (this->wantedSettings.mode) {
        return this->wantedSettings.mode;
    } else {
        return this->currentSettings.mode;
    }
}

const char* CN105Climate::getPowerSetting() {
    if (this->wantedSettings.power) {
        return this->wantedSettings.power;
    } else {
        return this->currentSettings.power;
    }
}

const char* CN105Climate::getVaneSetting() {
    if (this->wantedSettings.vane) {
        return this->wantedSettings.vane;
    } else {
        return this->currentSettings.vane;
    }
}

const char* CN105Climate::getWideVaneSetting() {
    return this->profile_->wide_vane_setting();
}

const char* CN105Climate::getFanSpeedSetting() {
    if (this->wantedSettings.fan) {
        return this->wantedSettings.fan;
    } else {
        return this->currentSettings.fan;
    }
}

float CN105Climate::getTemperatureSetting() {
    if (this->wantedSettings.temperature != -1.0) {
        return this->wantedSettings.temperature;
    } else {
        return this->currentSettings.temperature;
    }
}
const char* CN105Climate::getAirflowControlSetting() {
    if (this->wantedRunStates.airflow_control) {
        return this->wantedRunStates.airflow_control;
    } else {
        return this->currentRunStates.airflow_control;
    }
}
bool CN105Climate::getAirPurifierRunState() {
    if (this->wantedRunStates.air_purifier != this->currentRunStates.air_purifier) {
        return this->wantedRunStates.air_purifier;
    } else {
        return this->currentRunStates.air_purifier;
    }
}
bool CN105Climate::getNightModeRunState() {
    if (this->wantedRunStates.night_mode != this->currentRunStates.night_mode) {
        return this->wantedRunStates.night_mode;
    } else {
        return this->currentRunStates.night_mode;
    }
}
bool CN105Climate::getCirculatorRunState() {
    if (this->wantedRunStates.circulator != this->currentRunStates.circulator) {
        return this->wantedRunStates.circulator;
    } else {
        return this->currentRunStates.circulator;
    }
}


void CN105Climate::createPacket(uint8_t* packet) {
    prepareSetPacket(packet, PACKET_LEN);

    this->profile_->encode_control(packet);
    packet[21] = checkSum(packet, 21);
}

void CN105Climate::publishWantedSettingsStateToHA() {
    this->profile_->apply_wanted_settings();
    this->publish_state();
}

void CN105Climate::publishWantedRunStatesStateToHA() {
    this->profile_->apply_wanted_run_states();
}


void CN105Climate::sendWantedSettingsDelegate() {
    this->wantedSettings.hasBeenSent = true;
    this->lastSend = CUSTOM_MILLIS;
    ESP_LOGI(TAG, "sending wantedSettings..");
    this->debugSettings("wantedSettings", wantedSettings);
    // and then we send the update packet
    uint8_t packet[PACKET_LEN] = {};
    this->createPacket(packet);
    this->writePacket(packet, PACKET_LEN);
    this->hpPacketDebug(packet, 22, "WRITE_SETTINGS");

    this->publishWantedSettingsStateToHA();

    // as soon as the packet is sent, we reset the settings
    this->wantedSettings.resetSettings();

    // as we've just sent a packet to the heatpump, we let it time for process
    // this might not be necessary but, we give it a try because of issue #32
    // https://github.com/echavet/MitsubishiCN105ESPHome/issues/32
    this->loopCycle.deferCycle();
}

/**
 * builds and send all an update packet to the heatpump
 *
 *
*/
void CN105Climate::sendWantedSettings() {
    if (this->isHeatpumpConnectionActive() && this->isUARTReady_()) {
        if (CUSTOM_MILLIS - this->lastSend > 300) {        // we don't want to send too many packets

            //this->cycleEnded();   // only if we let the cycle be interrupted to send wented settings

#ifdef USE_ESP32
            std::lock_guard<std::mutex> guard(wantedSettingsMutex);
            this->sendWantedSettingsDelegate();
#else
            this->emulateMutex("WRITE_SETTINGS", std::bind(&CN105Climate::sendWantedSettingsDelegate, this));

#endif

        } else {
            ESP_LOGD(TAG, "will sendWantedSettings later because we've sent one too recently...");
        }
    } else {
        this->reconnectIfConnectionLost();
    }
}

void CN105Climate::buildAndSendRequestPacket(int packetType) {
    // Legacy path kept temporarily if some callsites still pass packetType indices.
    // Map legacy indices to real codes and delegate to buildAndSendInfoPacket.
    uint8_t code = 0x02; // default to settings
    switch (packetType) {
    case 0: code = 0x02; break; // RQST_PKT_SETTINGS
    case 1: code = 0x03; break; // RQST_PKT_ROOM_TEMP
    case 2: code = 0x04; break; // RQST_PKT_UNKNOWN
    case 3: code = 0x05; break; // RQST_PKT_TIMERS
    case 4: code = 0x06; break; // RQST_PKT_STATUS
    case 5: code = 0x09; break; // RQST_PKT_STANDBY
    case 6: code = 0x42; break; // RQST_PKT_HVAC_OPTIONS
    default: code = 0x02; break;
    }
    this->buildAndSendInfoPacket(code);
}

void CN105Climate::buildAndSendInfoPacket(uint8_t code) {
    uint8_t packet[PACKET_LEN] = {};
    createInfoPacket(packet, code);
    this->writePacket(packet, PACKET_LEN);
}

void CN105Climate::buildAndSendRequestsInfoPackets() {
    if (this->isHeatpumpConnected()) {
        ESP_LOGV(LOG_UPD_INT_TAG, "triggering infopacket because of update interval tick");
        ESP_LOGV("CONTROL_WANTED_SETTINGS", "hasChanged is %s", wantedSettings.hasChanged ? "true" : "false");
        this->loopCycle.cycleStarted();
        this->nbCycles_++;
        // Envoie la première requête activable (la liste est enregistrée une fois au constructeur)
        this->scheduler_.send_next_after(0x00); // 0x00 -> start, pick first eligible
    } else {
        this->reconnectIfConnectionLost();
    }
}

void CN105Climate::createInfoPacket(uint8_t* packet, uint8_t code) {
    ESP_LOGD(TAG, "creating Info packet");
    prepareInfoPacket(packet, PACKET_LEN);

    // directly set requested info code (0x02, 0x03, 0x06, 0x09, 0x42, ...)
    packet[5] = code;

    // add the checksum
    uint8_t chkSum = checkSum(packet, 21);
    packet[21] = chkSum;
}


void CN105Climate::sendRemoteTemperaturePacket() {
    if (!this->profile_->allow_operation(cn105::ProfileFeature::REMOTE_TEMPERATURE)) return;

    // Build and send the remote temperature packet (0x07) without affecting watchdog/keep-alive timers

    // Debounce logic: avoid flooding the bus with identical temperature values
    // Only skip if: same temperature AND sent recently (within half of keep-alive interval, min 5s)
    uint32_t now = CUSTOM_MILLIS;
    uint32_t min_interval = this->remote_temp_keepalive_interval_ms_ > 0
        ? std::max(this->remote_temp_keepalive_interval_ms_ / 2, (uint32_t)5000)
        : 5000;  // Default 5s if keep-alive disabled

    bool temp_changed = (this->remoteTemperature_ != this->last_remote_temp_sent_);
    uint32_t elapsed = now - this->last_remote_temp_send_ms_;

    if (!temp_changed && elapsed < min_interval) {
        // Debounce: skip this send
        this->remote_temp_debounce_skip_count_++;

        // Detect conflicting heartbeat pattern: multiple rapid calls with same value
        // After 3 consecutive skips, warn the user (only once)
        // Only show warning if keep-alive is enabled - if disabled, manual heartbeat is intentional
        if (this->remote_temp_debounce_skip_count_ >= 3 &&
            !this->remote_temp_heartbeat_warning_shown_ &&
            this->remote_temp_keepalive_interval_ms_ > 0) {
            this->remote_temp_heartbeat_warning_shown_ = true;
            ESP_LOGW(LOG_REMOTE_TEMP,
                "Detected repeated remote temperature calls with unchanged value (%.1f). "
                "If you have a manual heartbeat/interval in YAML, consider removing it - "
                "the built-in keep-alive (every %lu ms) handles this automatically. "
                "See 'remote_temperature_keepalive_interval' option in documentation.",
                this->remoteTemperature_, (unsigned long)this->remote_temp_keepalive_interval_ms_);
        }

        ESP_LOGD(LOG_REMOTE_TEMP, "Debounce: skipping remote temp send (same value %.1f, %lu ms since last send, min interval %lu ms, skip #%d)",
            this->remoteTemperature_, (unsigned long)elapsed, (unsigned long)min_interval, this->remote_temp_debounce_skip_count_);
        return;
    }

    // Reset debounce skip counter on successful send
    this->remote_temp_debounce_skip_count_ = 0;

    uint8_t packet[PACKET_LEN] = {};

    prepareSetPacket(packet, PACKET_LEN);

    this->profile_->encode_remote_temperature(packet);
    // add the checksum
    uint8_t chkSum = checkSum(packet, 21);
    packet[21] = chkSum;

    ESP_LOGD(LOG_REMOTE_TEMP, "Sending remote temperature packet... -> %.1f%s",
        this->remoteTemperature_, temp_changed ? " (changed)" : " (keep-alive)");
    writePacket(packet, PACKET_LEN);

    // Update debounce tracking
    this->last_remote_temp_send_ms_ = now;
    this->last_remote_temp_sent_ = this->remoteTemperature_;
}

void CN105Climate::sendRemoteTemperature() {
    this->shouldSendExternalTemperature_ = false;

    // Send the packet
    this->sendRemoteTemperaturePacket();
}

void CN105Climate::sendWantedRunStates() {
    if (!this->profile_->allow_operation(cn105::ProfileFeature::AUXILIARY_CONTROLS)) {
        this->wantedRunStates.resetSettings();
        return;
    }

    uint8_t packet[PACKET_LEN] = {};

    prepareSetPacket(packet, PACKET_LEN);

    this->profile_->encode_run_states(packet);
    // Add the checksum
    uint8_t chkSum = checkSum(packet, 21);
    packet[21] = chkSum;
    ESP_LOGD(LOG_SET_RUN_STATE, "Sending set run state package (0x08)");
    writePacket(packet, PACKET_LEN);

    this->publishWantedRunStatesStateToHA();

    this->wantedRunStates.resetSettings();
    this->loopCycle.deferCycle();
}
