#include "cn105.h"

#include <cmath>
#include <map>

using namespace esphome;

/**
 * processInput: reads available bytes from UART and feeds them to the FrameParser.
 * When a complete frame is detected, delegates to processDataPacket().
 */


bool CN105Climate::processInput(void) {
    bool processed = false;
    while (this->get_hw_serial_()->available()) {
        processed = true;
        uint8_t inputData;
        if (this->get_hw_serial_()->read_byte(&inputData)) {
            ESP_LOGV("Decoder", "--> %02X", inputData);
            this->parser_.feed(inputData);
            if (this->parser_.frame_complete()) {
                this->processDataPacket();
                this->parser_.reset();
            }
        }
    }
    return processed;
}

/**
 * processDataPacket: called when the FrameParser has assembled a complete frame.
 * Validates checksum, sets the data pointer, and dispatches to processCommand().
 */
void CN105Climate::processDataPacket() {

    ESP_LOGV(TAG, "processing data packet...");

    // Point data at the payload section of the parser buffer
    // Note: cast away const because downstream code uses non-const data pointer
    this->data = const_cast<uint8_t*>(this->parser_.data());

    this->hpPacketDebug(this->parser_.raw(), this->parser_.frame_size(), "READ");

    // During handshake, log every received frame for diagnostics. Handshake replies are
    // logged even when connected, so stray ones (e.g. a late 0x7B) show up in the log.
    const uint8_t command = this->parser_.command();
    if (!this->isHeatpumpConnected() || command == 0x7A || command == 0x7B) {
        ESP_LOGD(LOG_CONN_TAG, "RX during handshake (cmd=0x%02X len=%d)",
            command, this->parser_.data_length());
        this->hpPacketDebug(this->parser_.raw(), this->parser_.frame_size(), LOG_CONN_TAG);
    }

    if (this->parser_.checksum_valid()) {
        ESP_LOGD("chkSum", "OK");
        // checkpoint of a heatpump response
        this->lastResponseMs = CUSTOM_MILLIS;

        // processing the specific command
        processCommand();
    } else {
        ESP_LOGW("chkSum", "KO -> checksum mismatch (cmd=0x%02X len=%d)",
            this->parser_.command(), this->parser_.data_length());
        if (!this->isHeatpumpConnected()) {
            ESP_LOGD(LOG_CONN_TAG, "Checksum KO during handshake");  // frame already dumped above
        }
    }
}

void CN105Climate::getPowerFromResponsePacket() {
    if (this->profile_->decode_submode()) this->publish_state();
}

void CN105Climate::getSettingsFromResponsePacket() {
    this->profile_->decode_settings();
}

void CN105Climate::getRoomTemperatureFromResponsePacket() {
    auto receivedStatus = this->profile_->decode_room_status(this->data, this->currentStatus);

    // Update the remote temperature control sensor (Issue 290)
    if (this->remote_temp_sensor_ != nullptr) {
        bool is_remote = false;
        if (this->remote_temp_keepalive_active_ && this->remoteTemperature_ > 0) {
            float diff = abs(receivedStatus.roomTemperature - this->remoteTemperature_);
            if (diff <= this->remote_temp_margin_) {
                is_remote = true;
            }
        }
        this->remote_temp_sensor_->publish_state(is_remote);
    }

    ESP_LOGD("Decoder", "[Room °C: %f]", receivedStatus.roomTemperature);
    ESP_LOGD("Decoder", "[OAT  °C: %f]", receivedStatus.outsideAirTemperature);
    this->statusChanged(receivedStatus);
}

void CN105Climate::getOperatingAndCompressorFreqFromResponsePacket() {
    this->profile_->decode_status();
}

void CN105Climate::getHVACOptionsFromResponsePacket() {
    this->profile_->decode_hvac_options();
}

void CN105Climate::terminateCycle() {
    if (this->shouldSendExternalTemperature_) {
        // We will receive ACK packet for this.
        // Sending WantedSettings must be delayed in this case (lastSend timestamp updated).
        ESP_LOGD(LOG_REMOTE_TEMP, "Sending remote temperature...");
        this->sendRemoteTemperature();
    }

    this->loopCycle.cycleEnded();

    this->nbCompleteCycles_++;
}
void CN105Climate::getErrorInfoFromResponsePacket() {
    if (this->error_code_sensor_ != nullptr)
        this->error_code_sensor_->publish_state(this->profile_->decode_error(this->data));
}

void CN105Climate::getDataFromResponsePacket() {

    // D'abord, laissons l'orchestrateur traiter les codes connus
    const uint8_t code = this->data[0];
    if (this->scheduler_.process_response(code)) {
        return;
    }
    // Sinon, switch pour les cas non gérés par l'orchestrateur
    switch (code) {

    case 0x04:
        // Handled by orchestrator (r_error_info onResponse → getErrorInfoFromResponsePacket)
        // Reaching here means the scheduler did not intercept this response — unexpected
        ESP_LOGW("Decoder", "[0x04] reached switch fallback — should have been handled by orchestrator");
        break; // orchestrator

    case 0x05:
        /* timer packet */
        ESP_LOGW("Decoder", "[0x05 is Timer : not implemented]");
        //this->last_received_packet_sensor->publish_state("0x62-> 0x05: Data -> Timer Packet");
        break;

    case 0x06:
        break; // orchestrator
    case 0x09:
        break; // orchestrator

    case 0x10:
        ESP_LOGD("Decoder", "[0x10 is Unknown : not implemented]");
        break;

    case 0x20: // fallthrough
    case 0x22:
        break; // orchestrator

    case 0x42:
        break; // orchestrator

    default:
        ESP_LOGW("Decoder", "packet type [%02X] <-- unknown and unexpected", data[0]);
        //this->last_received_packet_sensor->publish_state("0x62-> ?? : Data -> Unknown");
        break;
    }

}

void CN105Climate::updateSuccess() {
    ESP_LOGD(LOG_ACK, "Last heatpump data update successful!");
    // nothing can be done here because we have no mean to know wether it is an external temp ack
    // or a wantedSettings update ack
}

void CN105Climate::processCommand() {
    switch (this->parser_.command()) {
    case 0x61:  /* last update was successful */
        this->hpPacketDebug(this->parser_.raw(), this->parser_.frame_size(), LOG_ACK);
        this->updateSuccess();
        break;
    case 0x62:  /* packet contains data (room °C, settings, timer, status, or functions...)*/
        this->getDataFromResponsePacket();
        break;
    case 0x7a:  // Standard handshake
        if (this->profile_->accepts_handshake(this->parser_.command(), this->parser_.raw()[3])) {
            this->handshake_.on_standard_reply();
        } else {
            ESP_LOGW(LOG_CONN_TAG, "Ignoring unexpected handshake response for %s", this->profile_->name());
        }
        break;
    case 0x7b:  // Installer handshake
        if (this->profile_->accepts_handshake(this->parser_.command(), this->parser_.raw()[3])) {
            this->handshake_.on_installer_reply();
        } else {
            ESP_LOGW(LOG_CONN_TAG, "Ignoring unexpected installer handshake response for %s", this->profile_->name());
        }
        break;
    default:
        break;
    }
}

void CN105Climate::onHandshakeEvent(cn105::HandshakeEvent event) {
    using cn105::HandshakeEvent;
    switch (event) {
    case HandshakeEvent::SESSION_STARTED:
        ESP_LOGI(LOG_CONN_TAG, "--> %s replied: connection success (User, 0x%02X)! <--",
            this->profile_->name(),
            this->parser_.command());
        this->loopCycle.lastCompleteCycleMs = CUSTOM_MILLIS;
        // Reset cached settings so the first read after reconnect performs a full sync.
        this->currentSettings.resetSettings();
        this->currentRunStates.resetSettings();
        this->profile_->reset();
        break;
    case HandshakeEvent::INSTALLER_SKIPPED:
        ESP_LOGI(LOG_CONN_TAG, "Skipping installer handshake (0x5B): the unit declined it earlier this boot.");
        break;
    case HandshakeEvent::INSTALLER_TIMEOUT:
        ESP_LOGW(LOG_CONN_TAG, "No reply to installer handshake (0x5B). Staying on the standard session until reboot.");
        // Update lastResponseMs so we don't get a timeout and reconnect.
        this->lastResponseMs = CUSTOM_MILLIS;
        break;
    case HandshakeEvent::INSTALLER_ACTIVE:
        ESP_LOGI(LOG_CONN_TAG, "--> %s replied: installer session active (0x%02X)! <--",
            this->profile_->name(),
            this->parser_.command());
        break;
    case HandshakeEvent::UNEXPECTED_STANDARD:
        ESP_LOGW(LOG_CONN_TAG, "Ignoring stray standard handshake reply (0x7A) from %s: no CONNECT (0x5A) pending", this->profile_->name());
        break;
    case HandshakeEvent::UNEXPECTED_INSTALLER:
        ESP_LOGW(LOG_CONN_TAG, "Ignoring stray installer handshake reply (0x7B) from %s: no installer CONNECT (0x5B) pending", this->profile_->name());
        break;
    }
}

void CN105Climate::statusChanged(heatpumpStatus status) {
    if (status != currentStatus) {
        this->debugStatus("current", currentStatus);
        this->debugStatus("received", status);
        cn105::CommonProtocol::apply_status(*this, status);
    } // else no change
}


void CN105Climate::publishStateToHA(heatpumpSettings& settings) {
    this->profile_->apply_received_settings(settings);
    this->publish_state();
}

void CN105Climate::heatpumpUpdate(heatpumpSettings& settings) {
    // settings correponds to current settings
    ESP_LOGV(LOG_SETTINGS_TAG, "Settings received");
    // if received settings are different from current settings
    if (settings != this->currentSettings) {
        ESP_LOGI(LOG_SETTINGS_TAG, "Settings changed, updating HA states");
        this->debugSettings("current", this->currentSettings);
        this->debugSettings("received", settings);
        this->debugSettings("wanted", this->wantedSettings);
        this->debugClimate("climate");
        this->publishStateToHA(settings);
    }

}
