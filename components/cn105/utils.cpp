#include "cn105.h"



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