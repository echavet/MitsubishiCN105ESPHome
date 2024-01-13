#include "cn105.h"
#include "Globals.h"

static const char* LOG_SETTINGS_TAG = "SETTINGS"; // Logging tag
static const char* LOG_STATUS_TAG = "STATUS"; // Logging tag

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


const char* getIfNotNull(const char* what, const char* defaultValue) {
    if (what == NULL) {
        return defaultValue;
    }
    return what;
}

void CN105Climate::debugSettings(const char* settingName, heatpumpSettings settings) {

    ESP_LOGI(LOG_SETTINGS_TAG, "[%-*s]-> [power: %-*s, target °C: %2f, mode: %-*s, fan: %-*s, vane: %-*s]",
        15, getIfNotNull(settingName, "unnamed"),
        3, getIfNotNull(settings.power, "-"),
        settings.temperature,
        6, getIfNotNull(settings.mode, "-"),
        6, getIfNotNull(settings.fan, "-"),
        6, getIfNotNull(settings.vane, "-")
    );
}


void CN105Climate::debugStatus(const char* statusName, heatpumpStatus status) {

    ESP_LOGI(LOG_STATUS_TAG, "[%-*s]-> [room C°: %.1f, operating: %-*s, compressor freq: %2d Hz]",
        15, statusName,
        status.roomTemperature,
        3, status.operating ? "YES" : "NO ",
        status.compressorFrequency);

}


void CN105Climate::debugSettingsAndStatus(const char* settingName, heatpumpSettings settings, heatpumpStatus status) {
    this->debugSettings(settingName, settings);
    this->debugStatus(settingName, status);
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