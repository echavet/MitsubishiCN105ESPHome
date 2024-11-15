#include "cn105.h"



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
        ESP_LOGD(TAG, "Envoi du packet de connexion...");
        uint8_t packet[CONNECT_LEN];
        memcpy(packet, CONNECT, CONNECT_LEN);
        //for(int count = 0; count < 2; count++) {

        this->writePacket(packet, CONNECT_LEN, false);      // checkIsActive=false because it's the first packet and we don't have any reply yet

        this->lastSend = CUSTOM_MILLIS;
        this->lastConnectRqTimeMs = CUSTOM_MILLIS;
        this->nbHeatpumpConnections_++;

        // we wait for a 10s timeout to check if the hp has replied to connection packet
        this->set_timeout("checkFirstConnection", 10000, [this]() {
            if (!this->isHeatpumpConnected_) {
                ESP_LOGE(TAG, "--> Heatpump did not reply: NOT CONNECTED <--");
                ESP_LOGI(TAG, "Trying to connect again...");
                this->sendFirstConnectionPacket();
            }});

    } else {
        ESP_LOGE(TAG, "UART doesn't seem to be connected...");
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
        this->set_timeout("write", 4000, [this, packet, length]() { this->writePacket(packet, length); });
    }
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



void CN105Climate::createPacket(uint8_t* packet) {
    prepareSetPacket(packet, PACKET_LEN);

    //ESP_LOGD(TAG, "checking differences bw asked settings and current ones...");
    ESP_LOGD(TAG, "building packet for writing...");

    if (this->wantedSettings.power != nullptr) {
        ESP_LOGD(TAG, "power -> %s", getPowerSetting());
        packet[8] = POWER[lookupByteMapIndex(POWER_MAP, 2, getPowerSetting(), "power (write)")];
        packet[6] += CONTROL_PACKET_1[0];
    }

    if (this->wantedSettings.mode != nullptr) {
        ESP_LOGD(TAG, "heatpump mode -> %s", getModeSetting());
        packet[9] = MODE[lookupByteMapIndex(MODE_MAP, 5, getModeSetting(), "mode (write)")];
        packet[6] += CONTROL_PACKET_1[1];
    }

    if (wantedSettings.temperature != -1) {
        if (!tempMode) {
            ESP_LOGD(TAG, "temperature (tempmode is false) -> %f", getTemperatureSetting());
            packet[10] = TEMP[lookupByteMapIndex(TEMP_MAP, 16, getTemperatureSetting(), "temperature (write)")];
            packet[6] += CONTROL_PACKET_1[2];
        } else {
            ESP_LOGD(TAG, "temperature (tempmode is true) -> %f", getTemperatureSetting());
            float temp = (getTemperatureSetting() * 2) + 128;
            packet[19] = (int)temp;
            packet[6] += CONTROL_PACKET_1[2];
        }
    }

    if (this->wantedSettings.fan != nullptr) {
        ESP_LOGD(TAG, "heatpump fan -> %s", getFanSpeedSetting());
        packet[11] = FAN[lookupByteMapIndex(FAN_MAP, 6, getFanSpeedSetting(), "fan (write)")];
        packet[6] += CONTROL_PACKET_1[3];
    }

    if (this->wantedSettings.vane != nullptr) {
        ESP_LOGD(TAG, "heatpump vane -> %s", getVaneSetting());
        packet[12] = VANE[lookupByteMapIndex(VANE_MAP, 7, getVaneSetting(), "vane (write)")];
        packet[6] += CONTROL_PACKET_1[4];
    }

    if (this->wantedSettings.wideVane != nullptr) {
        ESP_LOGD(TAG, "heatpump widevane -> %s", getWideVaneSetting());
        packet[18] = WIDEVANE[lookupByteMapIndex(WIDEVANE_MAP, 7, getWideVaneSetting(), "wideVane (write)")] | (this->wideVaneAdj ? 0x80 : 0x00);
        packet[7] += CONTROL_PACKET_2[0];
    }


    // add the checksum
    uint8_t chkSum = checkSum(packet, 21);
    packet[21] = chkSum;
    //ESP_LOGD(TAG, "debug before write packet:");
    //this->hpPacketDebug(packet, 22, "WRITE");
}





void CN105Climate::publishWantedSettingsStateToHA() {

    if ((this->wantedSettings.mode != nullptr) || (this->wantedSettings.mode != nullptr)) {
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
    this->target_temperature = this->getTemperatureSetting();

    // publish to HA
    this->publish_state();

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
    uint8_t packet[PACKET_LEN] = {};
    createInfoPacket(packet, packetType);
    this->writePacket(packet, PACKET_LEN);
}



void CN105Climate::buildAndSendRequestsInfoPackets() {
    if (this->isHeatpumpConnected_) {
        ESP_LOGV(LOG_UPD_INT_TAG, "triggering infopacket because of update interval tick");
        ESP_LOGV("CONTROL_WANTED_SETTINGS", "hasChanged is %s", wantedSettings.hasChanged ? "true" : "false");
        ESP_LOGD(TAG, "sending a request for settings packet (0x02)");
        this->loopCycle.cycleStarted();
        this->nbCycles_++;
        ESP_LOGD(LOG_CYCLE_TAG, "2a: Sending settings request (0x02)");
        this->buildAndSendRequestPacket(RQST_PKT_SETTINGS);
    } else {
        this->reconnectIfConnectionLost();
    }
}





void CN105Climate::createInfoPacket(uint8_t* packet, uint8_t packetType) {
    ESP_LOGD(TAG, "creating Info packet");
    // add the header to the packet
    for (int i = 0; i < INFOHEADER_LEN; i++) {
        packet[i] = INFOHEADER[i];
    }

    // set the mode - settings or room temperature
    if (packetType != PACKET_TYPE_DEFAULT) {
        packet[5] = INFOMODE[packetType];
    } else {
        // request current infoMode, and increment for the next request
        packet[5] = INFOMODE[infoMode];
        if (infoMode == (INFOMODE_LEN - 1)) {
            infoMode = 0;
        } else {
            infoMode++;
        }
    }

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
        this->remoteTemperature_ = this->remoteTemperature_ * 2;
        this->remoteTemperature_ = round(this->remoteTemperature_);
        this->remoteTemperature_ = this->remoteTemperature_ / 2;
        float temp1 = 3 + ((this->remoteTemperature_ - 10) * 2);
        packet[7] = (int)temp1;
        float temp2 = (this->remoteTemperature_ * 2) + 128;
        packet[8] = (int)temp2;
    } else {
        packet[6] = 0x00;
        packet[8] = 0x80; //MHK1 send 80, even though it could be 00, since ControlByte is 00
    }
    // add the checksum
    uint8_t chkSum = checkSum(packet, 21);
    packet[21] = chkSum;
    ESP_LOGD(TAG, "sending remote temperature packet...");
    writePacket(packet, PACKET_LEN);

    // this resets the timeout
    this->pingExternalTemperature();


}
