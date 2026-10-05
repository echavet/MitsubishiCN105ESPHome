#include "cn105.h"
#include "heatpumpFunctions.h"
#include "Globals.h"

using namespace esphome;
//#region heatpump_functions fonctions clim

void CN105Climate::functionsArrived() {
    if (this->profile_->allow_operation(cn105::ProfileFeature::AUXILIARY_CONTROLS)) {
        this->profile_->apply_functions();
    }
}

bool CN105Climate::setFunctions(heatpumpFunctions const& functions) {
    if (!this->profile_->allow_operation(cn105::ProfileFeature::AUXILIARY_CONTROLS)) {
        return false;
    }

    uint8_t packet1[PACKET_LEN] = {};
    uint8_t packet2[PACKET_LEN] = {};
    prepareSetPacket(packet1, PACKET_LEN);
    prepareSetPacket(packet2, PACKET_LEN);
    if (!this->profile_->encode_functions(packet1, packet2, functions)) return false;

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
