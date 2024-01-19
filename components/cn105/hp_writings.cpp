#include "cn105.h"



byte CN105Climate::checkSum(uint8_t bytes[], int len) {
    uint8_t sum = 0;
    for (int i = 0; i < len; i++) {
        sum += bytes[i];
    }
    return (0xfc - sum) & 0xff;
}


void CN105Climate::sendFirstConnectionPacket() {
    if (this->isConnected_) {
        this->isHeatpumpConnected_ = false;

        ESP_LOGD(TAG, "Envoi du packet de connexion...");
        uint8_t packet[CONNECT_LEN];
        memcpy(packet, CONNECT, CONNECT_LEN);
        //for(int count = 0; count < 2; count++) {

        this->writePacket(packet, CONNECT_LEN, false);      // checkIsActive=false because it's the first packet and we don't have any reply yet

        lastSend = CUSTOM_MILLIS;

        // we wait for a 4s timeout to check if the hp has replied to connection packet
        this->set_timeout("checkFirstConnection", 4000, [this]() {
            if (!this->isHeatpumpConnected_) {
                ESP_LOGE(TAG, "--> Heatpump did not reply: NOT CONNECTED <--");
            } else {
                // not usefull because the response has been processed in processCommand()
            }

            });

    } else {
        ESP_LOGE(TAG, "Vous devez dabord connecter l'appareil via l'UART");
    }
}




void CN105Climate::statusChanged() {
    ESP_LOGD(TAG, "hpStatusChanged ->");
    this->current_temperature = currentStatus.roomTemperature;

    ESP_LOGD(TAG, "t°: %f", currentStatus.roomTemperature);
    ESP_LOGD(TAG, "operating: %d", currentStatus.operating);
    ESP_LOGD(TAG, "compressor freq: %d", currentStatus.compressorFrequency);

    this->updateAction();
    this->publish_state();
}

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

    if ((this->isConnected_) &&
        (this->isHeatpumpConnectionActive() || (!checkIsActive))) {

        if (this->get_hw_serial_()->availableForWrite() >= length) {
            ESP_LOGD(TAG, "writing packet...");
            this->hpPacketDebug(packet, length, "WRITE");

            for (int i = 0; i < length; i++) {
                this->get_hw_serial_()->write((uint8_t)packet[i]);
            }
        } else {
            ESP_LOGW(TAG, "delaying packet writing because serial buffer is not ready...");
            this->set_timeout("write", 200, [this, packet, length]() { this->writePacket(packet, length); });
        }
    } else {
        ESP_LOGW(TAG, "could not write as asked, because UART is not connected");
        this->disconnectUART();
        this->setupUART();
        this->sendFirstConnectionPacket();

        ESP_LOGW(TAG, "delaying packet writing because we need to reconnect first...");
        this->set_timeout("write", 500, [this, packet, length]() { this->writePacket(packet, length); });
    }
}

void CN105Climate::createPacket(byte* packet, heatpumpSettings settings) {
    prepareSetPacket(packet, PACKET_LEN);

    ESP_LOGD(TAG, "checking differences bw asked settings and current ones...");



    //if (this->hasChanged(currentSettings.power, settings.power, "power (wantedSettings)")) {
    ESP_LOGD(TAG, "power is always set -> %s", settings.power);
    packet[8] = POWER[lookupByteMapIndex(POWER_MAP, 2, settings.power)];
    packet[6] += CONTROL_PACKET_1[0];
    //}

    //if (this->hasChanged(currentSettings.mode, settings.mode, "mode (wantedSettings)")) {
    ESP_LOGD(TAG, "heatpump mode changed -> %s", settings.mode);
    packet[9] = MODE[lookupByteMapIndex(MODE_MAP, 5, settings.mode)];
    packet[6] += CONTROL_PACKET_1[1];
    //}
    //if (!tempMode && settings.temperature != currentSettings.temperature) {
    if (!tempMode) {
        ESP_LOGD(TAG, "temperature changed (tempmode is false) -> %f", settings.temperature);
        packet[10] = TEMP[lookupByteMapIndex(TEMP_MAP, 16, settings.temperature)];
        packet[6] += CONTROL_PACKET_1[2];
        //} else if (tempMode && settings.temperature != currentSettings.temperature) {
    } else {
        ESP_LOGD(TAG, "temperature changed (tempmode is true) -> %f", settings.temperature);
        float temp = (settings.temperature * 2) + 128;
        packet[19] = (int)temp;
        packet[6] += CONTROL_PACKET_1[2];
    }

    //if (this->hasChanged(currentSettings.fan, settings.fan, "fan (wantedSettings)")) {
    ESP_LOGD(TAG, "heatpump fan changed -> %s", settings.fan);
    packet[11] = FAN[lookupByteMapIndex(FAN_MAP, 6, settings.fan)];
    packet[6] += CONTROL_PACKET_1[3];
    //}

    //if (this->hasChanged(currentSettings.vane, settings.vane, "vane (wantedSettings)")) {
    ESP_LOGD(TAG, "heatpump vane changed -> %s", settings.vane);
    packet[12] = VANE[lookupByteMapIndex(VANE_MAP, 7, settings.vane)];
    packet[6] += CONTROL_PACKET_1[4];
    //}

    // add the checksum
    uint8_t chkSum = checkSum(packet, 21);
    packet[21] = chkSum;
    //ESP_LOGD(TAG, "debug before write packet:");
    //this->hpPacketDebug(packet, 22, "WRITE");
}

/**
 * builds and send all an update packet to the heatpump
 * SHEDULER_INTERVAL_SYNC_NAME scheduler is canceled
 *
 *
*/
void CN105Climate::sendWantedSettings() {

    if (this->isHeatpumpConnectionActive() && this->isConnected_) {
        if (CUSTOM_MILLIS - this->lastSend > 500) {        // we don't want to send too many packets

            this->wantedSettings.hasBeenSent = true;
            this->lastSend = CUSTOM_MILLIS;
            ESP_LOGI(TAG, "sending wantedSettings..");

            this->debugSettings("wantedSettings", wantedSettings);

            if (this->autoUpdate) {
                ESP_LOGD(TAG, "cancelling the update loop during the push of the settings..");
                /*  we don't want the autoupdate loop to interfere with this packet communication
                    So we first cancel the SHEDULER_INTERVAL_SYNC_NAME */
                this->cancel_timeout(SHEDULER_INTERVAL_SYNC_NAME);
                this->cancel_timeout("2ndPacket");
                this->cancel_timeout("3rdPacket");
            }

            // and then we send the update packet
            byte packet[PACKET_LEN] = {};
            this->createPacket(packet, wantedSettings);
            this->writePacket(packet, PACKET_LEN);
            this->hpPacketDebug(packet, 22, "WRITE_SETTINGS");

            // here we know the update packet has been sent but we don't know if it has been received
            // so we have to program a check to be sure we will get a response
            // todo: we should check if we are already waiting for a response before programming a new check
            // todo: initialise wantedSettings only when the response is received

            // wantedSettings are sent so we don't need to keep them anymore
            // this is usefull because we might look at wantedSettings later to check if a request is pending
            // wantedSettings = {};
            // wantedSettings.temperature = -1;    // to know user did not ask 

            // here we restore the update scheduler we had canceled 
            this->set_timeout(DEFER_SHEDULER_INTERVAL_SYNC_NAME, DEFER_SCHEDULE_UPDATE_LOOP_DELAY, [this]() {
                this->programUpdateInterval();
                });

        } else {
            ESP_LOGD(TAG, "will sendWantedSettings later because we've sent one too recently...");
        }
    }

}



// void CN105Climate::programResponseCheck(byte* packet) {
//     int packetType = packet[5];
//     // 0x01 Settings
//     // 0x06 status
//     // 0x07 Update remote temp
//     if ((packetType == 0x01) || (packetType == 0x06) || (packetType == 0x07)) {
//         // increment the counter which will be decremented by the response handled by
//         // getDataFromResponsePacket() method case 0x06 
//         // processCommand (case 0x61)
//         this->nonResponseCounter++;

//     }

// }

// TODO: changer cette methode afin qu'elle programme le check aussi pour les paquets de 
// setRemoteTemperature et pour les sendWantedSettings
// deprecated: we now use isHeatpumpConnectionActive() each time a packet is written 
// checkPoints are when we get a packet and before we send one

void CN105Climate::programResponseCheck(int packetType) {
    if (packetType == RQST_PKT_STATUS) {
        // increment the counter which will be decremented by the response handled by
        //getDataFromResponsePacket() method case 0x06
        this->nonResponseCounter++;

        this->set_timeout("checkpacketResponse", this->update_interval_ * 0.9, [this]() {

            if (this->nonResponseCounter > MAX_NON_RESPONSE_REQ) {
                ESP_LOGI(TAG, "There are too many status resquests without response: %d of max %d", this->nonResponseCounter, MAX_NON_RESPONSE_REQ);
                ESP_LOGI(TAG, "Heater is not connected anymore");
                this->disconnectUART();
                this->setupUART();
                this->sendFirstConnectionPacket();
                this->nonResponseCounter = 0;
            }

            });
    }

}
void CN105Climate::buildAndSendRequestPacket(int packetType) {
    uint8_t packet[PACKET_LEN] = {};
    createInfoPacket(packet, packetType);
    this->writePacket(packet, PACKET_LEN);
}


/**
 * builds ans send all 3 types of packet to get a full informations back from heatpump
 * 3 packets are sent at 300 ms interval
*/
void CN105Climate::buildAndSendRequestsInfoPackets() {

    ESP_LOGD("CONTROL_WANTED_SETTINGS", "buildAndSendRequestsInfoPackets() wantedSettings.hasChanged is %s", wantedSettings.hasChanged ? "true" : "false");

    if (!wantedSettings.hasChanged) {       // we don't want to interfere with the update settings process, this is a user command

        if (this->isHeatpumpConnected_) {

            uint32_t interval = 300;
            if (this->update_interval_ > 0) {
                // we get the max interval of update_interval_ / 4 or interval (300)
                interval = (this->update_interval_ / 4) > interval ? interval : (this->update_interval_ / 4);
            }

            ESP_LOGD(TAG, "buildAndSendRequestsInfoPackets: sending 3 request packet at interval: %d", interval);

            ESP_LOGD(TAG, "sending a request for settings packet (0x02)");
            this->buildAndSendRequestPacket(RQST_PKT_SETTINGS);
            this->set_timeout("2ndPacket", interval, [this, interval]() {
                ESP_LOGD(TAG, "sending a request room temp packet (0x03)");
                this->buildAndSendRequestPacket(RQST_PKT_ROOM_TEMP);
                this->set_timeout("3rdPacket", interval, [this]() {
                    ESP_LOGD(TAG, "sending a request status paquet (0x06)");
                    this->buildAndSendRequestPacket(RQST_PKT_STATUS);
                    });
                });

        } else {
            ESP_LOGE(TAG, "sync impossible: heatpump not connected");
            //this->setupUART();
            //this->sendFirstConnectionPacket();
        }
    } else {
        // here we want to manage a case that might happen:
        // "updateSuccess response (ACK)  was never received"
        // in this case: as we don't call buildAndSendRequestPacket()  wantedSettings.hasChanged will never be set to false
        // But actually, if this happens, wantedSettings.hasChanged keeps being true, and we keep sending the same settings
        // over and over again.
        // In this case with have to set a limit to the number of deffered requests here

        ESP_LOGD("CONTROL_WANTED_SETTINGS", "deffering requestInfo because wantedSettings.hasChanged is true");

        wantedSettings.nb_deffered_requests++;

        if (wantedSettings.nb_deffered_requests > 10) {
            ESP_LOGW(TAG, "update success ACK was never received or never sent");
            ESP_LOGW(TAG, "we're probably not connected to heatpump anymore");
            wantedSettings.nb_deffered_requests = 0;
            this->reconnectUART();
        }
    }
    this->programUpdateInterval();
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
void CN105Climate::set_remote_temperature(float setting) {
    uint8_t packet[PACKET_LEN] = {};

    prepareSetPacket(packet, PACKET_LEN);

    packet[5] = 0x07;
    if (setting > 0) {
        packet[6] = 0x01;
        setting = setting * 2;
        setting = round(setting);
        setting = setting / 2;
        float temp1 = 3 + ((setting - 10) * 2);
        packet[7] = (int)temp1;
        float temp2 = (setting * 2) + 128;
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
    // optimistic
    this->currentStatus.roomTemperature = setting;
}