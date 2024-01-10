// TODO: extra_components: add a last_request_datetime et last_response_datetime
// TODO: instead of having to entry points, control and update, we should only record control modifications and run them in the nexte update call

#include "cn105.h"
#include <exception>

// Définition des constantes
//const uint32_t ESPMHP_POLL_INTERVAL_DEFAULT = 500;
const uint8_t ESPMHP_MIN_TEMPERATURE = 16;
const uint8_t ESPMHP_MAX_TEMPERATURE = 31;
const float ESPMHP_TEMPERATURE_STEP = 0.5;

using namespace esphome;



//#region operators
bool operator==(const heatpumpSettings& lhs, const heatpumpSettings& rhs) {
    return lhs.power == rhs.power &&
        lhs.mode == rhs.mode &&
        lhs.temperature == rhs.temperature &&
        lhs.fan == rhs.fan &&
        lhs.vane == rhs.vane &&
        lhs.wideVane == rhs.wideVane &&
        lhs.iSee == rhs.iSee;
}

bool operator!=(const heatpumpSettings& lhs, const heatpumpSettings& rhs) {
    return lhs.power != rhs.power ||
        lhs.mode != rhs.mode ||
        lhs.temperature != rhs.temperature ||
        lhs.fan != rhs.fan ||
        lhs.vane != rhs.vane ||
        lhs.wideVane != rhs.wideVane ||
        lhs.iSee != rhs.iSee;
}

bool operator!(const heatpumpSettings& settings) {
    return !settings.power &&
        !settings.mode &&
        !settings.temperature &&
        !settings.fan &&
        !settings.vane &&
        !settings.wideVane &&
        !settings.iSee;
}

bool operator==(const heatpumpTimers& lhs, const heatpumpTimers& rhs) {
    return lhs.mode == rhs.mode &&
        lhs.onMinutesSet == rhs.onMinutesSet &&
        lhs.onMinutesRemaining == rhs.onMinutesRemaining &&
        lhs.offMinutesSet == rhs.offMinutesSet &&
        lhs.offMinutesRemaining == rhs.offMinutesRemaining;
}

bool operator!=(const heatpumpTimers& lhs, const heatpumpTimers& rhs) {
    return lhs.mode != rhs.mode ||
        lhs.onMinutesSet != rhs.onMinutesSet ||
        lhs.onMinutesRemaining != rhs.onMinutesRemaining ||
        lhs.offMinutesSet != rhs.offMinutesSet ||
        lhs.offMinutesRemaining != rhs.offMinutesRemaining;
}

class VanOrientationSelect : public select::Select {
public:
    VanOrientationSelect(CN105Climate* parent) : parent_(parent) {}

    void control(const std::string& value) override {

        ESP_LOGD("VAN_CTRL", "Demande un chgt de réglage de la vane: %s", value.c_str());

        parent_->setVaneSetting(value.c_str());
        parent_->sendWantedSettings();

    }
private:
    CN105Climate* parent_;

};




//#endregion operators
CN105Climate::CN105Climate(HardwareSerial* hw_serial)
    : hw_serial_(hw_serial) {
    this->traits_.set_supports_action(true);
    this->traits_.set_supports_current_temperature(true);
    this->traits_.set_supports_two_point_target_temperature(false);
    this->traits_.set_visual_min_temperature(ESPMHP_MIN_TEMPERATURE);
    this->traits_.set_visual_max_temperature(ESPMHP_MAX_TEMPERATURE);
    this->traits_.set_visual_temperature_step(ESPMHP_TEMPERATURE_STEP);


    this->traits_.set_supported_modes({
        //climate::CLIMATE_MODE_AUTO,
        climate::CLIMATE_MODE_COOL,
        climate::CLIMATE_MODE_DRY,
        climate::CLIMATE_MODE_FAN_ONLY,
        climate::CLIMATE_MODE_HEAT,
        climate::CLIMATE_MODE_OFF });

    this->traits_.set_supported_fan_modes({
        climate::CLIMATE_FAN_AUTO,
        climate::CLIMATE_FAN_QUIET,
        climate::CLIMATE_FAN_LOW,
        climate::CLIMATE_FAN_HIGH,
        climate::CLIMATE_FAN_MEDIUM });


    this->traits_.set_supported_swing_modes({
        //climate::CLIMATE_SWING_BOTH,
        //climate::CLIMATE_SWING_HORIZONTAL,
        climate::CLIMATE_SWING_VERTICAL,
        climate::CLIMATE_SWING_OFF });


    this->isConnected_ = false;
    this->tempMode = false;
    this->wideVaneAdj = false;
    this->functions = heatpumpFunctions();
    this->autoUpdate = false;
    this->firstRun = true;
    this->externalUpdate = false;
    this->lastSend = 0;
    this->infoMode = 0;
    this->currentStatus.operating = false;
    this->currentStatus.compressorFrequency = -1;

    generateExtraComponents();


}

void CN105Climate::generateExtraComponents() {
    this->compressor_frequency_sensor = new sensor::Sensor();
    this->compressor_frequency_sensor->set_name("Compressor Frequency");
    this->compressor_frequency_sensor->set_unit_of_measurement("Hz");
    this->compressor_frequency_sensor->set_accuracy_decimals(0);
    this->compressor_frequency_sensor->publish_state(0);
    // Enregistrer le capteur pour que ESPHome le gère
    App.register_sensor(compressor_frequency_sensor);


    this->van_orientation = new  VanOrientationSelect(this);
    this->van_orientation->set_name("Van orientation");

    //this->van_orientation->traits.set_options({ "AUTO", "1", "2", "3", "4", "5", "SWING" });    
    std::vector<std::string> vaneOptions(std::begin(VANE_MAP), std::end(VANE_MAP));
    this->van_orientation->traits.set_options(vaneOptions);

    App.register_select(this->van_orientation);

}


void CN105Climate::set_baud_rate(int baud) {
    this->baud_ = baud;
    ESP_LOGD(TAG, "set_baud_rate()");
    ESP_LOGD(TAG, "baud: %d", baud);
}

// void CN105Climate::set_wifi_connected_state(bool state) {

//     ESP_LOGD(TAG, "setting WiFi connection state do %d", state);

//     this->wifi_connected_state_ = state;
//     if (state) {
//         // Réinitialiser la variable de temporisation et planifier la fin du délai        
//         this->setWifiDelayCompleted(false);
//         this->set_timeout("wifi_delay", 5000, [this]() { this->setWifiDelayCompleted(true); });
//     }
// }



void CN105Climate::check_logger_conflict_() {
#ifdef USE_LOGGER
    if (this->get_hw_serial_() == logger::global_logger->get_hw_serial()) {
        ESP_LOGW(TAG, "  You're using the same serial port for logging"
            " and the MitsubishiHeatPump component. Please disable"
            " logging over the serial port by setting"
            " logger:baud_rate to 0.");

        //this->mark_failed();

    }
#endif
}

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


int CN105Climate::lookupByteMapIndex(const int valuesMap[], int len, int lookupValue) {
    for (int i = 0; i < len; i++) {
        if (valuesMap[i] == lookupValue) {
            return i;
        }
    }
    ESP_LOGW("lookup", "Attention valeur %d non trouvée, on retourne -1", lookupValue);
    esphome::delay(200);
    return -1;
}
int CN105Climate::lookupByteMapIndex(const char* valuesMap[], int len, const char* lookupValue) {
    for (int i = 0; i < len; i++) {
        if (strcasecmp(valuesMap[i], lookupValue) == 0) {
            return i;
        }
    }
    ESP_LOGW("lookup", "Attention valeur %s non trouvée, on retourne -1", lookupValue);
    esphome::delay(200);
    return -1;
}
const char* CN105Climate::lookupByteMapValue(const char* valuesMap[], const byte byteMap[], int len, byte byteValue) {
    for (int i = 0; i < len; i++) {
        if (byteMap[i] == byteValue) {
            return valuesMap[i];
        }
    }
    ESP_LOGW("lookup", "Attention valeur %d non trouvée, on retourne la valeur au rang 0", byteValue);
    return valuesMap[0];
}
int CN105Climate::lookupByteMapValue(const int valuesMap[], const byte byteMap[], int len, byte byteValue) {
    for (int i = 0; i < len; i++) {
        if (byteMap[i] == byteValue) {
            return valuesMap[i];
        }
    }
    ESP_LOGW("lookup", "Attention valeur %d non trouvée, on retourne la valeur au rang 0", byteValue);
    return valuesMap[0];
}


bool CN105Climate::checkSum() {
    // TODO: use the CN105Climate::checkSum(byte bytes[], int len) function

    byte packetCheckSum = storedInputData[this->bytesRead];
    byte processedCS = 0;

    ESP_LOGV("chkSum", "controling chkSum should be: %02X ", packetCheckSum);

    for (int i = 0;i < this->dataLength + 5;i++) {
        ESP_LOGV("chkSum", "adding %02X to %02X --> ", this->storedInputData[i], processedCS, processedCS + this->storedInputData[i]);
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

byte CN105Climate::checkSum(byte bytes[], int len) {
    byte sum = 0;
    for (int i = 0; i < len; i++) {
        sum += bytes[i];
    }
    return (0xfc - sum) & 0xff;
}



uint32_t CN105Climate::get_update_interval() const { return this->update_interval_; }
void CN105Climate::set_update_interval(uint32_t update_interval) {
    this->update_interval_ = update_interval;
    this->autoUpdate = (update_interval != 0);
}


/**
 * Programs the sending of a request
*/
void CN105Climate::programUpdateInterval() {
    if (autoUpdate) {
        ESP_LOGD(TAG, "Autoupdate is ON --> creating a loop for reccurent updates...");

        ESP_LOGI(TAG, "Programming update interval : %d", this->get_update_interval());

        this->cancel_timeout(SHEDULER_INTERVAL_SYNC_NAME);     // in case a loop is already programmed


        this->set_timeout(SHEDULER_INTERVAL_SYNC_NAME, this->get_update_interval(), [this]() {

            this->buildAndSendRequestsInfoPackets();


            });
    }
}


bool CN105Climate::hasChanged(const char* before, const char* now, const char* field) {
    if (now == NULL) {
        ESP_LOGE(TAG, "CAUTION: expected value in hasChanged() function for %s, got NULL", field);
        return false;
    }
    return ((before == NULL) || (strcmp(before, now) != 0));
}


void CN105Climate::settingsChanged(heatpumpSettings settings) {

    /*if (settings.power == NULL) {
        // should never happen because settingsChanged is only called from getDataFromResponsePacket()
        ESP_LOGW(TAG, "Waiting for HeatPump to read the settings the first time.");
        return;
    }*/


    checkPowerAndModeSettings(settings);

    this->updateAction();

    checkFanSettings(settings);

    checkVaneSettings(settings);

    /*
     * ******** HANDLE TARGET TEMPERATURE CHANGES ********
     */

    this->currentSettings.temperature = settings.temperature;
    this->currentSettings.iSee = settings.iSee;
    this->currentSettings.connected = settings.connected;

    this->target_temperature = currentSettings.temperature;
    ESP_LOGD(TAG, "Target temp is: %f", this->target_temperature);

    /*
     * ******** Publish state back to ESPHome. ********
     */
    this->publish_state();


}
void CN105Climate::checkVaneSettings(heatpumpSettings& settings) {
    /* ******** HANDLE MITSUBISHI VANE CHANGES ********
         * const char* VANE_MAP[7]        = {"AUTO", "1", "2", "3", "4", "5", "SWING"};
         */
    if (this->hasChanged(currentSettings.vane, settings.vane, "vane")) { // vane setting change ?
        ESP_LOGI(TAG, "vane setting changed");
        currentSettings.vane = settings.vane;

        if (strcmp(currentSettings.vane, "SWING") == 0) {
            this->swing_mode = climate::CLIMATE_SWING_VERTICAL;
        } else {
            this->swing_mode = climate::CLIMATE_SWING_OFF;
        }
        ESP_LOGD(TAG, "Swing mode is: %i", this->swing_mode);
    }

    if (this->hasChanged(this->van_orientation->state.c_str(), settings.vane, "select vane")) {
        ESP_LOGI(TAG, "vane setting (extra select component) changed");
        this->van_orientation->publish_state(currentSettings.vane);
    }
}
void CN105Climate::checkFanSettings(heatpumpSettings& settings) {
    /*
         * ******* HANDLE FAN CHANGES ********
         *
         * const char* FAN_MAP[6]         = {"AUTO", "QUIET", "1", "2", "3", "4"};
         */
         // currentSettings.fan== NULL is true when it is the first time we get en answer from hp

    if (this->hasChanged(currentSettings.fan, settings.fan, "fan")) { // fan setting change ?
        ESP_LOGI(TAG, "fan setting changed");
        currentSettings.fan = settings.fan;
        if (strcmp(currentSettings.fan, "QUIET") == 0) {
            this->fan_mode = climate::CLIMATE_FAN_QUIET;
        } else if (strcmp(currentSettings.fan, "1") == 0) {
            this->fan_mode = climate::CLIMATE_FAN_LOW;
        } else if (strcmp(currentSettings.fan, "2") == 0) {
            this->fan_mode = climate::CLIMATE_FAN_MEDIUM;
        } else if (strcmp(currentSettings.fan, "3") == 0) {
            this->fan_mode = climate::CLIMATE_FAN_MIDDLE;
        } else if (strcmp(currentSettings.fan, "4") == 0) {
            this->fan_mode = climate::CLIMATE_FAN_HIGH;
        } else { //case "AUTO" or default:
            this->fan_mode = climate::CLIMATE_FAN_AUTO;
        }
        ESP_LOGD(TAG, "Fan mode is: %i", this->fan_mode);
    }
}
void CN105Climate::checkPowerAndModeSettings(heatpumpSettings& settings) {
    // currentSettings.power== NULL is true when it is the first time we get en answer from hp
    if (this->hasChanged(currentSettings.power, settings.power, "power") ||
        this->hasChanged(currentSettings.mode, settings.mode, "mode")) {           // mode or power change ?

        ESP_LOGI(TAG, "power or mode changed");
        currentSettings.power = settings.power;
        currentSettings.mode = settings.mode;

        if (strcmp(currentSettings.power, "ON") == 0) {
            if (strcmp(currentSettings.mode, "HEAT") == 0) {
                this->mode = climate::CLIMATE_MODE_HEAT;
            } else if (strcmp(currentSettings.mode, "DRY") == 0) {
                this->mode = climate::CLIMATE_MODE_DRY;
            } else if (strcmp(currentSettings.mode, "COOL") == 0) {
                this->mode = climate::CLIMATE_MODE_COOL;
                /*if (cool_setpoint != currentSettings.temperature) {
                    cool_setpoint = currentSettings.temperature;
                    save(currentSettings.temperature, cool_storage);
                }*/
            } else if (strcmp(currentSettings.mode, "FAN") == 0) {
                this->mode = climate::CLIMATE_MODE_FAN_ONLY;
            } else if (strcmp(currentSettings.mode, "AUTO") == 0) {
                this->mode = climate::CLIMATE_MODE_HEAT_COOL;
            } else {
                ESP_LOGW(
                    TAG,
                    "Unknown climate mode value %s received from HeatPump",
                    currentSettings.mode
                );
            }
        } else {
            this->mode = climate::CLIMATE_MODE_OFF;
        }
    }
}
int CN105Climate::get_compressor_frequency() {
    return currentStatus.compressorFrequency;
}
bool CN105Climate::is_operating() {
    return currentStatus.operating;
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

void CN105Climate::setActionIfOperatingTo(climate::ClimateAction action) {
    if (currentStatus.operating) {
        this->action = action;
    } else {
        this->action = climate::CLIMATE_ACTION_IDLE;
    }
    ESP_LOGD(TAG, "setting action to -> %d", this->action);
}

void CN105Climate::setActionIfOperatingAndCompressorIsActiveTo(climate::ClimateAction action) {
    if (currentStatus.compressorFrequency <= 0) {
        this->action = climate::CLIMATE_ACTION_IDLE;
    } else {
        this->setActionIfOperatingTo(action);
    }
}

void CN105Climate::updateAction() {
    ESP_LOGV(TAG, "updating action back to espHome...");
    switch (this->mode) {
    case climate::CLIMATE_MODE_HEAT:
        this->setActionIfOperatingAndCompressorIsActiveTo(climate::CLIMATE_ACTION_HEATING);
        break;
    case climate::CLIMATE_MODE_COOL:
        this->setActionIfOperatingAndCompressorIsActiveTo(climate::CLIMATE_ACTION_COOLING);
        break;
    case climate::CLIMATE_MODE_HEAT_COOL:
        this->setActionIfOperatingAndCompressorIsActiveTo(
            (this->current_temperature > this->target_temperature ?
                climate::CLIMATE_ACTION_COOLING :
                climate::CLIMATE_ACTION_HEATING));
        break;
    case climate::CLIMATE_MODE_DRY:
        this->setActionIfOperatingAndCompressorIsActiveTo(climate::CLIMATE_ACTION_DRYING);
        break;
    case climate::CLIMATE_MODE_FAN_ONLY:
        this->action = climate::CLIMATE_ACTION_FAN;
        break;
    default:
        this->action = climate::CLIMATE_ACTION_OFF;
    }

    ESP_LOGD(TAG, "Climate mode is: %i", this->mode);
    ESP_LOGD(TAG, "Climate action is: %i", this->action);
}



void CN105Climate::setupUART() {
    // Votre code ici
    ESP_LOGI(TAG, "setupUART() with baudrate %d", this->baud_);

    this->isHeatpumpConnected_ = false;
    this->isConnected_ = false;

    ESP_LOGI(
        TAG,
        "hw_serial(%p) is &Serial(%p)? %s",
        this->get_hw_serial_(),
        &Serial,
        YESNO(this->get_hw_serial_() == &Serial)
    );

    this->check_logger_conflict_();

    this->uart_setup_switch = true;

    if (this->get_hw_serial_() != NULL) {
        ESP_LOGD(TAG, "Serial->begin...");
        this->get_hw_serial_()->begin(this->baud_, SERIAL_8E1);
        this->isConnected_ = true;
        this->initBytePointer();
    } else {
        ESP_LOGE(TAG, "L'UART doit être défini.");
    }

    //this->publish_state();
}
void CN105Climate::sendFirstConnectionPacket() {
    if (this->isConnected_) {
        this->isHeatpumpConnected_ = false;

        ESP_LOGD(TAG, "Envoi du packet de connexion...");
        byte packet[CONNECT_LEN];
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



void CN105Climate::disconnectUART() {
    ESP_LOGD(TAG, "disconnectUART()");
    this->uart_setup_switch = false;

    this->isHeatpumpConnected_ = false;
    this->isConnected_ = false;
    this->cancel_timeout(SHEDULER_INTERVAL_SYNC_NAME);
    this->publish_state();
    if (this->get_hw_serial_() != NULL) {
        this->get_hw_serial_()->end();
    } else {
        ESP_LOGE(TAG, "L'UART doit être défini.");
    }
}


//#region input_parsing

/**
 *
 * La taille totale d'une trame, se compose de plusieurs éléments :
 * Taille du Header : Le header a une longueur fixe de 5 octets (INFOHEADER_LEN).
 * Longueur des Données : La longueur des données est variable et est spécifiée par le quatrième octet du header (header[4]).
 * Checksum : Il y a 1 octet de checksum à la fin de la trame.
 *
 * La taille totale d'une trame est donc la somme de ces éléments : taille du header (5 octets) + longueur des données (variable) + checksum (1 octet).
 * Pour calculer la taille totale, on peut utiliser la formule :
 * Taille totale = 5 (header) + Longueur des données + 1 (checksum)
 * La taille totale dépend de la longueur spécifique des données pour chaque trame individuelle.
 */
void CN105Climate::parse(byte inputData) {

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

void CN105Climate::checkHeader(byte inputData) {
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

void CN105Climate::processInput(void) {

    /*if (this->get_hw_serial_()->available()) {
        this->isReading = true;
    }*/

    while (this->get_hw_serial_()->available()) {
        int inputData = this->get_hw_serial_()->read();
        parse(inputData);
    }
    //this->isReading = false;
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
void CN105Climate::getDataFromResponsePacket() {
    switch (this->data[0]) {
    case 0x02: {            /* setting information */
        ESP_LOGD("Decoder", "[0x02 is settings]");
        heatpumpSettings receivedSettings;
        receivedSettings.connected = true;      // we're here so we're connected (actually not used property)
        receivedSettings.power = lookupByteMapValue(POWER_MAP, POWER, 2, data[3]);
        receivedSettings.iSee = data[4] > 0x08 ? true : false;
        receivedSettings.mode = lookupByteMapValue(MODE_MAP, MODE, 5, receivedSettings.iSee ? (data[4] - 0x08) : data[4]);

        ESP_LOGD("Decoder", "[Power : %s]", receivedSettings.power);
        ESP_LOGD("Decoder", "[iSee  : %d]", receivedSettings.iSee);
        ESP_LOGD("Decoder", "[Mode  : %s]", receivedSettings.mode);

        if (data[11] != 0x00) {
            int temp = data[11];
            temp -= 128;
            receivedSettings.temperature = (float)temp / 2;
            tempMode = true;
        } else {
            receivedSettings.temperature = lookupByteMapValue(TEMP_MAP, TEMP, 16, data[5]);
            ESP_LOGD("Decoder", "[Consigne °C: %f]", receivedSettings.temperature);
        }

        receivedSettings.fan = lookupByteMapValue(FAN_MAP, FAN, 6, data[6]);
        ESP_LOGD("Decoder", "[Fan: %s]", receivedSettings.fan);

        receivedSettings.vane = lookupByteMapValue(VANE_MAP, VANE, 7, data[7]);
        ESP_LOGD("Decoder", "[Vane: %s]", receivedSettings.vane);


        receivedSettings.wideVane = lookupByteMapValue(WIDEVANE_MAP, WIDEVANE, 7, data[10] & 0x0F);



        wideVaneAdj = (data[10] & 0xF0) == 0x80 ? true : false;

        ESP_LOGD("Decoder", "[wideVane: %s (adj:%d)]", receivedSettings.wideVane, wideVaneAdj);

        // moved to settingsChanged()
        //currentSettings = receivedSettings;

        if (this->firstRun) {
            //wantedSettings = currentSettings;
            wantedSettings = receivedSettings;
            firstRun = false;
        }

        this->settingsChanged(receivedSettings);

    }


             break;
    case 0x03: {
        /* room temperature reading */
        ESP_LOGD("Decoder", "[0x03 room temperature]");
        heatpumpStatus receivedStatus;

        if (data[6] != 0x00) {
            int temp = data[6];
            temp -= 128;
            receivedStatus.roomTemperature = (float)temp / 2;
        } else {
            receivedStatus.roomTemperature = lookupByteMapValue(ROOM_TEMP_MAP, ROOM_TEMP, 32, data[3]);
        }
        ESP_LOGD("Decoder", "[Room °C: %f]", receivedStatus.roomTemperature);

        currentStatus.roomTemperature = receivedStatus.roomTemperature;
        this->current_temperature = currentStatus.roomTemperature;

    }
             break;

    case 0x04:
        /* unknown */
        ESP_LOGI("Decoder", "[0x04 is unknown]");
        break;

    case 0x05:
        /* timer packet */
        ESP_LOGW("Decoder", "[0x05 is timer packet: not implemented]");
        break;

    case 0x06: {
        /* status */

        ESP_LOGD("Decoder", "[0x06 is status]");

        // reset counter (because a reply indicates it is connected)
        this->nonResponseCounter = 0;

        heatpumpStatus receivedStatus;
        receivedStatus.operating = data[4];
        receivedStatus.compressorFrequency = data[3];
        currentStatus.operating = receivedStatus.operating;
        currentStatus.compressorFrequency = receivedStatus.compressorFrequency;

        this->compressor_frequency_sensor->publish_state(currentStatus.compressorFrequency);

        ESP_LOGD("Decoder", "[Operating: %d]", currentStatus.operating);
        ESP_LOGD("Decoder", "[Compressor Freq: %d]", currentStatus.compressorFrequency);

        // RCVD_PKT_STATUS;
    }
             break;

    case 0x09:
        /* unknown */
        ESP_LOGD("Decoder", "[0x09 is unknown]");
        break;
    case 0x20:
    case 0x22: {
        ESP_LOGD("Decoder", "[Packet Functions 0x20 et 0x22]");
        if (dataLength == 0x10) {
            if (data[0] == 0x20) {
                functions.setData1(&data[1]);
            } else {
                functions.setData2(&data[1]);
            }

            // RCVD_PKT_FUNCTIONS;
        }

    }
             break;

    default:
        ESP_LOGW("Decoder", "type de packet [%02X] <-- inconnu et inattendu", data[0]);
        break;
    }
}
void CN105Climate::processCommand() {
    switch (this->command) {
    case 0x61:  /* last update was successful */
        ESP_LOGI(TAG, "Last heatpump data update successful!");

        if (!this->autoUpdate) {
            this->buildAndSendRequestsInfoPackets();
        }

        break;

    case 0x62:  /* packet contains data (room °C, settings, timer, status, or functions...)*/
        this->getDataFromResponsePacket();

        break;
    case 0x7a:
        ESP_LOGI(TAG, "--> Heatpump did reply: connection success! <--");
        this->isHeatpumpConnected_ = true;
        programUpdateInterval();        // we know a check in this method is done on autoupdate value        
        break;
    default:
        break;
    }
}
//#endregion input_parsing

//#region esp_home framework functions

/**
 * This method is call by the esphome framework to initialize the component
 * We don't try to connect to the heater here because errors could not be logged fine because the
 * UART is used for communication with the heatpump
 * setupUART will handle the
*/
void CN105Climate::setup() {
    // Votre code ici
    ESP_LOGD(TAG, "Initialisation du composant: appel de setup()");
    this->current_temperature = NAN;
    this->target_temperature = NAN;
    this->fan_mode = climate::CLIMATE_FAN_OFF;
    this->swing_mode = climate::CLIMATE_SWING_OFF;
    this->initBytePointer();

    ESP_LOGI(
        TAG,
        "hw_serial(%p) is &Serial(%p)? %s",
        this->get_hw_serial_(),
        &Serial,
        YESNO(this->get_hw_serial_() == &Serial)
    );

    ESP_LOGI(TAG, "bauds: %d", this->baud_);

    this->check_logger_conflict_();

    this->setupUART();
    this->sendFirstConnectionPacket();
}
void CN105Climate::loop() {
    this->processInput();
}

/**
 * delays for 12s the call of setup() by esphome to let the time for OTA logs to initialize
*/
bool CN105Climate::can_proceed() {
    if (!this->init_delay_initiated_) {
        ESP_LOGI(TAG, "delaying setup process for 12 seconds..");
        this->init_delay_initiated_ = true;
        this->init_delay_completed_ = false;
        this->set_timeout("init_delay", 12000, [this]() { this->init_delay_completed_ = true; });
    }

    if (this->init_delay_completed_) {
        ESP_LOGI(TAG, "delay expired setup will start...");
    }
    return this->init_delay_completed_;
}

//#endregion esp_home


//#region settingClimSettings
void CN105Climate::setModeSetting(const char* setting) {
    int index = lookupByteMapIndex(MODE_MAP, 5, setting);
    if (index > -1) {
        wantedSettings.mode = MODE_MAP[index];
    } else {
        wantedSettings.mode = MODE_MAP[0];
    }
}

void CN105Climate::setPowerSetting(const char* setting) {
    int index = lookupByteMapIndex(POWER_MAP, 2, setting);
    if (index > -1) {
        wantedSettings.power = POWER_MAP[index];
    } else {
        wantedSettings.power = POWER_MAP[0];
    }
}

void CN105Climate::setFanSpeed(const char* setting) {
    int index = lookupByteMapIndex(FAN_MAP, 6, setting);
    if (index > -1) {
        wantedSettings.fan = FAN_MAP[index];
    } else {
        wantedSettings.fan = FAN_MAP[0];
    }
}

void CN105Climate::setVaneSetting(const char* setting) {
    int index = lookupByteMapIndex(VANE_MAP, 7, setting);
    if (index > -1) {
        wantedSettings.vane = VANE_MAP[index];
    } else {
        wantedSettings.vane = VANE_MAP[0];
    }
}

void CN105Climate::setWideVaneSetting(const char* setting) {
    int index = lookupByteMapIndex(WIDEVANE_MAP, 7, setting);
    if (index > -1) {
        wantedSettings.wideVane = WIDEVANE_MAP[index];
    } else {
        wantedSettings.wideVane = WIDEVANE_MAP[0];
    }
}




//#endregion clim

//#region climate
void CN105Climate::control(const esphome::climate::ClimateCall& call) {

    ESP_LOGD("control", "espHome control() interface method called...");
    bool updated = false;
    // Traiter les commandes de climatisation ici
    if (call.get_mode().has_value()) {
        ESP_LOGD("control", "Mode change asked");
        // Changer le mode de climatisation
        this->mode = *call.get_mode();
        updated = true;
        controlMode();
    }

    if (call.get_target_temperature().has_value()) {
        // Changer la température cible
        ESP_LOGI("control", "Setting heatpump setpoint : %.1f", *call.get_target_temperature());
        this->target_temperature = *call.get_target_temperature();
        updated = true;
        controlTemperature();
    }

    if (call.get_fan_mode().has_value()) {
        ESP_LOGD("control", "Fan change asked");
        // Changer le mode de ventilation
        this->fan_mode = *call.get_fan_mode();
        updated = true;
        this->controlFan();
    }
    if (call.get_swing_mode().has_value()) {
        ESP_LOGD("control", "Swing change asked");
        // Changer le mode de balancement
        this->swing_mode = *call.get_swing_mode();
        updated = true;
        this->controlSwing();
    }



    if (updated) {
        ESP_LOGD(TAG, "User changed something, sending change to heatpump...");
        this->sendWantedSettings();
    }

    // send the update back to esphome:
    this->publish_state();
}
void CN105Climate::controlSwing() {
    switch (this->swing_mode) {
    case climate::CLIMATE_SWING_OFF:
        this->setVaneSetting("AUTO");
        //setVaneSetting supports:  AUTO 1 2 3 4 5 and SWING
        //this->setWideVaneSetting("|");
        break;
    case climate::CLIMATE_SWING_VERTICAL:
        this->setVaneSetting("SWING");
        //this->setWideVaneSetting("|");
        break;
    case climate::CLIMATE_SWING_HORIZONTAL:
        this->setVaneSetting("3");
        this->setWideVaneSetting("SWING");
        break;
    case climate::CLIMATE_SWING_BOTH:
        this->setVaneSetting("SWING");
        this->setWideVaneSetting("SWING");
        break;
    default:
        ESP_LOGW(TAG, "control - received unsupported swing mode request.");
    }
}
void CN105Climate::controlFan() {

    switch (this->fan_mode.value()) {
    case climate::CLIMATE_FAN_OFF:
        this->setPowerSetting("OFF");
        break;
    case climate::CLIMATE_FAN_QUIET:
        this->setFanSpeed("QUIET");
        break;
    case climate::CLIMATE_FAN_DIFFUSE:
        this->setFanSpeed("QUIET");
        break;
    case climate::CLIMATE_FAN_LOW:
        this->setFanSpeed("1");
        break;
    case climate::CLIMATE_FAN_MEDIUM:
        this->setFanSpeed("2");
        break;
    case climate::CLIMATE_FAN_MIDDLE:
        this->setFanSpeed("3");
        break;
    case climate::CLIMATE_FAN_HIGH:
        this->setFanSpeed("4");
        break;
    case climate::CLIMATE_FAN_ON:
    case climate::CLIMATE_FAN_AUTO:
    default:
        this->setFanSpeed("AUTO");
        break;
    }
}
void CN105Climate::controlTemperature() {
    float setting = this->target_temperature;

    if (!this->tempMode) {
        this->wantedSettings.temperature = this->lookupByteMapIndex(TEMP_MAP, 16, (int)(setting + 0.5)) > -1 ? setting : TEMP_MAP[0];
    } else {
        setting = setting * 2;
        setting = round(setting);
        setting = setting / 2;
        this->wantedSettings.temperature = setting < 10 ? 10 : (setting > 31 ? 31 : setting);
    }
}


void CN105Climate::controlMode() {
    switch (this->mode) {
    case climate::CLIMATE_MODE_COOL:
        ESP_LOGI("control", "changing mode to COOL");
        this->setModeSetting("COOL");
        this->setPowerSetting("ON");
        break;
    case climate::CLIMATE_MODE_HEAT:
        ESP_LOGI("control", "changing mode to HEAT");
        this->setModeSetting("HEAT");
        this->setPowerSetting("ON");
        break;
    case climate::CLIMATE_MODE_DRY:
        ESP_LOGI("control", "changing mode to DRY");
        this->setModeSetting("DRY");
        this->setPowerSetting("ON");
        break;
    case climate::CLIMATE_MODE_HEAT_COOL:
        ESP_LOGI("control", "changing mode to HEAT_COOL");
        this->setModeSetting("AUTO");
        this->setPowerSetting("ON");
        break;
    case climate::CLIMATE_MODE_FAN_ONLY:
        ESP_LOGI("control", "changing mode to FAN_ONLY");
        this->setModeSetting("FAN");
        this->setPowerSetting("ON");
        break;
    case climate::CLIMATE_MODE_OFF:
        ESP_LOGI("control", "changing mode to OFF");
        this->setPowerSetting("OFF");
        break;
    default:
        ESP_LOGW("control", "mode non pris en charge");
    }
}



climate::ClimateTraits CN105Climate::traits() {
    // Définir les caractéristiques de ton climatiseur ici

    return traits_;
    // auto traits = climate::ClimateTraits();

    // traits.set_supports_current_temperature(true);
    // traits.set_supports_two_point_target_temperature(false);
    // traits.set_supports_cool_mode(true);
    // traits.set_supports_heat_mode(true);
    // traits.set_supports_fan_mode_auto(true);
    // traits.set_supports_fan_mode_on(true);
    // traits.set_supports_fan_mode_off(true);
    // traits.set_supports_swing_mode_off(true);
    // traits.set_supports_swing_mode_both(true);
    // traits.set_visual_min_temperature(16);
    // traits.set_visual_max_temperature(30);
    // traits.set_visual_temperature_step(0.5f);
    // return traits;
}


/**
 * Modify our supported traits.
 *
 * Returns:
 *   A reference to this class' supported climate::ClimateTraits.
 */
climate::ClimateTraits& CN105Climate::config_traits() {
    return traits_;
}

bool CN105Climate::isHeatpumpConnectionActive() {
    long lrTimeMs = CUSTOM_MILLIS - this->lastResponseMs;

    if (lrTimeMs > MAX_DELAY_RESPONSE) {
        ESP_LOGW(TAG, "Heatpump has not replied for %d ms", lrTimeMs);
        ESP_LOGI(TAG, "We think Heatpump is not connected anymore..");
    }

    return  (lrTimeMs < MAX_DELAY_RESPONSE);
}
//#endregion climate

//#region packet_management Gestion des paquets

void CN105Climate::writePacket(byte* packet, int length, bool checkIsActive) {

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

    if (settings.power != currentSettings.power) {
        ESP_LOGD(TAG, "power changed -> %s", settings.power);
        packet[8] = POWER[lookupByteMapIndex(POWER_MAP, 2, settings.power)];
        packet[6] += CONTROL_PACKET_1[0];
    }
    if (settings.mode != currentSettings.mode) {
        ESP_LOGD(TAG, "heatpump mode changed -> %s", settings.mode);
        packet[9] = MODE[lookupByteMapIndex(MODE_MAP, 5, settings.mode)];
        packet[6] += CONTROL_PACKET_1[1];
    }
    if (!tempMode && settings.temperature != currentSettings.temperature) {
        ESP_LOGD(TAG, "temperature changed (tempmode is false) -> %f", settings.temperature);
        packet[10] = TEMP[lookupByteMapIndex(TEMP_MAP, 16, settings.temperature)];
        packet[6] += CONTROL_PACKET_1[2];
    } else if (tempMode && settings.temperature != currentSettings.temperature) {
        ESP_LOGD(TAG, "temperature changed (tempmode is true) -> %f", settings.temperature);
        float temp = (settings.temperature * 2) + 128;
        packet[19] = (int)temp;
        packet[6] += CONTROL_PACKET_1[2];
    }
    if (settings.fan != currentSettings.fan) {
        ESP_LOGD(TAG, "heatpump fan changed -> %s", settings.fan);
        packet[11] = FAN[lookupByteMapIndex(FAN_MAP, 6, settings.fan)];
        packet[6] += CONTROL_PACKET_1[3];
    }
    if (settings.vane != currentSettings.vane) {
        ESP_LOGD(TAG, "heatpump vane changed -> %s", settings.vane);
        packet[12] = VANE[lookupByteMapIndex(VANE_MAP, 7, settings.vane)];
        packet[6] += CONTROL_PACKET_1[4];
    }
    if (settings.wideVane != currentSettings.wideVane) {
        ESP_LOGD(TAG, "heatpump widevane changed -> %s", settings.wideVane);
        packet[18] = WIDEVANE[lookupByteMapIndex(WIDEVANE_MAP, 7, settings.wideVane)] | (wideVaneAdj ? 0x80 : 0x00);
        packet[7] += CONTROL_PACKET_2[0];
    }
    // add the checksum
    byte chkSum = checkSum(packet, 21);
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

    // here we restore the update scheduler we had canceled 
    this->set_timeout(DEFER_SHEDULER_INTERVAL_SYNC_NAME, DEFER_SCHEDULE_UPDATE_LOOP_DELAY, [this]() {
        this->programUpdateInterval();
        });

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
    byte packet[PACKET_LEN] = {};
    createInfoPacket(packet, packetType);
    this->writePacket(packet, PACKET_LEN);

    // When we send a status request, we expect a response
    // and we use that expectation as a connection status
    // deprecade: cet appel est remplacé par une check isHeatpumpConnectionActive dans la methode writePacket     
    // this->programResponseCheck(packetType);
}


/**
 * builds ans send all 3 types of packet to get a full informations back from heatpump
 * 3 packets are sent at 300 ms interval
*/
void CN105Climate::buildAndSendRequestsInfoPackets() {


    // TODO: faire 3 fonctions au lieu d'une
    // TODO: utiliser this->retry() de Component pour différer l'exécution si une écriture ou une lecture est en cours.

    /*if (this->isHeatpumpConnected_) {
        ESP_LOGD(TAG, "buildAndSendRequestsInfoPackets..");
        ESP_LOGD(TAG, "sending a request for settings packet (0x02)");
        this->buildAndSendRequestPacket(RQST_PKT_SETTINGS);
        ESP_LOGD(TAG, "sending a request room temp packet (0x03)");
        this->buildAndSendRequestPacket(RQST_PKT_ROOM_TEMP);
        ESP_LOGD(TAG, "sending a request status paquet (0x06)");
        this->buildAndSendRequestPacket(RQST_PKT_STATUS);
    } else {
        ESP_LOGE(TAG, "sync impossible: heatpump not connected");
    }

    this->programUpdateInterval();*/

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
    this->programUpdateInterval();
}




void CN105Climate::createInfoPacket(byte* packet, byte packetType) {
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
    byte chkSum = checkSum(packet, 21);
    packet[21] = chkSum;
}
void CN105Climate::set_remote_temperature(float setting) {
    byte packet[PACKET_LEN] = {};

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
    byte chkSum = checkSum(packet, 21);
    packet[21] = chkSum;
    ESP_LOGD(TAG, "sending remote temperature packet...");
    writePacket(packet, PACKET_LEN);
}
void CN105Climate::hpPacketDebug(byte* packet, unsigned int length, const char* packetDirection) {
    char buffer[4]; // Petit tampon pour stocker chaque octet sous forme de texte
    char outputBuffer[length * 4 + 1]; // Tampon pour stocker l'ensemble des octets sous forme de texte

    // Initialisation du tampon de sortie avec une chaîne vide
    outputBuffer[0] = '\0';

    for (unsigned int i = 0; i < length; i++) {
        snprintf(buffer, sizeof(buffer), "%02X ", packet[i]); // Utilisation de snprintf pour éviter les dépassements de tampon
        strcat(outputBuffer, buffer);
    }

    ESP_LOGD(packetDirection, "%s", outputBuffer);
}


void CN105Climate::prepareInfoPacket(byte* packet, int length) {
    ESP_LOGV(TAG, "preparing info packet...");

    memset(packet, 0, length * sizeof(byte));

    for (int i = 0; i < INFOHEADER_LEN && i < length; i++) {
        packet[i] = INFOHEADER[i];
    }
}

void CN105Climate::prepareSetPacket(byte* packet, int length) {
    ESP_LOGV(TAG, "preparing Set packet...");
    memset(packet, 0, length * sizeof(byte));

    for (int i = 0; i < HEADER_LEN && i < length; i++) {
        packet[i] = HEADER[i];
    }
}

//#endregion <packet_management>

//#region heatpump_functions fonctions clim

heatpumpFunctions CN105Climate::getFunctions() {
    ESP_LOGV(TAG, "getting the list of functions...");

    functions.clear();

    byte packet1[PACKET_LEN] = {};
    byte packet2[PACKET_LEN] = {};

    prepareInfoPacket(packet1, PACKET_LEN);
    packet1[5] = FUNCTIONS_GET_PART1;
    packet1[21] = checkSum(packet1, 21);

    prepareInfoPacket(packet2, PACKET_LEN);
    packet2[5] = FUNCTIONS_GET_PART2;
    packet2[21] = checkSum(packet2, 21);

    /*while (!canSend(false)) {
        CUSTOM_DELAY(10); // esphome::CUSTOM_DELAY(10);
    }*/
    ESP_LOGD(TAG, "sending a getFunctions packet part 1");
    writePacket(packet1, PACKET_LEN);
    //readPacket();

    /*while (!canSend(false)) {
        //esphome::CUSTOM_DELAY(10);
        CUSTOM_DELAY(10);
    }*/
    ESP_LOGD(TAG, "sending a getFunctions packet part 2");
    writePacket(packet2, PACKET_LEN);
    //readPacket();

    // retry reading a few times in case responses were related
    // to other requests
    /*for (int i = 0; i < 5 && !functions.isValid(); ++i) {
        //esphome::CUSTOM_DELAY(100);
        CUSTOM_DELAY(100);
        readPacket();
    }*/

    return functions;
}

bool CN105Climate::setFunctions(heatpumpFunctions const& functions) {
    if (!functions.isValid()) {
        return false;
    }

    byte packet1[PACKET_LEN] = {};
    byte packet2[PACKET_LEN] = {};

    prepareSetPacket(packet1, PACKET_LEN);
    packet1[5] = FUNCTIONS_SET_PART1;

    prepareSetPacket(packet2, PACKET_LEN);
    packet2[5] = FUNCTIONS_SET_PART2;

    functions.getData1(&packet1[6]);
    functions.getData2(&packet2[6]);

    // sanity check, we expect data byte 15 (index 20) to be 0
    if (packet1[20] != 0 || packet2[20] != 0)
        return false;

    // make sure all the other data bytes are set
    for (int i = 6; i < 20; ++i) {
        if (packet1[i] == 0 || packet2[i] == 0)
            return false;
    }

    packet1[21] = checkSum(packet1, 21);
    packet2[21] = checkSum(packet2, 21);
    /*
        while (!canSend(false)) {
            //esphome::CUSTOM_DELAY(10);
            CUSTOM_DELAY(10);
        }*/
    ESP_LOGD(TAG, "sending a setFunctions packet part 1");
    writePacket(packet1, PACKET_LEN);
    //readPacket();

    /*while (!canSend(false)) {
        //esphome::CUSTOM_DELAY(10);
        CUSTOM_DELAY(10);
    }*/
    ESP_LOGD(TAG, "sending a setFunctions packet part 2");
    writePacket(packet2, PACKET_LEN);
    //readPacket();

    return true;
}


heatpumpFunctions::heatpumpFunctions() {
    clear();
}

bool heatpumpFunctions::isValid() const {
    return _isValid1 && _isValid2;
}

void heatpumpFunctions::setData1(byte* data) {
    memcpy(raw, data, 15);
    _isValid1 = true;
}

void heatpumpFunctions::setData2(byte* data) {
    memcpy(raw + 15, data, 15);
    _isValid2 = true;
}

void heatpumpFunctions::getData1(byte* data) const {
    memcpy(data, raw, 15);
}

void heatpumpFunctions::getData2(byte* data) const {
    memcpy(data, raw + 15, 15);
}

void heatpumpFunctions::clear() {
    memset(raw, 0, sizeof(raw));
    _isValid1 = false;
    _isValid2 = false;
}

int heatpumpFunctions::getCode(byte b) {
    return ((b >> 2) & 0xff) + 100;
}

int heatpumpFunctions::getValue(byte b) {
    return b & 3;
}

int heatpumpFunctions::getValue(int code) {
    if (code > 128 || code < 101)
        return 0;

    for (int i = 0; i < MAX_FUNCTION_CODE_COUNT; ++i) {
        if (getCode(raw[i]) == code)
            return getValue(raw[i]);
    }

    return 0;
}

bool heatpumpFunctions::setValue(int code, int value) {
    if (code > 128 || code < 101)
        return false;

    if (value < 1 || value > 3)
        return false;

    for (int i = 0; i < MAX_FUNCTION_CODE_COUNT; ++i) {
        if (getCode(raw[i]) == code) {
            raw[i] = ((code - 100) << 2) + value;
            return true;
        }
    }

    return false;
}

heatpumpFunctionCodes heatpumpFunctions::getAllCodes() {
    heatpumpFunctionCodes result;
    for (int i = 0; i < MAX_FUNCTION_CODE_COUNT; ++i) {
        int code = getCode(raw[i]);
        result.code[i] = code;
        result.valid[i] = (code >= 101 && code <= 128);
    }

    return result;
}

bool heatpumpFunctions::operator==(const heatpumpFunctions& rhs) {
    return this->isValid() == rhs.isValid() && memcmp(this->raw, rhs.raw, MAX_FUNCTION_CODE_COUNT * sizeof(int)) == 0;
}

bool heatpumpFunctions::operator!=(const heatpumpFunctions& rhs) {
    return !(*this == rhs);
}
//#endregion heatpump_functions