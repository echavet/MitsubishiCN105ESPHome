#pragma once
#include "Globals.h"
#include "heatpumpFunctions.h"
#include "van_orientation_select.h"
#include "compressor_frequency_sensor.h"
#include "isee_sensor.h"
#include <esphome/components/sensor/sensor.h>
#include <esphome/components/binary_sensor/binary_sensor.h>
#ifdef USE_ESP32
#include <mutex>
#endif

using namespace esphome;


//class VaneOrientationSelect;  // Déclaration anticipée, définie dans extraComponents



class CN105Climate : public climate::Climate, public Component, public uart::UARTDevice {

    //friend class VaneOrientationSelect;

public:

    CN105Climate(uart::UARTComponent* hw_serial);


    void set_vertical_vane_select(VaneOrientationSelect* vertical_vane_select);
    void set_horizontal_vane_select(VaneOrientationSelect* horizontal_vane_select);
    void set_compressor_frequency_sensor(esphome::sensor::Sensor* compressor_frequency_sensor);
    void set_isee_sensor(esphome::binary_sensor::BinarySensor* iSee_sensor);

    //sensor::Sensor* compressor_frequency_sensor;
    binary_sensor::BinarySensor* iSee_sensor_ = nullptr;
    //select::Select* van_orientation;


    VaneOrientationSelect* vertical_vane_select_ =
        nullptr;  // Select to store manual position of vertical swing
    VaneOrientationSelect* horizontal_vane_select_ =
        nullptr;  // Select to store manual position of horizontal swing
    sensor::Sensor* compressor_frequency_sensor_ =
        nullptr;  // Sensor to store compressor frequency

    // When received command to change the vane positions
    void on_horizontal_swing_change(const std::string& swing);
    void on_vertical_swing_change(const std::string& swing);

    //text_sensor::TextSensor* last_sent_packet_sensor;
    //text_sensor::TextSensor* last_received_packet_sensor;

    int get_compressor_frequency();
    bool is_operating();

    // checks if the field has changed
    bool hasChanged(const char* before, const char* now, const char* field, bool checkNotNull = false);
    bool isWantedSettingApplied(const char* wantedSettingProp, const char* currentSettingProp, const char* field);

    float get_setup_priority() const override {
        return setup_priority::AFTER_WIFI;  // Configurez ce composant après le WiFi
    }




    void generateExtraComponents();

    void setup() override;
    void loop() override;
    void set_baud_rate(int baud_rate);
    void set_tx_rx_pins(uint8_t tx_pin, uint8_t rx_pin);
    //void set_wifi_connected_state(bool state);
    void setupUART();
    void disconnectUART();
    void reconnectUART();
    void buildAndSendRequestsInfoPackets();
    void buildAndSendRequestPacket(int packetType);
    bool isHeatpumpConnectionActive();
    // will check if hp did respond
    void programResponseCheck(int packetType);

    bool sendWantedSettings();
    // Use the temperature from an external sensor. Use
    // set_remote_temp(0) to switch back to the internal sensor.
    void set_remote_temperature(float);

    void set_remote_temp_timeout(uint32_t timeout);

    void set_debounce_delay(uint32_t delay);

    // this is the ping or heartbeat of the setRemotetemperature for timeout management
    void setExternalTemperatureCheckout();

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

    //bool can_proceed() override;


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

    const char* getIfNotNull(const char* what, const char* defaultValue);

protected:
    // HeatPump object using the underlying Arduino library.
    // same as PolingComponent
    uint32_t update_interval_;

    climate::ClimateTraits traits_;
    //Accessor method for the HardwareSerial pointer
    uart::UARTComponent* get_hw_serial_() {
        return this->parent_;
    }

    bool processInput(void);
    void parse(uint8_t inputData);
    void checkHeader(uint8_t inputData);
    void initBytePointer();
    void processDataPacket();
    void getDataFromResponsePacket();
    void getSettingsFromResponsePacket();
    void getRoomTemperatureFromResponsePacket();
    void getOperatingAndCompressorFreqFromResponsePacket();

    void programUpdateInterval();
    void updateSuccess();
    void processCommand();
    bool checkSum();
    uint8_t checkSum(uint8_t bytes[], int len);

    const char* getModeSetting();
    const char* getPowerSetting();
    const char* getVaneSetting();
    const char* getWideVaneSetting();
    const char* getFanSpeedSetting();
    float getTemperatureSetting();

    void setModeSetting(const char* setting);
    void setPowerSetting(const char* setting);
    void setVaneSetting(const char* setting);
    void setWideVaneSetting(const char* setting);
    void setFanSpeed(const char* setting);


    bool isCycleRunning() {
        return cycleRunning;
    }


    void cycleStarted() {
        this->lastRequestInfo = CUSTOM_MILLIS;
        cycleRunning = true;
    }
    void cycleEnded() {
        cycleRunning = false;
        // a complete cycle is done
        this->lastCompleteCycle = CUSTOM_MILLIS;
    }

    bool hasUpdateIntervalPassed() {
        return (CUSTOM_MILLIS - this->lastCompleteCycle) > update_interval_;
    }

private:

    const char* lookupByteMapValue(const char* valuesMap[], const uint8_t byteMap[], int len, uint8_t byteValue, const char* debugInfo = "", const char* defaultValue = nullptr);
    int lookupByteMapValue(const int valuesMap[], const uint8_t byteMap[], int len, uint8_t byteValue, const char* debugInfo = "");
    int lookupByteMapIndex(const char* valuesMap[], int len, const char* lookupValue, const char* debugInfo = "");
    int lookupByteMapIndex(const int valuesMap[], int len, int lookupValue, const char* debugInfo = "");

    void writePacket(uint8_t* packet, int length, bool checkIsActive = true);
    void prepareInfoPacket(uint8_t* packet, int length);
    void prepareSetPacket(uint8_t* packet, int length);

    void publishStateToHA(heatpumpSettings settings);

    void heatpumpUpdate(heatpumpSettings settings);

    void statusChanged(heatpumpStatus status);

    void checkPendingWantedSettings();
    void checkPowerAndModeSettings(heatpumpSettings& settings);
    void checkFanSettings(heatpumpSettings& settings);
    void checkVaneSettings(heatpumpSettings& settings);
    void updateExtraSelectComponents(heatpumpSettings& settings);

    void statusChanged();
    void updateAction();
    void setActionIfOperatingTo(climate::ClimateAction action);
    void setActionIfOperatingAndCompressorIsActiveTo(climate::ClimateAction action);
    void hpPacketDebug(uint8_t* packet, unsigned int length, const char* packetDirection);

    void debugSettings(const char* settingName, heatpumpSettings settings);
    void debugSettings(const char* settingName, wantedHeatpumpSettings settings);
    void debugStatus(const char* statusName, heatpumpStatus status);
    void debugSettingsAndStatus(const char* settingName, heatpumpSettings settings, heatpumpStatus status);
    void debugClimate(const char* settingName);

    void createPacket(uint8_t* packet);
    void createInfoPacket(uint8_t* packet, uint8_t packetType);
    heatpumpSettings currentSettings{};
    wantedHeatpumpSettings wantedSettings{};

#ifdef USE_ESP32
    std::mutex wantedSettingsMutex;
#endif

    unsigned long lastResponseMs;

    uint32_t remote_temp_timeout_;
    uint32_t debounce_delay_;

    int baud_ = 0;
    int tx_pin_ = -1;
    int rx_pin_ = -1;

    bool isConnected_ = false;
    bool isHeatpumpConnected_ = false;

    //HardwareSerial* _HardSerial{ nullptr };
    unsigned long lastSend;

    unsigned long lastRequestInfo;

    unsigned long lastCompleteCycle;

    uint8_t storedInputData[MAX_DATA_BYTES]; // multi-byte data
    uint8_t* data;

    // initialise to all off, then it will update shortly after connect;
    heatpumpStatus currentStatus{ 0, false, {TIMER_MODE_MAP[0], 0, 0, 0, 0}, 0 };
    heatpumpFunctions functions;

    bool cycleRunning = false;
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
    uint8_t command = 0;
};
