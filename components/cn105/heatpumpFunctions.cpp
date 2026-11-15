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
    if (this->isFunctionsWriteInFlight()) {
        return false;
    }

    uint8_t packet1[PACKET_LEN] = {};
    prepareSetPacket(packet1, PACKET_LEN);
    prepareSetPacket(packet2, PACKET_LEN);
    if (!this->profile_->encode_functions(packet1, packet2, functions)) return false;

    packet1[21] = checkSum(packet1, 21);
    packet2[21] = checkSum(packet2, 21);

    this->functions_write_phase_ = FunctionsWritePhase::AWAIT_FIRST_ACK;
    ESP_LOGD(TAG, "sending a setFunctions packet part 1");
    writePacket(packet1, PACKET_LEN);
    this->set_timeout(FUNCTIONS_ACK_TIMER, FUNCTIONS_ACK_TIMEOUT_MS, [this]() {
        this->abortFunctionsWrite();
    });
    return true;
}

void CN105Climate::onFunctionsAck() {
    this->cancel_timeout(FUNCTIONS_ACK_TIMER);

    switch (this->functions_write_phase_) {
    case FunctionsWritePhase::AWAIT_FIRST_ACK:
        this->functions_write_phase_ = FunctionsWritePhase::AWAIT_SECOND_ACK;
        ESP_LOGD(TAG, "sending a setFunctions packet part 2");
        writePacket(this->packet2, PACKET_LEN);
        this->set_timeout(FUNCTIONS_ACK_TIMER, FUNCTIONS_ACK_TIMEOUT_MS, [this]() {
            this->abortFunctionsWrite();
        });
        break;
    case FunctionsWritePhase::AWAIT_SECOND_ACK:
        this->functions_write_phase_ = FunctionsWritePhase::IDLE;
        ESP_LOGD(TAG, "setFunctions complete");
        this->isGetFunctions_ = true;
        break;
    case FunctionsWritePhase::IDLE:
        break;
    }
}
void CN105Climate::abortFunctionsWrite() {
    ESP_LOGW(TAG, "No ack for setFunctions (phase %u), aborting and re-reading settings",
        static_cast<unsigned>(this->functions_write_phase_));
    this->functions_write_phase_ = FunctionsWritePhase::IDLE;
    this->isGetFunctions_ = true;
}
