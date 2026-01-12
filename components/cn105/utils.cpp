#include "cn105.h"
#include "Globals.h"
#include <math.h>

using namespace esphome;

void esphome::log_info_uint32(const char* tag, const char* msg, uint32_t value, const char* suffix) {
#if __GNUC__ >= 11
    ESP_LOGI(tag, "%s %lu %s", msg, (unsigned long)value, suffix);
#else
    ESP_LOGI(tag, "%s %u %s", msg, (unsigned int)value, suffix);
#endif
}
void esphome::log_debug_uint32(const char* tag, const char* msg, uint32_t value, const char* suffix) {
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


const char* CN105Climate::getIfNotNull(const char* what, const char* defaultValue) {
    if (what == NULL) {
        return defaultValue;
    }
    return what;
}

/**
 * This function calculates the temperature setting based on the mode and the temperature points.
 * It returns the temperature setting.
 */
float CN105Climate::calculateTemperatureSetting(float setting) {
    if (!this->tempMode) {
        return this->lookupByteMapIndex(TEMP_MAP, 16, (int)(setting + 0.5)) > -1 ? setting : TEMP_MAP[0];
    } else {
        setting = std::round(2.0f * setting) / 2.0f;  // Round to the nearest half-degree.
        return setting < 10 ? 10 : (setting > 31 ? 31 : setting);
    }
}

/**
 * This function updates the target temperatures from the settings.
 * It also calculates the temperature setting based on the mode and the temperature points.
 * It returns the temperature setting.
 */

void CN105Climate::updateTargetTemperaturesFromSettings(float temperature) {
    if (this->traits().has_feature_flags(climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {

        if (this->mode == climate::CLIMATE_MODE_HEAT) {
            this->setTargetTemperatureLow(temperature);
            if (std::isnan(this->getTargetTemperatureHigh())) {
                this->setTargetTemperatureHigh(temperature);
            }
        } else if (this->mode == climate::CLIMATE_MODE_COOL) {
            this->setTargetTemperatureHigh(temperature);
            if (std::isnan(this->getTargetTemperatureLow())) {
                this->setTargetTemperatureLow(temperature);
            }
        } else if (this->mode == climate::CLIMATE_MODE_DRY) {
            this->setTargetTemperatureHigh(temperature);
            if (std::isnan(this->getTargetTemperatureLow())) {
                this->setTargetTemperatureLow(temperature);
            }
        } else if (this->mode == climate::CLIMATE_MODE_HEAT_COOL) {
            // En HEAT_COOL (mapped from Mitsubishi AUTO): si les deux bornes existent déjà, ne pas recentrer
            bool lowDefined = !std::isnan(this->getTargetTemperatureLow());
            bool highDefined = !std::isnan(this->getTargetTemperatureHigh());

            if (lowDefined && highDefined) {
                ESP_LOGD(LOG_SETTINGS_TAG, "HEAT_COOL keep dual setpoints [%.1f - %.1f], median %.1f",
                    this->getTargetTemperatureLow(), this->getTargetTemperatureHigh(), temperature);
            } else if (lowDefined && !highDefined) {
                this->setTargetTemperatureHigh(this->getTargetTemperatureLow() + 2.0f);
                ESP_LOGD(LOG_SETTINGS_TAG, "HEAT_COOL fill missing high: [%.1f - %.1f]",
                    this->getTargetTemperatureLow(), this->getTargetTemperatureHigh());
            } else if (!lowDefined && highDefined) {
                this->setTargetTemperatureLow(this->getTargetTemperatureHigh() - 2.0f);
                ESP_LOGD(LOG_SETTINGS_TAG, "HEAT_COOL fill missing low: [%.1f - %.1f]",
                    this->getTargetTemperatureLow(), this->getTargetTemperatureHigh());
            } else {
                // aucune borne connue: initialiser autour de la médiane fournie
                this->setTargetTemperatureLow(temperature - 2.0f);
                this->setTargetTemperatureHigh(temperature + 2.0f);
                ESP_LOGD(LOG_SETTINGS_TAG, "HEAT_COOL init dual setpoints [%.1f - %.1f], median %.1f",
                    this->getTargetTemperatureLow(), this->getTargetTemperatureHigh(), temperature);
            }

            // Mémoriser dans currentSettings pour détection de glissement ultérieur
            this->currentSettings.dual_low_target = this->getTargetTemperatureLow();
            this->currentSettings.dual_high_target = this->getTargetTemperatureHigh();
        } else {

            if (std::isnan(this->getTargetTemperatureLow())) {
                this->setTargetTemperatureLow(temperature);
            }
            if (std::isnan(this->getTargetTemperatureHigh())) {
                this->setTargetTemperatureHigh(temperature);
            }

            float theoricalSetPoint = this->calculateTemperatureSetting((this->getTargetTemperatureLow() + this->getTargetTemperatureHigh()) / 2.0f);

            if (theoricalSetPoint != temperature) {
                float delta = (this->getTargetTemperatureHigh() - this->getTargetTemperatureLow()) / 2.0f;
                this->setTargetTemperatureLow(theoricalSetPoint - delta);
                this->setTargetTemperatureHigh(theoricalSetPoint + delta);
            }

        }
    } else {
        ESP_LOGD(LOG_SETTINGS_TAG, "SINGLE SETPOINT %.1f",
            temperature);
        this->setTargetTemperature(temperature);
    }
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

float CN105Climate::getTargetTemperatureInCurrentMode() {
    if (this->traits_.has_feature_flags(climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
        if (this->mode == climate::CLIMATE_MODE_HEAT) {
            return this->getTargetTemperatureLow();
        } else if (this->mode == climate::CLIMATE_MODE_COOL) {
            return this->getTargetTemperatureHigh();
        } else if (this->mode == climate::CLIMATE_MODE_DRY) {
            return this->getTargetTemperatureHigh();
        } else {
            return (this->getTargetTemperatureLow() + this->getTargetTemperatureHigh()) / 2.0f;
        }
    } else {
        return this->getTargetTemperature();
    }
}

float CN105Climate::getTargetTemperature() {
    return this->fahrenheitSupport_.normalizeUiTemperatureToHeatpumpTemperature(this->target_temperature);
}

float CN105Climate::getTargetTemperatureLow() {
    return this->fahrenheitSupport_.normalizeUiTemperatureToHeatpumpTemperature(this->target_temperature_low);
}

float CN105Climate::getTargetTemperatureHigh() {
    return this->fahrenheitSupport_.normalizeUiTemperatureToHeatpumpTemperature(this->target_temperature_high);
}

float CN105Climate::getCurrentTemperature() {
    return this->fahrenheitSupport_.normalizeUiTemperatureToHeatpumpTemperature(this->current_temperature);
}

void CN105Climate::setTargetTemperature(float temperature) {
    this->target_temperature = this->fahrenheitSupport_.normalizeHeatpumpTemperatureToUiTemperature(temperature);
}

void CN105Climate::setTargetTemperatureLow(float temperature) {
    this->target_temperature_low = this->fahrenheitSupport_.normalizeHeatpumpTemperatureToUiTemperature(temperature);
}

void CN105Climate::setTargetTemperatureHigh(float temperature) {
    this->target_temperature_high = this->fahrenheitSupport_.normalizeHeatpumpTemperatureToUiTemperature(temperature);
}

void CN105Climate::setCurrentTemperature(float temperature) {
    this->current_temperature = this->fahrenheitSupport_.normalizeHeatpumpTemperatureToUiTemperature(temperature);
}

void CN105Climate::sanitizeDualSetpoints() {
    if (!this->traits_.has_feature_flags(climate::CLIMATE_REQUIRES_TWO_POINT_TARGET_TEMPERATURE)) {
        return;
    }
    ESP_LOGD(LOG_DUAL_SP_TAG, "sanitizing dual setpoints...");
    // Si une borne est NaN, la reconstruire à partir de l'autre borne ou d'une valeur raisonnable
    bool lowIsNaN = std::isnan(this->getTargetTemperatureLow());
    bool highIsNaN = std::isnan(this->getTargetTemperatureHigh());

    if (lowIsNaN && highIsNaN) {
        // Rien à faire si on n'a aucune info; essayer currentSettings.temperature si valide
        if (!std::isnan(this->currentSettings.temperature) && this->currentSettings.temperature > 0) {
            this->setTargetTemperatureLow(this->currentSettings.temperature - 2.0f);
            this->setTargetTemperatureHigh(this->currentSettings.temperature + 2.0f);
        } else {
            ESP_LOGD(LOG_DUAL_SP_TAG, "No known temperature, using default setpoints [%.1f - %.1f]",
                ESPMHP_DEFAULT_LOW_SETPOINT, ESPMHP_DEFAULT_HIGH_SETPOINT);
            this->setTargetTemperatureLow(ESPMHP_DEFAULT_LOW_SETPOINT);
            this->setTargetTemperatureHigh(ESPMHP_DEFAULT_HIGH_SETPOINT);
        }

        ESP_LOGD(LOG_DUAL_SP_TAG, "HEAT_COOL sanitized dual setpoints [%.1f - %.1f]",
            this->getTargetTemperatureLow(), this->getTargetTemperatureHigh());

        return;
    }

    if (lowIsNaN && !highIsNaN) {
        // Reconstruire low à partir de high
        this->setTargetTemperatureLow((this->mode == climate::CLIMATE_MODE_HEAT_COOL)
            ? (this->getTargetTemperatureHigh() - 4.0f)
            : this->getTargetTemperatureHigh()); // en HEAT/COOL, une seule consigne peut suffire
    } else if (!lowIsNaN && highIsNaN) {
        // Reconstruire high à partir de low
        this->setTargetTemperatureHigh((this->mode == climate::CLIMATE_MODE_HEAT_COOL)
            ? (this->getTargetTemperatureLow() + 4.0f)
            : this->getTargetTemperatureLow());
    }


    ESP_LOGD(LOG_DUAL_SP_TAG, "HEAT_COOL sanitized dual setpoints [%.1f - %.1f]",
        this->getTargetTemperatureLow(), this->getTargetTemperatureHigh());
}

void CN105Climate::debugClimate(const char* settingName) {
    ESP_LOGD(LOG_SETTINGS_TAG, "[%s]-> [mode: %s, target °C: %.1f, fan: %s, swing: %s]",
        settingName,
        LOG_STR_ARG(climate_mode_to_string(this->mode)), // Utilisation de LOG_STR_ARG
        this->getTargetTemperatureInCurrentMode(),
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
    // Déclarez un buffer (tableau de char) pour la conversion float -> string
    // 6 caractères suffisent pour "-99.9\0"
    static char outside_temp_buffer[6];

#ifdef USE_ESP32
    ESP_LOGI(LOG_STATUS_TAG, "[%s]-> [room C°: %.1f, outside C°: %s, operating: %s, compressor freq: %.1f Hz]",
        statusName,
        status.roomTemperature,
        // Utilisation de snprintf dans l'expression ternaire
        isnan(status.outsideAirTemperature)
        ? "N/A"
        : (snprintf(outside_temp_buffer, sizeof(outside_temp_buffer), "%.1f", status.outsideAirTemperature) > 0 ? outside_temp_buffer : "ERR"),
        status.operating ? "YES" : "NO ",
        status.compressorFrequency);
#else
    // Le buffer doit être dans la portée pour le bloc #else aussi
    // Si on veut qu'il soit statique, il faut le définir avant #ifdef
    // Si la définition est locale, c'est bon :

    ESP_LOGI(LOG_STATUS_TAG, "[%-*s]-> [room C°: %.1f, outside C°: %s, operating: %-*s, compressor freq: %.1f Hz]",
        15, statusName,
        status.roomTemperature,
        // Utilisation de snprintf dans l'expression ternaire
        isnan(status.outsideAirTemperature)
        ? "N/A"
        : (snprintf(outside_temp_buffer, sizeof(outside_temp_buffer), "%.1f", status.outsideAirTemperature) > 0 ? outside_temp_buffer : "ERR"),
        3, status.operating ? "YES" : "NO ",
        status.compressorFrequency);
#endif
}


void CN105Climate::debugSettingsAndStatus(const char* settingName, heatpumpSettings settings, heatpumpStatus status) {
    this->debugSettings(settingName, settings);
    this->debugStatus(settingName, status);
}



void CN105Climate::hpPacketDebug(uint8_t* packet, unsigned int length, const char* packetDirection) {
    // Construire la chaîne de sortie de façon sûre et performante
    std::string output;
    output.reserve(length * 3 + 1); // "FF " par octet

    char byteBuf[4];
    for (unsigned int i = 0; i < length; i++) {
        // Toujours borné à 3 caractères + NUL
        int written = snprintf(byteBuf, sizeof(byteBuf), "%02X ", packet[i]);
        if (written > 0) {
            output.append(byteBuf, static_cast<size_t>(written));
        }
    }

    char outputForSensor[15];
    // Tronquer proprement pour la publication éventuelle sur un capteur
    strncpy(outputForSensor, output.c_str(), sizeof(outputForSensor) - 1);
    outputForSensor[sizeof(outputForSensor) - 1] = '\0';

    /*if (strcasecmp(packetDirection, "WRITE") == 0) {
        this->last_sent_packet_sensor->publish_state(outputForSensor);
    }*/

    ESP_LOGD(packetDirection, "%s", output.c_str());
}

void CN105Climate::hpFunctionsDebug(uint8_t* packet, unsigned int length) {
    if (length < 2) return; // Pas de données à décoder

    std::string output;
    output.reserve(length * 8); // Pré-allocation pour éviter les réallocations

    char buffer[16];

    // On commence à i=1 pour sauter l'octet de commande (0x20 ou 0x22)
    for (unsigned int i = 1; i < length; i++) {
        uint8_t byte = packet[i];

        // Logique de décodage Mitsubishi (copiée de heatpumpFunctions)
        int code = ((byte >> 2) & 0xff) + 100;
        int value = byte & 3;

        // Formatage "Code:Valeur" (ex: " 102:3")
        snprintf(buffer, sizeof(buffer), " %d:%d", code, value);
        output += buffer;
    }

    // Affichage avec le tag LOG_FUNCTIONS_TAG (défini dans cn105_types.h)
    // Affiche par exemple : [FUNCTIONS] Decoded 20: 101:1 102:3 103:2 ...
    ESP_LOGD(LOG_FUNCTIONS_TAG, "Decoded %02X:%s", packet[0], output.c_str());
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
