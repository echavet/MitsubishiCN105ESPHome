// #pragma once
// #include "Globals.h"

// using namespace esphome;

// class ControlManager {
// private:
//     wantedHeatpumpSettings wantedSettings{};
//     std::queue<wantedHeatpumpSettings> wantedSettingsQueue;
//     std::mutex queueMutex;
//     bool sendWantedSettings();
//     void checkPendingWantedSettings();
//     /**
//      * Puts the fresh wantedSettings in the queue
//     */
//     void pushNewWantedSettings(wantedHeatpumpSettings& wantedSettings);

//     wantedHeatpumpSettings popWantedSettings();

// public:
//     ControlManager(/* args */);
//     ~ControlManager();

//     /**
//      * Builds a new wantedSettings layer over currentSettings
//     */
//     void buildNewWantedSettings(heatpumpSettings& currentSettings);


//     wantedHeatpumpSettings& currentWantedSettings() {
//         std::lock_guard<std::mutex> lock(queueMutex);
//         if (settingsQueue.empty()) {
//             return false;
//         }
//         settings = settingsQueue.front();
//         return settings;
//     }


// };






