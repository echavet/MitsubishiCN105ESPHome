#pragma once
#include <esphome.h>
#include "esphome/components/uart/uart.h"

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#endif

#define CUSTOM_MILLIS esphome::millis()
#define CUSTOM_DELAY(x) esphome::delay(x)

#define MAX_DATA_BYTES     64         // max number of data bytes in incoming messages
#define MAX_DELAY_RESPONSE_FACTOR 10  // update_interval*10 seconds max without response

//#define TEST_MODE

static const char* LOG_ACTION_EVT_TAG = "EVT_SETS";
static const char* TAG = "CN105"; // Logging tag
static const char* LOG_REMOTE_TEMP = "REMOTE_TEMP"; // Logging tag
static const char* LOG_ACK = "ACK"; // Logging tag
static const char* LOG_SETTINGS_TAG = "SETTINGS";   // Logging settings changes
static const char* LOG_STATUS_TAG = "STATUS";       // Logging status changes
static const char* LOG_CYCLE_TAG = "CYCLE";         // loop cycles logs
static const char* LOG_UPD_INT_TAG = "UPDT_ITVL";   // update interval logging
static const char* LOG_SET_RUN_STATE = "SET_RUN_STATE";


static const char* SHEDULER_REMOTE_TEMP_TIMEOUT = "->remote_temp_timeout";

// defering delay for update_interval when we've just sent a wentedSettings
static const int DEFER_SCHEDULE_UPDATE_LOOP_DELAY = 750;

static const int PACKET_LEN = 22;
static const int PACKET_TYPE_DEFAULT = 99;

static const int CONNECT_LEN = 8;
static const uint8_t CONNECT[CONNECT_LEN] = { 0xfc, 0x5a, 0x01, 0x30, 0x02, 0xca, 0x01, 0xa8 };
static const int HEADER_LEN = 8;
static const uint8_t HEADER[HEADER_LEN] = { 0xfc, 0x41, 0x01, 0x30, 0x10, 0x01, 0x00, 0x00 };

static const int INFOHEADER_LEN = 5;
static const uint8_t INFOHEADER[INFOHEADER_LEN] = { 0xfc, 0x42, 0x01, 0x30, 0x10 };


static const int INFOMODE_LEN = 7;
static const uint8_t INFOMODE[INFOMODE_LEN] = {
  0x02, // request a settings packet - RQST_PKT_SETTINGS
  0x03, // request the current room temp - RQST_PKT_ROOM_TEMP
  0x04, // unknown
  0x05, // request the timers - RQST_PKT_TIMERS
  0x06, // request status - RQST_PKT_STATUS
  0x09, // request standby mode (maybe?) RQST_PKT_STANDBY
  0x42  // request HVAC options - RQST_PKT_HVAC_OPTIONS
};

static const int RCVD_PKT_NONE = -1;
static const int RCVD_PKT_FAIL = 0;
static const int RCVD_PKT_CONNECT_SUCCESS = 1;
static const int RCVD_PKT_SETTINGS = 2;
static const int RCVD_PKT_ROOM_TEMP = 3;
static const int RCVD_PKT_UPDATE_SUCCESS = 4;
static const int RCVD_PKT_STATUS = 5;
static const int RCVD_PKT_TIMER = 6;
static const int RCVD_PKT_FUNCTIONS = 7;

// the nb of request without response before we declare UART is not connected anymore
static const int MAX_NON_RESPONSE_REQ = 5;

static const uint8_t CONTROL_PACKET_1[5] = { 0x01,    0x02,  0x04,  0x08, 0x10 };
//{"POWER","MODE","TEMP","FAN","VANE"};
static const uint8_t CONTROL_PACKET_2[1] = { 0x01 };
//{"WIDEVANE"};
static const uint8_t RUN_STATE_PACKET_1[5] = { 0x01, 0x04, 0x08, 0x10, 0x20 };
static const uint8_t RUN_STATE_PACKET_2[5] = { 0x02, 0x04, 0x08, 0x10, 0x20 };
static const uint8_t POWER[2] = { 0x00, 0x01 };
static const char* POWER_MAP[2] = { "OFF", "ON" };
static const uint8_t MODE[5] = { 0x01,   0x02,  0x03, 0x07, 0x08 };
static const char* MODE_MAP[5] = { "HEAT", "DRY", "COOL", "FAN", "AUTO" };
static const uint8_t TEMP[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
static const int TEMP_MAP[16] = { 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16 };
static const uint8_t FAN[6] = { 0x00,  0x01,   0x02, 0x03, 0x05, 0x06 };
static const char* FAN_MAP[6] = { "AUTO", "QUIET", "1", "2", "3", "4" };
static const uint8_t VANE[7] = { 0x00,  0x01, 0x02, 0x03, 0x04, 0x05, 0x07 };
static const char* VANE_MAP[7] = { "AUTO", "↑↑", "↑", "—", "↓", "↓↓", "SWING" };
static const uint8_t WIDEVANE[8] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x08, 0x0c, 0x00 };
static const char* WIDEVANE_MAP[8] = { "←←", "←", "|", "→", "→→", "←→", "SWING", "AIRFLOW CONTROL" };
static const uint8_t ROOM_TEMP[32] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                                  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f };
static const int ROOM_TEMP_MAP[32] = { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                                  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41 };
static const uint8_t TIMER_MODE[4] = { 0x00,  0x01,  0x02, 0x03 };
static const char* TIMER_MODE_MAP[4] = { "NONE", "OFF", "ON", "BOTH" };

static const uint8_t AIRFLOW_CONTROL[3] = { 0x00, 0x01, 0x02 };
static const char* AIRFLOW_CONTROL_MAP[3] = { "EVEN", "INDIRECT", "DIRECT" };

//added NET to work with additional data
static const uint8_t STAGE[7] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 };
static const char* STAGE_MAP[7] = { "IDLE", "LOW", "GENTLE", "MEDIUM", "MODERATE", "HIGH", "DIFFUSE" };

static const uint8_t SUB_MODE[4] = { 0x00, 0x02, 0x04, 0x08 };
static const char* SUB_MODE_MAP[4] = { "NORMAL", "DEFROST", "PREHEAT", "STANDBY" };
static const uint8_t AUTO_SUB_MODE[4] = { 0x00, 0x01, 0x02, 0x03 };
static const char* AUTO_SUB_MODE_MAP[4] = { "AUTO_OFF","AUTO_COOL", "AUTO_HEAT", "AUTO_LEADER" };

static const int TIMER_INCREMENT_MINUTES = 10;

static const uint8_t FUNCTIONS_SET_PART1 = 0x1F;
static const uint8_t FUNCTIONS_GET_PART1 = 0x20;
static const uint8_t FUNCTIONS_SET_PART2 = 0x21;
static const uint8_t FUNCTIONS_GET_PART2 = 0x22;


// Déclaration de la constante - pas de définition ici
//extern const uint32_t ESPMHP_POLL_INTERVAL_DEFAULT;
//extern const uint8_t ESPMHP_MIN_TEMPERATURE;
//extern const uint8_t ESPMHP_MAX_TEMPERATURE;
//extern const float ESPMHP_TEMPERATURE_STEP;


static const int RQST_PKT_SETTINGS = 0;
static const int RQST_PKT_ROOM_TEMP = 1;
static const int RQST_PKT_TIMERS = 3;
static const int RQST_PKT_STATUS = 4;
static const int RQST_PKT_STANDBY = 5;
static const int RQST_PKT_UNKNOWN = 2;
static const int RQST_PKT_HVAC_OPTIONS = 6;

const uint8_t ESPMHP_MIN_TEMPERATURE = 16; //16
const uint8_t ESPMHP_MAX_TEMPERATURE = 26; //31
const float ESPMHP_TEMPERATURE_STEP = 0.5;


struct heatpumpSettings {
    const char* power;
    const char* mode;
    float temperature;
    const char* fan;
    const char* vane; //vertical vane, up/down
    const char* wideVane; //horizontal vane, left/right
    bool iSee;   //iSee sensor, at the moment can only detect it, not set it
    bool connected;
    const char* stage;
    const char* sub_mode;
    const char* auto_sub_mode;

    void resetSettings() {
        power = nullptr;
        mode = nullptr;
        temperature = -1.0f;
        fan = nullptr;
        vane = nullptr;
        wideVane = nullptr;
    }

    heatpumpSettings& operator=(const heatpumpSettings& other) {
        if (this != &other) { // protection contre l'auto-affectation
            power = other.power;
            mode = other.mode;
            temperature = other.temperature;
            fan = other.fan;
            vane = other.vane;
            wideVane = other.wideVane;
            iSee = other.iSee;
            connected = other.connected;
            stage = other.stage;
            sub_mode = other.sub_mode;
            auto_sub_mode = other.auto_sub_mode;
        }
        return *this;
    }


    bool operator==(const heatpumpSettings& other) const {
        return power == other.power &&
            mode == other.mode &&
            temperature == other.temperature &&
            fan == other.fan &&
            vane == other.vane &&
            wideVane == other.wideVane;
        //iSee == other.iSee;
    }

    bool operator!=(const heatpumpSettings& other) {
        return !(this->operator==(other));
    }

};

struct wantedHeatpumpSettings : heatpumpSettings {
    bool hasChanged;
    bool hasBeenSent;
    uint8_t nb_deffered_requests;
    long lastChange;

    void resetSettings() {
        heatpumpSettings::resetSettings();

        hasChanged = false;
        hasBeenSent = false;
        //nb_deffered_requests = 0;
        //lastChange = 0;
    }

    wantedHeatpumpSettings& operator=(const wantedHeatpumpSettings& other) {
        if (this != &other) { // self-assignment protection
            heatpumpSettings::operator=(other); // Appel à l'opérateur d'affectation de la classe de base
            hasChanged = other.hasChanged;
            hasBeenSent = other.hasBeenSent;
        }
        return *this;
    }

    wantedHeatpumpSettings& operator=(const heatpumpSettings& other) {
        if (this != &other) { // self-assignment protection
            heatpumpSettings::operator=(other); // Copie des membres de base
        }
        return *this;
    }
};

struct heatpumpTimers {
    const char* mode;
    int onMinutesSet;
    int onMinutesRemaining;
    int offMinutesSet;
    int offMinutesRemaining;

    heatpumpTimers& operator=(const heatpumpTimers& other) {
        if (this != &other) { // protection contre l'auto-affectation
            mode = other.mode;
            onMinutesSet = other.onMinutesSet;
            onMinutesRemaining = other.onMinutesRemaining;
            offMinutesSet = other.offMinutesSet;
            offMinutesRemaining = other.offMinutesRemaining;
        }
        return *this;
    }
    bool operator==(const heatpumpTimers& other) const {
        return
            mode == other.mode &&
            onMinutesSet == other.onMinutesSet &&
            onMinutesRemaining == other.onMinutesRemaining &&
            offMinutesSet == other.offMinutesSet &&
            offMinutesRemaining == other.offMinutesRemaining;
    }


    bool operator!=(const heatpumpTimers& other) const {
        return !(this->operator==(other));
    }
};


struct heatpumpStatus {
    float roomTemperature;
    float outsideAirTemperature;
    bool operating; // if true, the heatpump is operating to reach the desired temperature
    heatpumpTimers timers;
    float compressorFrequency;
    float inputPower;
    float kWh;
    float runtimeHours;

    bool operator==(const heatpumpStatus& other) const {
        return (std::isnan(roomTemperature) ? std::isnan(other.roomTemperature) : roomTemperature == other.roomTemperature) &&
            (std::isnan(outsideAirTemperature) ? std::isnan(other.outsideAirTemperature) : outsideAirTemperature == other.outsideAirTemperature) &&
            operating == other.operating &&
            //timers == other.timers &&  // Assurez-vous que l'opérateur == est également défini pour heatpumpTimers
            compressorFrequency == other.compressorFrequency &&
            inputPower == other.inputPower &&
            kWh == other.kWh &&
            runtimeHours == other.runtimeHours;
    }

    bool operator!=(const heatpumpStatus& other) const {
        return !(*this == other);
    }
};

struct heatpumpRunStates {
    int8_t air_purifier;
    int8_t night_mode;
    int8_t circulator;
    const char* airflow_control;
    
    void resetSettings() {
        air_purifier = -1;
        night_mode = -1;
        circulator = -1;
        airflow_control = nullptr;
    }
    
    heatpumpRunStates& operator=(const heatpumpRunStates& other) {
        if (this != &other) {
            air_purifier = other.air_purifier;
            night_mode = other.night_mode;
            circulator = other.circulator;
            airflow_control = other.airflow_control;
        }
        return *this;
    }
    
    bool operator==(const heatpumpRunStates& other) const {
        return air_purifier == other.air_purifier &&
            night_mode == other.night_mode &&
            circulator == other.circulator &&
            airflow_control == other.airflow_control;
    }
    
    bool operator!=(const heatpumpRunStates& other) {
        return !(this->operator==(other));
    }
};

struct wantedHeatpumpRunStates : heatpumpRunStates {
    bool hasChanged;
    bool hasBeenSent;
    long lastChange;
    
    void resetSettings() {
        heatpumpRunStates::resetSettings();
        
        hasChanged = false;
        hasBeenSent = false;
    }
    
    wantedHeatpumpRunStates& operator=(const wantedHeatpumpRunStates& other) {
        if (this != &other) {
            heatpumpRunStates::operator=(other);
            hasChanged = other.hasChanged;
            hasBeenSent = other.hasBeenSent;
        }
        return *this;
    }
    
    wantedHeatpumpRunStates& operator=(const heatpumpRunStates& other) {
        if (this != &other) {
            heatpumpRunStates::operator=(other);
        }
        return *this;
    }
};