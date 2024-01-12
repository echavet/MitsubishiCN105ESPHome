#include "cn105.h"

//#region heatpump_functions fonctions clim

heatpumpFunctions CN105Climate::getFunctions() {
    ESP_LOGV(TAG, "getting the list of functions...");

    functions.clear();

    byte packet1[PACKET_LEN] = {};
    byte packet2[PACKET_LEN] = {};

    prepareInfoPacket(packet1, PACKET_LEN);
    packet1[5] = FUNCTIONS_GET_PART1;
    packet1[21] = checkSum(packet1, 21);

    prepareInfoPacket(packet2, PACKET_LEN);
    packet2[5] = FUNCTIONS_GET_PART2;
    packet2[21] = checkSum(packet2, 21);

    /*while (!canSend(false)) {
        CUSTOM_DELAY(10); // esphome::CUSTOM_DELAY(10);
    }*/
    ESP_LOGD(TAG, "sending a getFunctions packet part 1");
    writePacket(packet1, PACKET_LEN);
    //readPacket();

    /*while (!canSend(false)) {
        //esphome::CUSTOM_DELAY(10);
        CUSTOM_DELAY(10);
    }*/
    ESP_LOGD(TAG, "sending a getFunctions packet part 2");
    writePacket(packet2, PACKET_LEN);
    //readPacket();

    // retry reading a few times in case responses were related
    // to other requests
    /*for (int i = 0; i < 5 && !functions.isValid(); ++i) {
        //esphome::CUSTOM_DELAY(100);
        CUSTOM_DELAY(100);
        readPacket();
    }*/

    return functions;
}

bool CN105Climate::setFunctions(heatpumpFunctions const& functions) {
    if (!functions.isValid()) {
        return false;
    }

    byte packet1[PACKET_LEN] = {};
    byte packet2[PACKET_LEN] = {};

    prepareSetPacket(packet1, PACKET_LEN);
    packet1[5] = FUNCTIONS_SET_PART1;

    prepareSetPacket(packet2, PACKET_LEN);
    packet2[5] = FUNCTIONS_SET_PART2;

    functions.getData1(&packet1[6]);
    functions.getData2(&packet2[6]);

    // sanity check, we expect data byte 15 (index 20) to be 0
    if (packet1[20] != 0 || packet2[20] != 0)
        return false;

    // make sure all the other data bytes are set
    for (int i = 6; i < 20; ++i) {
        if (packet1[i] == 0 || packet2[i] == 0)
            return false;
    }

    packet1[21] = checkSum(packet1, 21);
    packet2[21] = checkSum(packet2, 21);
    /*
        while (!canSend(false)) {
            //esphome::CUSTOM_DELAY(10);
            CUSTOM_DELAY(10);
        }*/
    ESP_LOGD(TAG, "sending a setFunctions packet part 1");
    writePacket(packet1, PACKET_LEN);
    //readPacket();

    /*while (!canSend(false)) {
        //esphome::CUSTOM_DELAY(10);
        CUSTOM_DELAY(10);
    }*/
    ESP_LOGD(TAG, "sending a setFunctions packet part 2");
    writePacket(packet2, PACKET_LEN);
    //readPacket();

    return true;
}


heatpumpFunctions::heatpumpFunctions() {
    clear();
}

bool heatpumpFunctions::isValid() const {
    return _isValid1 && _isValid2;
}

void heatpumpFunctions::setData1(byte* data) {
    memcpy(raw, data, 15);
    _isValid1 = true;
}

void heatpumpFunctions::setData2(byte* data) {
    memcpy(raw + 15, data, 15);
    _isValid2 = true;
}

void heatpumpFunctions::getData1(byte* data) const {
    memcpy(data, raw, 15);
}

void heatpumpFunctions::getData2(byte* data) const {
    memcpy(data, raw + 15, 15);
}

void heatpumpFunctions::clear() {
    memset(raw, 0, sizeof(raw));
    _isValid1 = false;
    _isValid2 = false;
}

int heatpumpFunctions::getCode(byte b) {
    return ((b >> 2) & 0xff) + 100;
}

int heatpumpFunctions::getValue(byte b) {
    return b & 3;
}

int heatpumpFunctions::getValue(int code) {
    if (code > 128 || code < 101)
        return 0;

    for (int i = 0; i < MAX_FUNCTION_CODE_COUNT; ++i) {
        if (getCode(raw[i]) == code)
            return getValue(raw[i]);
    }

    return 0;
}

bool heatpumpFunctions::setValue(int code, int value) {
    if (code > 128 || code < 101)
        return false;

    if (value < 1 || value > 3)
        return false;

    for (int i = 0; i < MAX_FUNCTION_CODE_COUNT; ++i) {
        if (getCode(raw[i]) == code) {
            raw[i] = ((code - 100) << 2) + value;
            return true;
        }
    }

    return false;
}

heatpumpFunctionCodes heatpumpFunctions::getAllCodes() {
    heatpumpFunctionCodes result;
    for (int i = 0; i < MAX_FUNCTION_CODE_COUNT; ++i) {
        int code = getCode(raw[i]);
        result.code[i] = code;
        result.valid[i] = (code >= 101 && code <= 128);
    }

    return result;
}

bool heatpumpFunctions::operator==(const heatpumpFunctions& rhs) {
    return this->isValid() == rhs.isValid() && memcmp(this->raw, rhs.raw, MAX_FUNCTION_CODE_COUNT * sizeof(int)) == 0;
}

bool heatpumpFunctions::operator!=(const heatpumpFunctions& rhs) {
    return !(*this == rhs);
}
//#endregion heatpump_functions