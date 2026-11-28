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

    this->captureStateBeforeFunctions();

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

// The save/restore decisions live in cn105::FunctionsSnapshot (functions_restore.h).
// These wrappers supply the clock and apply the outcome to the wanted settings.

void CN105Climate::discardFunctionsSnapshot() {
    this->functions_snapshot_.discard();
}

void CN105Climate::captureStateBeforeFunctions() {
    if (this->functions_snapshot_.capture(this->currentSettings, CUSTOM_MILLIS)) {
        ESP_LOGD(LOG_FUNCTIONS_TAG, "Saved unit state before functions access (power=%s mode=%s)",
            this->functions_snapshot_.saved().power, this->functions_snapshot_.saved().mode);
    }
}

void CN105Climate::openFunctionsRestoreWindow() {
    // The power-off can only be seen in a 0x02 reply, which arrives once per poll cycle:
    // leave room for one full update interval on top of the base window.
    this->functions_snapshot_.open_window(CUSTOM_MILLIS, this->update_interval_ + FUNCTIONS_RESTORE_WINDOW_MS);
}

// Called with every settings reply (0x02), before it is applied.
void CN105Climate::restoreStateAfterFunctions(const heatpumpSettings& received) {
    const auto restore = this->functions_snapshot_.on_settings(received, this->wantedSettings.hasChanged, CUSTOM_MILLIS);
    if (!restore.restore) {
        return;
    }
    ESP_LOGW(LOG_FUNCTIONS_TAG, "Unit was powered off by the functions access, restoring previous state");

    this->wantedSettings.power = restore.power;
    if (restore.mode != nullptr) this->wantedSettings.mode = restore.mode;
    if (restore.fan != nullptr) this->wantedSettings.fan = restore.fan;
    if (restore.vane != nullptr) this->wantedSettings.vane = restore.vane;
    if (restore.wide_vane != nullptr) this->wantedSettings.wideVane = restore.wide_vane;
    if (restore.set_temperature) {
        this->wantedSettings.temperature = this->calculateTemperatureSetting(restore.temperature);
    }
    this->wantedSettings.hasChanged = true;
    this->wantedSettings.hasBeenSent = false;
    this->wantedSettings.lastChange = CUSTOM_MILLIS;
}
