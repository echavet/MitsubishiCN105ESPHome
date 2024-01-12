#pragma once
#include <esphome.h>
#include <esphome/core/preferences.h>

#define CUSTOM_MILLIS ::millis()
#define MAX_DATA_BYTES     64       // max number of data bytes in incoming messages
#define MAX_DELAY_RESPONSE 10000    // 10 seconds max without response

static const char* TAG = "CN105"; // Logging tag

static const char* SHEDULER_INTERVAL_SYNC_NAME = "hp->sync"; // name of the scheduler to prpgram hp updates
static const char* DEFER_SHEDULER_INTERVAL_SYNC_NAME = "hp->sync_defer"; // name of the scheduler to prpgram hp updates

static const int DEFER_SCHEDULE_UPDATE_LOOP_DELAY = 500;
static const int PACKET_LEN = 22;
static const int PACKET_SENT_INTERVAL_MS = 1000;
static const int PACKET_INFO_INTERVAL_MS = 2000;
static const int PACKET_TYPE_DEFAULT = 99;
static const int AUTOUPDATE_GRACE_PERIOD_IGNORE_EXTERNAL_UPDATES_MS = 30000;

static const int CONNECT_LEN = 8;
static const byte CONNECT[CONNECT_LEN] = { 0xfc, 0x5a, 0x01, 0x30, 0x02, 0xca, 0x01, 0xa8 };
static const int HEADER_LEN = 8;
static const byte HEADER[HEADER_LEN] = { 0xfc, 0x41, 0x01, 0x30, 0x10, 0x01, 0x00, 0x00 };

static const int INFOHEADER_LEN = 5;
static const byte INFOHEADER[INFOHEADER_LEN] = { 0xfc, 0x42, 0x01, 0x30, 0x10 };


static const int INFOMODE_LEN = 6;
static const byte INFOMODE[INFOMODE_LEN] = {
  0x02, // request a settings packet - RQST_PKT_SETTINGS
  0x03, // request the current room temp - RQST_PKT_ROOM_TEMP
  0x04, // unknown
  0x05, // request the timers - RQST_PKT_TIMERS
  0x06, // request status - RQST_PKT_STATUS
  0x09  // request standby mode (maybe?) RQST_PKT_STANDBY
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

static const byte CONTROL_PACKET_1[5] = { 0x01,    0x02,  0x04,  0x08, 0x10 };
//{"POWER","MODE","TEMP","FAN","VANE"};
static const byte CONTROL_PACKET_2[1] = { 0x01 };
//{"WIDEVANE"};
static const byte POWER[2] = { 0x00, 0x01 };
static const char* POWER_MAP[2] = { "OFF", "ON" };
static const byte MODE[5] = { 0x01,   0x02,  0x03, 0x07, 0x08 };
static const char* MODE_MAP[5] = { "HEAT", "DRY", "COOL", "FAN", "AUTO" };
static const byte TEMP[16] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f };
static const int TEMP_MAP[16] = { 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16 };
static const byte FAN[6] = { 0x00,  0x01,   0x02, 0x03, 0x05, 0x06 };
static const char* FAN_MAP[6] = { "AUTO", "QUIET", "1", "2", "3", "4" };
static const byte VANE[7] = { 0x00,  0x01, 0x02, 0x03, 0x04, 0x05, 0x07 };
static const char* VANE_MAP[7] = { "AUTO", "1", "2", "3", "4", "5", "SWING" };
static const byte WIDEVANE[7] = { 0x01, 0x02, 0x03, 0x04, 0x05, 0x08, 0x0c };
static const char* WIDEVANE_MAP[7] = { "<<", "<",  "|",  ">",  ">>", "<>", "SWING" };
static const byte ROOM_TEMP[32] = { 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f,
                                  0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f };
static const int ROOM_TEMP_MAP[32] = { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25,
                                  26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41 };
static const byte TIMER_MODE[4] = { 0x00,  0x01,  0x02, 0x03 };
static const char* TIMER_MODE_MAP[4] = { "NONE", "OFF", "ON", "BOTH" };

static const int TIMER_INCREMENT_MINUTES = 10;

static const byte FUNCTIONS_SET_PART1 = 0x1F;
static const byte FUNCTIONS_GET_PART1 = 0x20;
static const byte FUNCTIONS_SET_PART2 = 0x21;
static const byte FUNCTIONS_GET_PART2 = 0x22;


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


const uint8_t ESPMHP_MIN_TEMPERATURE = 16;
const uint8_t ESPMHP_MAX_TEMPERATURE = 31;
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
};

struct heatpumpTimers {
    const char* mode;
    int onMinutesSet;
    int onMinutesRemaining;
    int offMinutesSet;
    int offMinutesRemaining;
};

struct heatpumpStatus {
    float roomTemperature;
    bool operating; // if true, the heatpump is operating to reach the desired temperature
    heatpumpTimers timers;
    int compressorFrequency;
};

#define MAX_FUNCTION_CODE_COUNT 30

struct heatpumpFunctionCodes {
    bool valid[MAX_FUNCTION_CODE_COUNT];
    int code[MAX_FUNCTION_CODE_COUNT];
};

