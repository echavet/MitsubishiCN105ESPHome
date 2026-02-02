#include "cn105.h"

using namespace esphome;

uint8_t CN105Climate::checkSum(uint8_t bytes[], int len) {
    uint8_t sum = 0;
    for (int i = 0; i < len; i++) {
        sum += bytes[i];
    }
    return (0xfc - sum) & 0xff;
}


void CN105Climate::sendFirstConnectionPacket() {
    if (this->isUARTConnected_) {
        this->lastReconnectTimeMs = CUSTOM_MILLIS;          // marker to prevent to many reconnections
        this->setHeatpumpConnected(false);
        uint8_t packet[CONNECT_LEN];
        memcpy(packet, CONNECT, CONNECT_LEN);

        // Choix du mode de handshake: standard (0x5A) ou installateur (0x5B)
        packet[1] = this->installer_mode_effective_ ? 0x5B : 0x5A;
        // CONNECT a un checksum pré-calculé dans la constante; si on modifie l'octet commande, on doit le recalculer.
        packet[CONNECT_LEN - 1] = checkSum(packet, CONNECT_LEN - 1);

        ESP_LOGI(LOG_CONN_TAG, "Envoi du paquet de connexion en mode %s (0x%02X)...", this->installer_mode_effective_ ? "Installateur" : "Standard", packet[1]);

        // Détails des octets en DEBUG sur le tag de connexion
        this->hpPacketDebug(packet, CONNECT_LEN, LOG_CONN_TAG);

        this->writePacket(packet, CONNECT_LEN, false);      // checkIsActive=false because it's the first packet and we don't have any reply yet

        this->lastSend = CUSTOM_MILLIS;
        this->lastConnectRqTimeMs = CUSTOM_MILLIS;
        this->nbHeatpumpConnections_++;

        // we wait for a 10s timeout to check if the hp has replied to connection packet
        this->set_timeout("checkFirstConnection", 10000, [this]() {
            if (!this->isHeatpumpConnected_) {
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
}

void CN105Climate::prepareSetPacket(uint8_t* packet, int length) {
    ESP_LOGV(TAG, "preparing Set packet...");
    memset(packet, 0, length * sizeof(uint8_t));

    for (int i = 0; i < HEADER_LEN && i < length; i++) {
        packet[i] = HEADER[i];
    }
}

void CN105Climate::writePacket(uint8_t* packet, int length, bool checkIsActive) {

    if ((this->isUARTConnected_) &&
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
    if (!this->isUARTConnected_) {
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
    if (this->wantedSettings.wideVane) {
        if (strcmp(this->wantedSettings.wideVane, lookupByteMapValue(WIDEVANE_MAP, WIDEVANE, 8, 0x80 & 0x0F)) == 0 && !this->currentSettings.iSee) {
            this->wantedSettings.wideVane = this->currentSettings.wideVane;
        }
        return this->wantedSettings.wideVane;
    } else {
        return this->currentSettings.wideVane;
    }
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

    //ESP_LOGD(TAG, "checking differences bw asked settings and current ones...");
    ESP_LOGD(TAG, "building packet for writing...");

    if (this->wantedSettings.power != nullptr) {
        ESP_LOGD(TAG, "power -> %s", getPowerSetting());
        int idx = lookupByteMapIndex(POWER_MAP, 2, getPowerSetting(), "power (write)");
        if (idx >= 0) { packet[8] = POWER[idx]; packet[6] += CONTROL_PACKET_1[0]; } else { ESP_LOGW(TAG, "Ignoring invalid power setting while building packet"); }
    }

    if (this->wantedSettings.mode != nullptr) {
        ESP_LOGD(TAG, "heatpump mode -> %s", getModeSetting());
        int idx = lookupByteMapIndex(MODE_MAP, 5, getModeSetting(), "mode (write)");
        if (idx >= 0) { packet[9] = MODE[idx]; packet[6] += CONTROL_PACKET_1[1]; } else { ESP_LOGW(TAG, "Ignoring invalid mode setting while building packet"); }
    }

    if (wantedSettings.temperature != -1) {
        if (!tempMode) {
            ESP_LOGD(TAG, "temperature (tempmode is false) -> %f", getTemperatureSetting());
            int idx = lookupByteMapIndex(TEMP_MAP, 16, getTemperatureSetting(), "temperature (write)");
            if (idx >= 0) { packet[10] = TEMP[idx]; packet[6] += CONTROL_PACKET_1[2]; } else { ESP_LOGW(TAG, "Ignoring invalid temperature setting while building packet"); }
        } else {
            ESP_LOGD(TAG, "temperature (tempmode is true) -> %f", getTemperatureSetting());
            float temp = (getTemperatureSetting() * 2) + 128;
            packet[19] = (int)temp;
            packet[6] += CONTROL_PACKET_1[2];
        }
    }

    if (this->wantedSettings.fan != nullptr) {
        ESP_LOGD(TAG, "heatpump fan -> %s", getFanSpeedSetting());
        int idx = lookupByteMapIndex(FAN_MAP, 6, getFanSpeedSetting(), "fan (write)");
        if (idx >= 0) { packet[11] = FAN[idx]; packet[6] += CONTROL_PACKET_1[3]; } else { ESP_LOGW(TAG, "Ignoring invalid fan setting while building packet"); }
    }

    if (this->wantedSettings.vane != nullptr) {
        ESP_LOGD(TAG, "heatpump vane -> %s", getVaneSetting());
        int idx = lookupByteMapIndex(VANE_MAP, 7, getVaneSetting(), "vane (write)");
        if (idx >= 0) { packet[12] = VANE[idx]; packet[6] += CONTROL_PACKET_1[4]; } else { ESP_LOGW(TAG, "Ignoring invalid vane setting while building packet"); }
    }

    if (this->wantedSettings.wideVane != nullptr) {
        ESP_LOGD(TAG, "heatpump widevane -> %s", getWideVaneSetting());
        int idx = lookupByteMapIndex(WIDEVANE_MAP, 8, getWideVaneSetting(), "wideVane (write)");
        if (idx >= 0) {
            packet[18] = WIDEVANE[idx] | (this->wideVaneAdj ? 0x80 : 0x00);
            packet[7] += CONTROL_PACKET_2[0];

            // Experimental: Left Horizontal Vane support for dual vane units
            // Byte 16 is used in IR protocol for Left Vane (which corresponds to Horizontal/Wide Vane on these units)
            if (this->horizontal_vanes_ > 1) {
                // Copy the base WIDEVANE value (without adjustment bit) to Byte 16
                packet[16] = WIDEVANE[idx];
            }
        } else { ESP_LOGW(TAG, "Ignoring invalid wideVane setting while building packet"); }
    }


    // add the checksum
    uint8_t chkSum = checkSum(packet, 21);
    packet[21] = chkSum;
    //ESP_LOGD(TAG, "debug before write packet:");
    //this->hpPacketDebug(packet, 22, "WRITE");
}





void CN105Climate::publishWantedSettingsStateToHA() {

    if ((this->wantedSettings.mode != nullptr) || (this->wantedSettings.power != nullptr)) {
        checkPowerAndModeSettings(this->wantedSettings, false);
        this->updateAction();       // update action info on HA climate component
    }

    if (this->wantedSettings.fan != nullptr) {
        checkFanSettings(this->wantedSettings, false);
    }


    if ((this->wantedSettings.vane != nullptr) || (this->wantedSettings.wideVane != nullptr)) {
        if (this->wantedSettings.vane == nullptr) { // to prevent a nullpointer error
            this->wantedSettings.vane = this->currentSettings.vane;
        }
        if (this->wantedSettings.wideVane == nullptr) { // to prevent a nullpointer error
            this->wantedSettings.wideVane = this->currentSettings.wideVane;
        }

        checkVaneSettings(this->wantedSettings, false);
    }

    // HA Temp
    this->updateTargetTemperaturesFromSettings(this->getTemperatureSetting());

    // publish to HA
    this->publish_state();

}

void CN105Climate::publishWantedRunStatesStateToHA() {
    if (this->wantedRunStates.airflow_control != nullptr) {
        if (this->wantedRunStates.airflow_control == nullptr) {
            this->wantedRunStates.airflow_control = this->currentRunStates.airflow_control;
        }
        if (this->hasChanged(this->airflow_control_select_->current_option(), this->wantedRunStates.airflow_control, "select airflow control")) {
            ESP_LOGI(TAG, "airflow control setting changed");
            this->airflow_control_select_->publish_state(wantedRunStates.airflow_control);
        }
    }
    if (this->wantedRunStates.air_purifier > -1) {
        if (this->wantedRunStates.air_purifier == -1) {
            this->wantedRunStates.air_purifier = this->currentRunStates.air_purifier;
        }
        if (this->air_purifier_switch_->state != this->wantedRunStates.air_purifier) {
            ESP_LOGI(TAG, "air purifier setting changed");
            this->air_purifier_switch_->publish_state(wantedRunStates.air_purifier);
        }
    }
    if (this->wantedRunStates.night_mode > -1) {
        if (this->wantedRunStates.night_mode == -1) {
            this->wantedRunStates.night_mode = this->currentRunStates.night_mode;
        }
        if (this->night_mode_switch_->state != this->wantedRunStates.night_mode) {
            ESP_LOGI(TAG, "night mode setting changed");
            this->night_mode_switch_->publish_state(wantedRunStates.night_mode);
        }
    }
    if (this->wantedRunStates.circulator > -1) {
        if (this->wantedRunStates.circulator == -1) {
            this->wantedRunStates.circulator = this->currentRunStates.circulator;
        }
        if (this->circulator_switch_->state != this->wantedRunStates.circulator) {
            ESP_LOGI(TAG, "circulator setting changed");
            this->circulator_switch_->publish_state(wantedRunStates.circulator);
        }
    }
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
    if (this->isHeatpumpConnectionActive() && this->isUARTConnected_) {
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
    if (this->isHeatpumpConnected_) {
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
    // add the header to the packet
    for (int i = 0; i < INFOHEADER_LEN; i++) {
        packet[i] = INFOHEADER[i];
    }

    // directly set requested info code (0x02, 0x03, 0x06, 0x09, 0x42, ...)
    packet[5] = code;

    // pad the packet out
    for (int i = 0; i < 15; i++) {
        packet[i + 6] = 0x00;
    }

    // add the checksum
    uint8_t chkSum = checkSum(packet, 21);
    packet[21] = chkSum;
}


void CN105Climate::sendRemoteTemperature() {

    this->shouldSendExternalTemperature_ = false;

    uint8_t packet[PACKET_LEN] = {};

    prepareSetPacket(packet, PACKET_LEN);

    packet[5] = 0x07;
    if (this->remoteTemperature_ > 0) {
        packet[6] = 0x01;
        float temp = round(this->remoteTemperature_ * 2);
        packet[7] = static_cast<uint8_t>(temp - 16);
        packet[8] = static_cast<uint8_t>(temp + 128);
    } else {
        packet[8] = 0x80; //MHK1 send 80, even though it could be 00, since ControlByte is 00
    }
    // add the checksum
    uint8_t chkSum = checkSum(packet, 21);
    packet[21] = chkSum;
    ESP_LOGD(LOG_REMOTE_TEMP, "Sending remote temperature packet... -> %f", this->remoteTemperature_);
    writePacket(packet, PACKET_LEN);

    // this resets the timeout
    this->pingExternalTemperature();
}

void CN105Climate::sendWantedRunStates() {
    uint8_t packet[PACKET_LEN] = {};

    prepareSetPacket(packet, PACKET_LEN);

    packet[5] = 0x08;
    if (this->wantedRunStates.airflow_control != nullptr) {
        ESP_LOGD(TAG, "airflow control -> %s", getAirflowControlSetting());
        packet[11] = AIRFLOW_CONTROL[lookupByteMapIndex(AIRFLOW_CONTROL_MAP, 3, getAirflowControlSetting(), "run state (write)")];
        packet[6] += RUN_STATE_PACKET_1[4];
    }
    if (this->wantedRunStates.air_purifier > -1) {
        if (getAirPurifierRunState() != currentRunStates.air_purifier) {
            ESP_LOGI(TAG, "air purifier switch state -> %s", getAirPurifierRunState() ? "ON" : "OFF");
            packet[17] = getAirPurifierRunState() ? 0x01 : 0x00;
            packet[7] += RUN_STATE_PACKET_2[1];
        }
    }
    if (this->wantedRunStates.night_mode > -1) {
        if (getNightModeRunState() != currentRunStates.night_mode) {
            ESP_LOGI(TAG, "night mode switch state -> %s", this->getNightModeRunState() ? "ON" : "OFF");
            packet[18] = getNightModeRunState() ? 0x01 : 0x00;
            packet[7] += RUN_STATE_PACKET_2[2];
        }
    }
    if (this->wantedRunStates.circulator > -1) {
        if (getCirculatorRunState() != currentRunStates.circulator) {
            ESP_LOGI(TAG, "circulator switch state -> %s", getCirculatorRunState() ? "ON" : "OFF");
            packet[19] = getCirculatorRunState() ? 0x01 : 0x00;
            packet[7] += RUN_STATE_PACKET_2[3];
        }
    }

    // Add the checksum
    uint8_t chkSum = checkSum(packet, 21);
    packet[21] = chkSum;
    ESP_LOGD(LOG_SET_RUN_STATE, "Sending set run state package (0x08)");
    writePacket(packet, PACKET_LEN);

    this->publishWantedRunStatesStateToHA();

    this->wantedRunStates.resetSettings();
    this->loopCycle.deferCycle();
}
