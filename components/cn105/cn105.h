#pragma once
#include "Globals.h"
#include "heatpumpFunctions.h"
#include "van_orientation_select.h"
#include "uptime_connection_sensor.h"
#include "compressor_frequency_sensor.h"
#include "input_power_sensor.h"
#include "kwh_sensor.h"
#include "runtime_hours_sensor.h"
#include "outside_air_temperature_sensor.h"
#include "auto_sub_mode_sensor.h"
#include "isee_sensor.h"
#include "stage_sensor.h"
#include "functions_sensor.h"
#include "functions_number.h"
#include "functions_button.h"
#include "sub_mode_sensor.h"
#include "hvac_option_switch.h"
#include <esphome/components/sensor/sensor.h>
#include <esphome/components/button/button.h>
#include <esphome/components/binary_sensor/binary_sensor.h>
#include "cycle_management.h"

#ifdef USE_ESP32
#include <mutex>
#endif

namespace esphome {

    void log_info_uint32(const char* tag, const char* msg, uint32_t value, const char* suffix = "");
    void log_debug_uint32(const char* tag, const char* msg, uint32_t value, const char* suffix = "");

    class CN105Climate : public climate::Climate, public Component, public uart::UARTDevice {

        //friend class VaneOrientationSelect;

    public:

        CN105Climate(uart::UARTComponent* hw_serial);

        void set_vertical_vane_select(VaneOrientationSelect* vertical_vane_select);
        void set_horizontal_vane_select(VaneOrientationSelect* horizontal_vane_select);
        void set_airflow_control_select(VaneOrientationSelect* airflow_control_select);
        void set_compressor_frequency_sensor(esphome::sensor::Sensor* compressor_frequency_sensor);
        void set_input_power_sensor(esphome::sensor::Sensor* input_power_sensor);
        void set_kwh_sensor(esphome::sensor::Sensor* kwh_sensor);
        void set_runtime_hours_sensor(esphome::sensor::Sensor* runtime_hours_sensor);
        void set_outside_air_temperature_sensor(esphome::sensor::Sensor* outside_air_temperature_sensor);
        void set_isee_sensor(esphome::binary_sensor::BinarySensor* iSee_sensor);
        void set_stage_sensor(esphome::text_sensor::TextSensor* Stage_sensor);
        void set_use_stage_for_operating_status(bool value);
        void set_use_fahrenheit_support_mode(bool value);
        void set_air_purifier_switch(HVACOptionSwitch* air_purifier_switch);
        void set_night_mode_switch(HVACOptionSwitch* night_mode_switch);
        void set_circulator_switch(HVACOptionSwitch* circulator_switch);

        void set_functions_sensor(esphome::text_sensor::TextSensor* Functions_sensor);
        void set_functions_get_button(FunctionsButton* Button);
        void set_functions_set_button(FunctionsButton* Button);
        void set_functions_set_code(FunctionsNumber* Number);
        void set_functions_set_value(FunctionsNumber* Number);

        void set_sub_mode_sensor(esphome::text_sensor::TextSensor* Sub_mode_sensor);
        void set_auto_sub_mode_sensor(esphome::text_sensor::TextSensor* Auto_sub_mode_sensor);
        void set_hp_uptime_connection_sensor(uptime::HpUpTimeConnectionSensor* hp_up_connection_sensor);

        //sensor::Sensor* compressor_frequency_sensor;
        binary_sensor::BinarySensor* iSee_sensor_ = nullptr;
        text_sensor::TextSensor* stage_sensor_{ nullptr }; // to save ref if needed
        bool use_stage_for_operating_status_{ false };
        bool use_fahrenheit_support_mode_ = false;
        text_sensor::TextSensor* Functions_sensor_ = nullptr;
        FunctionsButton* Functions_get_button_ = nullptr;
        FunctionsButton* Functions_set_button_ = nullptr;
        FunctionsNumber* Functions_set_code_ = nullptr;
        FunctionsNumber* Functions_set_value_ = nullptr;
        text_sensor::TextSensor* Sub_mode_sensor_ = nullptr;
        text_sensor::TextSensor* Auto_sub_mode_sensor_ = nullptr;
        HVACOptionSwitch* air_purifier_switch_ = nullptr;
        HVACOptionSwitch* night_mode_switch_ = nullptr;
        HVACOptionSwitch* circulator_switch_ = nullptr;

        // The value of the code and value for the functions set.
        int functions_code_;
        int functions_value_;

        //select::Select* van_orientation;


        VaneOrientationSelect* vertical_vane_select_ =
            nullptr;  // Select to store manual position of vertical swing
        VaneOrientationSelect* horizontal_vane_select_ =
            nullptr;  // Select to store manual position of horizontal swing
        VaneOrientationSelect* airflow_control_select_ =
            nullptr;
        sensor::Sensor* compressor_frequency_sensor_ =
            nullptr;  // Sensor to store compressor frequency
        sensor::Sensor* input_power_sensor_ =
            nullptr;  // Sensor to store compressor frequency
        sensor::Sensor* kwh_sensor_ =
            nullptr;  // Sensor to store compressor frequency
        sensor::Sensor* runtime_hours_sensor_ =
            nullptr;  // Sensor to store compressor frequency
        sensor::Sensor* outside_air_temperature_sensor_ =
            nullptr;  // Outside air temperature

        // sensor to monitor heatpump connection time
        uptime::HpUpTimeConnectionSensor* hp_uptime_connection_sensor_ = nullptr;

        float get_compressor_frequency();
        float get_input_power();
        float get_kwh();
        float get_runtime_hours();
        bool is_operating();
        bool is_air_purifier();
        bool is_night_mode();
        bool is_circulator();

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
        void set_tx_rx_pins(int tx_pin, int rx_pin);
        //void set_wifi_connected_state(bool state);
        void setupUART();
        void disconnectUART();
        void reconnectUART();
        void buildAndSendRequestsInfoPackets();
        void buildAndSendRequestPacket(int packetType);
        bool isHeatpumpConnectionActive();
        void reconnectIfConnectionLost();

        void sendWantedSettings();
        void sendWantedSettingsDelegate();
        // Use the temperature from an external sensor. Use
        // set_remote_temp(0) to switch back to the internal sensor.
        void set_remote_temperature(float);
        void sendRemoteTemperature();
        void sendWantedRunStates();

        void set_remote_temp_timeout(uint32_t timeout);

        void set_debounce_delay(uint32_t delay);

        // this is the ping or heartbeat of the setRemotetemperature for timeout management
        void pingExternalTemperature();

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

        bool isUARTConnected_ = false;
        bool isHeatpumpConnected_ = false;
        bool shouldSendExternalTemperature_ = false;
        float remoteTemperature_ = 0;

        unsigned long nbCompleteCycles_ = 0;
        unsigned long nbCycles_ = 0;
        unsigned int nbHeatpumpConnections_ = 0;


        void sendFirstConnectionPacket();
        void terminateCycle();
        //bool can_proceed() override;


        void getFunctions();
        void getFunctionsPart2();
        void functionsArrived();
        bool setFunctions(heatpumpFunctions const& functions);

        // helpers
        const char* getIfNotNull(const char* what, const char* defaultValue);

#ifdef TEST_MODE
        void testMutex();
        void testCase1();
        void logDelegate();

#ifdef USE_ESP32
        std::mutex esp32Mutex;
#else
        void testEmulateMutex(const char* retryName, std::function<void()>&& f);
        bool esp8266Mutex = false;
#endif
#endif


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
        void getAutoModeStateFromResponsePacket(); //NET added
        void getPowerFromResponsePacket(); //NET added
        void getSettingsFromResponsePacket();
        void getRoomTemperatureFromResponsePacket();
        void getOperatingAndCompressorFreqFromResponsePacket();
        void getHVACOptionsFromResponsePacket();

        void updateSuccess();
        void processCommand();
        bool checkSum();
        uint8_t checkSum(uint8_t bytes[], int len);

        const char* getModeSetting();
        const char* getPowerSetting();
        const char* getVaneSetting();
        const char* getWideVaneSetting();
        const char* getAirflowControlSetting();
        const char* getFanSpeedSetting();
        float getTemperatureSetting();
        bool getAirPurifierRunState();
        bool getNightModeRunState();
        bool getCirculatorRunState();

        void setModeSetting(const char* setting);
        void setPowerSetting(const char* setting);
        void setVaneSetting(const char* setting);
        void setWideVaneSetting(const char* setting);
        void setAirflowControlSetting(const char* setting);
        void setFanSpeed(const char* setting);

        void setHeatpumpConnected(bool state);

    private:
        const char* lookupByteMapValue(const char* valuesMap[], const uint8_t byteMap[], int len, uint8_t byteValue, const char* debugInfo = "", const char* defaultValue = nullptr);
        int lookupByteMapValue(const int valuesMap[], const uint8_t byteMap[], int len, uint8_t byteValue, const char* debugInfo = "");
        int lookupByteMapIndex(const char* valuesMap[], int len, const char* lookupValue, const char* debugInfo = "");
        int lookupByteMapIndex(const int valuesMap[], int len, int lookupValue, const char* debugInfo = "");

        void writePacket(uint8_t* packet, int length, bool checkIsActive = true);
        void prepareInfoPacket(uint8_t* packet, int length);
        void prepareSetPacket(uint8_t* packet, int length);

        void publishStateToHA(heatpumpSettings& settings);
        void publishWantedSettingsStateToHA();
        void publishWantedRunStatesStateToHA();

        void heatpumpUpdate(heatpumpSettings& settings);

        void statusChanged(heatpumpStatus status);

        void checkPendingWantedSettings();
        void checkPendingWantedRunStates();
        void checkPowerAndModeSettings(heatpumpSettings& settings, bool updateCurrentSettings = true);
        void checkFanSettings(heatpumpSettings& settings, bool updateCurrentSettings = true);
        void checkVaneSettings(heatpumpSettings& settings, bool updateCurrentSettings = true);
        void checkWideVaneSettings(heatpumpSettings& settings, bool updateCurrentSettings = true);
//        void checkAirflowControlSettings(heatpumpRunStates& settings, bool updateCurrentSettings = true);
        void updateExtraSelectComponents(heatpumpSettings& settings);

        //void statusChanged();
        void updateAction();
        void setActionIfOperatingTo(climate::ClimateAction action);
        void setActionIfOperatingAndCompressorIsActiveTo(climate::ClimateAction action);
        void hpPacketDebug(uint8_t* packet, unsigned int length, const char* packetDirection);

        void debugSettings(const char* settingName, heatpumpSettings& settings);
        void debugSettings(const char* settingName, wantedHeatpumpSettings& settings);
        void debugStatus(const char* statusName, heatpumpStatus status);
        void debugSettingsAndStatus(const char* settingName, heatpumpSettings settings, heatpumpStatus status);
        void debugClimate(const char* settingName);

#ifndef USE_ESP32
        void emulateMutex(const char* retryName, std::function<void()>&& f);
#endif



        void controlDelegate(const esphome::climate::ClimateCall& call);

        void createPacket(uint8_t* packet);
        void createInfoPacket(uint8_t* packet, uint8_t packetType);
        heatpumpSettings currentSettings{};
        wantedHeatpumpSettings wantedSettings{};
        heatpumpRunStates currentRunStates{};
        wantedHeatpumpRunStates wantedRunStates{};
        cycleManagement loopCycle{};

#ifdef USE_ESP32
        std::mutex wantedSettingsMutex;
#else
        volatile bool wantedSettingsMutex = false;
#endif

        unsigned long lastResponseMs;


        uint32_t remote_temp_timeout_;
        uint32_t debounce_delay_;

        int baud_ = 0;
        int tx_pin_ = -1;
        int rx_pin_ = -1;



        //HardwareSerial* _HardSerial{ nullptr };
        unsigned long lastSend;
        unsigned long lastConnectRqTimeMs;
        unsigned long lastReconnectTimeMs;

        uint8_t storedInputData[MAX_DATA_BYTES]; // multi-byte data
        uint8_t* data;

        // initialise to all off, then it will update shortly after connect;
        heatpumpStatus currentStatus{ 0, 0, false, {TIMER_MODE_MAP[0], 0, 0, 0, 0}, 0, 0, 0, 0 };
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

        int powerRequestWithoutResponses = 0;

        bool isReading = false;
        bool isWriting = false;

        bool foundStart = false;
        int bytesRead = 0;
        int dataLength = 0;
        uint8_t command = 0;
    };
}
