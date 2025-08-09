#include "cn105.h"

#include <map>

using namespace esphome;

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
    receivedSettings.stage = lookupByteMapValue(STAGE_MAP, STAGE, 7, data[4], "current stage for delivery");
    receivedSettings.sub_mode = lookupByteMapValue(SUB_MODE_MAP, SUB_MODE, 4, data[3], "submode");
    receivedSettings.auto_sub_mode = lookupByteMapValue(AUTO_SUB_MODE_MAP, AUTO_SUB_MODE, 4, data[5], "auto mode sub mode");

    ESP_LOGD("Decoder", "[Stage : %s]", receivedSettings.stage);
    ESP_LOGD("Decoder", "[Sub Mode  : %s]", receivedSettings.sub_mode);
    ESP_LOGD("Decoder", "[Auto Mode Sub Mode  : %s]", receivedSettings.auto_sub_mode);

    //this->heatpumpUpdate(receivedSettings);
    if (this->stage_sensor_ != nullptr) {
        if (!this->currentSettings.stage || strcmp(receivedSettings.stage, this->currentSettings.stage) != 0) {
            this->currentSettings.stage = receivedSettings.stage;
            this->stage_sensor_->publish_state(receivedSettings.stage);
        }
    }
    if (this->Sub_mode_sensor_ != nullptr && (!this->currentSettings.sub_mode || strcmp(receivedSettings.sub_mode, this->currentSettings.sub_mode) != 0)) {
        this->currentSettings.sub_mode = receivedSettings.sub_mode;
        this->Sub_mode_sensor_->publish_state(receivedSettings.sub_mode);
    }
    if (this->Auto_sub_mode_sensor_ != nullptr && (!this->currentSettings.auto_sub_mode || strcmp(receivedSettings.auto_sub_mode, this->currentSettings.auto_sub_mode) != 0)) {
        this->currentSettings.auto_sub_mode = receivedSettings.auto_sub_mode;
        this->Auto_sub_mode_sensor_->publish_state(receivedSettings.auto_sub_mode);
    }
}

// Given a temperature in Celsius that will be converted to Fahrenheit, converts
// it to the Celsius value corresponding to the the Fahrenheit value that
// Mitsubishi thermostats would have converted the Celsius value to. For
// instance, 21.5°C is 70.7°F, but to get it to map to 70°F, this function
// returns 21.1°C.
static float mapCelsiusForConversionToFahrenheit(const float c) {
    static const auto& mapping = [] {
        auto* const m = new std::map<float, float>{
            {16.0, 61}, {16.5, 62}, {17.0, 63}, {17.5, 64}, {18.0, 65},
            {18.5, 66}, {19.0, 67}, {20.0, 68}, {21.0, 69}, {21.5, 70},
            {22.0, 71}, {22.5, 72}, {23.0, 73}, {23.5, 74}, {24.0, 75},
            {24.5, 76}, {25.0, 77}, {25.5, 78}, {26.0, 79}, {26.5, 80},
            {27.0, 81}, {27.5, 82}, {28.0, 83}, {28.5, 84}, {29.0, 85},
            {29.5, 86}, {30.0, 87}, {30.5, 88}
        };
        for (auto& pair : *m) {
            pair.second = (pair.second - 32.0f) / 1.8f;
        }
        return *m;
        }();

    auto it = mapping.find(c);
    if (it == mapping.end()) return c;
    return it->second;
}

void CN105Climate::getSettingsFromResponsePacket() {
    heatpumpSettings receivedSettings{};
    heatpumpRunStates receivedRunStates{};
    ESP_LOGD("Decoder", "[0x02 is settings]");

    receivedSettings.connected = true;
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
    } else {
        receivedSettings.temperature = lookupByteMapValue(TEMP_MAP, TEMP, 16, data[5], "temperature reading");
    }
    if (use_fahrenheit_support_mode_) {
        receivedSettings.temperature = mapCelsiusForConversionToFahrenheit(receivedSettings.temperature);
    }

    ESP_LOGD("Decoder", "[Temp °C: %f]", receivedSettings.temperature);

    receivedSettings.fan = lookupByteMapValue(FAN_MAP, FAN, 6, data[6], "fan reading");
    ESP_LOGD("Decoder", "[Fan: %s]", receivedSettings.fan);

    receivedSettings.vane = lookupByteMapValue(VANE_MAP, VANE, 7, data[7], "vane reading");
    ESP_LOGD("Decoder", "[Vane: %s]", receivedSettings.vane);

    // --- START OF MODIFIED SECTION - Reverted widevane section back to more or less original state
    if ((data[10] != 0) && (this->traits_.supports_swing_mode(climate::CLIMATE_SWING_HORIZONTAL))) {    // wideVane is not always supported
        receivedSettings.wideVane = lookupByteMapValue(WIDEVANE_MAP, WIDEVANE, 8, data[10] & 0x0F, "wideVane reading");
        this->wideVaneAdj = (data[10] & 0xF0) == 0x80 ? true : false;        
        ESP_LOGD("Decoder", "[wideVane: %s (adj:%d)]", receivedSettings.wideVane, this->wideVaneAdj);
    } else {
        ESP_LOGD("Decoder", "widevane is not supported");
    }
    // --- END OF MODIFIED SECTION ---

    if (this->iSee_sensor_ != nullptr) {
        this->iSee_sensor_->publish_state(receivedSettings.iSee);
    }

    // --- AIRFLOW CONTROL START
    if (this->airflow_control_select_ != nullptr) {
        if (data[10] == 0x80) {
            if (receivedSettings.iSee) { 
                receivedRunStates.airflow_control = lookupByteMapValue(AIRFLOW_CONTROL_MAP, AIRFLOW_CONTROL, 3, data[14], "airflow control reading");
            }
            else { 
                // For some reason data[10] is 0x80, but the i-See sensor is not active. 
                // Some units let us do this, but the real mode is unknown (might be powersave) and the i-See sensor does not get activated.
                //receivedRunStates.airflow_control = "N/A";
                ESP_LOGD("Decoder", "i-See sensor not present/active.");
                receivedRunStates.airflow_control = AIRFLOW_CONTROL_MAP[0];
            }
        } else {
            receivedRunStates.airflow_control = AIRFLOW_CONTROL_MAP[0];
        }
        if (!this->currentRunStates.airflow_control || strcmp(receivedRunStates.airflow_control, this->currentRunStates.airflow_control) != 0) {
            this->currentRunStates.airflow_control = receivedRunStates.airflow_control;
            this->airflow_control_select_->publish_state(receivedRunStates.airflow_control);
        }
    }
    
    // --- AIRFLOW CONTROL END

    this->heatpumpUpdate(receivedSettings);
}

void CN105Climate::getRoomTemperatureFromResponsePacket() {

    heatpumpStatus receivedStatus{};

    //ESP_LOGD("Decoder", "[0x03 room temperature]");
    //this->last_received_packet_sensor->publish_state("0x62-> 0x03: Data -> Room temperature");
    //                 0  1  2  3  4  5  6  7  8  9 10 11 12 13 14 15
    // FC 62 01 30 10 03 00 00 0E 00 94 B0 B0 FE 42 00 01 0A 64 00 00 A9
    //                         RT    OT RT SP ?? ?? ?? RM RM RM
    // RT = room temperature (in old format and in new format)
    // OT = outside air temperature
    // SP = room setpoint temperature?
    // RM = indoor unit operating time in minutes

    if (data[5] > 1) {
        receivedStatus.outsideAirTemperature = (data[5] - 128) / 2.0f;
        if (use_fahrenheit_support_mode_) {
            receivedStatus.outsideAirTemperature = mapCelsiusForConversionToFahrenheit(receivedStatus.outsideAirTemperature);
        }
    } else {
        receivedStatus.outsideAirTemperature = NAN;
    }

    if (data[6] != 0x00) {
        int temp = data[6];
        temp -= 128;
        receivedStatus.roomTemperature = temp / 2.0f;
    } else {
        receivedStatus.roomTemperature = lookupByteMapValue(ROOM_TEMP_MAP, ROOM_TEMP, 32, data[3]);
    }
    if (use_fahrenheit_support_mode_) {
        receivedStatus.roomTemperature = mapCelsiusForConversionToFahrenheit(receivedStatus.roomTemperature);
    }

    receivedStatus.runtimeHours = float((data[11] << 16) | (data[12] << 8) | data[13]) / 60;

    ESP_LOGD("Decoder", "[Room °C: %f]", receivedStatus.roomTemperature);
    ESP_LOGD("Decoder", "[OAT  °C: %f]", receivedStatus.outsideAirTemperature);

    // no change with this packet to currentStatus for operating and compressorFrequency
    receivedStatus.operating = currentStatus.operating;
    receivedStatus.compressorFrequency = currentStatus.compressorFrequency;
    receivedStatus.inputPower = currentStatus.inputPower;
    receivedStatus.kWh = currentStatus.kWh;
    this->statusChanged(receivedStatus);
}

void CN105Climate::getOperatingAndCompressorFreqFromResponsePacket() {
    //FC 62 01 30 10 06 00 00 1A 01 00 00 00 00 00 00 00 00 00 00 00 3C
    //MSZ-RW25VGHZ-SC1 / MUZ-RW25VGHZ-SC1
    //FC 62 01 30 10 06 00 00 00 01 00 08 05 50 00 00 42 00 00 00 00 B7
    //                           OP IP IP EU EU       ??
    // OP = operating status (1 = compressor running, 0 = standby)
    // IP = Current input power in Watts (16-bit decimal)
    // EU = energy usage
    //      (used energy in kWh = value/10)
    //      TODO: Currently the maximum size of the counter is not known and
    //            if the counter extends to other bytes.
    // ?? = unknown bytes that appear to have a fixed/constant value
    heatpumpStatus receivedStatus{};
    ESP_LOGD("Decoder", "[0x06 is status]");
    //this->last_received_packet_sensor->publish_state("0x62-> 0x06: Data -> Heatpump Status");

    // reset counter (because a reply indicates it is connected)
    this->nonResponseCounter = 0;
    receivedStatus.operating = data[4];
    receivedStatus.compressorFrequency = data[3];
    receivedStatus.inputPower = (data[5] << 8) | data[6];
    receivedStatus.kWh = float((data[7] << 8) | data[8]) / 10;

    // no change with this packet to roomTemperature
    receivedStatus.roomTemperature = currentStatus.roomTemperature;
    receivedStatus.outsideAirTemperature = currentStatus.outsideAirTemperature;
    receivedStatus.runtimeHours = currentStatus.runtimeHours;
    this->statusChanged(receivedStatus);
}

void CN105Climate::getHVACOptionsFromResponsePacket() {
    //MSZ-LN25VG2W
    //FC 62 01 30 10 42 01 01 01 00 00 00 00 00 00 00 00 00 00 00 00 18
    //                  AP NM CL
    // AP = air purifier (1 = on, 0 = off)
    // NM = night mode (1 = on, 0 = off)
    // CL = circulator (1 = on, 0 = off) ! MIGHT BE SAME BYTE AS ECONOCOOL - NEEDS TESTING !
    heatpumpRunStates receivedRunStates{};
    ESP_LOGD("Decoder", "[0x42 is HVAC options]");
    
    if (this->air_purifier_switch_ != nullptr) {
        receivedRunStates.air_purifier = data[1];
        ESP_LOGD("Decoder", "[Air purifier : %s]", receivedRunStates.air_purifier ? "ON" : "OFF");
        if (receivedRunStates.air_purifier != this->currentRunStates.air_purifier || receivedRunStates.air_purifier != this->air_purifier_switch_->state) {
            this->currentRunStates.air_purifier = receivedRunStates.air_purifier;
            this->air_purifier_switch_->publish_state(receivedRunStates.air_purifier);
        }
    }
    if (this->night_mode_switch_ != nullptr) {
        receivedRunStates.night_mode = data[2];
        ESP_LOGD("Decoder", "[Night mode : %s]", receivedRunStates.night_mode ? "ON" : "OFF");
        if (receivedRunStates.night_mode != this->currentRunStates.night_mode || receivedRunStates.night_mode != this->night_mode_switch_->state) {
            this->currentRunStates.night_mode = receivedRunStates.night_mode;
            this->night_mode_switch_->publish_state(receivedRunStates.night_mode);
        }
    }
    if (this->circulator_switch_ != nullptr) {
        receivedRunStates.circulator = data[3];
        ESP_LOGD("Decoder", "[Circulator : %s]", receivedRunStates.circulator ? "ON" : "OFF");
        if (receivedRunStates.circulator != this->currentRunStates.circulator || receivedRunStates.circulator != this->circulator_switch_->state) {
            this->currentRunStates.circulator = receivedRunStates.circulator;
            this->circulator_switch_->publish_state(receivedRunStates.circulator);
        }
    }
}

void CN105Climate::terminateCycle() {
    if (this->shouldSendExternalTemperature_) {
        // We will receive ACK packet for this.
        // Sending WantedSettings must be delayed in this case (lastSend timestamp updated).        
        ESP_LOGD(LOG_REMOTE_TEMP, "Sending remote temperature...");
        this->sendRemoteTemperature();
    }

    this->loopCycle.cycleEnded();

    if (this->hp_uptime_connection_sensor_ != nullptr) {
        // if the uptime connection sensor is configured
        // we trigger  manual update at the end of a cycle.
        this->hp_uptime_connection_sensor_->update();
    }

    this->nbCompleteCycles_++;
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
        // next step is to get the heatpump extra function status (air purifier, night mode, circulator) case 0x42 if these are enabled in the YAML
        // or else
        // next step is to get the heatpump status (operating and compressor frequency) case 0x06
        if (this->air_purifier_switch_ != nullptr || this->night_mode_switch_ != nullptr || this->circulator_switch_ != nullptr) {
            ESP_LOGD(LOG_CYCLE_TAG, "3c: Sending HVAC options request (0x42)");
            this->buildAndSendRequestPacket(RQST_PKT_HVAC_OPTIONS);
        } else {
            ESP_LOGD(LOG_CYCLE_TAG, "4a: Sending status request (0x06)");
            this->buildAndSendRequestPacket(RQST_PKT_STATUS);
        }
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

        if (this->powerRequestWithoutResponses < 3) {         // if more than 3 requests are without reponse, we desactivate the power request (0x09)
            ESP_LOGD(LOG_CYCLE_TAG, "5a: Sending power request (0x09)");
            this->buildAndSendRequestPacket(RQST_PKT_STANDBY);
            this->powerRequestWithoutResponses++;
        } else {
            if (this->powerRequestWithoutResponses != 4) {
                this->powerRequestWithoutResponses = 4;
                ESP_LOGW(LOG_CYCLE_TAG, "power request (0x09) disabled (not supported)");
            }
            // in this case, the cycle ends up now
            this->terminateCycle();
        }
        break;

    case 0x09:
        /* Power */
        ESP_LOGD(LOG_CYCLE_TAG, "5b: Receiving Power/Standby response");
        this->getPowerFromResponsePacket();
        //FC 62 01 30 10 09 00 00 00 02 02 00 00 00 00 00 00 00 00 00 00 50
        // reset the powerRequestWithoutResponses to 0 as we had a response
        this->powerRequestWithoutResponses = 0;

        this->terminateCycle();
        break;

    case 0x10:
        ESP_LOGD("Decoder", "[0x10 is Unknown : not implemented]");
        //this->getAutoModeStateFromResponsePacket();
        break;

    case 0x20: // fallthrough
    case 0x22: {
        ESP_LOGD("Decoder", "[Packet Functions 0x20 et 0x22]");
        //this->last_received_packet_sensor->publish_state("0x62-> 0x20/0x22: Data -> Packet functions");
        if (dataLength == 0x10) {
            if (data[0] == 0x20) {
                functions.setData1(&data[1]);
                ESP_LOGI(LOG_CYCLE_TAG, "Got functions packet 1, requesting part 2");
                this->getFunctionsPart2();
            } else {
                functions.setData2(&data[1]);
                ESP_LOGI(LOG_CYCLE_TAG, "Got functions packet 2");
                this->functionsArrived();
            }
        }
    }
        break;

    case 0x42:
        /* HVAC Options */
        ESP_LOGD(LOG_CYCLE_TAG, "3d: Receiving HVAC options");
        this->getHVACOptionsFromResponsePacket();
        ESP_LOGD(LOG_CYCLE_TAG, "4a: Sending status request (0x06)");
        this->buildAndSendRequestPacket(RQST_PKT_STATUS);
        break;

    default:
        ESP_LOGW("Decoder", "packet type [%02X] <-- unknown and unexpected", data[0]);
        //this->last_received_packet_sensor->publish_state("0x62-> ?? : Data -> Unknown");
        break;
    }

}

void CN105Climate::updateSuccess() {
    ESP_LOGD(LOG_ACK, "Last heatpump data update successful!");
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
        //this->isHeatpumpConnected_ = true;
        this->setHeatpumpConnected(true);
        // let's say that the last complete cycle was over now
        this->loopCycle.lastCompleteCycleMs = CUSTOM_MILLIS;
        this->currentSettings.resetSettings();      // each time we connect, we need to reset current setting to force a complete sync with ha component state and receievdSettings
        this->currentRunStates.resetSettings();
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
        this->currentStatus.inputPower = status.inputPower;
        this->currentStatus.kWh = status.kWh;
        this->currentStatus.runtimeHours = status.runtimeHours;
        this->currentStatus.roomTemperature = status.roomTemperature;
        this->currentStatus.outsideAirTemperature = status.outsideAirTemperature;
        this->current_temperature = currentStatus.roomTemperature;

        this->updateAction();       // update action info on HA climate component
        this->publish_state();

        if (this->compressor_frequency_sensor_ != nullptr) {
            this->compressor_frequency_sensor_->publish_state(currentStatus.compressorFrequency);
        }

        if (this->input_power_sensor_ != nullptr) {
            this->input_power_sensor_->publish_state(currentStatus.inputPower);
        }

        if (this->kwh_sensor_ != nullptr) {
            this->kwh_sensor_->publish_state(currentStatus.kWh);
        }

        if (this->runtime_hours_sensor_ != nullptr) {
            this->runtime_hours_sensor_->publish_state(currentStatus.runtimeHours);
        }

        if (this->outside_air_temperature_sensor_ != nullptr) {
            this->outside_air_temperature_sensor_->publish_state(currentStatus.outsideAirTemperature);
        }
    } // else no change
}


void CN105Climate::publishStateToHA(heatpumpSettings& settings) {

    if ((this->wantedSettings.mode == nullptr) && (this->wantedSettings.power == nullptr)) {        // to prevent overwriting a user demand
        checkPowerAndModeSettings(settings);
    }

    this->updateAction();       // update action info on HA climate component

    if (this->wantedSettings.fan == nullptr) {  // to prevent overwriting a user demand
        checkFanSettings(settings);
    }

    if (this->wantedSettings.vane == nullptr) { // to prevent overwriting a user demand
        checkVaneSettings(settings);
    }

    if (this->wantedSettings.wideVane == nullptr) { // to prevent overwriting a user demand
        checkWideVaneSettings(settings);
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


void CN105Climate::heatpumpUpdate(heatpumpSettings& settings) {
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
    if (this->hasChanged(currentSettings.vane, settings.vane, "vane")) {    // widevane setting change ?
        ESP_LOGI(LOG_SETTINGS_TAG, "vane setting changed");

        //this->debugSettings("settings", settings);

        if (updateCurrentSettings) {
            //ESP_LOGD(LOG_SETTINGS_TAG, "updating currentSetting with new value");
            currentSettings.vane = settings.vane;
        }

        if (strcmp(settings.vane, "SWING") == 0) {
            if ((currentSettings.wideVane != nullptr) && (strcmp(currentSettings.wideVane, "SWING") == 0)) {
                this->swing_mode = climate::CLIMATE_SWING_BOTH;
            } else {
                this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
            }
        } else {
            if ((currentSettings.wideVane != nullptr) && (strcmp(currentSettings.wideVane, "SWING") == 0)) {
                this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
            } else {
                this->swing_mode = climate::CLIMATE_SWING_OFF;
            }
        }
        ESP_LOGD(LOG_SETTINGS_TAG, "Swing mode is: %i", this->swing_mode);
    }


    updateExtraSelectComponents(settings);
}

void CN105Climate::checkWideVaneSettings(heatpumpSettings& settings, bool updateCurrentSettings) {

    /* ******** HANDLE MITSUBISHI VANE CHANGES ********
     * VANE_MAP[7]        = {"AUTO", "1", "2", "3", "4", "5", "SWING"};
     * WIDEVANE_MAP[8]   = { "<<", "<",  "|",  ">",  ">>", "<>", "SWING", "AIRFLOW CONTROL" }
     */

    if (this->hasChanged(currentSettings.wideVane, settings.wideVane, "wideVane")) {    // widevane setting change ?
        ESP_LOGI(TAG, "widevane setting changed");
        this->debugSettings("settings", settings);

        // here I hope that the vane and widevane are always sent together
        if (updateCurrentSettings) {
            currentSettings.wideVane = settings.wideVane;
        }

        if (strcmp(settings.wideVane, "SWING") == 0) {
            if ((currentSettings.vane != nullptr) && (strcmp(currentSettings.vane, "SWING") == 0)) {
                this->swing_mode = climate::CLIMATE_SWING_BOTH;
            } else {
                this->swing_mode = climate::CLIMATE_SWING_HORIZONTAL;
            }
        } else {
            if ((currentSettings.vane != nullptr) && (strcmp(currentSettings.vane, "SWING") == 0)) {
                this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
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
            this->vertical_vane_select_->publish_state(settings.vane);
        }
    }
    if (this->horizontal_vane_select_ != nullptr) {
        if (this->hasChanged(this->horizontal_vane_select_->state.c_str(), settings.wideVane, "select wideVane")) {
            ESP_LOGI(TAG, "widevane setting (extra select component) changed");
            this->horizontal_vane_select_->publish_state(settings.wideVane);
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

        if (strcmp(settings.fan, "QUIET") == 0) {
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
