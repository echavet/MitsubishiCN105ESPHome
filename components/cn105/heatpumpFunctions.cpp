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
    prepareSetPacket(packet1, PACKET_LEN);
    prepareSetPacket(this->functions_second_packet_, PACKET_LEN);
    if (!this->profile_->encode_functions(packet1, this->functions_second_packet_, functions)) {
        return false;
    }

    packet1[21] = checkSum(packet1, 21);
    this->functions_second_packet_[21] = checkSum(this->functions_second_packet_, 21);

    // Part 2 is sent from handleHeatpumpAck() once part 1 has been acknowledged.
    if (!this->bus_gate_.begin(cn105::BusRequestKind::SET_FUNCTIONS_PART1, cn105::BusExpectedResponse::ACK, CUSTOM_MILLIS)) {
        ESP_LOGW(TAG, "setFunctions not sent: another request is waiting for its reply");
        // retry on a later pass
        this->isSetFunctions_ = true;
        return false;
    }
    ESP_LOGD(TAG, "sending a setFunctions packet part 1");
    if (!writePacket(packet1, PACKET_LEN)) {
        // Do not blindly rewrite hardware configuration after a reconnect: re-read what the unit
        // actually holds instead.
        this->isGetFunctions_ = true;
        return false;
    }

    return true;
}
