#pragma once
#include "Globals.h"

using namespace esphome;



class heatpumpFunctions {
private:
    byte raw[MAX_FUNCTION_CODE_COUNT];
    bool _isValid1;
    bool _isValid2;

    int getCode(byte b);
    int getValue(byte b);

public:
    heatpumpFunctions();

    bool isValid() const;

    // data must be 15 bytes
    void setData1(byte* data);
    void setData2(byte* data);
    void getData1(byte* data) const;
    void getData2(byte* data) const;

    void clear();

    int getValue(int code);
    bool setValue(int code, int value);

    heatpumpFunctionCodes getAllCodes();

    bool operator==(const heatpumpFunctions& rhs);
    bool operator!=(const heatpumpFunctions& rhs);
};


bool operator==(const heatpumpTimers& lhs, const heatpumpTimers& rhs);
bool operator!=(const heatpumpTimers& lhs, const heatpumpTimers& rhs);


bool operator==(const heatpumpSettings& lhs, const heatpumpSettings& rhs);
bool operator!=(const heatpumpSettings& lhs, const heatpumpSettings& rhs);

class VaneOrientationSelect;  // Déclaration anticipée



class CN105Climate : public climate::Climate, public Component {

    friend class VaneOrientationSelect;

public:



    sensor::Sensor* compressor_frequency_sensor;
    binary_sensor::BinarySensor* iSee_sensor;
    select::Select* van_orientation;

    //text_sensor::TextSensor* last_sent_packet_sensor;
    //text_sensor::TextSensor* last_received_packet_sensor;

    int get_compressor_frequency();
    bool is_operating();

    // checks if the field has changed
    bool hasChanged(const char* before, const char* now, const char* field, bool checkNotNull = false);

    float get_setup_priority() const override {
        return setup_priority::AFTER_WIFI;  // Configurez ce composant après le WiFi
    }


    CN105Climate(HardwareSerial* hw_serial);

    void generateExtraComponents();

    void setup() override;
    void loop() override;
    void set_baud_rate(int baud_rate);
    //void set_wifi_connected_state(bool state);
    void setupUART();
    void disconnectUART();
    void buildAndSendRequestsInfoPackets();
    void buildAndSendRequestPacket(int packetType);
    bool isHeatpumpConnectionActive();
    // will check if hp did respond
    void programResponseCheck(int packetType);

    void sendWantedSettings();
    // Use the temperature from an external sensor. Use
    // set_remote_temp(0) to switch back to the internal sensor.
    void set_remote_temperature(float);

    uint32_t get_update_interval() const;
    void set_update_interval(uint32_t update_interval);

    climate::ClimateTraits traits() override;

    // Get a mutable reference to the traits that we support.
    climate::ClimateTraits& config_traits();

    void control(const esphome::climate::ClimateCall& call) override;
    void controlMode();
    void controlTemperature();
    void controlFan();
    void controlSwing();

    // Configure the climate object with traits that we support.


    /// le bouton de setup de l'UART
    bool uart_setup_switch;

    void sendFirstConnectionPacket();

    bool can_proceed() override;


    heatpumpFunctions getFunctions();
    bool setFunctions(heatpumpFunctions const& functions);

    // helpers

    float FahrenheitToCelsius(int tempF) {
        float temp = (tempF - 32) / 1.8;
        return ((float)round(temp * 2)) / 2;                 //Round to nearest 0.5C
    }

    int CelsiusToFahrenheit(float tempC) {
        float temp = (tempC * 1.8) + 32;                //round up if heat, down if cool or any other mode
        return (int)(temp + 0.5);
    }

protected:
    // HeatPump object using the underlying Arduino library.
    // same as PolingComponent
    uint32_t update_interval_;

    climate::ClimateTraits traits_;
    //Accessor method for the HardwareSerial pointer
    HardwareSerial* get_hw_serial_() {
        return this->hw_serial_;
    }

    //Print a warning message if we're using the sole hardware UART on an
        //ESP8266 or UART0 on ESP32
    void check_logger_conflict_();

    void processInput(void);
    void parse(byte inputData);
    void checkHeader(byte inputData);
    void initBytePointer();
    void processDataPacket();
    void getDataFromResponsePacket();
    void programUpdateInterval();

    void processCommand();
    bool checkSum();
    byte checkSum(byte bytes[], int len);

    void setModeSetting(const char* setting);
    void setPowerSetting(const char* setting);
    void setVaneSetting(const char* setting);
    void setWideVaneSetting(const char* setting);
    void setFanSpeed(const char* setting);
private:

    const char* lookupByteMapValue(const char* valuesMap[], const byte byteMap[], int len, byte byteValue);
    int lookupByteMapValue(const int valuesMap[], const byte byteMap[], int len, byte byteValue);
    int lookupByteMapIndex(const char* valuesMap[], int len, const char* lookupValue);
    int lookupByteMapIndex(const int valuesMap[], int len, int lookupValue);
    void writePacket(byte* packet, int length, bool checkIsActive = true);
    void prepareInfoPacket(byte* packet, int length);
    void prepareSetPacket(byte* packet, int length);


    void settingsChanged(heatpumpSettings settings);
    void checkPowerAndModeSettings(heatpumpSettings& settings);
    void checkFanSettings(heatpumpSettings& settings);
    void checkVaneSettings(heatpumpSettings& settings);

    void statusChanged();
    void updateAction();
    void setActionIfOperatingTo(climate::ClimateAction action);
    void setActionIfOperatingAndCompressorIsActiveTo(climate::ClimateAction action);
    void hpPacketDebug(byte* packet, unsigned int length, const char* packetDirection);
    void createPacket(byte* packet, heatpumpSettings settings);
    void createInfoPacket(byte* packet, byte packetType);
    heatpumpSettings currentSettings{};
    heatpumpSettings wantedSettings{};


    unsigned long lastResponseMs;


    HardwareSerial* hw_serial_;
    int baud_ = 0;

    bool init_delay_completed_ = false;
    bool init_delay_initiated_ = false;

    bool isConnected_ = false;
    bool isHeatpumpConnected_ = false;

    //HardwareSerial* _HardSerial{ nullptr };
    unsigned long lastSend;
    byte storedInputData[MAX_DATA_BYTES]; // multi-byte data
    byte* data;

    // initialise to all off, then it will update shortly after connect;
    heatpumpStatus currentStatus{ 0, false, {TIMER_MODE_MAP[0], 0, 0, 0, 0}, 0 };
    heatpumpFunctions functions;

    bool tempMode = false;
    bool wideVaneAdj;
    bool autoUpdate;
    bool firstRun;
    int infoMode;
    bool externalUpdate;

    // counter for status request for checking heatpump is still connected
    // is the counter > MAX_NON_RESPONSE_REQ then we conclude uart is not connected anymore
    int nonResponseCounter = 0;

    bool isReading = false;
    bool isWriting = false;

    bool foundStart = false;
    int bytesRead = 0;
    int dataLength = 0;
    byte command = 0;
};
