#include "cn105.h"
#include "Globals.h"

void log_info_uint32(const char* tag, const char* msg, uint32_t value, const char* suffix) {
#if __GNUC__ >= 11
    ESP_LOGI(tag, "%s %lu %s", msg, (unsigned long)value, suffix);
#else
    ESP_LOGI(tag, "%s %u %s", msg, (unsigned int)value, suffix);
#endif
}
void log_debug_uint32(const char* tag, const char* msg, uint32_t value, const char* suffix) {
#if __GNUC__ >= 11
    ESP_LOGD(tag, "%s %lu %s", msg, (unsigned long)value, suffix);
#else
    ESP_LOGD(tag, "%s %u %s", msg, (unsigned int)value, suffix);
#endif
}

bool CN105Climate::hasChanged(const char* before, const char* now, const char* field, bool checkNotNull) {
    if (now == NULL) {
        if (checkNotNull) {
            ESP_LOGE(TAG, "CAUTION: expected value in hasChanged() function for %s, got NULL", field);
        } else {
            ESP_LOGD(TAG, "No value in hasChanged() function for %s", field);
        }
        return false;
    }
    return ((before == NULL) || (strcmp(before, now) != 0));
}



bool CN105Climate::isWantedSettingApplied(const char* wantedSettingProp, const char* currentSettingProp, const char* field) {

    bool isEqual = ((wantedSettingProp == NULL) || (strcmp(wantedSettingProp, currentSettingProp) == 0));

    if (!isEqual) {
        ESP_LOGD(TAG, "wanted %s is not set yet", field);
        ESP_LOGD(TAG, "Wanted %s is not set yet, want:%s, got: %s", field, wantedSettingProp, currentSettingProp);
    }

    if (wantedSettingProp != NULL) {
        ESP_LOGE(TAG, "CAUTION: expected value in hasChanged() function for %s, got NULL", field);
        ESP_LOGD(TAG, "No value in hasChanged() function for %s", field);
    }

    return isEqual;
}


const char* CN105Climate::getIfNotNull(const char* what, const char* defaultValue) {
    if (what == NULL) {
        return defaultValue;
    }
    return what;
}

void CN105Climate::debugSettings(const char* settingName, wantedHeatpumpSettings& settings) {
#ifdef USE_ESP32
    ESP_LOGD(LOG_ACTION_EVT_TAG, "[%s]-> [power: %s, target °C: %.1f, mode: %s, fan: %s, vane: %s, wvane: %s, hasChanged ? -> %s, hasBeenSent ? -> %s]",
        getIfNotNull(settingName, "unnamed"),
        getIfNotNull(settings.power, "-"),
        settings.temperature,
        getIfNotNull(settings.mode, "-"),
        getIfNotNull(settings.fan, "-"),
        getIfNotNull(settings.vane, "-"),
        getIfNotNull(settings.wideVane, "-"),
        settings.hasChanged ? "YES" : " NO",
        settings.hasBeenSent ? "YES" : " NO"
    );
#else
    ESP_LOGD(LOG_ACTION_EVT_TAG, "[%-*s]-> [power: %-*s, target °C: %.1f, mode: %-*s, fan: %-*s, vane: %-*s, wvane: %-*s, hasChanged ? -> %s, hasBeenSent ? -> %s]",
        15, getIfNotNull(settingName, "unnamed"),
        3, getIfNotNull(settings.power, "-"),
        settings.temperature,
        6, getIfNotNull(settings.mode, "-"),
        6, getIfNotNull(settings.fan, "-"),
        6, getIfNotNull(settings.vane, "-"),
        6, getIfNotNull(settings.wideVane, "-"),
        settings.hasChanged ? "YES" : " NO",
        settings.hasBeenSent ? "YES" : " NO"
    );
#endif
}

void CN105Climate::debugClimate(const char* settingName) {
    ESP_LOGD(LOG_SETTINGS_TAG, "[%s]-> [mode: %s, target °C: %.1f, fan: %s, swing: %s]",
        settingName,
        LOG_STR_ARG(climate_mode_to_string(this->mode)), // Utilisation de LOG_STR_ARG
        this->target_temperature,
        this->fan_mode.has_value() ? LOG_STR_ARG(climate_fan_mode_to_string(this->fan_mode.value())) : "-",
        LOG_STR_ARG(climate_swing_mode_to_string(this->swing_mode)));
}







void CN105Climate::debugSettings(const char* settingName, heatpumpSettings& settings) {
#ifdef USE_ESP32
    ESP_LOGD(LOG_SETTINGS_TAG, "[%s]-> [power: %s, target °C: %.1f, mode: %s, fan: %s, vane: %s, wvane: %s]",
        getIfNotNull(settingName, "unnamed"),
        getIfNotNull(settings.power, "-"),
        settings.temperature,
        getIfNotNull(settings.mode, "-"),
        getIfNotNull(settings.fan, "-"),
        getIfNotNull(settings.vane, "-"),
        getIfNotNull(settings.wideVane, "-")
    );
#else
    ESP_LOGD(LOG_SETTINGS_TAG, "[%-*s]-> [power: %-*s, target °C: %.1f, mode: %-*s, fan: %-*s, vane: %-*s, wvane: %-*s]",
        15, getIfNotNull(settingName, "unnamed"),
        3, getIfNotNull(settings.power, "-"),
        settings.temperature,
        6, getIfNotNull(settings.mode, "-"),
        6, getIfNotNull(settings.fan, "-"),
        6, getIfNotNull(settings.vane, "-"),
        6, getIfNotNull(settings.wideVane, "-")
    );
#endif
}


void CN105Climate::debugStatus(const char* statusName, heatpumpStatus status) {
#ifdef USE_ESP32
    ESP_LOGI(LOG_STATUS_TAG, "[%s]-> [room C°: %.1f, operating: %s, compressor freq: %.1f Hz]",
        statusName,
        status.roomTemperature,
        status.operating ? "YES" : "NO ",
        status.compressorFrequency);
#else
    ESP_LOGI(LOG_STATUS_TAG, "[%-*s]-> [room C°: %.1f, operating: %-*s, compressor freq: %.1f Hz]",
        15, statusName,
        status.roomTemperature,
        3, status.operating ? "YES" : "NO ",
        status.compressorFrequency);
#endif
}


void CN105Climate::debugSettingsAndStatus(const char* settingName, heatpumpSettings settings, heatpumpStatus status) {
    this->debugSettings(settingName, settings);
    this->debugStatus(settingName, status);
}



void CN105Climate::hpPacketDebug(uint8_t* packet, unsigned int length, const char* packetDirection) {
    char buffer[4]; // Small buffer to store each byte as text
    char outputBuffer[length * 4 + 1]; // Buffer to store all bytes as text

    // Initialisation du tampon de sortie avec une chaîne vide
    outputBuffer[0] = '\0';

    for (unsigned int i = 0; i < length; i++) {
        snprintf(buffer, sizeof(buffer), "%02X ", packet[i]); // Using snprintf to avoid buffer overflows
        strcat(outputBuffer, buffer);
    }

    char outputForSensor[15];
    strncpy(outputForSensor, outputBuffer, 14);
    outputForSensor[14] = '\0'; // Ajouter un caract

    /*if (strcasecmp(packetDirection, "WRITE") == 0) {
        this->last_sent_packet_sensor->publish_state(outputForSensor);
    }*/

    ESP_LOGD(packetDirection, "%s", outputBuffer);
}


int CN105Climate::lookupByteMapIndex(const int valuesMap[], int len, int lookupValue, const char* debugInfo) {
    for (int i = 0; i < len; i++) {
        if (valuesMap[i] == lookupValue) {
            return i;
        }
    }
    ESP_LOGW("lookup", "%s caution value %d not found, returning -1", debugInfo, lookupValue);
    //esphome::delay(200);
    return -1;
}
int CN105Climate::lookupByteMapIndex(const char* valuesMap[], int len, const char* lookupValue, const char* debugInfo) {
    for (int i = 0; i < len; i++) {
        if (strcasecmp(valuesMap[i], lookupValue) == 0) {
            return i;
        }
    }
    ESP_LOGW("lookup", "%s caution value %s not found, returning -1", debugInfo, lookupValue);
    //esphome::delay(200);
    return -1;
}
const char* CN105Climate::lookupByteMapValue(const char* valuesMap[], const uint8_t byteMap[], int len, uint8_t byteValue, const char* debugInfo, const char* defaultValue) {
    for (int i = 0; i < len; i++) {
        if (byteMap[i] == byteValue) {
            return valuesMap[i];
        }
    }

    if (defaultValue != nullptr) {
        return defaultValue;
    } else {
        ESP_LOGW("lookup", "%s caution: value %d not found, returning value at index 0", debugInfo, byteValue);
        return valuesMap[0];
    }

}
int CN105Climate::lookupByteMapValue(const int valuesMap[], const uint8_t byteMap[], int len, uint8_t byteValue, const char* debugInfo) {
    for (int i = 0; i < len; i++) {
        if (byteMap[i] == byteValue) {
            return valuesMap[i];
        }
    }
    ESP_LOGW("lookup", "%s caution: value %d not found, returning value at index 0", debugInfo, byteValue);
    return valuesMap[0];
}

#ifndef USE_ESP32
/**
 * This methode emulates the esp32 lock_guard feature with a boolean variable
 *
*/
void CN105Climate::emulateMutex(const char* retryName, std::function<void()>&& f) {
    this->set_retry(retryName, 100, 10, [this, f, retryName](uint8_t retry_count) {
        if (this->wantedSettingsMutex) {
            if (retry_count < 1) {
                ESP_LOGW(retryName, "10 retry calls failed because mutex was locked, forcing unlock...");
                this->wantedSettingsMutex = true;
                f();
                this->wantedSettingsMutex = false;
                return RetryResult::DONE;
            }
            ESP_LOGI(retryName, "wantedSettingsMutex is already locked, defferring...");
            return RetryResult::RETRY;
        } else {
            this->wantedSettingsMutex = true;
            ESP_LOGD(retryName, "emulateMutex normal behaviour, locking...");
            f();
            ESP_LOGD(retryName, "emulateMutex unlocking...");
            this->wantedSettingsMutex = false;
            return RetryResult::DONE;
        }
        }, 1.2f);
}
#ifdef TEST_MODE

void CN105Climate::testEmulateMutex(const char* retryName, std::function<void()>&& f) {
    this->set_retry(retryName, 100, 10, [this, f, retryName](uint8_t retry_count) {
        if (this->esp8266Mutex) {
            if (retry_count < 1) {
                ESP_LOGW(retryName, "10 retry calls failed because mutex was locked, forcing unlock...");
                this->esp8266Mutex = true;
                f();
                this->esp8266Mutex = false;
                return RetryResult::DONE;
            }
            ESP_LOGI(retryName, "testMutex is already locked, defferring...");
            return RetryResult::RETRY;
        } else {
            this->esp8266Mutex = true;
            ESP_LOGD(retryName, "emulateMutex normal behaviour, locking...");
            f();
            ESP_LOGD(retryName, "emulateMutex unlocking...");
            this->esp8266Mutex = false;
            return RetryResult::DONE;
        }
        }, 1.2f);
}
#endif
#endif

#ifdef TEST_MODE
void CN105Climate::logDelegate() {
#ifndef USE_ESP32
    ESP_LOGI("testMutex", "Delegate exécuté. Mutex est %s", this->esp8266Mutex ? "verrouillé" : "déverrouillé");
#else
    if (this->esp32Mutex.try_lock()) {
        ESP_LOGI("testMutex", "Mutex n'est pas verrouillé");
    } else {
        ESP_LOGI("testMutex", "Mutex est déjà verrouillé");
    }
#endif
}

void CN105Climate::testCase1() {
    int testDelay = 600;

#ifdef USE_ESP32

    ESP_LOGI("testMutex", "Test 1: VERROUILLAGE ET APPEL DE logDelegate...");
    ESP_LOGI("testMutex", "verrouillage du mutex...");
    if (true) {
        std::lock_guard<std::mutex> guard(this->esp32Mutex);
        ESP_LOGI("testMutex", "verification...");
        this->logDelegate();
    }
    ESP_LOGI("testMutex", "déverrouillage du mutex...");
    this->logDelegate();

    ESP_LOGI("testMutex", "Test 2: PAS DE VERROU ET APPEL DE logDelegate...");
    this->logDelegate();
#else
    ESP_LOGI("testMutex", "Test 1: VERROUILLAGE ET APPEL DE logDelegate...");
    ESP_LOGI("testMutex", "verrouillage du mutex...");
    this->esp8266Mutex = true;
    this->testEmulateMutex("testMutex", std::bind(&CN105Climate::logDelegate, this));
    CUSTOM_DELAY(testDelay);
    ESP_LOGI("testMutex", "Déverrouillage du mutex...");
    this->esp8266Mutex = false;
    CUSTOM_DELAY(200);
    ESP_LOGI("testMutex", "verrouillage du mutex...");
    this->esp8266Mutex = true;
    this->testEmulateMutex("testMutex", std::bind(&CN105Climate::logDelegate, this));
    ESP_LOGI("testMutex", "blocage de 2,5s...");
    CUSTOM_DELAY(2500);
    ESP_LOGI("testMutex", "fin du test");

#endif
}

void CN105Climate::testMutex() {

    ESP_LOGI("testMutex", "Test de gestion des mutex...");
    this->testCase1();

}

#endif
