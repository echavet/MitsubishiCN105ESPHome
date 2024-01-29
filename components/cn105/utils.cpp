#include "cn105.h"
#include "Globals.h"



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

void CN105Climate::debugSettings(const char* settingName, wantedHeatpumpSettings settings) {
#ifdef USE_ESP32
    ESP_LOGI(LOG_ACTION_EVT_TAG, "[%s]-> [power: %s, target °C: %.1f, mode: %s, fan: %s, vane: %s, wvane: %s, hasChanged ? -> %s, hasBeenSent ? -> %s]",
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
    ESP_LOGI(LOG_ACTION_EVT_TAG, "[%-*s]-> [power: %-*s, target °C: %.1f, mode: %-*s, fan: %-*s, vane: %-*s, wvane: %-*s, hasChanged ? -> %s, hasBeenSent ? -> %s]",
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

void CN105Climate::debugSettings(const char* settingName, heatpumpSettings settings) {
#ifdef USE_ESP32
    ESP_LOGI(LOG_SETTINGS_TAG, "[%s]-> [power: %s, target °C: %.1f, mode: %s, fan: %s, vane: %s, wvane: %s]",
        getIfNotNull(settingName, "unnamed"),
        getIfNotNull(settings.power, "-"),
        settings.temperature,
        getIfNotNull(settings.mode, "-"),
        getIfNotNull(settings.fan, "-"),
        getIfNotNull(settings.vane, "-"),
        getIfNotNull(settings.wideVane, "-")
    );
#else
    ESP_LOGI(LOG_SETTINGS_TAG, "[%-*s]-> [power: %-*s, target °C: %.1f, mode: %-*s, fan: %-*s, vane: %-*s, wvane: %-*s]",
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
    ESP_LOGI(LOG_STATUS_TAG, "[%s]-> [room C°: %.1f, operating: %s, compressor freq: %2d Hz]",
        statusName,
        status.roomTemperature,
        status.operating ? "YES" : "NO ",
        status.compressorFrequency);
#else
    ESP_LOGI(LOG_STATUS_TAG, "[%-*s]-> [room C°: %.1f, operating: %-*s, compressor freq: %2d Hz]",
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
    char buffer[4]; // Petit tampon pour stocker chaque octet sous forme de texte
    char outputBuffer[length * 4 + 1]; // Tampon pour stocker l'ensemble des octets sous forme de texte

    // Initialisation du tampon de sortie avec une chaîne vide
    outputBuffer[0] = '\0';

    for (unsigned int i = 0; i < length; i++) {
        snprintf(buffer, sizeof(buffer), "%02X ", packet[i]); // Utilisation de snprintf pour éviter les dépassements de tampon
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
const char* CN105Climate::lookupByteMapValue(const char* valuesMap[], const uint8_t byteMap[], int len, uint8_t byteValue) {
    for (int i = 0; i < len; i++) {
        if (byteMap[i] == byteValue) {
            return valuesMap[i];
        }
    }
    ESP_LOGW("lookup", "Attention valeur %d non trouvée, on retourne la valeur au rang 0", byteValue);
    return valuesMap[0];
}
int CN105Climate::lookupByteMapValue(const int valuesMap[], const uint8_t byteMap[], int len, uint8_t byteValue) {
    for (int i = 0; i < len; i++) {
        if (byteMap[i] == byteValue) {
            return valuesMap[i];
        }
    }
    ESP_LOGW("lookup", "Attention valeur %d non trouvée, on retourne la valeur au rang 0", byteValue);
    return valuesMap[0];
}