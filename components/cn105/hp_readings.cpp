#include "cn105.h"

/**
 * Seek the byte pointer to the beginning of the array
 * Initializes few variables
*/
void CN105Climate::initBytePointer() {
    this->foundStart = false;
    this->bytesRead = 0;
    this->dataLength = -1;
    this->command = 0;
}

/**
 *
* The total size of a frame is made up of several elements:
  * Header size: The header has a fixed length of 5 bytes (INFOHEADER_LEN).
  * Data Length: The data length is variable and is specified by the fourth byte of the header (header[4]).
  * Checksum: There is 1 checksum byte at the end of the frame.
  *
  * The total size of a frame is therefore the sum of these elements: header size (5 bytes) + data length (variable) + checksum (1 byte).
  * To calculate the total size, we can use the formula:
  * Total size = 5 (header) + Data length + 1 (checksum)
  *The total size depends on the specific data length for each individual frame.
 */
void CN105Climate::parse(uint8_t inputData) {

    ESP_LOGV("Decoder", "--> %02X [nb: %d]", inputData, this->bytesRead);

    if (!this->foundStart) {                // no packet yet
        if (inputData == HEADER[0]) {
            this->foundStart = true;
            storedInputData[this->bytesRead++] = inputData;
        } else {
            // unknown bytes
        }
    } else {                                // we are getting a packet
        storedInputData[this->bytesRead] = inputData;

        checkHeader(inputData);

        if (this->dataLength != -1) {       // is header complete ?

            if ((this->bytesRead) == this->dataLength + 5) {

                this->processDataPacket();
                this->initBytePointer();
            } else {                                        // packet is still filling
                this->bytesRead++;                          // more data to come
            }
        } else {
            ESP_LOGV("Decoder", "data length toujours pas connu");
            // header is not complete yet
            this->bytesRead++;
        }
    }

}


bool CN105Climate::checkSum() {
    // TODO: use the CN105Climate::checkSum(byte bytes[], int len) function

    uint8_t packetCheckSum = storedInputData[this->bytesRead];
    uint8_t processedCS = 0;

    ESP_LOGV("chkSum", "controling chkSum should be: %02X ", packetCheckSum);

    for (int i = 0;i < this->dataLength + 5;i++) {
        ESP_LOGV("chkSum", "adding %02X to %03X --> %X", this->storedInputData[i], processedCS, processedCS + this->storedInputData[i]);
        processedCS += this->storedInputData[i];
    }

    processedCS = (0xfc - processedCS) & 0xff;

    if (packetCheckSum == processedCS) {
        ESP_LOGD("chkSum", "OK-> %02X=%02X ", processedCS, packetCheckSum);
    } else {
        ESP_LOGW("chkSum", "KO-> %02X!=%02X ", processedCS, packetCheckSum);
    }

    return (packetCheckSum == processedCS);
}


void CN105Climate::checkHeader(uint8_t inputData) {
    if (this->bytesRead == 4) {
        if (storedInputData[2] == HEADER[2] && storedInputData[3] == HEADER[3]) {
            ESP_LOGV("Header", "header matches HEADER");
            ESP_LOGV("Header", "[%02X] (%02X) %02X %02X [%02X]<-- header", storedInputData[0], storedInputData[1], storedInputData[2], storedInputData[3], storedInputData[4]);
            ESP_LOGD("Header", "command: (%02X) data length: [%02X]<-- header", storedInputData[1], storedInputData[4]);
            this->command = storedInputData[1];
        }
        this->dataLength = storedInputData[4];
    }
}

bool CN105Climate::processInput(void) {
    bool processed = false;
    while (this->get_hw_serial_()->available()) {
        processed = true;
        u_int8_t inputData;
        if (this->get_hw_serial_()->read_byte(&inputData)) {
            parse(inputData);
        }

    }
    return processed;
}

void CN105Climate::processDataPacket() {

    ESP_LOGV(TAG, "processing data packet...");

    this->data = &this->storedInputData[5];

    this->hpPacketDebug(this->storedInputData, this->bytesRead + 1, "READ");

    if (this->checkSum()) {
        // checkPoint of a heatpump response
        this->lastResponseMs = CUSTOM_MILLIS;    //esphome::CUSTOM_MILLIS;        

        // processing the specific command
        processCommand();
    }
}


void CN105Climate::getAutoModeStateFromResponsePacket() {
    heatpumpSettings receivedSettings{};

    if (data[10] == 0x00) {
        ESP_LOGD("Decoder", "[0x10 is 0x00]");

    } else if (data[10] == 0x01) {
        ESP_LOGD("Decoder", "[0x10 is 0x01]");

    } else if (data[10] == 0x02) {
        ESP_LOGD("Decoder", "[0x10 is 0x02]");

    } else {
        ESP_LOGD("Decoder", "[0x10 is unknown]");

    }
}

void CN105Climate::getPowerFromResponsePacket() {
    ESP_LOGD("Decoder", "[0x09 is sub modes]");

    heatpumpSettings receivedSettings{};
    receivedSettings.stage = lookupByteMapValue(STAGE_MAP, STAGE, 6, data[4], "current stage for delivery");
    receivedSettings.sub_mode = lookupByteMapValue(SUB_MODE_MAP, SUB_MODE, 4, data[3], "submode");
    receivedSettings.auto_sub_mode = lookupByteMapValue(AUTO_SUB_MODE_MAP, AUTO_SUB_MODE, 4, data[5], "auto mode sub mode");

    ESP_LOGD("Decoder", "[Stage : %s]", receivedSettings.stage);
    ESP_LOGD("Decoder", "[Sub Mode  : %s]", receivedSettings.sub_mode);
    ESP_LOGD("Decoder", "[Auto Mode Sub Mode  : %s]", receivedSettings.auto_sub_mode);

    //this->heatpumpUpdate(receivedSettings);
    if (this->Stage_sensor_ != nullptr) {
        this->Stage_sensor_->publish_state(receivedSettings.stage);
    }
    if (this->Sub_mode_sensor_ != nullptr) {
        this->Sub_mode_sensor_->publish_state(receivedSettings.sub_mode);
    }
    if (this->Auto_sub_mode_sensor_ != nullptr) {
        this->Auto_sub_mode_sensor_->publish_state(receivedSettings.auto_sub_mode);
    }
}

void CN105Climate::getSettingsFromResponsePacket() {
    heatpumpSettings receivedSettings{};
    ESP_LOGD("Decoder", "[0x02 is settings]");
    //02 00 00 01 08 0A 00 07 00 00 03 AA 00 00 00 00 94
    //this->last_received_packet_sensor->publish_state("0x62-> 0x02: Data -> Settings");        
    receivedSettings.connected = true;      // we're here so we're connected (actually not used property)
    receivedSettings.power = lookupByteMapValue(POWER_MAP, POWER, 2, data[3], "power reading");
    receivedSettings.iSee = data[4] > 0x08 ? true : false;
    receivedSettings.mode = lookupByteMapValue(MODE_MAP, MODE, 5, receivedSettings.iSee ? (data[4] - 0x08) : data[4], "mode reading");

    ESP_LOGD("Decoder", "[Power : %s]", receivedSettings.power);
    ESP_LOGD("Decoder", "[iSee  : %d]", receivedSettings.iSee);
    ESP_LOGD("Decoder", "[Mode  : %s]", receivedSettings.mode);

    if (data[11] != 0x00) {
        int temp = data[11];
        temp -= 128;
        receivedSettings.temperature = (float)temp / 2;
        this->tempMode = true;
        //ESP_LOGD("Decoder", "tempMode is true");
    } else {
        receivedSettings.temperature = lookupByteMapValue(TEMP_MAP, TEMP, 16, data[5], "temperature reading");
    }

    ESP_LOGD("Decoder", "[Temp °C: %f]", receivedSettings.temperature);

    receivedSettings.fan = lookupByteMapValue(FAN_MAP, FAN, 6, data[6], "fan reading");
    ESP_LOGD("Decoder", "[Fan: %s]", receivedSettings.fan);

    receivedSettings.vane = lookupByteMapValue(VANE_MAP, VANE, 7, data[7], "vane reading");
    ESP_LOGD("Decoder", "[Vane: %s]", receivedSettings.vane);


    receivedSettings.wideVane = lookupByteMapValue(WIDEVANE_MAP, WIDEVANE, 7, data[10] & 0x0F, "wideVane reading", WIDEVANE_MAP[2]);
    wideVaneAdj = (data[10] & 0xF0) == 0x80 ? true : false;

    /*if ((data[10] != 0) && (this->traits_.supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL))) {    // wideVane is not always supported
        receivedSettings.wideVane = lookupByteMapValue(WIDEVANE_MAP, WIDEVANE, 7, data[10] & 0x0F, "wideVane reading");
        wideVaneAdj = (data[10] & 0xF0) == 0x80 ? true : false;
        ESP_LOGD("Decoder", "[wideVane: %s (adj:%d)]", receivedSettings.wideVane, wideVaneAdj);
    } else {
        ESP_LOGD("Decoder", "widevane is not supported");
    }*/

    if (this->iSee_sensor_ != nullptr) {
        this->iSee_sensor_->publish_state(receivedSettings.iSee);
    }

    this->heatpumpUpdate(receivedSettings);

}

void CN105Climate::getRoomTemperatureFromResponsePacket() {

    heatpumpStatus receivedStatus{};

    //ESP_LOGD("Decoder", "[0x03 room temperature]");
    //this->last_received_packet_sensor->publish_state("0x62-> 0x03: Data -> Room temperature");        

    if (data[6] != 0x00) {
        int temp = data[6];
        temp -= 128;
        receivedStatus.roomTemperature = (float)temp / 2;
    } else {
        receivedStatus.roomTemperature = lookupByteMapValue(ROOM_TEMP_MAP, ROOM_TEMP, 32, data[3]);
    }
    ESP_LOGD("Decoder", "[Room °C: %f]", receivedStatus.roomTemperature);

    // no change with this packet to currentStatus for operating and compressorFrequency
    receivedStatus.operating = currentStatus.operating;
    receivedStatus.compressorFrequency = currentStatus.compressorFrequency;
    this->statusChanged(receivedStatus);

}
void CN105Climate::getOperatingAndCompressorFreqFromResponsePacket() {
    //FC 62 01 30 10 06 00 00 1A 01 00 00 00 00 00 00 00 00 00 00 00 3C
    heatpumpStatus receivedStatus{};
    ESP_LOGD("Decoder", "[0x06 is status]");
    //this->last_received_packet_sensor->publish_state("0x62-> 0x06: Data -> Heatpump Status");

    // reset counter (because a reply indicates it is connected)
    this->nonResponseCounter = 0;
    receivedStatus.operating = data[4];
    receivedStatus.compressorFrequency = data[3];

    // no change with this packet to roomTemperature
    receivedStatus.roomTemperature = currentStatus.roomTemperature;
    this->statusChanged(receivedStatus);
}

void CN105Climate::getDataFromResponsePacket() {

    switch (this->data[0]) {
    case 0x02:             /* setting information */
        ESP_LOGD(LOG_CYCLE_TAG, "2b: Receiving settings response");
        this->getSettingsFromResponsePacket();
        // next step is to get the room temperature case 0x03        
        ESP_LOGD(LOG_CYCLE_TAG, "3a: Sending room °C request (0x03)");
        this->buildAndSendRequestPacket(RQST_PKT_ROOM_TEMP);
        break;

    case 0x03:
        /* room temperature reading */
        ESP_LOGD(LOG_CYCLE_TAG, "3b: Receiving room °C response");
        this->getRoomTemperatureFromResponsePacket();
        // next step is to get the heatpump status (operating and compressor frequency) case 0x06        
        ESP_LOGD(LOG_CYCLE_TAG, "4a: Sending status request (0x06)");
        this->buildAndSendRequestPacket(RQST_PKT_STATUS);
        break;

    case 0x04:
        /* unknown */
        ESP_LOGI("Decoder", "[0x04 is unknown : not implemented]");
        //this->last_received_packet_sensor->publish_state("0x62-> 0x04: Data -> Unknown");
        break;

    case 0x05:
        /* timer packet */
        ESP_LOGW("Decoder", "[0x05 is Timer : not implemented]");
        //this->last_received_packet_sensor->publish_state("0x62-> 0x05: Data -> Timer Packet");
        break;

    case 0x06:
        /* status */
        ESP_LOGD(LOG_CYCLE_TAG, "4b: Receiving status response");
        this->getOperatingAndCompressorFreqFromResponsePacket();
        ESP_LOGD(LOG_CYCLE_TAG, "5a: Sending power request (0x09)");
        this->buildAndSendRequestPacket(RQST_PKT_STANDBY);
        break;

    case 0x09:
        /* Power */
        ESP_LOGD(LOG_CYCLE_TAG, "5b: Receiving Power/Standby response");
        this->getPowerFromResponsePacket();
        //FC 62 01 30 10 09 00 00 00 02 02 00 00 00 00 00 00 00 00 00 00 50                     
        this->loopCycle.cycleEnded();
        break;

    case 0x10:
        ESP_LOGD("Decoder", "[0x10 is Unknown : not implemented]");
        //this->getAutoModeStateFromResponsePacket();
        break;

    case 0x20:
    case 0x22: {
        ESP_LOGD("Decoder", "[Packet Functions 0x20 et 0x22]");
        //this->last_received_packet_sensor->publish_state("0x62-> 0x20/0x22: Data -> Packet functions");
        if (dataLength == 0x10) {
            if (data[0] == 0x20) {
                functions.setData1(&data[1]);
            } else {
                functions.setData2(&data[1]);
            }

        }

    }
             break;

    default:
        ESP_LOGW("Decoder", "packet type [%02X] <-- unknown and unexpected", data[0]);
        //this->last_received_packet_sensor->publish_state("0x62-> ?? : Data -> Unknown");
        break;
    }

}

void CN105Climate::updateSuccess() {
    ESP_LOGI(TAG, "Last heatpump data update successful!");
    // nothing can be done here because we have no mean to know wether it is an external temp ack
    // or a wantedSettings update ack
}

void CN105Climate::processCommand() {
    switch (this->command) {
    case 0x61:  /* last update was successful */
        this->hpPacketDebug(this->storedInputData, this->bytesRead + 1, "Update-ACK");
        this->updateSuccess();
        break;

    case 0x62:  /* packet contains data (room °C, settings, timer, status, or functions...)*/
        this->getDataFromResponsePacket();
        break;
    case 0x7a:
        ESP_LOGI(TAG, "--> Heatpump did reply: connection success! <--");
        this->isHeatpumpConnected_ = true;
        // let's say that the last complete cycle was over now        
        this->loopCycle.lastCompleteCycleMs = CUSTOM_MILLIS;
        this->currentSettings.resetSettings();      // each time we connect, we need to reset current setting to force a complete sync with ha component state and receievdSettings 
        break;
    default:
        break;
    }
}


void CN105Climate::statusChanged(heatpumpStatus status) {

    if (status != currentStatus) {
        this->debugStatus("received", status);
        this->debugStatus("current", currentStatus);


        this->currentStatus.operating = status.operating;
        this->currentStatus.compressorFrequency = status.compressorFrequency;
        this->currentStatus.roomTemperature = status.roomTemperature;
        this->current_temperature = currentStatus.roomTemperature;

        this->updateAction();       // update action info on HA climate component        
        this->publish_state();

        if (this->compressor_frequency_sensor_ != nullptr) {
            this->compressor_frequency_sensor_->publish_state(currentStatus.compressorFrequency);
        }
    } // else no change
}


void CN105Climate::publishStateToHA(heatpumpSettings settings) {

    if ((this->wantedSettings.mode == nullptr) && (this->wantedSettings.power == nullptr)) {        // to prevent overwriting a user demand 
        checkPowerAndModeSettings(settings);
    }

    this->updateAction();       // update action info on HA climate component

    if (this->wantedSettings.fan == nullptr) {  // to prevent overwriting a user demand
        checkFanSettings(settings);
    }

    if ((this->wantedSettings.vane == nullptr) && (this->wantedSettings.wideVane == nullptr)) { // to prevent overwriting a user demand
        checkVaneSettings(settings);
    }

    // HA Temp
    if (this->wantedSettings.temperature == -1) { // to prevent overwriting a user demand
        this->target_temperature = settings.temperature;
        this->currentSettings.temperature = settings.temperature;
    }

    this->currentSettings.iSee = settings.iSee;
    this->currentSettings.connected = true;

    // publish to HA
    this->publish_state();

}


void CN105Climate::heatpumpUpdate(heatpumpSettings settings) {
    // settings correponds to current settings 
    ESP_LOGV(LOG_SETTINGS_TAG, "Settings received");

    this->debugSettings("current", this->currentSettings);
    this->debugSettings("received", settings);
    this->debugSettings("wanted", this->wantedSettings);
    this->debugClimate("climate");

    if (this->currentSettings != settings) {
        ESP_LOGD(LOG_SETTINGS_TAG, "Settings changed, updating HA states");
        this->publishStateToHA(settings);
    }

}


void CN105Climate::checkVaneSettings(heatpumpSettings& settings, bool updateCurrentSettings) {

    /* ******** HANDLE MITSUBISHI VANE CHANGES ********
     * VANE_MAP[7]        = {"AUTO", "1", "2", "3", "4", "5", "SWING"};
     * WIDEVANE_MAP[7]    = { "<<", "<",  "|",  ">",  ">>", "<>", "SWING" }
     */

    if (this->hasChanged(currentSettings.vane, settings.vane, "vane") ||                // vane setting change ?
        this->hasChanged(currentSettings.wideVane, settings.wideVane, "wideVane")) {    // widevane setting change ?
        ESP_LOGI(TAG, "vane or widevane setting changed");

        // here I hope that the vane and widevane are always sent together
        if (updateCurrentSettings) {
            currentSettings.vane = settings.vane;
            currentSettings.wideVane = settings.wideVane;
        }
        if (strcmp(settings.vane, "SWING") == 0) {
            if (strcmp(settings.wideVane, "SWING") == 0) {
                this->swing_mode = climate::CLIMATE_SWING_BOTH;
            } else {
                this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
            }

        } else {
            if (strcmp(settings.wideVane, "SWING") == 0) {
                this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
            } else {
                this->swing_mode = climate::CLIMATE_SWING_OFF;
            }
        }
        ESP_LOGD(TAG, "Swing mode is: %i", this->swing_mode);
    }

    /*if (this->hasChanged(this->van_orientation->state.c_str(), settings.vane, "select vane")) {
        ESP_LOGI(TAG, "vane setting (extra select component) changed");
        this->van_orientation->publish_state(currentSettings.vane);
    }*/

    updateExtraSelectComponents(settings);
}
void CN105Climate::updateExtraSelectComponents(heatpumpSettings& settings) {
    if (this->vertical_vane_select_ != nullptr) {
        if (this->hasChanged(this->vertical_vane_select_->state.c_str(), settings.vane, "select vane")) {
            ESP_LOGI(TAG, "vane setting (extra select component) changed");
            this->vertical_vane_select_->publish_state(currentSettings.vane);
        }
    }
    if (this->horizontal_vane_select_ != nullptr) {
        if (this->hasChanged(this->horizontal_vane_select_->state.c_str(), settings.vane, "select wideVane")) {
            ESP_LOGI(TAG, "widevane setting (extra select component) changed");
            this->horizontal_vane_select_->publish_state(currentSettings.wideVane);
        }
    }
}
void CN105Climate::checkFanSettings(heatpumpSettings& settings, bool updateCurrentSettings) {
    /*
         * ******* HANDLE FAN CHANGES ********
         *
         * const char* FAN_MAP[6]         = {"AUTO", "QUIET", "1", "2", "3", "4"};
         */
         // currentSettings.fan== NULL is true when it is the first time we get en answer from hp

    if (this->hasChanged(currentSettings.fan, settings.fan, "fan")) { // fan setting change ?
        ESP_LOGI(TAG, "fan setting changed");
        if (updateCurrentSettings) {
            currentSettings.fan = settings.fan;
        }
        if (strcmp(currentSettings.fan, "QUIET") == 0) {
            this->fan_mode = climate::CLIMATE_FAN_QUIET;
        } else if (strcmp(settings.fan, "1") == 0) {
            this->fan_mode = climate::CLIMATE_FAN_LOW;
        } else if (strcmp(settings.fan, "2") == 0) {
            this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
        } else if (strcmp(settings.fan, "3") == 0) {
            this->fan_mode = climate::CLIMATE_FAN_MIDDLE;
        } else if (strcmp(settings.fan, "4") == 0) {
            this->fan_mode = climate::CLIMATE_FAN_HIGH;
        } else { //case "AUTO" or default:
            this->fan_mode = climate::CLIMATE_FAN_AUTO;
        }
        if (this->fan_mode.has_value()) {
            ESP_LOGD(TAG, "Fan mode is: %i", static_cast<int>(this->fan_mode.value()));
        } else {
            ESP_LOGD(TAG, "Fan mode is not set");
        }
    }
}
void CN105Climate::checkPowerAndModeSettings(heatpumpSettings& settings, bool updateCurrentSettings) {
    // currentSettings.power== NULL is true when it is the first time we get en answer from hp
    if (this->hasChanged(currentSettings.power, settings.power, "power") ||
        this->hasChanged(currentSettings.mode, settings.mode, "mode")) {           // mode or power change ?

        ESP_LOGI(TAG, "power or mode changed");
        if (updateCurrentSettings) {
            currentSettings.power = settings.power;
            currentSettings.mode = settings.mode;
        }
        if (strcmp(settings.power, "ON") == 0) {
            if (strcmp(settings.mode, "HEAT") == 0) {
                this->mode = climate::CLIMATE_MODE_HEAT;
            } else if (strcmp(settings.mode, "DRY") == 0) {
                this->mode = climate::CLIMATE_MODE_DRY;
            } else if (strcmp(settings.mode, "COOL") == 0) {
                this->mode = climate::CLIMATE_MODE_COOL;
                /*if (cool_setpoint != currentSettings.temperature) {
                    cool_setpoint = currentSettings.temperature;
                    save(currentSettings.temperature, cool_storage);
                }*/
            } else if (strcmp(settings.mode, "FAN") == 0) {
                this->mode = climate::CLIMATE_MODE_FAN_ONLY;
            } else if (strcmp(settings.mode, "AUTO") == 0) {
                this->mode = climate::CLIMATE_MODE_AUTO;
            } else {
                ESP_LOGW(
                    TAG,
                    "Unknown climate mode value %s received from HeatPump",
                    settings.mode
                );
            }
        } else {
            this->mode = climate::CLIMATE_MODE_OFF;
        }
    }
}

